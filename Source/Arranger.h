// SoulChord - turns settings + a progression into timed note events (pure C++).
#pragma once

#include "Theory.h"
#include <string>
#include <vector>

namespace sc
{
enum Genre { GenrePop = 0, GenreRnB, GenreHipHop, NumGenres };

enum PassType { PassOff = 0, PassSecDom, PassDim, PassIIV, PassTritone, PassAuto, NumPassTypes };
enum PassWhere { WhereEvery = 0, WhereEveryOther, WhereLoopEnd, NumPassWhere };
enum Rhythm { RhythmSustain = 0, RhythmPulse, RhythmSynco, RhythmStabs, NumRhythms };
enum Sound { SoundPiano = 0, SoundRhodes, SoundPad, SoundBell, NumSounds };

struct ProgressionPreset
{
    int genre;
    const char* name;   // UTF-8
    const char* text;   // degree notation
    int scale;          // suggested scale
};

const std::vector<ProgressionPreset>& progressionLibrary();
int customProgressionIndex();   // index meaning "custom" (== library size)

// Suggested defaults applied when the user picks a genre
struct GenreDefaults
{
    int color, voicing, bass, pass, where, rhythm, sound;
    float roll, swing;
};
GenreDefaults genreDefaults (int genre);

struct Settings
{
    int key = 0;
    int scale = Major;
    int color = 1;
    VoiceSettings voice;
    int bassMode = BassFollow;
    int passType = PassOff;
    int passWhere = WhereEvery;
    int rhythm = RhythmSustain;
    float rollMs = 0.0f;
    float swing = 0.0f;   // 0..1
    double stepBeats = 4.0;
    double bpm = 120.0;
    std::vector<Step> steps;
};

struct NoteEvent
{
    double on = 0, off = 0;   // beats within the loop
    int note = 60;
    int vel = 100;
    int ch = 0;               // 0 chords, 1 bass
};

struct StepInfo
{
    double start = 0, len = 0;
    std::string name, roman;
    bool passing = false;
};

// Everything the audio thread needs for "one key = one chord" mode (POD)
struct TriggerContext
{
    int key = 0, scale = Major, color = 1, bassMode = BassFollow;
    VoiceSettings voice;
    float rollMs = 0.0f;
};

struct TriggerChord
{
    Chord chord;
    bool valid = false;
};

TriggerChord triggerChordFor (const TriggerContext& ctx, int relPc);

struct Performance
{
    std::vector<NoteEvent> events;   // sorted by on time
    std::vector<StepInfo> steps;
    double loopBeats = 16.0;
    TriggerContext trigger;
    std::string triggerNames[12], triggerRoman[12];
    std::string error;
    bool flats = false;
};

Performance buildPerformance (const Settings& s);
} // namespace sc
