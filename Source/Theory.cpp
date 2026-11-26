#include "Theory.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace sc
{
static const int kScales[NumScales][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },  // Major
    { 0, 2, 3, 5, 7, 8, 10 },  // Natural minor
    { 0, 2, 3, 5, 7, 8, 11 },  // Harmonic minor
    { 0, 2, 3, 5, 7, 9, 10 },  // Dorian
    { 0, 2, 4, 5, 7, 9, 10 },  // Mixolydian
    { 0, 1, 3, 5, 7, 8, 10 },  // Phrygian
};

int mod12 (int x) { return ((x % 12) + 12) % 12; }

const int* scaleIntervals (int scale)
{
    if (scale < 0 || scale >= NumScales) scale = 0;
    return kScales[scale];
}

bool scaleContains (int scale, int relPc)
{
    const int* s = scaleIntervals (scale);
    relPc = mod12 (relPc);
    for (int i = 0; i < 7; ++i)
        if (s[i] == relPc) return true;
    return false;
}

//==============================================================================
bool parseProgression (const std::string& text, std::vector<Step>& out, std::string& error)
{
    out.clear();
    error.clear();
    size_t i = 0;
    const size_t len = text.size();

    auto isSep = [] (char ch) { return std::isspace ((unsigned char) ch) || ch == '-' || ch == ',' || ch == '|'; };

    while (i < len)
    {
        while (i < len && isSep (text[i])) ++i;
        if (i >= len) break;

        size_t start = i;
        while (i < len && ! isSep (text[i])) ++i;
        std::string tok = text.substr (start, i - start);

        Step s;
        size_t p = 0;
        if (p < tok.size() && (tok[p] == 'b' || tok[p] == '#'))
        {
            s.acc = tok[p] == 'b' ? -1 : 1;
            ++p;
        }
        if (p >= tok.size() || tok[p] < '1' || tok[p] > '7')
        {
            error = "'" + tok + "': cần một bậc từ 1 đến 7";
            return false;
        }
        s.degree = tok[p] - '0';
        ++p;

        if (p < tok.size())
        {
            char q = tok[p];
            if (q == 'M' || q == 'm' || q == '7' || q == 'o')
            {
                s.quality = q;
                ++p;
            }
            else if (q == 's' && p + 1 < tok.size() && (tok[p + 1] == '2' || tok[p + 1] == '4'))
            {
                s.quality = tok[p + 1];
                p += 2;
            }
        }
        if (p < tok.size() && tok[p] == '/')
        {
            ++p;
            if (p >= tok.size() || tok[p] < '1' || tok[p] > '3')
            {
                error = "'" + tok + "': sau dấu / là 1 (nốt 3), 2 (nốt 5) hoặc 3 (nốt 7)";
                return false;
            }
            s.slash = tok[p] - '0';
            ++p;
        }
        if (p < tok.size() && tok[p] == '*')
        {
            ++p;
            if (p >= tok.size() || tok[p] < '1' || tok[p] > '8')
            {
                error = "'" + tok + "': sau dấu * là số lần độ dài (1-8)";
                return false;
            }
            s.mult = tok[p] - '0';
            ++p;
        }
        if (p != tok.size())
        {
            error = "Không hiểu ký hiệu '" + tok + "'";
            return false;
        }
        out.push_back (s);
        if (out.size() > 32)
        {
            error = "Tối đa 32 hợp âm";
            return false;
        }
    }
    if (out.empty())
    {
        error = "Vòng hợp âm đang trống";
        return false;
    }
    return true;
}

//==============================================================================
bool Chord::has (int semis) const
{
    for (int i = 0; i < n; ++i)
        if (iv[i] == semis) return true;
    return false;
}

int Chord::third() const
{
    for (int t : { 4, 3, 2, 5 })
        if (has (t)) return t;
    return -1;
}

int Chord::fifth() const
{
    for (int t : { 7, 6, 8 })
        if (has (t)) return t;
    return -1;
}

int Chord::seventh() const
{
    for (int t : { 10, 11, 9 })
        if (has (t)) return t;
    return -1;
}

static void addIv (Chord& c, int semis)
{
    if (c.n < 8 && ! c.has (semis)) c.iv[c.n++] = semis;
}

