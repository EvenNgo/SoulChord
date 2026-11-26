// SoulChord - music theory core (pure C++, no JUCE).
// Everything used on the audio thread here is allocation-free (POD structs).
#pragma once

#include <string>
#include <vector>

namespace sc
{
enum Scale
{
    Major = 0,
    Minor,
    HarmonicMinor,
    Dorian,
    Mixolydian,
    Phrygian,
    NumScales
};

enum VoicingStyle
{
    Close = 0,
    Open,     // drop 2 + 4
    Drop2,
    Spread,
    Rootless,
    NumVoicingStyles
};

enum BassMode
{
    BassOff = 0,
    BassRoot,
    BassFollow,   // root, or the slash note written in the progression (e.g. 5/1)
    BassThird,
    BassFifth,
    BassPedal,    // tonic pedal
    BassSmooth,   // nearest chord tone to the previous bass note (walking lines)
    NumBassModes
};

const int* scaleIntervals (int scale);   // 7 entries
bool scaleContains (int scale, int relPc);
int  mod12 (int x);

// One step of a written progression, e.g. "b77" or "5/1" or "6m*2"
struct Step
{
    int degree = 1;      // 1..7
    int acc = 0;         // -1 flat, +1 sharp
    char quality = 0;    // 0 diatonic, 'M' major, 'm' minor, '7' dominant 7, 'o' dim7, '2' sus2, '4' sus4
    int slash = 0;       // 0 none, 1 = 3rd in bass, 2 = 5th in bass, 3 = 7th in bass
    int mult = 1;        // length multiplier
};

bool parseProgression (const std::string& text, std::vector<Step>& out, std::string& error);

enum ChordKind
{
    KindMajor = 0,
    KindMinor,
    KindDim,
    KindDominant,
    KindSus2,
    KindSus4
};

struct Chord
{
    int root = 0;        // absolute pitch class 0..11
    int kind = KindMajor;
    int n = 0;           // number of intervals
    int core = 0;        // first `core` intervals are root/3rd/5th/7th, the rest are extensions
    int iv[8] {};        // semitones above root (extensions are > 12)
    int bassPc = -1;     // requested slash bass pitch class, -1 = none
    int spell = 0;       // 0 = follow key, -1 = flats (bVII, tritone sub), +1 = sharps (passing dim)

    bool has (int semis) const;
    int  third() const;  // interval of 3rd (or sus tone), -1 if none
    int  fifth() const;
    int  seventh() const;
};

// color: 0 triad, 1 add9, 2 seventh, 3 ninth, 4 eleventh/thirteenth
Chord buildChord (int key, int scale, const Step& step, int color);
// third/fifth/seventh are semitones above root. forceSeventh adds the 7th even at low color.
Chord makeChord (int rootPc, int third, int fifth, int seventh, bool forceSeventh, int color, int key, int scale);

struct Voicing
{
    int n = 0;
    int notes[10] {};
};

struct VoiceSettings
{
    int style = Close;
    int inversion = 0;
    bool voiceLead = true;
    int octave = 0;      // -2..+2
};

Voicing voiceChord (const Chord& c, const VoiceSettings& vs, const Voicing* prev);

// returns MIDI note of the bass, or -1 when bass is off
int chooseBass (const Chord& c, int bassMode, int key, int prevBass);

bool preferFlats (int key, int scale);
std::string noteName (int pc, bool flats);
std::string chordName (const Chord& c, bool flats);
std::string romanFor (const Step& s, const Chord& c);
} // namespace sc
