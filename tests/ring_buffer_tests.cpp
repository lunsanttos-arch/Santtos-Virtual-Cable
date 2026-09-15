#include "SpscAudioRingBuffer.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

using santtos::audio::SpscAudioRingBuffer;

static void integrityAndWrap() {
    SpscAudioRingBuffer ring(8, 2, sizeof(float));
    std::array<float, 12> first{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    std::array<float, 8> partial{};
    ring.write(first.data(), 6);
    ring.read(partial.data(), 4);
    assert(std::equal(partial.begin(), partial.end(), first.begin()));

    std::array<float, 12> second{12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
    ring.write(second.data(), 6);
    std::array<float, 16> output{};
    ring.read(output.data(), 8);

    const std::array<float, 16> expected{
        8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
    assert(output == expected);
}

static void underrunProducesSilence() {
    SpscAudioRingBuffer ring(8, 2, sizeof(std::int16_t));
    const std::array<std::int16_t, 4> input{100, -100, 200, -200};
    std::array<std::int16_t, 8> output{};
    output.fill(7);
    ring.write(input.data(), 2);
    ring.read(output.data(), 4);
    assert(std::equal(input.begin(), input.end(), output.begin()));
    assert(std::all_of(output.begin() + 4, output.end(), [](auto x) { return x == 0; }));
    assert(ring.underrunFrames() == 2);
}

static void overflowRejectsNewest() {
    SpscAudioRingBuffer ring(4, 1, sizeof(std::uint32_t));
    const std::array<std::uint32_t, 6> input{10, 11, 12, 13, 14, 15};
    std::array<std::uint32_t, 4> output{};
    ring.write(input.data(), input.size());
    ring.read(output.data(), output.size());
    const std::array<std::uint32_t, 4> expected{10, 11, 12, 13};
    assert(output == expected);
    assert(ring.overflowFrames() == 2);
}

static void sustainedSpscIntegrity() {
    constexpr std::uint32_t totalFrames = 250000;
    SpscAudioRingBuffer ring(4096, 1, sizeof(std::uint32_t));
    std::vector<std::uint32_t> output(totalFrames);

    std::thread producer([&] {
        std::uint32_t next = 0;
        while (next < totalFrames) {
            if (ring.availableFrames() > 4000) {
                std::this_thread::yield();
                continue;
            }
            ring.write(&next, 1);
            ++next;
        }
    });

    std::uint32_t received = 0;
    while (received < totalFrames) {
        if (ring.availableFrames() == 0) {
            std::this_thread::yield();
            continue;
        }
        ring.read(&output[received], 1);
        ++received;
    }
    producer.join();

    for (std::uint32_t i = 0; i < totalFrames; ++i) assert(output[i] == i);
    assert(ring.underrunFrames() == 0);
    assert(ring.overflowFrames() == 0);
}

int main() {
    integrityAndWrap();
    underrunProducesSilence();
    overflowRejectsNewest();
    sustainedSpscIntegrity();
    std::cout << "All audio ring tests passed\n";
}
