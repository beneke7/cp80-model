#include "../src/cp80_adapter.hpp"
#include "../src/demo_score.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

static std::vector<float> render(int block)
{
    constexpr int sr = 48000, total = 11 * sr;
    cp80::CP80Adapter adapter;
    adapter.prepare(sr, block);
    const auto score = cp80::makeDemoScore(sr);
    std::vector<float> output(total), buffer(block);
    std::vector<cp80::AdapterEvent> events;
    events.reserve(score.size());
    size_t next = 0;
    for (int pos = 0; pos < total; pos += block) {
        const int n = std::min(block, total - pos);
        events.clear();
        while (next < score.size() && score[next].sample < pos + n) {
            const auto& e = score[next++];
            events.push_back({e.sample - pos,
                e.on ? cp80::AdapterEventType::NoteOn : cp80::AdapterEventType::NoteOff,
                e.note, e.velocity});
        }
        adapter.process(buffer.data(), n, events.data(), int(events.size()));
        std::memcpy(output.data() + pos, buffer.data(), sizeof(float) * size_t(n));
    }
    return output;
}

int main()
{
    const auto a = render(64);
    const auto b = render(1024);
    double maxError = 0.0, sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        const double d = double(a[i]) - double(b[i]);
        maxError = std::max(maxError, std::fabs(d));
        sum += d * d;
    }
    const double rms = std::sqrt(sum / double(a.size()));
    std::printf("adapter block invariance: max %.3g, rms %.3g\n", maxError, rms);
    return maxError > 5e-6 ? 1 : 0; // block partitioning changes only round-off
}
