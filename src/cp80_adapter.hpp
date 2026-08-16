#pragma once

#include "cp80.hpp"

namespace cp80 {

struct AdapterParameters {
    float volume = 1.f;
    float bassDb = 0.f;
    float midDb = -8.f;
    float trebleDb = 0.f;
    int brilliance = 1;
};

enum class AdapterEventType : unsigned char { NoteOn, NoteOff, Sustain };

struct AdapterEvent {
    int offset = 0;                 // samples from the start of this block
    AdapterEventType type = AdapterEventType::NoteOn;
    int note = 0;
    float value = 0.f;              // velocity, or 0/1 for sustain
};

// Host-neutral boundary. Events must be sorted by nondecreasing offset.
class CP80Adapter {
public:
    void prepare(double sampleRate, int maxBlock)
    {
        engine.prepare(sampleRate, maxBlock);
        engine.setTone(target.bassDb, target.midDb, target.trebleDb);
        engine.setBrilliance(target.brilliance);
        engine.setVolume(1.f); // adapter owns the smoothed output gain
        applied = target;
        volume = target.volume;
    }

    void setParameters(const AdapterParameters& p) { target = p; }

    void process(float* out, int n, const AdapterEvent* events = nullptr, int count = 0)
    {
        if (!out || n <= 0) return;
        if (!events || count <= 0) count = 0;
        if (target.bassDb != applied.bassDb || target.midDb != applied.midDb ||
            target.trebleDb != applied.trebleDb) {
            engine.setTone(target.bassDb, target.midDb, target.trebleDb);
            applied.bassDb = target.bassDb;
            applied.midDb = target.midDb;
            applied.trebleDb = target.trebleDb;
        }
        if (target.brilliance != applied.brilliance) {
            engine.setBrilliance(target.brilliance);
            applied.brilliance = target.brilliance;
        }

        const float startVolume = volume;
        const float volumeStep = (target.volume - startVolume) / float(n);
        int pos = 0, eventIndex = 0;
        while (eventIndex < count && events[eventIndex].offset <= 0)
            apply(events[eventIndex++]);

        while (pos < n) {
            int end = n;
            if (eventIndex < count)
                end = std::max(pos, std::min(n, events[eventIndex].offset));
            if (end > pos) {
                engine.process(out + pos, end - pos);
                for (int i = pos; i < end; ++i)
                    out[i] *= startVolume + volumeStep * float(i + 1);
                pos = end;
            }
            while (eventIndex < count && events[eventIndex].offset <= pos)
                apply(events[eventIndex++]);
        }
        volume = target.volume;
    }

    void setBodyGainForDiagnostics(float gain) { engine.setBodyGain(gain); }
    int activeVoices() const { return engine.activeVoices(); }
    int activeModes() const { return engine.activeModes(); }

private:
    void apply(const AdapterEvent& event)
    {
        switch (event.type) {
        case AdapterEventType::NoteOn:  engine.noteOn(event.note, event.value); break;
        case AdapterEventType::NoteOff: engine.noteOff(event.note); break;
        case AdapterEventType::Sustain: engine.setSustain(event.value >= 0.5f); break;
        }
    }

    CP80 engine;
    AdapterParameters target, applied;
    float volume = 1.f;
};

} // namespace cp80
