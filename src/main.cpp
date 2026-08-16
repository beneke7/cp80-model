// =====================================================================================
//  main.cpp -- demo host for cp80.hpp
//
//  build:  g++ -O3 -march=native -ffast-math -std=c++17 main.cpp -o cp80demo
//  run:    ./cp80demo out.wav
// =====================================================================================
#include "cp80.hpp"
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

    cp80::CP80 piano;
    piano.prepare(SR, BLOCK);
    if (const char* bg = std::getenv("CP80_BODY_GAIN"))
        piano.setBodyGain(float(std::atof(bg)));

    // A little Fm9 -> Bb13 -> Ebmaj9 thing, plus a single hard-struck low note at the
    // end so you can hear the bass inharmonicity on its own.
    struct Ev { double t; bool on; int note; float vel; };
    std::vector<Ev> score;
    auto chord = [&](double t, double dur, std::initializer_list<int> ns, float v) {
        int i = 0;
        for (int n : ns) {
            score.push_back({ t + i * 0.012, true,  n, v });
            score.push_back({ t + dur,       false, n, 0.f });
            ++i;
        }
    };
    // Classic CP-80 register: mid, open voicings, nothing muddy below C3.
    chord(0.20, 2.0, { 48, 55, 64, 67, 72, 76 }, 0.55f);   // Cmaj9    C3 G3 E4 G4 C5 E5
    chord(2.35, 2.0, { 45, 52, 64, 69, 72, 74 }, 0.50f);   // Am11
    chord(4.50, 2.0, { 46, 53, 62, 65, 69, 77 }, 0.46f);   // Bbmaj7#11
    chord(6.65, 2.6, { 43, 50, 59, 65, 67, 74 }, 0.42f);   // G7sus
    // a short arpeggio up top so the upper-register decay is audible on its own
    for (int i = 0; i < 8; ++i) {
        static const int up[8] = { 72, 76, 79, 84, 88, 91, 84, 79 };
        // Keep the top-run demo voices on the reference MP layer; at 0.40 they
        // were needlessly quieter than the inverse-volume comparison.
        score.push_back({ 6.90 + i * 0.16, true,  up[i], 0.47f });
        score.push_back({ 7.30 + i * 0.16, false, up[i], 0.f   });
    }
    std::sort(score.begin(), score.end(), [](const Ev& a, const Ev& b) { return a.t < b.t; });

    const int total = int(SECS * SR);
    std::vector<float> outBuf(size_t(total), 0.f);
    std::vector<float> blk(BLOCK);
    size_t ev = 0;
    int peakVoices = 0, peakModes = 0;

    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int pos = 0; pos < total; pos += BLOCK) {
        const int n = std::min(BLOCK, total - pos);
        const double tNow = double(pos) / SR;
        while (ev < score.size() && score[ev].t <= tNow) {
            if (score[ev].on) piano.noteOn(score[ev].note, score[ev].vel);
            else              piano.noteOff(score[ev].note);
            ++ev;
        }
        std::memset(blk.data(), 0, sizeof(float) * size_t(n));
        piano.process(blk.data(), n);
        std::memcpy(&outBuf[size_t(pos)], blk.data(), sizeof(float) * size_t(n));
        peakVoices = std::max(peakVoices, piano.activeVoices());
        peakModes  = std::max(peakModes,  piano.activeModes());
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
