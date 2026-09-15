# Santtos Virtual Cable

Virtual audio cable for Windows 10/11 x64, designed for broadcast workflows
with vMix and OBS. Audio stays local and uncompressed.

## Target profile

- Default: stereo, 48 kHz, IEEE 32-bit float PCM
- Compatibility: stereo 48 kHz, 16-bit and 24-bit PCM
- Optional studio profile: stereo 96 kHz, IEEE 32-bit float PCM
- No codec and no network transport
- WaveRT render endpoint: `Santtos Cable Input`
- WaveRT capture endpoint: `Santtos Cable Output`
- Event-driven transfer, RAW mode, no APO in the signal path
- Selectable periods: 48, 96, 144, 240 and 480 frames at 48 kHz

At 48 kHz stereo, 32-bit float transfers 3,072 kbit/s. The earlier 380 kbit/s
requirement is therefore exceeded without compression or generational loss.

## Latency targets

| Mode | Period at 48 kHz | Virtual cable target* |
|---|---:|---:|
| Ultra | 48 frames / 1 ms | 2-4 ms |
| Live | 96 frames / 2 ms | 4-6 ms |
| Safe | 240 frames / 5 ms | 8-12 ms |
| Compatible | 480 frames / 10 ms | 15-25 ms |

\* Targets for the cable path, measured by impulse correlation. Application,
hardware output and monitoring buffers add their own latency.

`Live` is the intended default. `Ultra` is opt-in because DPC/ISR spikes on a
busy broadcast PC can cause clicks at a 1 ms period.

## Repository layout

- `docs/ARCHITECTURE.md`: implementation and verification specification
- `docs/QUALITY_TEST_REPORT.md`: current signal-quality results and limits
- `src/core/SpscAudioRingBuffer.hpp`: allocation-free, lock-free SPSC PCM ring
- `tests/ring_buffer_tests.cpp`: integrity, wrap, underrun and overflow tests
- `app/SanttosCableDiagnostics.cpp`: Windows bit-perfect diagnostic executable
- `.github/workflows/windows-build.yml`: automatic Windows x64 build and tests

## Portable core test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The WaveRT driver must be built on Windows with Visual Studio, SDK and WDK.
Public installation also requires Microsoft driver signing.

## Windows executable

Every push to `main` runs the complete test suite on Windows Server 2022 and
publishes the `Santtos-Virtual-Cable-Windows-x64` build artifact. It currently
contains `SanttosCableDiagnostics.exe`, which validates the PCM engine on
Windows. It is deliberately labelled as a diagnostic: the virtual playback and
capture devices require the separate WaveRT driver package still under
development.
