// Unit tests for the theory core: g++ -std=c++17 -I../Source theory_test.cpp ../Source/Theory.cpp ../Source/Arranger.cpp
#include "Arranger.h"
#include "Theory.h"

#include <cmath>
#include <cstdio>
#include <set>

using namespace sc;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (! (_a == _b)) { std::printf ("FAIL %s:%d  %s == %s  (got '%s')\n", __FILE__, __LINE__, #a, #b, std::string (_a).c_str()); ++failures; } } while (0)

static std::string nameOf (const char* prog, int idx, int key, int scale, int color)
{
    std::vector<Step> steps;
    std::string err;
    parseProgression (prog, steps, err);
    return chordName (buildChord (key, scale, steps[idx], color), preferFlats (key, scale));
}

static std::string names (const char* prog, int key, int scale, int color)
{
    std::vector<Step> steps;
    std::string err;
    if (! parseProgression (prog, steps, err)) return "ERR:" + err;
    std::string out;
    for (auto& s : steps)
    {
        if (! out.empty()) out += " ";
        out += chordName (buildChord (key, scale, s, color), preferFlats (key, scale));
    }
    return out;
}

int main()
{
    // --- diatonic triads / sevenths
    CHECK_EQ (names ("1 2 3 4 5 6 7", 0, Major, 0), "C Dm Em F G Am Bdim");
    CHECK_EQ (names ("1 2 3 4 5 6 7", 0, Major, 2), "Cmaj7 Dm7 Em7 Fmaj7 G7 Am7 Bm7b5");
    CHECK_EQ (names ("1 4 5", 9, Minor, 0), "Am Dm Em");
    CHECK_EQ (names ("1 4 5", 9, HarmonicMinor, 2), "Am(maj7) Dm7 E7");
    CHECK_EQ (names ("1 6 7", 9, Minor, 0), "Am F G");
    CHECK_EQ (names ("1 2", 4, Phrygian, 0), "Em F");

    // --- color levels on C major
    CHECK_EQ (nameOf ("1", 0, 0, Major, 1), "Cadd9");
    CHECK_EQ (nameOf ("2", 0, 0, Major, 3), "Dm9");
    CHECK_EQ (nameOf ("2", 0, 0, Major, 4), "Dm11");
    CHECK_EQ (nameOf ("5", 0, 0, Major, 4), "G13");
    CHECK_EQ (nameOf ("1", 0, 0, Major, 4), "Cmaj13");
    CHECK_EQ (nameOf ("5", 0, 9, HarmonicMinor, 3), "E7b9");

    // --- tokens: slash, forced quality, accidentals, sus
    CHECK_EQ (names ("1 5/1 6 3/1 4 1/1 2 5", 0, Major, 0), "C G/B Am Em/G F C/E Dm G");
    CHECK_EQ (names ("1 37 6 4", 0, Major, 0), "C E7 Am F");
    CHECK_EQ (names ("4 4m b77 1", 0, Major, 0), "F Fm Bb7 C");
    CHECK_EQ (names ("b6 b7 1", 0, Major, 2), "Abmaj7 Bbmaj7 Cmaj7");
    CHECK_EQ (names ("1s4 1s2 5o", 0, Major, 0), "Csus4 Csus2 Gdim7");
    CHECK_EQ (names ("1 4", 5, Major, 0), "F Bb");   // flats in F major
    CHECK_EQ (names ("1-5-6-4", 7, Major, 0), "G D Em C");

    // --- parser errors
    {
        std::vector<Step> st;
        std::string err;
        CHECK (! parseProgression ("1 9 4", st, err));
        CHECK (! parseProgression ("   ", st, err));
        CHECK (! parseProgression ("1/5", st, err));
        CHECK (parseProgression ("2 5 1*2", st, err) && st.size() == 3 && st[2].mult == 2);
    }

    // --- voicing: close root position C major around middle C
    {
        Step st; Chord c = buildChord (0, Major, st, 0);
        VoiceSettings vs; vs.voiceLead = false; vs.inversion = 0;
        Voicing v = voiceChord (c, vs, nullptr);
        CHECK (v.n == 3);
        CHECK (v.notes[0] % 12 == 0 && v.notes[1] % 12 == 4 && v.notes[2] % 12 == 7);
        CHECK (v.notes[2] - v.notes[0] < 12);
        vs.inversion = 1;
        v = voiceChord (c, vs, nullptr);
        CHECK (v.notes[0] % 12 == 4);   // first inversion: E in the bottom
        vs.inversion = 2;
        v = voiceChord (c, vs, nullptr);
        CHECK (v.notes[0] % 12 == 7);
    }

    // --- drop 2 spreads the voicing
    {
        Step st; st.degree = 2;
        Chord c = buildChord (0, Major, st, 2);   // Dm7
        VoiceSettings vs; vs.voiceLead = false; vs.style = Drop2;
        Voicing v = voiceChord (c, vs, nullptr);
        CHECK (v.n == 4);
        CHECK (v.notes[3] - v.notes[0] > 12);
    }

    // --- rootless removes the root on 7th chords
    {
        Step st; st.degree = 2;
        Chord c = buildChord (0, Major, st, 3);   // Dm9
        VoiceSettings vs; vs.voiceLead = false; vs.style = Rootless;
        Voicing v = voiceChord (c, vs, nullptr);
        bool hasRoot = false;
        for (int i = 0; i < v.n; ++i) hasRoot |= (v.notes[i] % 12 == 2);
        CHECK (! hasRoot);
    }

    // --- voice leading keeps movement small: C -> G -> Am -> F
    {
        Settings s;
        s.key = 0; s.scale = Major; s.color = 0; s.bassMode = BassRoot;
        std::string err;
        parseProgression ("1 5 6 4", s.steps, err);
        s.voice.voiceLead = true;
        Performance p = buildPerformance (s);
        CHECK (p.error.empty());
        CHECK (p.steps.size() == 4);
        CHECK (std::abs (p.loopBeats - 16.0) < 1e-9);
        // collect chord notes per step
        std::vector<std::vector<int>> per (4);
        for (auto& e : p.events)
            if (e.ch == 0) per[(int) (e.on / 4.0)].push_back (e.note);
        for (int i = 0; i < 4; ++i)
        {
            const auto& a = per[i];
            const auto& b = per[(i + 1) % 4];
            int total = 0;
            for (int x : a) { int best = 99; for (int y : b) best = std::min (best, std::abs (x - y)); total += best; }
            CHECK (total <= 6);
        }
        // bass is root, in bass register
        for (auto& e : p.events)
            if (e.ch == 1) CHECK (e.note >= 31 && e.note <= 47);
    }

    // --- passing chords: secondary dominant before Am gives E7, total length unchanged
    {
        Settings s;
        std::string err;
        parseProgression ("1 6", s.steps, err);
        s.passType = PassSecDom;
        s.passWhere = WhereEvery;
        s.color = 0;
        Performance p = buildPerformance (s);
        CHECK (p.steps.size() == 4);
        CHECK_EQ (p.steps[1].name, "E7");
        CHECK_EQ (p.steps[1].roman, "V7/vi");
        CHECK_EQ (p.steps[3].name, "G7");     // V7 back to tonic
        CHECK (std::abs (p.loopBeats - 8.0) < 1e-9);
        double sum = 0; for (auto& st : p.steps) sum += st.len;
        CHECK (std::abs (sum - 8.0) < 1e-9);

        s.passType = PassDim;
        parseProgression ("1 2", s.steps, err);
        p = buildPerformance (s);
        CHECK_EQ (p.steps[1].name, "C#dim7");

        s.passType = PassTritone;
        parseProgression ("2 1", s.steps, err);
        p = buildPerformance (s);
        CHECK_EQ (p.steps[1].name, "Db7");
        CHECK_EQ (p.steps[1].roman, "subV7");

        s.passType = PassIIV;
        parseProgression ("1 4", s.steps, err);
        p = buildPerformance (s);
        CHECK (p.steps.size() == 6);
        CHECK_EQ (p.steps[1].name, "Gm7");
        CHECK_EQ (p.steps[2].name, "C7");
    }

    // --- rhythm patterns stay inside the loop and events are sane
    for (int r = 0; r < NumRhythms; ++r)
        for (int pt = 0; pt < NumPassTypes; ++pt)
            for (int g = 0; g < (int) progressionLibrary().size(); ++g)
            {
                const auto& pre = progressionLibrary()[g];
                Settings s;
                std::string err;
                CHECK (parseProgression (pre.text, s.steps, err));
                s.scale = pre.scale;
                s.key = g % 12;
                s.rhythm = r;
                s.passType = pt;
                s.color = g % 5;
                s.voice.style = g % NumVoicingStyles;
                s.bassMode = g % NumBassModes;
                s.rollMs = 30;
                s.swing = 0.5f;
                Performance p = buildPerformance (s);
                for (auto& e : p.events)
                {
                    CHECK (e.on >= 0 && e.off <= p.loopBeats + 1e-9 && e.off > e.on);
                    CHECK (e.note >= 24 && e.note <= 100);
                    CHECK (e.vel >= 1 && e.vel <= 127);
                }
            }

    // --- trigger mode: white keys diatonic, black keys secondary dominants / borrowed
    {
        TriggerContext ctx;
        ctx.key = 0; ctx.scale = Major; ctx.color = 0;
        auto nm = [&] (int rel) { return chordName (triggerChordFor (ctx, rel).chord, false); };
        CHECK_EQ (nm (0), "C");
        CHECK_EQ (nm (2), "Dm");
        CHECK_EQ (nm (7), "G");
        CHECK_EQ (nm (1), "A7");    // V7/ii
        CHECK_EQ (nm (6), "D7");    // V7/V
        CHECK_EQ (nm (8), "E7");    // V7/vi
        CHECK_EQ (nm (10), "Bb");   // borrowed bVII (B is diminished)
        Settings s;
        std::string err;
        parseProgression ("1", s.steps, err);
        Performance p = buildPerformance (s);
        CHECK_EQ (p.triggerRoman[8], "V7/vi");
        CHECK_EQ (p.triggerRoman[10], "bVII");
    }

    // print a demo
    {
        Settings s;
        std::string err;
        parseProgression ("1 5/1 6 3/1 4 1/1 2 5", s.steps, err);
        s.color = 2; s.passType = PassAuto; s.passWhere = WhereEveryOther; s.bassMode = BassFollow;
        Performance p = buildPerformance (s);
        for (auto& st : p.steps) std::printf ("%s%s(%s) ", st.passing ? "~" : "", st.name.c_str(), st.roman.c_str());
        std::printf ("\n");
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