Chord makeChord (int rootPc, int third, int fifth, int seventh, bool forceSeventh, int color, int key, int scale)
{
    Chord c;
    c.root = mod12 (rootPc);
    const int rel = mod12 (c.root - key);
    const bool dimish = (third == 3 && fifth == 6);
    const bool dominant = (third == 4 && seventh == 10);
    const bool useSeventh = forceSeventh || color >= 2;

    addIv (c, 0);
    addIv (c, third);
    addIv (c, fifth);
    if (useSeventh) addIv (c, seventh);
    c.core = c.n;

    if (color == 1 && ! forceSeventh && ! dimish && third != 2)
        addIv (c, 14);

    if (color >= 3 && ! dimish && third != 2)
    {
        int ninth = 14;
        if (dominant && scaleContains (scale, rel + 1) && ! scaleContains (scale, rel + 2))
            ninth = 13;
        addIv (c, ninth);
    }

    if (color >= 4)
    {
        if (third == 3)
            addIv (c, 17);
        else if (third == 4)
        {
            int thirteenth = 21;
            if (dominant && scaleContains (scale, rel + 8) && ! scaleContains (scale, rel + 9))
                thirteenth = 20;
            addIv (c, thirteenth);
        }
    }

    // Too many notes: drop the perfect fifth (common practice for 6-note chords)
    if (c.n >= 6 && fifth == 7)
    {
        int w = 0;
        for (int i = 0; i < c.n; ++i)
            if (c.iv[i] != 7) c.iv[w++] = c.iv[i];
        c.n = w;
        c.core -= 1;
    }

    if (third == 4) c.kind = (useSeventh && seventh == 10) ? KindDominant : KindMajor;
    else if (third == 3) c.kind = (fifth == 6) ? KindDim : KindMinor;
    else if (third == 2) c.kind = KindSus2;
    else c.kind = KindSus4;
    return c;
}

Chord buildChord (int key, int scale, const Step& step, int color)
{
    const int* s = scaleIntervals (scale);
    const int d = std::max (0, std::min (6, step.degree - 1));
    const int rootRel = mod12 (s[d] + step.acc);
    const int root = mod12 (key + rootRel);

    int third = 4, fifth = 7, seventh = 11;
    bool force = false;

    auto majorSeventh = [&] { return scaleContains (scale, rootRel + 11) ? 11 : 10; };

    switch (step.quality)
    {
        case 'M': third = 4; fifth = 7; seventh = majorSeventh(); break;
        case 'm': third = 3; fifth = 7; seventh = 10; break;
        case 'o': third = 3; fifth = 6; seventh = 9; force = true; break;
        case '7': third = 4; fifth = 7; seventh = 10; force = true; break;
        case '2': third = 2; fifth = 7; seventh = 10; break;
        case '4': third = 5; fifth = 7; seventh = 10; break;
        default:
            if (step.acc == 0)
            {
                third = mod12 (s[(d + 2) % 7] - s[d]);
                fifth = mod12 (s[(d + 4) % 7] - s[d]);
                seventh = mod12 (s[(d + 6) % 7] - s[d]);
            }
            else if (step.acc < 0)
            {
                third = 4; fifth = 7; seventh = majorSeventh();
            }
            else
            {
                third = 3; fifth = 6; seventh = 9;
            }
            break;
    }

    Chord c = makeChord (root, third, fifth, seventh, force, color, key, scale);
    c.spell = step.acc;

    if (step.slash > 0)
    {
        int b = -1;
        if (step.slash == 1) b = c.third();
        else if (step.slash == 2) b = c.fifth() >= 0 ? c.fifth() : fifth;
        else b = c.seventh() >= 0 ? c.seventh() : seventh;
        if (b >= 0) c.bassPc = mod12 (c.root + b);
    }
    return c;
}

//==============================================================================
static int nextAbove (int prev, int pc)
{
    int p = prev + 1;
    while (mod12 (p) != mod12 (pc)) ++p;
    return p;
}

