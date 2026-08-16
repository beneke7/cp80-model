// =====================================================================================
//  main.cpp -- demo host for the SDK-free CP-80 adapter
//
//  build:  g++ -O3 -march=native -std=c++17 main.cpp -o cp80demo
//  run:    ./cp80demo out.wav
// =====================================================================================
#include "cp80_adapter.hpp"
#include "demo_score.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

// ----------------------------------------------------------------------- WAV writer
static void writeWav(const char* path, const std::vector<float>& x, int sr)
{
    FILE* f = std::fopen(path, "wb");
    if (!f) { std::perror("fopen"); return; }
    const uint32_t nB = uint32_t(x.size() * 2);
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + nB); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(1);
    u32(uint32_t(sr)); u32(uint32_t(sr) * 2); u16(2); u16(16);
    std::fwrite("data", 1, 4, f); u32(nB);
    for (float v : x) {
        float c = v < -1.f ? -1.f : (v > 1.f ? 1.f : v);
        u16(uint16_t(int16_t(c * 32767.f)));
    }
    std::fclose(f);
}

// ----------------------------------------------------------------------------- main
int main(int argc, char** argv)
{
    const char* outPath = (argc > 1) ? argv[1] : "cp80.wav";
    const int    SR    = 48000;
    const int    BLOCK = 128;
    const double SECS  = 11.0;

    cp80::CP80Adapter piano;
    cp80::AdapterParameters parameters;
    if (const char* md = std::getenv("CP80_TONE_MID")) {
        parameters.midDb = float(std::atof(md));
        if (const char* bass = std::getenv("CP80_TONE_BASS")) parameters.bassDb = float(std::atof(bass));
        if (const char* treble = std::getenv("CP80_TONE_TREBLE")) parameters.trebleDb = float(std::atof(treble));
    }
    piano.setParameters(parameters);
    piano.prepare(SR, BLOCK);
    if (const char* bg = std::getenv("CP80_BODY_GAIN"))
        piano.setBodyGainForDiagnostics(float(std::atof(bg)));

    const auto score = cp80::makeDemoScore(SR);

    const int total = int(SECS * SR);
    std::vector<float> outBuf(size_t(total), 0.f);
    std::vector<float> blk(BLOCK);
    std::vector<cp80::AdapterEvent> blockEvents;
    blockEvents.reserve(score.size());
    size_t ev = 0;
    int peakVoices = 0, peakModes = 0;

    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int pos = 0; pos < total; pos += BLOCK) {
        const int n = std::min(BLOCK, total - pos);
        blockEvents.clear();
        while (ev < score.size()) {
            if (score[ev].sample >= pos + n) break;
            blockEvents.push_back({score[ev].sample - pos,
                score[ev].on ? cp80::AdapterEventType::NoteOn : cp80::AdapterEventType::NoteOff,
                score[ev].note, score[ev].velocity});
            ++ev;
        }
        std::memset(blk.data(), 0, sizeof(float) * size_t(n));
        piano.process(blk.data(), n, blockEvents.data(), int(blockEvents.size()));
        std::memcpy(&outBuf[size_t(pos)], blk.data(), sizeof(float) * size_t(n));
        peakVoices = std::max(peakVoices, piano.activeVoices());
        peakModes  = std::max(peakModes, piano.activeModes());
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    double peak = 0.0;
    for (float v : outBuf) peak = std::max(peak, double(std::fabs(v)));
    if (peak > 0.0) for (float& v : outBuf) v = float(v / peak * 0.89);

    const double render = std::chrono::duration<double>(t1 - t0).count();
    std::printf("rendered %.1f s in %.3f s  ->  %.0fx realtime  (%.2f%% of one core)\n",
                SECS, render, SECS / render, 100.0 * render / SECS);
    std::printf("peak voices %d, peak simultaneous modes %d, raw peak %.3e\n",
                peakVoices, peakModes, peak);

    writeWav(outPath, outBuf, SR);
    std::printf("wrote %s\n", outPath);
    return 0;
}
