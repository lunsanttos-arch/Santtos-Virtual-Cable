#include "SanttosCableBuffer.h"

namespace
{
    // 32 KiB is enough to absorb scheduler jitter without imposing a fixed delay.
    // At 48 kHz, stereo Float32 this is 85.3 ms of emergency capacity.
    constexpr ULONG kCapacity = 32 * 1024;
    static_assert((kCapacity % SANTTOS_FRAME_BYTES) == 0, "frame-aligned buffer");

    KSPIN_LOCK g_lock;
    volatile LONG g_initialized = 0;
    UCHAR g_audio[kCapacity] = {};
    ULONG g_read = 0;
    ULONG g_write = 0;
    ULONG g_used = 0;

    ULONG FrameAlignDown(ULONG value)
    {
        return value - (value % SANTTOS_FRAME_BYTES);
    }

    void CopyIntoRing(const UCHAR* source, ULONG byteCount)
    {
        const ULONG first = min(byteCount, kCapacity - g_write);
        RtlCopyMemory(g_audio + g_write, source, first);
        if (byteCount > first)
        {
            RtlCopyMemory(g_audio, source + first, byteCount - first);
        }
        g_write = (g_write + byteCount) % kCapacity;
    }

    void CopyFromRing(UCHAR* destination, ULONG byteCount)
    {
        const ULONG first = min(byteCount, kCapacity - g_read);
        RtlCopyMemory(destination, g_audio + g_read, first);
        if (byteCount > first)
        {
            RtlCopyMemory(destination + first, g_audio, byteCount - first);
        }
        g_read = (g_read + byteCount) % kCapacity;
    }
}

void SanttosCableInitialize()
{
    if (InterlockedCompareExchange(&g_initialized, 1, 0) == 0)
    {
        KeInitializeSpinLock(&g_lock);
        g_read = 0;
        g_write = 0;
        g_used = 0;
        KeMemoryBarrier();
        InterlockedExchange(&g_initialized, 2);
    }
}

void SanttosCableReset()
{
    if (InterlockedCompareExchange(&g_initialized, 2, 2) != 2)
    {
        return;
    }

    KeAcquireSpinLockAtDpcLevel(&g_lock);
    g_read = 0;
    g_write = 0;
    g_used = 0;
    RtlZeroMemory(g_audio, sizeof(g_audio));
    KeReleaseSpinLockFromDpcLevel(&g_lock);
}

void SanttosCablePush(const UCHAR* source, ULONG byteCount)
{
    if (source == nullptr || InterlockedCompareExchange(&g_initialized, 2, 2) != 2)
    {
        return;
    }

    byteCount = FrameAlignDown(byteCount);
    if (byteCount == 0)
    {
        return;
    }

    // If a stalled capture client lets the buffer fill, retain the newest audio.
    if (byteCount > kCapacity)
    {
        source += byteCount - kCapacity;
        byteCount = kCapacity;
    }

    KeAcquireSpinLockAtDpcLevel(&g_lock);
    const ULONG freeBytes = kCapacity - g_used;
    if (byteCount > freeBytes)
    {
        const ULONG discard = byteCount - freeBytes;
        g_read = (g_read + discard) % kCapacity;
        g_used -= discard;
    }

    CopyIntoRing(source, byteCount);
    g_used += byteCount;
    KeReleaseSpinLockFromDpcLevel(&g_lock);
}

void SanttosCablePop(UCHAR* destination, ULONG byteCount)
{
    if (destination == nullptr || byteCount == 0)
    {
        return;
    }

    const ULONG alignedBytes = FrameAlignDown(byteCount);
    if (InterlockedCompareExchange(&g_initialized, 2, 2) != 2)
    {
        RtlZeroMemory(destination, byteCount);
        return;
    }

    KeAcquireSpinLockAtDpcLevel(&g_lock);
    const ULONG available = min(alignedBytes, g_used);
    CopyFromRing(destination, available);
    g_used -= available;
    KeReleaseSpinLockFromDpcLevel(&g_lock);

    if (available < byteCount)
    {
        RtlZeroMemory(destination + available, byteCount - available);
    }
}
