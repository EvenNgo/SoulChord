#include "Arranger.h"

#include <algorithm>
#include <cmath>

namespace sc
{
//==============================================================================
const std::vector<ProgressionPreset>& progressionLibrary()
{
    static const std::vector<ProgressionPreset> lib = {
        // ---------------- Pop / Vpop / Ballad
        { GenrePop, "Pop kinh điển 1-5-6-4",          "1 5 6 4",               Major },
        { GenrePop, "Ballad 6-4-1-5",                 "6 4 1 5",               Major },
        { GenrePop, "Cảm xúc 4-1-5-6",                "4 1 5 6",               Major },
        { GenrePop, "Canon - bass đi xuống",          "1 5/1 6 3/1 4 1/1 2 5", Major },
        { GenrePop, "Ballad Việt 1-3(7)-6-4",         "1 37 6 4",              Major },
        { GenrePop, "Thập niên 50 / Vpop 1-6-4-5",    "1 6 4 5",               Major },
        { GenrePop, "Royal road 4-5-3-6",             "4 5 3 6",               Major },
        { GenrePop, "Điệp khúc 6-5-4-5",              "6 5 4 5",               Major },
        { GenrePop, "Pop buồn (thứ) 1-6-3-7",         "1 6 3 7",               Minor },
        { GenrePop, "Ballad 1-1/3-4-4m (thứ hóa)", "1 1/1 4 4m",            Major },

        // ---------------- R&B / Neo-soul
        { GenreRnB, "Neo-soul 2-5-1",                 "2 5 1*2",               Major },
        { GenreRnB, "R&B đi xuống 4-3-2-1",           "4 3 2 1",               Major },
        { GenreRnB, "Backdoor 4-4m-b7(7)-1",          "4 4m b77 1",            Major },
        { GenreRnB, "90s R&B 1-6-2-5",                "1 6 2 5",               Major },
        { GenreRnB, "Slow jam 2-3-4-5",               "2 3 4 5",               Major },
        { GenreRnB, "R&B thứ 1-4-7-3",                "1 4 7 3",               Minor },
        { GenreRnB, "Dorian groove 1-4",              "1 4",                   Dorian },
        { GenreRnB, "Smooth 4-3-6-2-5",               "4 3 6 2 5",             Major },

        // ---------------- Hip hop / Trap / Lo-fi
        { GenreHipHop, "Trap tối 1-6-7-1",            "1 6 7 1",               Minor },
        { GenreHipHop, "Trap harmonic 1-4-5",         "1 4 5*2",               HarmonicMinor },
        { GenreHipHop, "Phrygian u tối 1-2",          "1 2",                   Phrygian },
        { GenreHipHop, "Melodic trap 1-6-3-7",        "1 6 3 7",               Minor },
        { GenreHipHop, "Hip hop thứ 1-4",             "1 4",                   Minor },
        { GenreHipHop, "Boom bap jazzy 2-5",          "2 5",                   Major },
        { GenreHipHop, "Lo-fi 4-3-2-1",               "4 3 2 1",               Major },
        { GenreHipHop, "Emo rap 6-4-1-5",             "6 4 1 5",               Major },
    };
    return lib;
}

int customProgressionIndex() { return (int) progressionLibrary().size(); }

GenreDefaults genreDefaults (int genre)
{
    switch (genre)
    {
        case GenreRnB:    return { 3, Drop2, BassFollow, PassAuto, WhereEveryOther, RhythmSynco, SoundRhodes, 25.0f, 0.25f };
        case GenreHipHop: return { 2, Close, BassRoot,   PassOff,  WhereEvery,      RhythmSustain, SoundPad,  0.0f, 0.0f };
        default:          return { 1, Close, BassFollow, PassOff,  WhereEvery,      RhythmPulse, SoundPiano,  0.0f, 0.0f };
    }
}

//==============================================================================
static std::string plainRoman (int key, int scale, const Step& st, int color)
{
    Step s = st;
    s.slash = 0;
    Chord c = buildChord (key, scale, s, color);
    std::string r = romanFor (s, c);
    return r;
}

TriggerChord triggerChordFor (const TriggerContext& ctx, int relPc)
{
    TriggerChord out;
    const int* s = scaleIntervals (ctx.scale);
    relPc = mod12 (relPc);

    for (int d = 0; d < 7; ++d)
    {
        if (s[d] == relPc)
        {
            Step st;
            st.degree = d + 1;
            out.chord = buildChord (ctx.key, ctx.scale, st, ctx.color);
            out.valid = true;
            return out;
        }
    }

    int target = 0;
    for (int d = 0; d < 7; ++d)
        if (s[d] > relPc) { target = d; break; }

    Step tst;
    tst.degree = target + 1;
    Chord tc = buildChord (ctx.key, ctx.scale, tst, 2);

    if (tc.kind == KindDim)
    {
        const int sev = scaleContains (ctx.scale, relPc + 11) ? 11 : 10;
        out.chord = makeChord (ctx.key + relPc, 4, 7, sev, false, ctx.color, ctx.key, ctx.scale);
        out.chord.spell = -1;
    }
    else
    {
        out.chord = makeChord (tc.root + 7, 4, 7, 10, true, std::max (ctx.color, 2), ctx.key, ctx.scale);
    }
    out.valid = true;
    return out;
}

static std::string triggerRomanFor (const TriggerContext& ctx, int relPc)
{
    const int* s = scaleIntervals (ctx.scale);
    for (int d = 0; d < 7; ++d)
        if (s[d] == relPc)
        {
            Step st;
            st.degree = d + 1;
            return plainRoman (ctx.key, ctx.scale, st, ctx.color);
        }

    int target = 0;
    for (int d = 0; d < 7; ++d)
        if (s[d] > relPc) { target = d; break; }
    Step tst;
    tst.degree = target + 1;
    Chord tc = buildChord (ctx.key, ctx.scale, tst, 2);
    if (tc.kind == KindDim)
    {
        static const char* up[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
        return std::string ("b") + up[target];
    }
    return "V7/" + plainRoman (ctx.key, ctx.scale, tst, ctx.color);
}

//==============================================================================
namespace
{
struct Seg
{
    Chord chord;
    double start = 0, len = 0;
    std::string roman;
    bool passing = false;
};

struct Hit
{
    double t, dur;
    int vel;
};

std::vector<Hit> makeHits (int rhythm, double len, float swing)
{
    std::vector<Hit> hits;
    const double sw = swing * (1.0 / 6.0);

    auto swingTime = [sw] (double t)
    {
        const double frac = t - std::floor (t);
        if (std::abs (frac - 0.5) < 1e-6) return t + sw;
        if (std::abs (frac - 0.25) < 1e-6 || std::abs (frac - 0.75) < 1e-6) return t + sw * 0.5;
        return t;
    };

    auto addTemplate = [&] (const std::vector<Hit>& tpl)
    {
        for (double bar = 0; bar < len; bar += 4.0)
            for (const auto& h : tpl)
                hits.push_back ({ bar + swingTime (h.t), h.dur, h.vel });
    };

    switch (rhythm)
    {
        case RhythmPulse:
            for (double b = 0; b < len; b += 0.5)
            {
                const bool onBeat = std::abs (b - std::floor (b)) < 1e-6;
                const int vel = onBeat ? (((int) b % 2) == 0 ? 100 : 90) : 74;
                hits.push_back ({ swingTime (b), 0.42, vel });
            }
            break;
        case RhythmSynco:
            addTemplate ({ { 0.0, 1.4, 100 }, { 1.5, 1.4, 84 }, { 3.0, 0.9, 90 } });
            break;
        case RhythmStabs:
            addTemplate ({ { 0.0, 0.3, 105 }, { 1.75, 0.25, 88 }, { 2.5, 0.4, 95 } });
            break;
        default:
            hits.push_back ({ 0.0, len, 96 });
            break;
    }

    std::vector<Hit> out;
    for (auto h : hits)
    {
        if (h.t >= len - 0.05) continue;
        h.dur = std::min (h.dur, len - h.t);
        out.push_back (h);
    }
    if (out.empty()) out.push_back ({ 0.0, len, 96 });
    return out;
}

bool isTonic (const Step& s) { return s.degree == 1 && s.acc == 0; }

void addPassing (std::vector<Seg>& segs, const Chord& cur, const Chord& next, const Step& nextStep,
                 const Settings& s, int index, double stepLen, double start, double P)
{
    int type = s.passType;
    const bool nextMinor = next.kind == KindMinor || next.kind == KindDim;
    const int up = mod12 (next.root - cur.root);

    if (type == PassAuto)
    {
        if (up == 2) type = PassDim;
        else if (nextMinor) type = PassSecDom;
        else if (index % 2 == 0 && stepLen >= 4.0) type = PassIIV;
        else type = PassTritone;
    }
    if (type == PassSecDom && mod12 (cur.root - next.root) == 7 && (cur.kind == KindDominant || cur.kind == KindMajor))
        type = PassDim;   // already a V chord: use the leading-tone diminished instead

    const std::string target = isTonic (nextStep) ? "" : "/" + plainRoman (s.key, s.scale, nextStep, s.color);
    const int domColor = std::max (2, s.color);

    auto push = [&] (const Chord& c, const std::string& roman, double st, double len)
    {
        Seg g;
        g.chord = c;
        g.start = st;
        g.len = len;
        g.roman = roman;
        g.passing = true;
        segs.push_back (g);
    };

    switch (type)
    {
        case PassSecDom:
            push (makeChord (next.root + 7, 4, 7, 10, true, domColor, s.key, s.scale), "V7" + target, start, P);
            break;
        case PassDim:
        {
            Chord d = makeChord (next.root - 1, 3, 6, 9, true, 2, s.key, s.scale);
            d.spell = scaleContains (s.scale, next.root - 1 - s.key) ? 0 : 1;
            push (d, "viio7" + target, start, P);
        }
            break;
        case PassIIV:
        {
            Chord ii = nextMinor ? makeChord (next.root + 2, 3, 6, 10, true, 2, s.key, s.scale)
                                 : makeChord (next.root + 2, 3, 7, 10, true, std::max (2, s.color), s.key, s.scale);
            Chord v = makeChord (next.root + 7, 4, 7, 10, true, domColor, s.key, s.scale);
            push (ii, (nextMinor ? "iiø7" : "ii7") + target, start, P * 0.5);
            push (v, "V7" + target, start + P * 0.5, P * 0.5);
            break;
        }
        case PassTritone:
        default:
        {
            Chord sub = makeChord (next.root + 1, 4, 7, 10, true, domColor, s.key, s.scale);
            sub.spell = scaleContains (s.scale, next.root + 1 - s.key) ? 0 : -1;
            push (sub, "subV7" + target, start, P);
        }
            break;
    }
}
} // namespace

//==============================================================================
Performance buildPerformance (const Settings& s)
{
    Performance p;
    p.flats = preferFlats (s.key, s.scale);

    p.trigger.key = s.key;
    p.trigger.scale = s.scale;
    p.trigger.color = s.color;
    p.trigger.bassMode = s.bassMode;
    p.trigger.voice = s.voice;
    p.trigger.rollMs = s.rollMs;

    for (int rel = 0; rel < 12; ++rel)
    {
        TriggerChord tc = triggerChordFor (p.trigger, rel);
        p.triggerNames[rel] = chordName (tc.chord, p.flats);
        p.triggerRoman[rel] = triggerRomanFor (p.trigger, rel);
    }

    if (s.steps.empty())
    {
        p.error = "Vòng hợp âm đang trống";
        p.loopBeats = 4.0;
        return p;
    }

    const int n = (int) s.steps.size();
    std::vector<Chord> mains;
    for (const auto& st : s.steps)
        mains.push_back (buildChord (s.key, s.scale, st, s.color));

    std::vector<Seg> segs;
    double t = 0;
    for (int i = 0; i < n; ++i)
    {
        const double stepLen = s.stepBeats * s.steps[i].mult;
        const int ni = (i + 1) % n;
        const bool want = s.passType != PassOff
                          && (s.passWhere == WhereEvery
                              || (s.passWhere == WhereEveryOther && i % 2 == 1)
                              || (s.passWhere == WhereLoopEnd && i == n - 1));
        const double P = want ? std::min (2.0, stepLen * 0.5) : 0.0;

        Seg g;
        g.chord = mains[i];
        g.start = t;
        g.len = stepLen - P;
        g.roman = romanFor (s.steps[i], mains[i]);
        segs.push_back (g);

        if (P > 0)
            addPassing (segs, mains[i], mains[ni], s.steps[ni], s, i, stepLen, t + stepLen - P, P);

        t += stepLen;
    }
    p.loopBeats = t;

    // Voice leading: run twice so the loop's first chord connects smoothly to the last one
    std::vector<Voicing> voicings (segs.size());
    std::vector<int> basses (segs.size(), -1);
    Voicing prev;
    int prevBass = -1;
    bool havePrev = false;
    for (int pass = 0; pass < 2; ++pass)
    {
        for (size_t i = 0; i < segs.size(); ++i)
        {
            VoiceSettings vs = s.voice;
            if (pass == 0 && i == 0) vs.voiceLead = false;
            voicings[i] = voiceChord (segs[i].chord, vs, havePrev ? &prev : nullptr);
            prev = voicings[i];
            havePrev = true;
            basses[i] = chooseBass (segs[i].chord, s.bassMode, s.key, prevBass);
            if (basses[i] >= 0) prevBass = basses[i];
        }
        if (! s.voice.voiceLead) break;
    }

    const double rollBeats = (s.rollMs / 1000.0) * (s.bpm / 60.0);

    for (size_t i = 0; i < segs.size(); ++i)
    {
        const Seg& g = segs[i];
        const Voicing& v = voicings[i];

        StepInfo info;
        info.start = g.start;
        info.len = g.len;
        info.name = chordName (g.chord, p.flats);
        info.roman = g.roman;
        info.passing = g.passing;
        p.steps.push_back (info);

        std::vector<Hit> hits = g.passing ? std::vector<Hit> { { 0.0, g.len, 88 } }
                                          : makeHits (s.rhythm, g.len, s.swing);

        for (size_t h = 0; h < hits.size(); ++h)
        {
            const Hit& hit = hits[h];
            const double hitEnd = g.start + hit.t + hit.dur - 0.02;
            for (int j = 0; j < v.n; ++j)
            {
                NoteEvent e;
                e.on = g.start + hit.t + std::min (j * rollBeats, std::max (0.0, hit.dur - 0.1));
                e.off = std::max (e.on + 0.03, hitEnd);
                e.note = v.notes[j];
                e.vel = std::max (1, std::min (127, hit.vel - (j == v.n - 1 ? 0 : 6)));
                e.ch = 0;
                p.events.push_back (e);
            }
        }

        if (basses[i] >= 0)
        {
            const bool perHit = ! g.passing && (s.rhythm == RhythmSynco || s.rhythm == RhythmStabs);
            if (! perHit)
            {
                p.events.push_back ({ g.start, g.start + g.len - 0.02, basses[i], 100, 1 });
            }
            else
            {
                for (size_t h = 0; h < hits.size(); ++h)
                {
                    const double on = g.start + hits[h].t;
                    const double end = (h + 1 < hits.size()) ? g.start + hits[h + 1].t : g.start + g.len;
                    p.events.push_back ({ on, std::max (on + 0.05, end - 0.04), basses[i], hits[h].vel, 1 });
                }
            }
        }
    }

    std::stable_sort (p.events.begin(), p.events.end(),
                      [] (const NoteEvent& a, const NoteEvent& b) { return a.on < b.on; });
    return p;
}
} // namespace sc