static int buildShape (const Chord& c, int style, int inv, int* out)
{
    int core[8], k = 0;
    for (int i = 0; i < c.core; ++i) core[k++] = c.iv[i];

    if (style == Rootless && c.n >= 4 && k >= 3)
    {
        int w = 0;
        for (int i = 0; i < k; ++i)
            if (core[i] != 0) core[w++] = core[i];
        k = w;
    }
    if (k == 0) return 0;
    inv = ((inv % k) + k) % k;

    int n = 0;
    out[n++] = core[inv];
    for (int j = 1; j < k; ++j)
        out[n] = nextAbove (out[n - 1], core[(inv + j) % k]), ++n;
    for (int i = c.core; i < c.n; ++i)
        out[n] = nextAbove (out[n - 1], c.iv[i]), ++n;

    std::sort (out, out + n);

    auto dropFromTop = [&] (int idxFromTop)
    {
        if (n > idxFromTop) out[n - 1 - idxFromTop] -= 12;
    };

    if (style == Drop2 && n >= 4)
    {
        dropFromTop (1);
    }
    else if (style == Open)
    {
        if (n >= 4) { dropFromTop (1); dropFromTop (3); }
        else if (n == 3) out[1] += 12;
    }
    else if (style == Spread)
    {
        for (int j = 1; j < n; ++j)
        {
            int p = out[j - 1] + 5;
            while (mod12 (p) != mod12 (out[j])) ++p;
            out[j] = p;
        }
    }
    std::sort (out, out + n);
    return n;
}

static double meanOf (const int* a, int n)
{
    double s = 0;
    for (int i = 0; i < n; ++i) s += a[i];
    return n > 0 ? s / n : 0.0;
}

static void placeNear (int* a, int n, int rootPc, double center)
{
    // shift so that pitch classes are right (root pc offset) and mean is near center
    for (int i = 0; i < n; ++i) a[i] += rootPc;
    double m = meanOf (a, n);
    int k = (int) std::lround ((center - m) / 12.0);
    for (int i = 0; i < n; ++i) a[i] += 12 * k;
}

static double moveCost (const int* a, int na, const int* b, int nb)
{
    double cost = 0;
    for (int i = 0; i < na; ++i)
    {
        int best = 999;
        for (int j = 0; j < nb; ++j) best = std::min (best, std::abs (a[i] - b[j]));
        cost += best;
    }
    for (int j = 0; j < nb; ++j)
    {
        int best = 999;
        for (int i = 0; i < na; ++i) best = std::min (best, std::abs (a[i] - b[j]));
        cost += best;
    }
    return cost;
}

Voicing voiceChord (const Chord& c, const VoiceSettings& vs, const Voicing* prev)
{
    Voicing v;
    const double center = 62.0 + 12.0 * vs.octave;
    int shape[10];

    int coreCount = c.core;
    if (vs.style == Rootless && c.n >= 4 && coreCount >= 3) coreCount -= 1;
    coreCount = std::max (1, coreCount);

    if (! vs.voiceLead || prev == nullptr || prev->n == 0)
    {
        int n = buildShape (c, vs.style, vs.inversion, shape);
        placeNear (shape, n, c.root, center);
        v.n = n;
        for (int i = 0; i < n; ++i) v.notes[i] = shape[i];
    }
    else
    {
        double bestCost = 1e9;
        for (int inv = 0; inv < coreCount; ++inv)
        {
            int base[10];
            int n = buildShape (c, vs.style, inv, base);
            placeNear (base, n, c.root, center);
            for (int sh = -1; sh <= 1; ++sh)
            {
                int cand[10];
                for (int i = 0; i < n; ++i) cand[i] = base[i] + 12 * sh;
                const double drift = std::abs (meanOf (cand, n) - center);
                if (drift > 8.0) continue;
                double cost = moveCost (cand, n, prev->notes, prev->n) + 0.35 * drift;
                if (inv == (vs.inversion % coreCount)) cost -= 0.01;
                if (cost < bestCost)
                {
                    bestCost = cost;
                    v.n = n;
                    for (int i = 0; i < n; ++i) v.notes[i] = cand[i];
                }
            }
        }
        if (v.n == 0)
        {
            int n = buildShape (c, vs.style, vs.inversion, shape);
            placeNear (shape, n, c.root, center);
            v.n = n;
            for (int i = 0; i < n; ++i) v.notes[i] = shape[i];
        }
    }

    for (int i = 0; i < v.n; ++i)
    {
        while (v.notes[i] < 36) v.notes[i] += 12;
        while (v.notes[i] > 96) v.notes[i] -= 12;
    }
    std::sort (v.notes, v.notes + v.n);
    return v;
}

//==============================================================================
static int placeBass (int pc) { return 33 + mod12 (pc - 9); }   // A1 .. G#2

