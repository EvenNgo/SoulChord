#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

using namespace juce;

namespace
{
const char* const kKeyNames[12] = { "C", "C#/Db", "D", "Eb", "E", "F", "F#/Gb", "G", "Ab", "A", "Bb", "B" };

StringArray utf8Array (std::initializer_list<const char*> items)
{
    StringArray a;
    for (auto* s : items) a.add (String::fromUTF8 (s));
    return a;
}
} // namespace

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout SoulChordProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto choice = [&p] (const char* id, const char* name, StringArray items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, String::fromUTF8 (name), items, def));
    };
    auto integer = [&p] (const char* id, const char* name, int lo, int hi, int def)
    {
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, String::fromUTF8 (name), lo, hi, def));
    };
    auto real = [&p] (const char* id, const char* name, float lo, float hi, float def, const char* unit)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, String::fromUTF8 (name),
                                                            NormalisableRange<float> (lo, hi), def,
                                                            AudioParameterFloatAttributes().withLabel (unit)));
    };

    StringArray keys;
    for (auto* k : kKeyNames) keys.add (k);

    choice ("mode", "Chế độ chơi", utf8Array ({ "Tự chạy theo tempo", "Mỗi phím 1 hợp âm" }), 0);
    choice ("genre", "Thể loại", utf8Array ({ "Pop", "R&B", "Hip hop" }), 0);
    choice ("key", "Giọng", keys, 0);
    choice ("scale", "Thang âm", utf8Array ({ "Trưởng (Major)", "Thứ (Minor)", "Thứ hòa âm", "Dorian", "Mixolydian", "Phrygian" }), 0);
    integer ("prog", "Vòng hòa âm", 0, sc::customProgressionIndex(), 0);
    choice ("length", "Độ dài mỗi hợp âm", utf8Array ({ "2 phách", "1 ô nhịp", "2 ô nhịp" }), 1);

    integer ("color", "Màu hòa âm", 0, 4, 1);
    choice ("voicing", "Kiểu voicing", utf8Array ({ "Close (sát)", "Open (mở)", "Drop 2", "Spread (rộng)", "Rootless (bỏ gốc)" }), 0);
    choice ("inversion", "Thế đảo", utf8Array ({ "Thế gốc", "Đảo 1", "Đảo 2", "Đảo 3" }), 0);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "voicelead", 1 }, String::fromUTF8 ("Đi giọng mượt"), true));
    integer ("octave", "Quãng tám", -2, 2, 0);

    choice ("bass", "Bass", utf8Array ({ "Tắt", "Nốt gốc", "Theo vòng (slash)", "Nốt 3", "Nốt 5", "Pedal (giữ nốt chủ)", "Đi mượt (walking)" }), 2);
    choice ("pass", "Passing chord", utf8Array ({ "Tắt", "Dominant phụ (V7/x)", "Giảm chuyển tiếp (dim7)", "ii-V", "Tritone sub", "Tự động" }), 0);
    choice ("where", "Vị trí passing", utf8Array ({ "Mỗi lần chuyển", "Cách một", "Cuối vòng" }), 0);

    choice ("rhythm", "Nhịp điệu", utf8Array ({ "Ngân dài", "Móc đơn (Pop)", "Lệch nhịp (R&B)", "Stab (Hip hop)" }), 1);
    real ("roll", "Rải hợp âm", 0.0f, 80.0f, 0.0f, "ms");
    real ("swing", "Swing", 0.0f, 1.0f, 0.0f, "");
    choice ("sound", "Tiếng", utf8Array ({ "Piano", "Rhodes", "Pad", "Bell" }), 0);
    real ("chordvol", "Âm lượng hợp âm", 0.0f, 1.0f, 0.7f, "");
    real ("bassvol", "Âm lượng bass", 0.0f, 1.0f, 0.7f, "");
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "preview", 1 }, String::fromUTF8 ("Nghe thử"), false));

    return { p.begin(), p.end() };
}

//==============================================================================
SoulChordProcessor::SoulChordProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SoulChord", createLayout())
{
    for (int i = 0; i < 24; ++i) chordSynth.addVoice (new sc::ChordVoice (soundSel));
    for (int i = 0; i < 4; ++i) bassSynth.addVoice (new sc::BassVoice());
    chordSynth.addSound (new sc::ScSound());
    bassSynth.addSound (new sc::ScSound());

    if (! apvts.state.hasProperty ("custom"))
        apvts.state.setProperty ("custom", "1 5 6 4", nullptr);

    rebuildIfNeeded (true);
    startTimerHz (20);
}

