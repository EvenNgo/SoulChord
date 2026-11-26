// Headless host test: drives SoulChordProcessor like FL Studio would and checks audio + MIDI.
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

#include <cmath>
#include <cstdio>

using namespace juce;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

struct FakeHost : public AudioPlayHead
{
    bool playing = false;
    double ppq = 0.0, bpm = 96.0;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setBpm (bpm);
        return p;
    }
};

static void pump (int ms = 120) { MessageManager::getInstance()->runDispatchLoopUntil (ms); }

struct RunStats
{
    int noteOns = 0, noteOffs = 0;
    float peak = 0.0f;
    double rms = 0.0;
    bool nan = false;
};

static RunStats run (SoulChordProcessor& p, FakeHost& host, double seconds, MidiBuffer* inject = nullptr,
                     AudioBuffer<float>* capture = nullptr)
{
    RunStats st;
    const int bs = 480;
    const double sr = 48000.0;
    AudioBuffer<float> buf (2, bs);
    const int blocks = (int) (seconds * sr / bs);
    double sum = 0;
    long count = 0;
    for (int b = 0; b < blocks; ++b)
    {
        MidiBuffer midi;
        if (inject != nullptr && b == 0) midi.swapWith (*inject);
        p.processBlock (buf, midi);
        for (const auto m : midi)
        {
            if (m.getMessage().isNoteOn()) ++st.noteOns;
            if (m.getMessage().isNoteOff()) ++st.noteOffs;
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < bs; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) st.nan = true;
                st.peak = jmax (st.peak, std::abs (v));
                sum += (double) v * v;
                ++count;
            }
        if (capture != nullptr)
        {
            const int off = b * bs;
            if (off + bs <= capture->getNumSamples())
                for (int c = 0; c < 2; ++c) capture->copyFrom (c, off, buf, c, 0, bs);
        }
        if (host.playing) host.ppq += bs / sr * host.bpm / 60.0;
    }
    st.rms = std::sqrt (sum / jmax (1L, count));
    return st;
}

static bool allSilent (SoulChordProcessor& p)
{
    for (int i = 0; i < 2; ++i)
        if (p.soundingChord[i].load() != 0 || p.soundingBass[i].load() != 0) return false;
    return true;
}

