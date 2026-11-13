// A small, soft pad for the Standalone app, so a progression can be heard
// without any other instrument. Detuned triangles and a quiet saw through a
// gentle low-pass, slow attack, long release, a little reverb.
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace kindred {

class PadSynth {
public:
    void prepare(double sampleRate) {
        sr = sampleRate; for (auto& v : voices) v = Voice();
        juce::Reverb::Parameters rp; rp.roomSize = 0.86f; rp.damping = 0.45f; rp.wetLevel = 0.30f; rp.dryLevel = 0.62f; rp.width = 1.0f;
        reverb.setParameters(rp); reverb.setSampleRate(sr); reverb.reset();
    }
    void noteOn(int ch, int pitch, int vel, bool bass) {
        Voice* v = nullptr;
        for (auto& x : voices) if (x.active && x.pitch == pitch && x.ch == ch) { v = &x; break; }
        if (!v) for (auto& x : voices) if (!x.active) { v = &x; break; }
        if (!v) { v = &voices[0]; for (auto& x : voices) if (x.env < v->env) v = &x; }
        const double f = 440.0 * std::pow(2.0, (pitch - 69) / 12.0);
        v->active = true; v->held = true; v->pitch = pitch; v->ch = ch; v->bass = bass; v->gain = (bass ? 0.30f : 0.11f) * (0.35f + 0.65f * (float) vel / 127.0f);
        const double det = std::pow(2.0, 6.0 / 1200.0);
        v->inc[0] = f / det / sr; v->inc[1] = f * det / sr; v->inc[2] = (bass ? 2.0 * f : f) / sr;
        v->attack = (float) (1.0 / ((bass ? 0.35 : 0.9) * sr)); v->release = (float) (1.0 / ((bass ? 1.2 : 2.2) * sr));
        const double cut = bass ? 420.0 : std::min(2400.0, 500.0 + f * 1.1);
        v->lpCoef = (float) (1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * cut / sr));
    }
    void noteOff(int ch, int pitch) { for (auto& v : voices) if (v.active && v.held && v.pitch == pitch && v.ch == ch) v.held = false; }
    void allOff() { for (auto& v : voices) v.held = false; }
    void render(juce::AudioBuffer<float>& buf, int start, int num, float volume) {
        if (buf.getNumChannels() < 1 || num <= 0) return;
        float* L = buf.getWritePointer(0, start); float* R = buf.getNumChannels() > 1 ? buf.getWritePointer(1, start) : nullptr;
        for (auto& v : voices) {
            if (!v.active) continue;
            for (int i = 0; i < num; i++) {
                if (v.held) v.env = std::min(1.0f, v.env + v.attack); else { v.env -= v.release * (0.25f + v.env); if (v.env <= 0) { v.env = 0; v.active = false; break; } }
                float s;
                if (v.bass) s = (float) std::sin(v.ph[0] * juce::MathConstants<double>::twoPi) + 0.35f * tri(v.ph[2]);
                else s = tri(v.ph[0]) + tri(v.ph[1]) + 0.22f * (float) (2.0 * v.ph[2] - 1.0);
                for (int k = 0; k < 3; k++) { v.ph[k] += v.inc[k]; if (v.ph[k] >= 1.0) v.ph[k] -= 1.0; }
                v.lp += v.lpCoef * (s - v.lp);
                const float o = v.lp * v.env * v.env * v.gain * volume;
                L[i] += o; if (R) R[i] += o;
            }
        }
        if (R) reverb.processStereo(L, R, num); else reverb.processMono(L, num);
    }
private:
    static float tri(double ph) { return (float) (4.0 * std::fabs(ph - 0.5) - 1.0); }
    struct Voice { bool active = false, held = false, bass = false; int pitch = 0, ch = 0; double ph[3] = { 0, 0.33, 0.61 }, inc[3] = { 0, 0, 0 }; float env = 0, gain = 0, attack = 0, release = 0, lp = 0, lpCoef = 0.1f; };
    std::array<Voice, 24> voices;
    juce::Reverb reverb;
    double sr = 44100;
};

}  // namespace kindred
