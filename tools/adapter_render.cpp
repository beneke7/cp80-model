#include "../src/cp80_adapter.hpp"
#include "../src/demo_score.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void writeWav(const char* path, const std::vector<float>& x, int sr)
{
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::perror("fopen"); return; }
    const uint32_t bytes = uint32_t(x.size() * 2);
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(1);
    u32(uint32_t(sr)); u32(uint32_t(sr) * 2); u16(2); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    for (float v : x) {
        const float c = std::max(-1.f, std::min(1.f, v));
        u16(uint16_t(int16_t(c * 32767.f)));
    }
    std::fclose(f);
}

int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "out/adapter-demo.wav";
    constexpr int sr = 48000, block = 128;
    constexpr double seconds = 11.0;
    cp80::CP80Adapter adapter;
    cp80::AdapterParameters parameters;
    if (const char* mid = std::getenv("CP80_TONE_MID")) parameters.midDb = float(std::atof(mid));
    if (const char* bass = std::getenv("CP80_TONE_BASS")) parameters.bassDb = float(std::atof(bass));
    if (const char* treble = std::getenv("CP80_TONE_TREBLE")) parameters.trebleDb = float(std::atof(treble));
    adapter.setParameters(parameters);
    adapter.prepare(sr, block);
    if (const char* body = std::getenv("CP80_BODY_GAIN"))
        adapter.setBodyGainForDiagnostics(float(std::atof(body)));

    const auto score = cp80::makeDemoScore(sr);
    const int total = int(seconds * sr);
    std::vector<float> output(size_t(total), 0.f), blockBuffer(block);
    std::vector<cp80::AdapterEvent> events;
    events.reserve(score.size());
    size_t next = 0;
    const auto started = std::chrono::high_resolution_clock::now();
    for (int pos = 0; pos < total; pos += block) {
        const int n = std::min(block, total - pos);
        events.clear();
        while (next < score.size() && score[next].sample < pos + n) {
            const auto& e = score[next++];
            events.push_back({e.sample - pos,
                e.on ? cp80::AdapterEventType::NoteOn : cp80::AdapterEventType::NoteOff,
                e.note, e.velocity});
        }
        adapter.process(blockBuffer.data(), n, events.data(), int(events.size()));
        std::memcpy(output.data() + pos, blockBuffer.data(), sizeof(float) * size_t(n));
    }

    double peak = 0.0;
    for (float v : output) peak = std::max(peak, double(std::fabs(v)));
    if (peak > 0.0) for (float& v : output) v = float(v / peak * 0.89);
    writeWav(path, output, sr);
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now() - started).count();
    std::printf("adapter rendered %.1f s in %.3f s (%.0fx RT) -> %s\n",
                seconds, elapsed, seconds / elapsed, path);
    return 0;
}