int main()
{
    ScopedJuceInitialiser_GUI init;

    SoulChordProcessor proc;
    FakeHost host;
    proc.setPlayHead (&host);
    proc.setPlayConfigDetails (0, 2, 48000.0, 480);
    proc.prepareToPlay (48000.0, 480);
    pump();

    // 1. stopped transport: silence, no notes
    {
        auto st = run (proc, host, 1.0);
        CHECK (st.noteOns == 0);
        CHECK (st.peak < 1e-6f);
    }

    // 2. every genre preset, every sound: play the loop with transport running
    for (int g = 0; g < sc::NumGenres; ++g)
    {
        proc.applyGenreDefaults (g);
        for (int s = 0; s < sc::NumSounds; ++s)
        {
            proc.setParam ("sound", (float) s);
            pump (80);
            host.playing = true;
            host.ppq = 0.0;
            auto st = run (proc, host, 6.0);
            host.playing = false;
            auto tail = run (proc, host, 1.5);
            std::printf ("genre %d sound %d: ons=%d offs=%d peak=%.3f rms=%.4f\n", g, s, st.noteOns, st.noteOffs + tail.noteOffs, st.peak, st.rms);
            CHECK (! st.nan && ! tail.nan);
            CHECK (st.noteOns > 0);
            CHECK (st.rms > 0.005);
            CHECK (st.peak <= 1.0f);
            CHECK (allSilent (proc));
            CHECK (tail.rms < st.rms);
        }
    }

    // 3. all progressions x passing types play without problems, ons/offs balanced
    proc.applyGenreDefaults (sc::GenreRnB);
    proc.setParam ("sound", 0);
    for (int prog = 0; prog <= sc::customProgressionIndex(); prog += 3)
        for (int pt = 0; pt < sc::NumPassTypes; ++pt)
        {
            proc.setParam ("prog", (float) prog);
            proc.setParam ("pass", (float) pt);
            proc.setParam ("bass", (float) (prog % sc::NumBassModes));
            proc.setParam ("rhythm", (float) (pt % sc::NumRhythms));
            pump (60);
            host.playing = true;
            host.ppq = 3.7;   // start in the middle of a chord
            auto st = run (proc, host, 3.0);
            host.playing = false;
            auto tail = run (proc, host, 0.3);
            CHECK (! st.nan);
            CHECK (st.noteOns > 0);
            CHECK (st.noteOns == st.noteOffs + tail.noteOffs);
            CHECK (allSilent (proc));
        }

    // 4. preview button runs without transport
    {
        proc.setParam ("preview", 1.0f);
        pump (60);
        auto st = run (proc, host, 3.0);
        CHECK (st.noteOns > 0);
        proc.setParam ("preview", 0.0f);
        run (proc, host, 0.2);
        CHECK (allSilent (proc));
    }

    // 5. trigger mode: one key = one chord
    {
        proc.applyGenreDefaults (sc::GenrePop);
        proc.setParam ("mode", 1.0f);
        proc.setParam ("key", 0.0f);
        proc.setParam ("scale", 0.0f);
        proc.setParam ("color", 0.0f);
        proc.setParam ("bass", 1.0f);   // root
        proc.setParam ("roll", 0.0f);
        pump (80);

        MidiBuffer in;
        in.addEvent (MidiMessage::noteOn (1, 69, (uint8) 100), 10);   // A -> Am
        AudioBuffer<float> cap (2, 48000);
        int ons = 0;
        std::vector<int> notes;
        {
            AudioBuffer<float> buf (2, 480);
            MidiBuffer midi;
            midi.swapWith (in);
            proc.processBlock (buf, midi);
            for (const auto m : midi)
                if (m.getMessage().isNoteOn()) { ++ons; notes.push_back (m.getMessage().getNoteNumber()); }
        }
        CHECK (ons == 4);   // A C E + bass A
        std::set<int> pcs;
        for (int n : notes) pcs.insert (n % 12);
        CHECK (pcs == std::set<int> ({ 9, 0, 4 }));
        CHECK ((proc.heldRelMask.load() >> 9) & 1);
        auto st = run (proc, host, 0.5);
        CHECK (st.rms > 0.003);

        // second key held at same time (G -> G), then release both
        MidiBuffer in2;
        in2.addEvent (MidiMessage::noteOn (1, 67, (uint8) 90), 0);
        run (proc, host, 0.2, &in2);
        MidiBuffer off;
        off.addEvent (MidiMessage::noteOff (1, 69), 0);
        off.addEvent (MidiMessage::noteOff (1, 67), 5);
        run (proc, host, 0.6, &off);
        CHECK (allSilent (proc));
        CHECK (proc.heldRelMask.load() == 0);

        // roll on: notes are spread over time and still released correctly
        proc.setParam ("roll", 60.0f);
        pump (80);
        MidiBuffer in3;
        in3.addEvent (MidiMessage::noteOn (1, 62, (uint8) 100), 0);
        auto r = run (proc, host, 0.4, &in3);
        CHECK (r.noteOns == 4);
        MidiBuffer off3;
        off3.addEvent (MidiMessage::noteOff (1, 62), 0);
        run (proc, host, 0.5, &off3);
        CHECK (allSilent (proc));

        // quick release before the roll finishes: no stuck notes
        MidiBuffer in4;
        in4.addEvent (MidiMessage::noteOn (1, 64, (uint8) 100), 0);
        in4.addEvent (MidiMessage::noteOff (1, 64), 200);
        run (proc, host, 0.4, &in4);
        CHECK (allSilent (proc));

        // switch back to sequencer while a key is held
        MidiBuffer in5;
        in5.addEvent (MidiMessage::noteOn (1, 60, (uint8) 100), 0);
        run (proc, host, 0.1, &in5);
        proc.setParam ("mode", 0.0f);
        run (proc, host, 0.1);
        CHECK (allSilent (proc));
    }

    // 6. MIDI export reads back
    {
        proc.applyGenreDefaults (sc::GenrePop);
        pump (80);
        for (int which = 0; which < 3; ++which)
        {
            File f = proc.writeMidiFile (which);
            CHECK (f.existsAsFile());
            FileInputStream in (f);
            MidiFile mf;
            CHECK (mf.readFrom (in));
            int ons = 0;
            for (int t = 0; t < mf.getNumTracks(); ++t)
                for (auto* e : *mf.getTrack (t))
                    if (e->message.isNoteOn()) ++ons;
            std::printf ("midi export %d: %s  notes=%d\n", which, f.getFileName().toRawUTF8(), ons);
            CHECK (ons > 0);
        }
    }

    // 7. state round trip incl. custom progression
    {
        proc.setCustomProgression ("2 5 1*2");
        proc.setParam ("prog", (float) sc::customProgressionIndex());
        proc.setParam ("color", 3.0f);
        pump (80);
        MemoryBlock mb;
        proc.getStateInformation (mb);

        SoulChordProcessor other;
        other.setStateInformation (mb.getData(), (int) mb.getSize());
        pump (150);
        CHECK (other.getCustomProgression() == "2 5 1*2");
        CHECK ((int) other.getParam ("color") == 3);
        auto P = other.getPerformance();
        CHECK (P != nullptr && P->steps.size() == 3);
        if (P) std::printf ("restored: %s %s %s\n", P->steps[0].name.c_str(), P->steps[1].name.c_str(), P->steps[2].name.c_str());

        // invalid custom text keeps the last valid loop and reports an error
        proc.setCustomProgression ("1 9");
        pump (150);
        auto Q = proc.getPerformance();
        CHECK (Q != nullptr && ! Q->error.empty() && ! Q->events.empty());
    }

    // 8. editor: create, render screenshots of both modes
    {
        proc.applyGenreDefaults (sc::GenreRnB);
        proc.setParam ("mode", 0.0f);
        pump (100);
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        pump (200);
        host.playing = true;
        host.ppq = 6.5;
        run (proc, host, 0.1);
        pump (200);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        File out ("/home/claude/shot_seq.png");
        out.deleteFile();
        { FileOutputStream os (out); PNGImageFormat().writeImageToStream (img, os); }
        host.playing = false;
        run (proc, host, 0.1);

        proc.setParam ("mode", 1.0f);
        pump (150);
        MidiBuffer in;
        in.addEvent (MidiMessage::noteOn (1, 64, (uint8) 100), 0);
        run (proc, host, 0.05, &in);
        pump (200);
        img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        File out2 ("/home/claude/shot_trig.png");
        out2.deleteFile();
        { FileOutputStream os (out2); PNGImageFormat().writeImageToStream (img, os); }
        MidiBuffer off;
        off.addEvent (MidiMessage::noteOff (1, 64), 0);
        run (proc, host, 0.3, &off);
        ed.reset();
        pump (50);
    }

    // 9. audio sample for listening: R&B loop with Rhodes
    {
        proc.applyGenreDefaults (sc::GenreRnB);
        proc.setParam ("mode", 0.0f);
        pump (100);
        host.playing = true;
        host.ppq = 0.0;
        host.bpm = 84.0;
        AudioBuffer<float> cap (2, 48000 * 14);
        run (proc, host, 14.0, nullptr, &cap);
        host.playing = false;
        File wav ("/home/claude/demo_rnb.wav");
        wav.deleteFile();
        WavAudioFormat fmt;
        std::unique_ptr<OutputStream> os (wav.createOutputStream().release());
        if (auto* w = fmt.createWriterFor (os.get(), 48000.0, 2, 16, {}, 0))
        {
            os.release();
            std::unique_ptr<AudioFormatWriter> writer (w);
            writer->writeFromAudioSampleBuffer (cap, 0, cap.getNumSamples());
        }
    }

    proc.setPlayHead (nullptr);
    std::printf (failures == 0 ? "ALL RENDER TESTS PASSED\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