SoulChordProcessor::~SoulChordProcessor() { stopTimer(); }

bool SoulChordProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
}

void SoulChordProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    chordSynth.setCurrentPlaybackSampleRate (sr);
    bassSynth.setCurrentPlaybackSampleRate (sr);
    bassBuffer.setSize (2, jmax (512, samplesPerBlock));
    chordMidi.ensureSize (8192);
    bassMidi.ensureSize (2048);
    outMidi.ensureSize (8192);

    numActive = 0;
    numPending = 0;
    seqRunning = false;
    for (auto& h : held) h.inNote = -1;
    for (auto& ch : noteRef) for (auto& r : ch) r = 0;
    for (int i = 0; i < 2; ++i) { soundingChord[i] = 0; soundingBass[i] = 0; }
}

//==============================================================================
String SoulChordProcessor::getCustomProgression() const
{
    return apvts.state.getProperty ("custom", "1 5 6 4").toString();
}

void SoulChordProcessor::setCustomProgression (const String& text)
{
    apvts.state.setProperty ("custom", text, nullptr);
}

void SoulChordProcessor::setParam (const String& id, float plainValue)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
        prm->endChangeGesture();
    }
}

float SoulChordProcessor::getParam (const String& id) const
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

void SoulChordProcessor::applyGenreDefaults (int genre)
{
    const auto d = sc::genreDefaults (genre);
    setParam ("genre", (float) genre);
    setParam ("color", (float) d.color);
    setParam ("voicing", (float) d.voicing);
    setParam ("bass", (float) d.bass);
    setParam ("pass", (float) d.pass);
    setParam ("where", (float) d.where);
    setParam ("rhythm", (float) d.rhythm);
    setParam ("sound", (float) d.sound);
    setParam ("roll", d.roll);
    setParam ("swing", d.swing);

    const auto& lib = sc::progressionLibrary();
    for (int i = 0; i < (int) lib.size(); ++i)
    {
        if (lib[i].genre == genre)
        {
            setParam ("prog", (float) i);
            setParam ("scale", (float) lib[i].scale);
            break;
        }
    }
}

//==============================================================================
void SoulChordProcessor::timerCallback()
{
    rebuildIfNeeded (forceRebuild.exchange (false));

    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(),
                                     [] (const std::shared_ptr<const sc::Performance>& p) { return p.use_count() <= 1; }),
                     graveyard.end());
}

void SoulChordProcessor::rebuildIfNeeded (bool force)
{
    auto geti = [this] (const char* id) { return (int) std::lround (getParam (id)); };

    sc::Settings s;
    s.key = geti ("key");
    s.scale = geti ("scale");
    s.color = geti ("color");
    s.voice.style = geti ("voicing");
    s.voice.inversion = geti ("inversion");
    s.voice.voiceLead = getParam ("voicelead") > 0.5f;
    s.voice.octave = geti ("octave");
    s.bassMode = geti ("bass");
    s.passType = geti ("pass");
    s.passWhere = geti ("where");
    s.rhythm = geti ("rhythm");
    s.rollMs = getParam ("roll");
    s.swing = getParam ("swing");
    static const double lengths[3] = { 2.0, 4.0, 8.0 };
    s.stepBeats = lengths[jlimit (0, 2, geti ("length"))];
    s.bpm = hostBpm.load();

    const int prog = jlimit (0, sc::customProgressionIndex(), geti ("prog"));
    const String text = prog >= sc::customProgressionIndex() ? getCustomProgression()
                                                              : String::fromUTF8 (sc::progressionLibrary()[(size_t) prog].text);

    String sig;
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            if (r->getParameterID() != "preview" && r->getParameterID() != "chordvol" && r->getParameterID() != "bassvol")
                sig << r->getParameterID() << "=" << r->getValue() << ";";
    sig << text << "|" << (s.rollMs > 0 ? roundToInt (s.bpm) : 0);

    if (! force && sig == lastSignature) return;
    lastSignature = sig;

    std::string err;
    std::vector<sc::Step> steps;
    if (sc::parseProgression (text.toStdString(), steps, err))
        lastValidSteps = steps;
    else if (lastValidSteps.empty())
        sc::parseProgression ("1", lastValidSteps, err);
    s.steps = lastValidSteps;

    auto p = std::make_shared<sc::Performance> (sc::buildPerformance (s));
    if (! err.empty()) p->error = err;

    std::shared_ptr<const sc::Performance> np = p;
    auto old = std::atomic_exchange (&perf, np);
    if (old) graveyard.push_back (std::move (old));
}

