#include "../src/cp80.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static float level(const std::vector<float>& x, int first, int last, float f, float fs)
{
    const float pi2 = 6.2831853071795864f;
    double re = 0.0, im = 0.0, wsum = 0.0;
    for (int k = first; k < last; ++k) {
        const float w = 0.5f - 0.5f * std::cos(6.2831853071795864f * float(k - first) / float(last - first - 1));
        const float ph = pi2 * f * float(k) / fs;
        re += double(w * x[k]) * std::cos(ph);
        im -= double(w * x[k]) * std::sin(ph);
        wsum += w;
    }
    return float(2.0 * std::sqrt(re * re + im * im) / wsum);
}

int main(int argc, char** argv)
{
    const int note = argc > 1 ? std::atoi(argv[1]) : 27;
    const float velocity = argc > 2 ? std::atof(argv[2]) : 0.8f;
    const float fs = 96000.f;
    const int total = int(0.24f * fs), first = int(0.025f * fs);
    const cp80::NoteSpec s = cp80::specForNote(note);

    std::printf("note=%d f1=%.5fHz B=%.7g\n", note, s.f0, s.B);
    for (int ms : {2, 4, 6, 8, 12}) {
        cp80::Voice v;
        v.start(s, velocity, fs, fs * 0.5, 1);
        const int pulse = std::max(2, int(float(ms) * fs / 1000.f));
        std::vector<float> out(size_t(total), 0.f);
        for (int k = 0; k < total; ++k) {
            const float force = k < pulse
                ? std::sin(3.1415926535897932f * float(k) / float(pulse - 1)) : 0.f;
            for (int m = 0; m < v.nActive; ++m) {
                const float u = v.ca1[m] * v.y1[m] + v.ca2[m] * v.y2[m] + v.bIn[m] * force;
                v.y2[m] = v.y1[m]; v.y1[m] = u;
                out[k] += v.wOut[m] * u * v.outGain;
            }
        }
        const float a1 = level(out, first, total, s.f0, fs);
        std::printf("pulse=%2dms", ms);
        for (int h = 2; h <= 8; ++h) {
            const float f = s.f0 * float(h) * std::sqrt(1.f + s.B * float(h * h));
            const float a = level(out, first, total, f, fs);
            std::printf(" H%d=%+.1fdB", h, 20.f * std::log10(std::max(a, 1e-30f) / a1));
        }
        std::putchar('\n');
    }
    return 0;
}
