#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "SpscAudioRingBuffer.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

using santtos::audio::SpscAudioRingBuffer;

namespace {

constexpr std::size_t kSampleRate = 48000;
constexpr std::size_t kChannels = 2;
constexpr std::size_t kPeriodFrames = 96;

struct TestResult {
    bool passed{};
    std::uint64_t differentSamples{};
    std::uint64_t underrunFrames{};
    std::uint64_t overflowFrames{};
};

TestResult runBitPerfectTest() {
    constexpr std::size_t seconds = 10;
    constexpr std::size_t totalFrames = seconds * kSampleRate;
    SpscAudioRingBuffer ring(2048, kChannels, sizeof(float));
    std::array<float, kPeriodFrames * kChannels> input{};
    std::array<float, kPeriodFrames * kChannels> output{};
    std::uint64_t differentSamples = 0;

    for (std::size_t cursor = 0; cursor < totalFrames; cursor += kPeriodFrames) {
        const auto frames = (std::min)(kPeriodFrames, totalFrames - cursor);
        for (std::size_t i = 0; i < frames; ++i) {
            const auto absoluteFrame = cursor + i;
            const auto phase = 2.0 * std::numbers::pi * 1000.0 *
                               static_cast<double>(absoluteFrame) / kSampleRate;
            const auto sample = 0.9F * static_cast<float>(std::sin(phase));
            input[i * 2] = sample;
            input[i * 2 + 1] = -sample;
        }

        ring.write(input.data(), frames);
        ring.read(output.data(), frames);
        for (std::size_t i = 0; i < frames * kChannels; ++i) {
            if (std::bit_cast<std::uint32_t>(input[i]) !=
                std::bit_cast<std::uint32_t>(output[i])) {
                ++differentSamples;
            }
        }
    }

    const auto underruns = ring.underrunFrames();
    const auto overflows = ring.overflowFrames();
    return {
        differentSamples == 0 && underruns == 0 && overflows == 0,
        differentSamples,
        underruns,
        overflows
    };
}

std::wstring buildReport(const TestResult& result) {
    std::wostringstream report;
    report << L"Santtos Virtual Cable - Diagnostico do nucleo PCM\r\n\r\n"
           << L"Formato: 48 kHz / Estereo / Float32 PCM\r\n"
           << L"Bitrate interno: 3.072 kbps sem compressao\r\n"
           << L"Periodo testado: 96 frames / 2 ms\r\n"
           << L"Sinal: 1 kHz, 10 segundos, canais com polaridade oposta\r\n\r\n"
           << L"Amostras diferentes: " << result.differentSamples << L"\r\n"
           << L"Underrun (frames): " << result.underrunFrames << L"\r\n"
           << L"Overflow (frames): " << result.overflowFrames << L"\r\n\r\n"
           << (result.passed
                   ? L"RESULTADO: APROVADO - transporte bit-perfect."
                   : L"RESULTADO: REPROVADO - houve alteracao ou descontinuidade.")
           << L"\r\n\r\nObservacao: este executavel valida o nucleo. "
              L"O dispositivo de audio so aparecera apos a instalacao do driver WaveRT.";
    return report.str();
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const auto result = runBitPerfectTest();
    const auto report = buildReport(result);
    MessageBoxW(nullptr, report.c_str(), L"Santtos Cable Diagnostics",
                MB_OK | (result.passed ? MB_ICONINFORMATION : MB_ICONERROR));
    return result.passed ? 0 : 1;
}
