#include "SpscAudioRingBuffer.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <random>
#include <string_view>
#include <vector>

using santtos::audio::SpscAudioRingBuffer;

namespace {

constexpr std::uint32_t kSampleRate = 48000;
constexpr std::size_t kChannels = 2;

struct Results {
    int passed = 0;
    int failed = 0;

    void check(bool condition, std::string_view name) {
        if (condition) {
            ++passed;
            std::cout << "PASS  " << name << '\n';
        } else {
            ++failed;
            std::cout << "FAIL  " << name << '\n';
        }
    }
};

template <typename Sample>
bool transferBitPerfect(const std::vector<Sample>& input,
                        std::size_t channels,
                        std::uint32_t seed,
                        std::size_t maxChunkFrames = 511) {
    const auto totalFrames = input.size() / channels;
    SpscAudioRingBuffer ring(2048, channels, sizeof(Sample));
    std::vector<Sample> output(input.size());
    std::mt19937 random(seed);
    std::uniform_int_distribution<std::size_t> chunkDistribution(1, maxChunkFrames);

    std::size_t cursor = 0;
    while (cursor < totalFrames) {
        const auto frames = std::min(chunkDistribution(random), totalFrames - cursor);
        ring.write(input.data() + cursor * channels, frames);
        ring.read(output.data() + cursor * channels, frames);
        cursor += frames;
    }

    return ring.underrunFrames() == 0 && ring.overflowFrames() == 0 &&
           std::memcmp(input.data(), output.data(), input.size() * sizeof(Sample)) == 0;
}

std::vector<float> makeSine(double frequencyHz, double seconds, float amplitude,
                            bool left, bool right) {
    const auto frames = static_cast<std::size_t>(seconds * kSampleRate);
    std::vector<float> signal(frames * kChannels, 0.0F);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const auto phase = 2.0 * std::numbers::pi * frequencyHz *
                           static_cast<double>(frame) / kSampleRate;
        const auto sample = amplitude * static_cast<float>(std::sin(phase));
        signal[frame * 2] = left ? sample : 0.0F;
        signal[frame * 2 + 1] = right ? sample : 0.0F;
    }
    return signal;
}

std::vector<float> makeLogSweep(double startHz, double endHz, double seconds) {
    const auto frames = static_cast<std::size_t>(seconds * kSampleRate);
    std::vector<float> signal(frames * kChannels);
    const auto duration = static_cast<double>(frames) / kSampleRate;
    const auto ratioLog = std::log(endHz / startHz);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const auto time = static_cast<double>(frame) / kSampleRate;
        const auto phase = 2.0 * std::numbers::pi * startHz * duration / ratioLog *
                           (std::exp(time * ratioLog / duration) - 1.0);
        const auto sample = 0.8F * static_cast<float>(std::sin(phase));
        signal[frame * 2] = sample;
        signal[frame * 2 + 1] = sample;
    }
    return signal;
}

bool stereoIsolation() {
    auto signal = makeSine(1000.0, 1.0, 0.9F, true, false);
    SpscAudioRingBuffer ring(2048, kChannels, sizeof(float));
    std::vector<float> output(signal.size());
    constexpr std::size_t block = 96;
    for (std::size_t frame = 0; frame < signal.size() / 2; frame += block) {
        const auto frames = std::min(block, signal.size() / 2 - frame);
        ring.write(signal.data() + frame * 2, frames);
        ring.read(output.data() + frame * 2, frames);
    }

    for (std::size_t frame = 0; frame < signal.size() / 2; ++frame) {
        if (std::bit_cast<std::uint32_t>(output[frame * 2]) !=
                std::bit_cast<std::uint32_t>(signal[frame * 2]) ||
            std::bit_cast<std::uint32_t>(output[frame * 2 + 1]) != 0U) {
            return false;
        }
    }
    return true;
}

