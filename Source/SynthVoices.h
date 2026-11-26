// SoulChord - built-in sounds: Piano, Rhodes (FM electric piano), Pad, Bell, plus a sub bass.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <cmath>

namespace sc
{
struct ScSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

//==============================================================================
class ChordVoice : public juce::SynthesiserVoice
{
public:
    explicit ChordVoice (std::atomic<int>& soundSel) : sound (soundSel) {}

    using juce::SynthesiserVoice::renderNextBlock;
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<ScSound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        const double sr = getSampleRate();
        type = sound.load();
        vel = velocity;
        freq = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        gain = 0.2f * std::pow (velocity, 1.3f);

        const double pitchScale = std::pow (2.0, -(midiNote - 60) / 24.0);

        juce::ADSR::Parameters p;
        switch (type)
        {
            case 1: p = { 0.002f, 0.1f, 1.0f, 0.35f }; break;   // Rhodes
            case 2: p = { 0.35f, 0.6f, 0.8f, 0.9f }; break;     // Pad
            case 3: p = { 0.001f, 0.1f, 1.0f, 0.7f }; break;    // Bell
            default: p = { 0.002f, 0.1f, 1.0f, 0.22f }; break;  // Piano
        }
        env.setSampleRate (sr);
        env.setParameters (p);
        env.reset();
        env.noteOn();

        for (int k = 0; k < kPartials; ++k)
        {
            phase[k] = 0.0;
            const int h = k + 1;
            const double B = 0.00035;
            inc[k] = juce::MathConstants<double>::twoPi * freq * h * std::sqrt (1.0 + B * h * h) / sr;
            const double tau = 1.8 * pitchScale / (1.0 + 0.55 * k);
            amp[k] = std::pow ((double) h, -(1.7 - 0.8 * velocity));
            mul[k] = std::exp (-1.0 / (std::max (0.05, tau) * sr));
            if (inc[k] > juce::MathConstants<double>::pi) amp[k] = 0.0;   // above Nyquist
        }
        double norm = 0;
        for (int k = 0; k < kPartials; ++k) norm += amp[k];
        for (int k = 0; k < kPartials; ++k) amp[k] /= (norm > 0 ? norm : 1.0);

        // FM / generic
        carInc = juce::MathConstants<double>::twoPi * freq / sr;
        carPh = modPh = bellPh = 0.0;
        ampEnv = 1.0;
        modEnv = 1.0;
        bellEnv = 1.0;
        ampMul = std::exp (-1.0 / ((type == 3 ? 2.2 : 2.6) * pitchScale * sr));
        modMul = std::exp (-1.0 / ((type == 3 ? 0.9 : 0.18) * sr));
        bellMul = std::exp (-1.0 / (0.05 * sr));