//==============================================================================
void SoulChordProcessor::setSounding (int ch, int note, bool on)
{
    std::atomic<uint64_t>* arr = (ch == 0) ? soundingChord : soundingBass;
    const uint64_t bit = (uint64_t) 1 << (note & 63);
    if (on) arr[note >> 6].fetch_or (bit);
    else arr[note >> 6].fetch_and (~bit);
}

void SoulChordProcessor::sendOn (int ch, int note, int vel, int samplePos)
{
    note = jlimit (0, 127, note);
    vel = jlimit (1, 127, vel);
    auto& dst = ch == 0 ? chordMidi : bassMidi;
    int& r = noteRef[ch][note];
    if (r > 0)
    {
        dst.addEvent (MidiMessage::noteOff (ch + 1, note), samplePos);
        outMidi.addEvent (MidiMessage::noteOff (ch + 1, note), samplePos);
    }
    ++r;
    dst.addEvent (MidiMessage::noteOn (ch + 1, note, (uint8) vel), samplePos);
    outMidi.addEvent (MidiMessage::noteOn (ch + 1, note, (uint8) vel), samplePos);
    setSounding (ch, note, true);
}

void SoulChordProcessor::sendOff (int ch, int note, int samplePos)
{
    note = jlimit (0, 127, note);
    int& r = noteRef[ch][note];
    if (r <= 0) return;
    if (--r == 0)
    {
        auto& dst = ch == 0 ? chordMidi : bassMidi;
        dst.addEvent (MidiMessage::noteOff (ch + 1, note), samplePos);
        outMidi.addEvent (MidiMessage::noteOff (ch + 1, note), samplePos);
        setSounding (ch, note, false);
    }
}

void SoulChordProcessor::startSeqNote (const sc::NoteEvent& e, double endAbs, int samplePos)
{
    sendOn (e.ch, e.note, e.vel, samplePos);
    if (numActive < (int) active.size())
        active[(size_t) numActive++] = { e.note, e.ch, endAbs };
    else
        sendOff (e.ch, e.note, samplePos);
}

void SoulChordProcessor::releaseAllSeq (int samplePos)
{
    for (int i = 0; i < numActive; ++i)
        sendOff (active[(size_t) i].ch, active[(size_t) i].note, samplePos);
    numActive = 0;
}

//==============================================================================
void SoulChordProcessor::triggerOn (const sc::Performance& p, int inNote, int vel, int samplePos)
{
    int slot = -1;
    for (int i = 0; i < (int) held.size(); ++i)
        if (held[(size_t) i].inNote == inNote) { triggerOff (inNote, samplePos); break; }
    for (int i = 0; i < (int) held.size(); ++i)
        if (held[(size_t) i].inNote < 0) { slot = i; break; }
    if (slot < 0)
    {
        triggerOff (held[0].inNote, samplePos);
        slot = 0;
    }

    const auto& ctx = p.trigger;
    const int rel = sc::mod12 (inNote - ctx.key);
    const sc::TriggerChord tc = sc::triggerChordFor (ctx, rel);
    const sc::Voicing v = sc::voiceChord (tc.chord, ctx.voice, havePrevTrig ? &prevTrig : nullptr);
    const int bass = sc::chooseBass (tc.chord, ctx.bassMode, ctx.key, prevTrigBass);

    Held& h = held[(size_t) slot];
    h.inNote = inNote;
    h.rel = rel;
    h.n = v.n;
    for (int j = 0; j < v.n; ++j) h.notes[j] = v.notes[j];
    h.bass = bass;
    h.started = 0;

    const double rollSamples = ctx.rollMs * 0.001 * sr;
    for (int j = 0; j < v.n; ++j)
    {
        const int velJ = jlimit (1, 127, vel - (j == v.n - 1 ? 0 : 5));
        const auto delay = (int64_t) (j * rollSamples);
        if (delay <= 0 || numPending >= (int) pending.size())
        {
            sendOn (0, v.notes[j], velJ, samplePos);
            h.started |= (1u << j);
        }
        else
        {
            pending[(size_t) numPending++] = { sampleCounter + samplePos + delay, v.notes[j], velJ, slot, j };
        }
    }
    if (bass >= 0)
    {
        sendOn (1, bass, jmin (127, vel + 10), samplePos);
        prevTrigBass = bass;
    }

    prevTrig = v;
    havePrevTrig = true;
    heldRelMask.fetch_or (1 << rel);
}

