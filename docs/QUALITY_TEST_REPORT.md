# Santtos Virtual Cable — Audio Quality Test Report

## Result

The current PCM transport core passed every automated quality and integrity
test. For same-format audio, the output bytes are identical to the input bytes.
The measured maximum sample error is zero.

## Test profile

- Reference format: 48 kHz, stereo, IEEE Float32
- Compatibility formats: PCM16 and signed PCM24-in-32
- Transport: allocation-free SPSC circular buffer after construction
- Build: optimized C++20 with all compiler warnings treated as errors
- Runtime checks: AddressSanitizer, UndefinedBehaviorSanitizer and ThreadSanitizer

## Signal tests

| Test | Expected | Result |
|---|---|---|
| Digital silence | All output bits remain zero | PASS |
| Bipolar impulse | Same frame, level and polarity | PASS |
| 20 Hz sine | Bit-identical output | PASS |
| 1 kHz sine | Bit-identical output | PASS |
| 10 kHz sine | Bit-identical output | PASS |
| 20 kHz sine | Bit-identical output | PASS |
| 20 Hz–20 kHz logarithmic sweep | Bit-identical output | PASS |
| Left-only signal | Right channel remains digital zero | PASS |
| Float samples above 0 dBFS | No hidden clipping | PASS |
| PCM16 full range | Bit-identical output | PASS |
| PCM24-in-32 full range | Bit-identical output | PASS |
| Accelerated 24-hour stream | No mutation, underrun or overflow | PASS |

## Measurements

| Metric | Current core result |
|---|---:|
| Maximum sample error | 0 |
| Added RMS noise | 0 |
| Frequency-response alteration | 0 dB |
| Channel crosstalk | None / digital zero |
| Hidden clipping | None |
| Same-format resampling | None |
| Sanitizer-reported memory error | None |
| Sanitizer-reported undefined behavior | None |
| Sanitizer-reported data race | None |

The optimized 24-hour equivalent run completed in 4.06 seconds, or 21,294×
real time, while comparing every output block byte-for-byte. The instrumented
Address/UndefinedBehaviorSanitizer run completed the same workload without a
reported fault. ThreadSanitizer also completed the concurrent producer/consumer
test without a reported race.

Because the output is bit-identical, a finite THD+N or SNR value would be
misleading: this core adds no measurable distortion or noise. Hardware DAC,
ADC, Windows resampling, application DSP and monitoring paths are not part of
this result.

## What this does not prove yet

These tests validate the platform-independent PCM engine, not a finished
Windows audio device. They do not yet validate:

- actual WaveRT/WASAPI scheduling latency;
- latency through vMix and OBS;
- behavior under Windows DPC/ISR spikes;
- suspend/resume and device restart;
- an eight-hour real-time broadcast workload;
- Driver Verifier, HLK or signed installer behavior.

Those tests become possible only after the WaveRT endpoints are implemented
and loaded on a dedicated Windows test machine. The production gate remains a
real-time impulse measurement with p50/p95/p99 latency plus an eight-hour vMix
to OBS soak test.
