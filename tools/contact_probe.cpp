#include "../src/cp80.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

int main(int argc, char** argv)
{
    if (const char* o = std::getenv("CP80_ANCHORS")) cp80::loadAnchorOverrides(o);
    const int note = argc > 1 ? std::atoi(argv[1]) : 27;
    const float velocity = argc > 2 ? std::atof(argv[2]) : 0.8f;
    const double seconds = argc > 3 ? std::atof(argv[3]) : 0.2;
    const bool wave = argc > 4 && std::atoi(argv[4]) != 0;
    const float waveScale = argc > 5 ? std::atof(argv[5]) : 1.f;
    const int sr = 48000, block = 128, srInternal = sr * cp80::kOversample;
    const int traceCapacity = int(seconds * srInternal) + srInternal / 10;

    cp80::CP80 p;
    p.prepare(sr, block);
    p.setWaveContact(wave);
    p.setWaveImpedanceScale(waveScale);
    if (const char* hs = std::getenv("CP80_HAMMER_SCALE"))
        p.setHammerScale(float(std::atof(hs)));
    if (const char* st = std::getenv("CP80_STRIKE"))
        p.setStrike(float(std::atof(st)));
    if (const char* hw = std::getenv("CP80_HAMMER_WIDTH"))
        p.setHammerWidth(float(std::atof(hw)));
    if (const char* hp = std::getenv("CP80_HAMMER_P"))
        p.setHammerExponent(float(std::atof(hp)));
    if (const char* hq = std::getenv("CP80_HAMMER_RATE_P"))
        p.setHammerRateExponent(float(std::atof(hq)));
    if (const char* hc = std::getenv("CP80_HAMMER_DAMPING"))
        p.setHammerDamping(float(std::atof(hc)));
    if (const char* hf = std::getenv("CP80_HAMMER_FACING")) {
        const float tau = std::getenv("CP80_HAMMER_TAU_MS")
            ? float(std::atof(std::getenv("CP80_HAMMER_TAU_MS"))) : 0.1f;
        p.setHammerFacing(float(std::atof(hf)), tau);
    }
    std::vector<float> force(size_t(traceCapacity), 0.f), audio(block);
    p.setContactTrace(force.data(), traceCapacity);
    p.noteOn(note, velocity);
    for (int done = 0; done < int(seconds * sr); done += block) {
        std::memset(audio.data(), 0, sizeof(float) * audio.size());
        p.process(audio.data(), block);
    }

    const int samples = p.contactTraceSamples();
    float peak = 0.f;
    for (int i = 0; i < samples; ++i) if (force[i] > peak) peak = force[i];
    std::printf("note=%d velocity=%.3f wave=%d Zscale=%.3g contact_samples=%d contact_ms=%.3f peak_N=%.6g\n",
                note, velocity, wave ? 1 : 0, waveScale, samples, 1000.f * samples / srInternal, peak);

    if (argc > 6) {
        FILE* f = std::fopen(argv[6], "wb");
        if (!f) return 2;
        const size_t written = std::fwrite(force.data(), sizeof(float), size_t(samples), f);
        std::fclose(f);
        if (written != size_t(samples)) return 3;
    }
    return 0;
}