        // Pad
        for (int i = 0; i < 3; ++i) sawPh[i] = juce::Random::getSystemRandom().nextDouble();
        const double det[3] = { -0.07, 0.0, 0.065 };
        for (int i = 0; i < 3; ++i) sawInc[i] = freq * std::pow (2.0, det[i] / 12.0) / sr;
        lp1 = lp2 = 0.0;
        const double cutoff = std::min (0.45 * sr, 900.0 + 2600.0 * velocity);
        lpA = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * cutoff / sr);
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) env.noteOff();
        else { env.reset(); clearCurrentNote(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive()) return;
        const int chans = out.getNumChannels();

        for (int i = 0; i < num; ++i)
        {
            const double s = nextSample();
            const float e = env.getNextSample();
            const float v = (float) s * e * gain;
            for (int c = 0; c < chans; ++c) out.addSample (c, start + i, v);

            if (! env.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    static constexpr int kPartials = 8;

    static double polyBlep (double t, double dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    double nextSample()
    {
        switch (type)
        {
            case 1:   // Rhodes: 1:1 FM with fast decaying index + short tine "ping"
            {
                const double idx = (0.6 + 2.2 * vel) * modEnv + 0.25;
                const double m = std::sin (modPh);
                double y = std::sin (carPh + idx * m) * ampEnv;
                y += 0.18 * vel * std::sin (bellPh) * bellEnv;
                carPh += carInc; modPh += carInc; bellPh += carInc * 14.0;
                ampEnv *= ampMul; modEnv *= modMul; bellEnv *= bellMul;
                wrap (carPh); wrap (modPh); wrap (bellPh);
                return y * 0.9;
            }
            case 2:   // Pad: 3 detuned saws, 2-pole lowpass
            {
                double y = 0;
                for (int k = 0; k < 3; ++k)
                {
                    double saw = 2.0 * sawPh[k] - 1.0;
                    saw -= polyBlep (sawPh[k], sawInc[k]);
                    y += saw;
                    sawPh[k] += sawInc[k];
                    if (sawPh[k] >= 1.0) sawPh[k] -= 1.0;
                }
                y /= 3.0;
                lp1 += lpA * (y - lp1);
                lp2 += lpA * (lp1 - lp2);
                return lp2 * 0.9;
            }
            case 3:   // Bell: inharmonic FM
            {
                const double idx = 2.5 * modEnv * (0.4 + vel);
                double y = std::sin (carPh + idx * std::sin (modPh)) * ampEnv;
                carPh += carInc; modPh += carInc * 3.5;
                ampEnv *= ampMul; modEnv *= modMul;
                wrap (carPh); wrap (modPh);
                return y * 0.7;
            }
            default:  // Piano: decaying, slightly inharmonic partials
            {
                double y = 0;
                for (int k = 0; k < kPartials; ++k)
                {
                    if (amp[k] < 1e-6) continue;
                    y += amp[k] * std::sin (phase[k]);
                    phase[k] += inc[k];
                    wrap (phase[k]);
                    amp[k] *= mul[k];
                }
                return y * 1.6;
            }
        }
    }

    static void wrap (double& ph)
    {
        if (ph > juce::MathConstants<double>::twoPi) ph -= juce::MathConstants<double>::twoPi;
    }

    std::atomic<int>& sound;
    int type = 0;
    float vel = 0.8f, gain = 0.1f;
    double freq = 440.0;
    juce::ADSR env;

    double phase[kPartials] {}, inc[kPartials] {}, amp[kPartials] {}, mul[kPartials] {};
    double carInc = 0, carPh = 0, modPh = 0, bellPh = 0, ampEnv = 1, modEnv = 1, bellEnv = 1;
    double ampMul = 1, modMul = 1, bellMul = 1;
    double sawPh[3] {}, sawInc[3] {}, lp1 = 0, lp2 = 0, lpA = 0.1;
};

//==============================================================================
class BassVoice : public juce::SynthesiserVoice
{
public:
    using juce::SynthesiserVoice::renderNextBlock;
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<ScSound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        const double sr = getSampleRate();
        inc = juce::MathConstants<double>::twoPi * juce::MidiMessage::getMidiNoteInHertz (midiNote) / sr;
        ph = 0.0;
        gain = 0.24f * std::pow (velocity, 1.1f);
        env.setSampleRate (sr);
        env.setParameters ({ 0.004f, 0.35f, 0.7f, 0.07f });
        env.reset();
        env.noteOn();
        click = 1.0;
        clickMul = std::exp (-1.0 / (0.012 * sr));
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) env.noteOff();
        else { env.reset(); clearCurrentNote(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive()) return;
        const int chans = out.getNumChannels();
        for (int i = 0; i < num; ++i)
        {
            double y = std::sin (ph) + 0.22 * std::sin (2.0 * ph) + 0.35 * click * std::sin (3.0 * ph);
            y = std::tanh (1.5 * y) / std::tanh (1.5);
            ph += inc;
            if (ph > juce::MathConstants<double>::twoPi) ph -= juce::MathConstants<double>::twoPi;
            click *= clickMul;
            const float v = (float) y * env.getNextSample() * gain;
            for (int c = 0; c < chans; ++c) out.addSample (c, start + i, v);
            if (! env.isActive()) { clearCurrentNote(); break; }
        }
    }

private:
    double inc = 0, ph = 0, click = 0, clickMul = 0;
    float gain = 0.3f;
    juce::ADSR env;
};
} // namespace sc
