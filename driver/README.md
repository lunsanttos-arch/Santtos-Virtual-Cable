# Santtos Virtual Cable — driver WaveRT

This directory contains the Santtos-specific data path and a reproducible patch
for Microsoft's official `audio/simpleaudiosample` WaveRT driver sample.

## Audio path

- Render endpoint: **Santtos Cable Input**
- Capture endpoint: **Santtos Cable Output**
- Fixed native format: 48 kHz, stereo, IEEE Float32
- Payload: 3,072 kbit/s, uncompressed
- Transport: nonpaged in-kernel FIFO; no network and no codec
- Event-driven WaveRT timer: 1 ms

Render frames are copied unchanged into the FIFO. Capture consumes those exact
bytes; an underrun is filled with digital silence and an overflow drops the
oldest complete stereo frames.

## Reproducible source

The CI workflow checks out Microsoft `Windows-driver-samples` at commit
`97429c5623590d52f001249460daf43e6749d777`, applies
`patch-simple-audio-sample.ps1`, restores the official WDK NuGet packages, and
builds only `audio.simpleaudiosample` for Release/x64.

## Safety and signing

The CI artifact is an engineering/test package, not a Microsoft production-
signed release. Do not install it on a production or live-broadcast machine.
Kernel-driver installation and latency measurement will be done as a separate,
explicitly controlled test after the package builds successfully.

