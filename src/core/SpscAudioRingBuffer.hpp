#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace santtos::audio {

// Portable reference implementation for the WaveRT driver's nonpaged PCM ring.
// One producer and one consumer only. Capacity is expressed in complete frames.
class SpscAudioRingBuffer final {
public:
    SpscAudioRingBuffer(std::size_t capacityFrames,
                        std::size_t channels,
                        std::size_t bytesPerSample)
        : capacityFrames_(capacityFrames),
          frameBytes_(channels * bytesPerSample),
          storage_(capacityFrames * frameBytes_, std::byte{0}) {
        if (capacityFrames < 2 || channels == 0 || bytesPerSample == 0) {
            throw std::invalid_argument("invalid audio ring dimensions");
        }
    }

    [[nodiscard]] std::size_t capacityFrames() const noexcept {
        return capacityFrames_;
    }

    [[nodiscard]] std::size_t availableFrames() const noexcept {
        const auto write = writeFrame_.load(std::memory_order_acquire);
        const auto read = readFrame_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(write - read);
    }

    // The producer never changes the consumer-owned read index. Incoming frames
    // that do not fit are rejected and counted, preserving SPSC correctness.
    void write(const void* source, std::size_t frames) noexcept {
        if (frames == 0) return;

        auto write = writeFrame_.load(std::memory_order_relaxed);
        auto read = readFrame_.load(std::memory_order_acquire);

        const auto used = static_cast<std::size_t>(write - read);
        const auto free = capacityFrames_ - std::min(used, capacityFrames_);
        if (frames > free) {
            overflowFrames_.fetch_add(frames - free, std::memory_order_relaxed);
            frames = free;
        }

        if (frames == 0) return;
        copyIntoRing(static_cast<const std::byte*>(source), write, frames);
        writeFrame_.store(write + frames, std::memory_order_release);
    }

    // Always fills the requested destination. Missing frames are digital zero.
    void read(void* destination, std::size_t frames) noexcept {
        if (frames == 0) return;

        auto read = readFrame_.load(std::memory_order_relaxed);
        const auto write = writeFrame_.load(std::memory_order_acquire);
        const auto available = static_cast<std::size_t>(write - read);
        const auto copied = std::min(frames, available);

        auto* destinationBytes = static_cast<std::byte*>(destination);
        copyFromRing(destinationBytes, read, copied);
        if (copied < frames) {
            const auto missing = frames - copied;
            std::memset(destinationBytes + copied * frameBytes_, 0,
                        missing * frameBytes_);
            underrunFrames_.fetch_add(missing, std::memory_order_relaxed);
        }
        readFrame_.store(read + copied, std::memory_order_release);
    }

    void reset() noexcept {
        readFrame_.store(0, std::memory_order_release);
        writeFrame_.store(0, std::memory_order_release);
        underrunFrames_.store(0, std::memory_order_relaxed);
        overflowFrames_.store(0, std::memory_order_relaxed);
        std::fill(storage_.begin(), storage_.end(), std::byte{0});
    }

    [[nodiscard]] std::uint64_t underrunFrames() const noexcept {
        return underrunFrames_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t overflowFrames() const noexcept {
        return overflowFrames_.load(std::memory_order_relaxed);
    }

private:
    void copyIntoRing(const std::byte* source,
                      std::uint64_t absoluteFrame,
                      std::size_t frames) noexcept {
        const auto offset = static_cast<std::size_t>(absoluteFrame % capacityFrames_);
        const auto first = std::min(frames, capacityFrames_ - offset);
        std::memcpy(storage_.data() + offset * frameBytes_, source,
                    first * frameBytes_);
        std::memcpy(storage_.data(), source + first * frameBytes_,
                    (frames - first) * frameBytes_);
    }

    void copyFromRing(std::byte* destination,
                      std::uint64_t absoluteFrame,
                      std::size_t frames) noexcept {
        const auto offset = static_cast<std::size_t>(absoluteFrame % capacityFrames_);
        const auto first = std::min(frames, capacityFrames_ - offset);
        std::memcpy(destination, storage_.data() + offset * frameBytes_,
                    first * frameBytes_);
        std::memcpy(destination + first * frameBytes_, storage_.data(),
                    (frames - first) * frameBytes_);
    }

    const std::size_t capacityFrames_;
    const std::size_t frameBytes_;
    std::vector<std::byte> storage_;
    alignas(64) std::atomic<std::uint64_t> readFrame_{0};
    alignas(64) std::atomic<std::uint64_t> writeFrame_{0};
    std::atomic<std::uint64_t> underrunFrames_{0};
    std::atomic<std::uint64_t> overflowFrames_{0};
};

} // namespace santtos::audio