int chooseBass (const Chord& c, int mode, int key, int prevBass)
{
    switch (mode)
    {
        case BassOff: return -1;
        case BassRoot: return placeBass (c.root);
        case BassFollow: return placeBass (c.bassPc >= 0 ? c.bassPc : c.root);
        case BassThird: { int t = c.third(); return placeBass (t >= 0 ? c.root + t : c.root); }
        case BassFifth: { int f = c.fifth(); return placeBass (f >= 0 ? c.root + f : c.root); }
        case BassPedal: return placeBass (key);
        case BassSmooth:
        {
            if (prevBass < 0) return placeBass (c.bassPc >= 0 ? c.bassPc : c.root);
            int bestPitch = placeBass (c.root);
            double bestCost = 1e9;
            for (int i = 0; i < c.core; ++i)
            {
                const int pc = mod12 (c.root + c.iv[i]);
                for (int p = 31; p <= 50; ++p)
                {
                    if (mod12 (p) != pc) continue;
                    double cost = std::abs (p - prevBass);
                    if (c.iv[i] != 0) cost += 0.6;      // slight preference for the root
                    if (p > prevBass) cost += 0.1;      // ties: walk down
                    if (p == prevBass) cost += 1.5;     // keep it moving
                    if (cost < bestCost) { bestCost = cost; bestPitch = p; }
                }
            }
            return bestPitch;
        }
        default: return -1;
    }
}

//==============================================================================
bool preferFlats (int key, int scale)
{
    int parent = key;
    switch (scale)
    {
        case Minor:
        case HarmonicMinor: parent = key + 3; break;
        case Dorian: parent = key - 2; break;
        case Mixolydian: parent = key + 5; break;
        case Phrygian: parent = key - 4; break;
        default: break;
    }
    parent = mod12 (parent);
    return parent == 5 || parent == 10 || parent == 3 || parent == 8 || parent == 1;
}

std::string noteName (int pc, bool flats)
{
    static const char* sharps[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatsN[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    pc = mod12 (pc);
    return flats ? flatsN[pc] : sharps[pc];
}

std::string chordName (const Chord& c, bool keyFlats)
{
    const bool flats = c.spell < 0 ? true : (c.spell > 0 ? false : keyFlats);
    std::string r = noteName (c.root, flats);
    const bool maj = c.has (4), mnr = ! maj && c.has (3);
    const bool b7 = c.has (10), M7 = c.has (11);
    const bool n9 = c.has (14), b9 = c.has (13), e11 = c.has (17), e13 = c.has (21), b13 = c.has (20);
    std::string s;

    if (maj)
    {
        const bool aug = c.has (8) && ! c.has (7);
        if (b7)
        {
            if (b9) s = "7b9";
            else if (e13) s = "13";
            else if (n9) s = "9";
            else s = "7";
            if (b13) s += "(b13)";
        }
        else if (M7)
            s = e13 ? "maj13" : (n9 ? "maj9" : "maj7");
        else
            s = n9 ? "add9" : "";
        if (aug) s = s.empty() ? "aug" : s + "#5";
    }
    else if (mnr)
    {
        if (c.has (6) && c.has (9) && ! c.has (7)) s = "dim7";
        else if (c.has (6) && ! c.has (7)) s = b7 ? (e11 ? "m7b5(11)" : "m7b5") : "dim";
        else if (b7) s = e11 ? "m11" : (n9 ? "m9" : "m7");
        else if (M7) s = n9 ? "m(maj9)" : "m(maj7)";
        else s = n9 ? "m(add9)" : "m";
    }
    else
    {
        const std::string sus = c.has (5) ? "sus4" : "sus2";
        if (b7) s = (n9 ? "9" : "7") + sus;
        else s = sus;
    }

    std::string name = r + s;
    if (c.bassPc >= 0 && c.bassPc != c.root) name += "/" + noteName (c.bassPc, flats);
    return name;
}

std::string romanFor (const Step& st, const Chord& c)
{
    static const char* up[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const char* lo[7] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
    const int d = std::max (0, std::min (6, st.degree - 1));
    std::string r = st.acc < 0 ? "b" : (st.acc > 0 ? "#" : "");
    const bool lower = (c.kind == KindMinor || c.kind == KindDim);
    r += lower ? lo[d] : up[d];
    const bool sev = c.seventh() >= 0;
    if (c.kind == KindDim) r += "o";
    else if (c.kind == KindSus2) r += "sus2";
    else if (c.kind == KindSus4) r += "sus4";

    if (st.slash == 0)
    {
        if (c.kind == KindDominant || (c.kind == KindDim && c.has (9) && ! c.has (7))) r += "7";
    }
    else if (sev)
        r += st.slash == 1 ? "65" : (st.slash == 2 ? "43" : "42");
    else
        r += st.slash == 1 ? "6" : "64";
    return r;
}
} // namespace sc