void SoulChordProcessor::triggerOff (int inNote, int samplePos)
{
    for (int i = 0; i < (int) held.size(); ++i)
    {
        Held& h = held[(size_t) i];
        if (h.inNote != inNote || inNote < 0) continue;

        for (int k = 0; k < numPending;)
        {
            if (pending[(size_t) k].heldIdx == i) pending[(size_t) k] = pending[(size_t) --numPending];
            else ++k;
        }
        for (int j = 0; j < h.n; ++j)
            if (h.started & (1u << j)) sendOff (0, h.notes[j], samplePos);
        if (h.bass >= 0) sendOff (1, h.bass, samplePos);
        h.inNote = -1;
        h.started = 0;
    }

    int mask = 0;
    for (auto& h : held)
        if (h.inNote >= 0) mask |= 1 << h.rel;
    heldRelMask = mask;
}

void SoulChordProcessor::releaseAllTrigger (int samplePos)
{
    for (auto& h : held)
        if (h.inNote >= 0) triggerOff (h.inNote, samplePos);
    numPending = 0;
    heldRelMask = 0;
}

void SoulChordProcessor::dispatchPending (int numSamples)
{
    const int64_t blockEnd = sampleCounter + numSamples;
    for (int k = 0; k < numPending;)
    {
        Pending& pe = pending[(size_t) k];
        if (pe.time < blockEnd)
        {
            const int pos = (int) jlimit<int64_t> (0, numSamples - 1, pe.time - sampleCounter);
            Held& h = held[(size_t) pe.heldIdx];
            if (h.inNote >= 0)
            {
                sendOn (0, pe.note, pe.vel, pos);
                h.started |= (1u << pe.j);
            }
            pending[(size_t) k] = pending[(size_t) --numPending];
        }
        else
            ++k;
    }
}

