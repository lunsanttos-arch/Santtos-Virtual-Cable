[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SampleRoot,

    [Parameter(Mandatory = $true)]
    [string]$OverlayRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Replace-Checked {
    param([string]$Path, [string]$Old, [string]$New)

    $text = [IO.File]::ReadAllText($Path)
    if (-not $text.Contains($Old)) {
        throw "Expected source fragment was not found in $Path"
    }
    [IO.File]::WriteAllText($Path, $text.Replace($Old, $New), [Text.UTF8Encoding]::new($false))
}

$main = Join-Path $SampleRoot 'Source/Main'
$filters = Join-Path $SampleRoot 'Source/Filters'
Copy-Item (Join-Path $OverlayRoot 'SanttosCableBuffer.h') $main -Force
Copy-Item (Join-Path $OverlayRoot 'SanttosCableBuffer.cpp') $main -Force

$stream = Join-Path $main 'minwavertstream.cpp'
Replace-Checked $stream '#include "minwavertstream.h"' "#include `"minwavertstream.h`"`r`n#include `"SanttosCableBuffer.h`""
Replace-Checked $stream '    KeInitializeSpinLock(&m_PositionSpinLock);' "    KeInitializeSpinLock(&m_PositionSpinLock);`r`n    SanttosCableInitialize();"
Replace-Checked $stream @'
        if (!g_DoNotCreateDataFiles)
        {
            // Read from buffer and write to a file.
            ReadBytes(ByteDisplacement);
        }
'@ @'
        // Forward the render frames to the capture endpoint.
        ReadBytes(ByteDisplacement);
'@
Replace-Checked $stream '        m_ToneGenerator.GenerateSine(m_pDmaBuffer + bufferOffset, runWrite);' '        SanttosCablePop(m_pDmaBuffer + bufferOffset, runWrite);'
Replace-Checked $stream '        m_SaveData.WriteData(m_pDmaBuffer + bufferOffset, runWrite);' '        SanttosCablePush(m_pDmaBuffer + bufferOffset, runWrite);'

$project = Join-Path $main 'Main.vcxproj'
Replace-Checked $project '    <ClCompile Include="minwavertstream.cpp" />' "    <ClCompile Include=`"minwavertstream.cpp`" />`r`n    <ClCompile Include=`"SanttosCableBuffer.cpp`" />"
Replace-Checked $project '<TargetName>SimpleAudioSample</TargetName>' '<TargetName>SanttosVirtualCable</TargetName>'

$speaker = Join-Path $filters 'speakerwavtable.h'
Replace-Checked $speaker '48KHz, 16-bit, stereo (PCM and NON-PCM)' '48KHz, 32-bit, stereo IEEE Float'
Replace-Checked $speaker '#define SPEAKER_HOST_MIN_BITS_PER_SAMPLE            16' '#define SPEAKER_HOST_MIN_BITS_PER_SAMPLE            32'
Replace-Checked $speaker '#define SPEAKER_HOST_MAX_BITS_PER_SAMPLE            16' '#define SPEAKER_HOST_MAX_BITS_PER_SAMPLE            32'
Replace-Checked $speaker '                192000,' '                384000,'
Replace-Checked $speaker '                4,' '                8,'
Replace-Checked $speaker '                16,' '                32,'
Replace-Checked $speaker '            16,' '            32,'
$speakerText = [IO.File]::ReadAllText($speaker).Replace('KSDATAFORMAT_SUBTYPE_PCM', 'KSDATAFORMAT_SUBTYPE_IEEE_FLOAT')
[IO.File]::WriteAllText($speaker, $speakerText, [Text.UTF8Encoding]::new($false))

$mic = Join-Path $filters 'micarraywavtable.h'
$micText = [IO.File]::ReadAllText($mic).Replace('KSDATAFORMAT_SUBTYPE_PCM', 'KSDATAFORMAT_SUBTYPE_IEEE_FLOAT')
[IO.File]::WriteAllText($mic, $micText, [Text.UTF8Encoding]::new($false))

$inf = Join-Path $main 'SimpleAudioSample.inx'
Replace-Checked $inf 'DriverVer   = 02/22/2016, 1.0.0.1' 'DriverVer   = 09/15/2026, 0.1.0.0'
Replace-Checked $inf 'ROOT\SimpleAudioSample' 'ROOT\SanttosVirtualCable'
Replace-Checked $inf 'ProviderName = "TODO-Set-Provider"' 'ProviderName = "Santtos Audio"'
Replace-Checked $inf 'MfgName      = "TODO-Set-Manufacturer"' 'MfgName      = "Santtos Audio"'
Replace-Checked $inf 'MsCopyRight  = "TODO-Set-Copyright"' 'MsCopyRight  = "Copyright (c) 2026 Santtos Audio"'
Replace-Checked $inf 'SIMPLEAUDIOSAMPLE_SA.DeviceDesc="Virtual Audio Device (WDM) - Simple Audio Sample"' 'SIMPLEAUDIOSAMPLE_SA.DeviceDesc="Santtos Virtual Cable"'
Replace-Checked $inf 'SimpleAudioSample.SvcDesc="Virtual Audio Device (WDM) - Simple Audio Sample Driver"' 'SimpleAudioSample.SvcDesc="Santtos Virtual Cable WaveRT Driver"'
Replace-Checked $inf 'SIMPLEAUDIOSAMPLE.WaveSpeaker.szPname="Simple Audio Sample Wave Speaker"' 'SIMPLEAUDIOSAMPLE.WaveSpeaker.szPname="Santtos Cable Input"'
Replace-Checked $inf 'SIMPLEAUDIOSAMPLE.TopologySpeaker.szPname="Simple Audio Sample Topology Speaker"' 'SIMPLEAUDIOSAMPLE.TopologySpeaker.szPname="Santtos Cable Input"'
Replace-Checked $inf 'SIMPLEAUDIOSAMPLE.WaveMicArray1.szPname="Simple Audio Sample Wave Microphone Array - Front"' 'SIMPLEAUDIOSAMPLE.WaveMicArray1.szPname="Santtos Cable Output"'
Replace-Checked $inf 'SIMPLEAUDIOSAMPLE.TopologyMicArray1.szPname="Simple Audio Sample Topology Microphone Array - Front"' 'SIMPLEAUDIOSAMPLE.TopologyMicArray1.szPname="Santtos Cable Output"'
Replace-Checked $inf 'MicArray1CustomName= "Internal Microphone Array - Front"' 'MicArray1CustomName= "Santtos Cable Output"'
$infText = [IO.File]::ReadAllText($inf)
$infText = $infText.Replace('SimpleAudioSample', 'SanttosVirtualCable')
$infText = $infText.Replace('simpleaudiosample', 'SanttosVirtualCable')
[IO.File]::WriteAllText($inf, $infText, [Text.UTF8Encoding]::new($false))
Move-Item $inf (Join-Path $main 'SanttosVirtualCable.inx') -Force

Write-Host 'Santtos WaveRT overlay applied successfully.'