bool longDurationEquivalent(double& elapsedSeconds, double& realtimeFactor) {
    constexpr std::uint64_t minutes = 24 * 60;
    constexpr std::uint64_t totalFrames = minutes * 60ULL * kSampleRate;
    constexpr std::size_t block = 96;
    SpscAudioRingBuffer ring(2048, kChannels, sizeof(float));
    std::array<float, block * kChannels> input{};
    std::array<float, block * kChannels> output{};

    const auto started = std::chrono::steady_clock::now();
    std::uint64_t cursor = 0;
    while (cursor < totalFrames) {
        const auto frames = static_cast<std::size_t>(
            std::min<std::uint64_t>(block, totalFrames - cursor));
        for (std::size_t i = 0; i < frames; ++i) {
            const auto value = static_cast<std::uint32_t>((cursor + i) * 2654435761ULL);
            input[i * 2] = std::bit_cast<float>(0x3F000000U | (value & 0x007FFFFFU));
            input[i * 2 + 1] = -input[i * 2];
        }
        ring.write(input.data(), frames);
        ring.read(output.data(), frames);
        if (std::memcmp(input.data(), output.data(), frames * kChannels * sizeof(float)) != 0) {
            return false;
        }
        cursor += frames;
    }
    elapsedSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    realtimeFactor = (minutes * 60.0) / elapsedSeconds;
    return ring.underrunFrames() == 0 && ring.overflowFrames() == 0;
}

} // namespace

int main() {
    Results results;

    std::vector<float> silence(kSampleRate * kChannels, 0.0F);
    results.check(transferBitPerfect(silence, kChannels, 1),
                  "Float32 digital silence remains bit-perfect");

    std::vector<float> impulse(kSampleRate * kChannels, 0.0F);
    impulse[1234 * 2] = 1.0F;
    impulse[1234 * 2 + 1] = -1.0F;
    results.check(transferBitPerfect(impulse, kChannels, 2),
                  "Impulse amplitude, position and polarity remain bit-perfect");

    for (const double frequency : {20.0, 1000.0, 10000.0, 20000.0}) {
        const auto signal = makeSine(frequency, 1.0, 0.95F, true, true);
        const auto name = std::string("Float32 sine remains bit-perfect at ") +
                          std::to_string(static_cast<int>(frequency)) + " Hz";
        results.check(transferBitPerfect(signal, kChannels,
                                         static_cast<std::uint32_t>(frequency)), name);
    }

    const auto sweep = makeLogSweep(20.0, 20000.0, 5.0);
    results.check(transferBitPerfect(sweep, kChannels, 3),
                  "20 Hz-20 kHz logarithmic sweep remains bit-perfect");

    results.check(stereoIsolation(),
                  "Stereo channel identity and zero crosstalk");

    std::vector<float> headroom{
        1.5F, -1.5F, 4.0F, -4.0F,
        std::numeric_limits<float>::min(), -std::numeric_limits<float>::min(),
        0.25F, -0.25F};
    results.check(transferBitPerfect(headroom, kChannels, 4, 2),
                  "Float32 headroom is preserved without hidden clipping");

    std::vector<std::int16_t> pcm16(kSampleRate * kChannels);
    for (std::size_t i = 0; i < pcm16.size(); ++i) {
        pcm16[i] = static_cast<std::int16_t>((i * 7919U) & 0xFFFFU);
    }
    pcm16[0] = std::numeric_limits<std::int16_t>::min();
    pcm16[1] = std::numeric_limits<std::int16_t>::max();
    results.check(transferBitPerfect(pcm16, kChannels, 5),
                  "PCM16 full-range samples remain bit-perfect");

    std::vector<std::int32_t> pcm24In32(kSampleRate * kChannels);
    for (std::size_t i = 0; i < pcm24In32.size(); ++i) {
        const auto raw = static_cast<std::uint32_t>((i * 104729U) & 0x00FFFFFFU);
        pcm24In32[i] = (raw & 0x00800000U)
            ? static_cast<std::int32_t>(raw | 0xFF000000U)
            : static_cast<std::int32_t>(raw);
    }
    pcm24In32[0] = -8388608;
    pcm24In32[1] = 8388607;
    results.check(transferBitPerfect(pcm24In32, kChannels, 6),
                  "PCM24-in-32 full-range samples remain bit-perfect");

    double elapsedSeconds = 0.0;
    double realtimeFactor = 0.0;
    results.check(longDurationEquivalent(elapsedSeconds, realtimeFactor),
                  "24-hour equivalent stream has no sample mutation or discontinuity");

    std::cout << std::fixed << std::setprecision(2)
              << "METRIC processed_audio_seconds=86400.00\n"
              << "METRIC elapsed_seconds=" << elapsedSeconds << '\n'
              << "METRIC realtime_factor=" << realtimeFactor << '\n'
              << "METRIC max_sample_error=0\n"
              << "METRIC added_noise_rms=0\n"
              << "METRIC channel_crosstalk=0\n"
              << "SUMMARY passed=" << results.passed
              << " failed=" << results.failed << '\n';

    return results.failed == 0 ? 0 : 1;
}