//==============================================================================
void SoulChordProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    chordMidi.clear();
    bassMidi.clear();
    outMidi.clear();
    if (n <= 0) return;

    const auto P = std::atomic_load (&perf);
    soundSel = (int) std::lround (getParam ("sound"));
    const int mode = (int) std::lround (getParam ("mode"));
    const bool preview = getParam ("preview") > 0.5f;

    bool playing = false;
    double ppq = 0.0, bpm = hostBpm.load();
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto v = pos->getPpqPosition()) ppq = *v;
            if (auto b = pos->getBpm()) if (*b > 1.0) bpm = *b;
        }
    }
    hostBpm = bpm;
    transportRunning = playing;

    if (mode != lastMode)
    {
        releaseAllSeq (0);
        releaseAllTrigger (0);
        seqRunning = false;
        currentStep = -1;
        lastMode = mode;
    }

    // ---- incoming MIDI (one key = one chord)
    for (const auto meta : midi)
    {
        if (mode != ModeTrigger || P == nullptr) break;
        const auto m = meta.getMessage();
        const int pos = jlimit (0, n - 1, meta.samplePosition);
        if (m.isNoteOn()) triggerOn (*P, m.getNoteNumber(), m.getVelocity(), pos);
        else if (m.isNoteOff()) triggerOff (m.getNoteNumber(), pos);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) releaseAllTrigger (pos);
    }
    dispatchPending (n);

    // ---- sequencer (follows the FL Studio transport, or the preview button)
    if (mode == ModeSequencer && P != nullptr && ! P->events.empty() && P->loopBeats > 0.0)
    {
        if (preview && ! lastPreview) previewPpq = 0.0;
        lastPreview = preview;
        const bool run = playing || preview;

        if (run)
        {
            const double bps = bpm / 60.0 / sr;
            const double start = playing ? ppq : previewPpq;
            const double end = start + n * bps;
            const double L = P->loopBeats;
            auto toPos = [&] (double beat) { return jlimit (0, n - 1, (int) ((beat - start) / bps)); };

            if (! seqRunning || std::abs (start - expectedPpq) > 0.05)
            {
                releaseAllSeq (0);
                const double base = std::floor (start / L) * L;
                for (const auto& e : P->events)
                    if (base + e.on < start && base + e.off > start + 0.01)
                        startSeqNote (e, base + e.off, 0);
            }

            for (int i = 0; i < numActive;)
            {
                if (active[(size_t) i].endAbs < end)
                {
                    sendOff (active[(size_t) i].ch, active[(size_t) i].note, toPos (active[(size_t) i].endAbs));
                    active[(size_t) i] = active[(size_t) --numActive];
                }
                else
                    ++i;
            }

            const auto k0 = (int64_t) std::floor (start / L);
            const auto k1 = (int64_t) std::floor (end / L);
            for (int64_t k = k0; k <= k1; ++k)
            {
                const double base = (double) k * L;
                for (const auto& e : P->events)
                {
                    const double on = base + e.on;
                    if (on >= start && on < end) startSeqNote (e, base + e.off, toPos (on));
                }
            }

            expectedPpq = end;
            seqRunning = true;
            if (! playing) previewPpq = end;

            double inLoop = std::fmod (start, L);
            if (inLoop < 0) inLoop += L;
            int cs = -1;
            for (size_t i = 0; i < P->steps.size(); ++i)
                if (inLoop >= P->steps[i].start && inLoop < P->steps[i].start + P->steps[i].len) { cs = (int) i; break; }
            currentStep = cs;
        }
        else if (seqRunning)
        {
            releaseAllSeq (0);
            seqRunning = false;
            currentStep = -1;
        }
    }
    else if (seqRunning)
    {
        releaseAllSeq (0);
        seqRunning = false;
        currentStep = -1;
    }

    // ---- render
    chordSynth.renderNextBlock (buffer, chordMidi, 0, n);
    const float cg = getParam ("chordvol");
    const float bg = getParam ("bassvol");
    buffer.applyGainRamp (0, n, lastChordGain, cg);

    bassBuffer.setSize (buffer.getNumChannels(), n, false, false, true);
    bassBuffer.clear();
    bassSynth.renderNextBlock (bassBuffer, bassMidi, 0, n);
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        buffer.addFromWithRamp (c, 0, bassBuffer.getReadPointer (c), n, lastBassGain, bg);
    lastChordGain = cg;
    lastBassGain = bg;

    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i) d[i] = std::tanh (d[i]);
    }

    midi.swapWith (outMidi);
    sampleCounter += n;
}

//==============================================================================
File SoulChordProcessor::writeMidiFile (int which) const
{
    const auto P = getPerformance();
    MidiMessageSequence seq;
    const double tpq = 960.0;
    const double bpm = hostBpm.load();
    seq.addEvent (MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / jmax (20.0, bpm))), 0.0);
    seq.addEvent (MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

    if (P != nullptr)
    {
        for (const auto& e : P->events)
        {
            if (which == 0 && e.ch != 0) continue;
            if (which == 1 && e.ch != 1) continue;
            auto on = MidiMessage::noteOn (e.ch + 1, e.note, (uint8) jlimit (1, 127, e.vel));
            on.setTimeStamp (e.on * tpq);
            auto off = MidiMessage::noteOff (e.ch + 1, e.note);
            off.setTimeStamp (e.off * tpq);
            seq.addEvent (on);
            seq.addEvent (off);
        }
    }
    seq.updateMatchedPairs();

    MidiFile mf;
    mf.setTicksPerQuarterNote ((int) tpq);
    mf.addTrack (seq);

    const int key = (int) std::lround (getParam ("key"));
    static const char* suffix[3] = { "HopAm", "Bass", "HopAm+Bass" };
    String keyName = String (kKeyNames[jlimit (0, 11, key)]).replace ("/", "-");
    auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("SoulChord");
    dir.createDirectory();
    auto file = dir.getChildFile ("SoulChord-" + keyName + "-" + suffix[jlimit (0, 2, which)] + ".mid");
    file.deleteFile();
    if (auto stream = file.createOutputStream())
        mf.writeTo (*stream, 1);
    return file;
}

//==============================================================================
void SoulChordProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SoulChordProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            forceRebuild = true;
        }
}

AudioProcessorEditor* SoulChordProcessor::createEditor() { return new SoulChordEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SoulChordProcessor(); }
