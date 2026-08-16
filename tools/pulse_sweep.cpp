#include "../src/cp80.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

// Offline only: drive the existing modal bank with a compact, asymmetric
// beta-shaped force pulse.  This isolates the force-pulse question from the
// hammer/string contact solver.
static float level(const std::vector<float>& x, int first, int last, float f, float fs)
{
    const float pi2 = 6.2831853071795864f;
    double re = 0.0, im = 0.0, wsum = 0.0;
    for (int k = first; k < last; ++k) {
        const float w = 0.5f - 0.5f * std::cos(pi2 * float(k - first) / float(last - first - 1));
        const float ph = pi2 * f * float(k) / fs;
        re += double(w * x[k]) * std::cos(ph);
        im -= double(w * x[k]) * std::sin(ph);
        wsum += w;
    }
    return float(2.0 * std::sqrt(re * re + im * im) / std::max(wsum, 1e-12));
}

static float bandDb(const std::vector<float>& x, int first, int length, float lo, float hi, float fs)
{
    std::vector<float> segment(size_t(length), 0.f);
    const int available = std::max(0, std::min(length, int(x.size()) - first));
    for (int i = 0; i < available; ++i)
        segment[size_t(i)] = x[size_t(first + i)] *
            (0.5f - 0.5f * std::cos(6.2831853071795864f * float(i) / float(length - 1)));
    double lowEnergy = 0.0, bandEnergy = 0.0;
    for (int k = 0; k <= length / 2; ++k) {
        const float f = fs * float(k) / float(length);
        double re = 0.0, im = 0.0;
        for (int i = 0; i < length; ++i) {
            const float ph = 6.2831853071795864f * float(k * i) / float(length);
            re += double(segment[size_t(i)]) * std::cos(ph);
            im -= double(segment[size_t(i)]) * std::sin(ph);
        }
        const double e = re * re + im * im;
        if (f >= 100.f && f < 2000.f) lowEnergy += e;
        if (f >= lo && f < hi) bandEnergy += e;
    }
    return 10.f * std::log10(float(std::max(bandEnergy, 1e-30) /
                                   std::max(lowEnergy, 1e-30)));
}

int main(int argc, char** argv)
{
    const int note = argc > 1 ? std::atoi(argv[1]) : 27;
    const float velocity = argc > 2 ? std::atof(argv[2]) : 0.8f;
    const float fs = 96000.f;
    const int total = int(0.24f * fs), first = int(0.025f * fs);
    const cp80::NoteSpec s = cp80::specForNote(note);
    cp80::Voice probe;
    probe.start(s, velocity, fs, fs * 0.5, 1);
    float maxModeHz = 0.f;
    for (int m = 0; m < probe.nActive; ++m)
        maxModeHz = std::max(maxModeHz, probe.wN[m] / 6.2831853071795864f);

    std::printf("# note=%d f1=%.8g B=%.8g nModes=%d maxModeHz=%.8g\n",
                note, s.f0, s.B, probe.nActive, maxModeHz);

    float durationMs = 0.f, rise = 0.f, fall = 0.f;
    while (std::cin >> durationMs >> rise >> fall) {
        if (!(durationMs > 0.f && rise > 0.f && fall > 0.f)) continue;
        cp80::Voice v;
        v.start(s, velocity, fs, fs * 0.5, 1);
        const int pulse = std::max(2, int(durationMs * fs / 1000.f + 0.5f));
        const float peakU = rise / (rise + fall);
        const float peak = std::pow(peakU, rise) * std::pow(1.f - peakU, fall);
        std::vector<float> out(size_t(total), 0.f);

        for (int k = 0; k < total; ++k) {
            float force = 0.f;
            if (k < pulse) {
                const float u = float(k) / float(pulse - 1);
                const float shape = std::pow(std::max(u, 1e-12f), rise) *
                                    std::pow(std::max(1.f - u, 1e-12f), fall);
                force = shape / peak;
            }
            for (int m = 0; m < v.nActive; ++m) {
                const float u = v.ca1[m] * v.y1[m] + v.ca2[m] * v.y2[m] + v.bIn[m] * force;
                v.y2[m] = v.y1[m]; v.y1[m] = u;
                out[k] += v.wOut[m] * u * v.outGain;
            }
        }

        const float a1 = level(out, first, total, s.f0, fs);
        std::printf("%.6g %.6g %.6g", durationMs, rise, fall);
        for (int h = 2; h <= 8; ++h) {
            const float f = s.f0 * float(h) * std::sqrt(1.f + s.B * float(h * h));
            const float ah = level(out, first, total, f, fs);
            std::printf(" %+.6f", 20.f * std::log10(std::max(ah, 1e-30f) / std::max(a1, 1e-30f)));
        }
        if (std::getenv("CP80_PULSE_BANDS"))
            std::printf(" %+.6f %+.6f", bandDb(out, 0, 2048, 3000.f, 8000.f, fs),
                        bandDb(out, 0, 2048, 8000.f, 18000.f, fs));
        std::putchar('\n');
    }
    return 0;
}
