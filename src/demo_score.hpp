#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace cp80 {

struct DemoEvent {
    int sample;
    bool on;
    int note;
    float velocity;
};

inline std::vector<DemoEvent> makeDemoScore(int sampleRate)
{
    std::vector<DemoEvent> score;
    auto chord = [&](double start, double duration,
                     std::initializer_list<int> notes, float velocity) {
        int i = 0;
        for (int note : notes) {
            score.push_back({int(std::llround((start + i * 0.012) * sampleRate)), true, note, velocity});
            score.push_back({int(std::llround((start + duration) * sampleRate)), false, note, 0.f});
            ++i;
        }
    };
    chord(0.20, 2.0, {48, 55, 64, 67, 72, 76}, 0.55f);
    chord(2.35, 2.0, {45, 52, 64, 69, 72, 74}, 0.50f);
    chord(4.50, 2.0, {46, 53, 62, 65, 69, 77}, 0.46f);
    chord(6.65, 2.6, {43, 50, 59, 65, 67, 74}, 0.42f);
    static const int upper[] = {72, 76, 79, 84, 88, 91, 84, 79};
    for (int i = 0; i < 8; ++i) {
        score.push_back({int(std::llround((6.90 + i * 0.16) * sampleRate)), true, upper[i], 0.47f});
        score.push_back({int(std::llround((7.30 + i * 0.16) * sampleRate)), false, upper[i], 0.f});
    }
    std::stable_sort(score.begin(), score.end(), [](const DemoEvent& a, const DemoEvent& b) {
        return a.sample < b.sample;
    });
    return score;
}

} // namespace cp80
