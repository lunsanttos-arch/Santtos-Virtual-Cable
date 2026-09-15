#pragma once

#include <ntddk.h>

// One frame is two IEEE-754 float samples (stereo, 32-bit each).
constexpr ULONG SANTTOS_FRAME_BYTES = 8;

void SanttosCableInitialize();
void SanttosCableReset();
void SanttosCablePush(_In_reads_bytes_(byteCount) const BYTE* source, ULONG byteCount);
void SanttosCablePop(_Out_writes_bytes_(byteCount) BYTE* destination, ULONG byteCount);

