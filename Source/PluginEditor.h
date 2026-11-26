#pragma once

#include "PluginProcessor.h"

namespace scui
{
namespace col
{
const juce::Colour bg     { 0xff15141c };
const juce::Colour panel  { 0xff201e2a };
const juce::Colour panel2 { 0xff2c2938 };
const juce::Colour accent { 0xff8b6cff };
const juce::Colour pass   { 0xff4a3d7a };
const juce::Colour bass   { 0xffffa04d };
const juce::Colour text   { 0xffe8e6f0 };
const juce::Colour dim    { 0xff9a96ab };
const juce::Colour error  { 0xffff6b6b };
} // namespace col

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class ChordStrip : public juce::Component
{
public:
    explicit ChordStrip (SoulChordProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    SoulChordProcessor& proc;
};

class KeyboardView : public juce::Component
{
public:
    explicit KeyboardView (SoulChordProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    SoulChordProcessor& proc;
};

class DragMidiButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragMidiButton (const juce::String& t, std::function<juce::File()> make) : text (t), makeFile (std::move (make)) {}
    std::function<void (const juce::String&)> onStatus;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::String text;
    std::function<juce::File()> makeFile;
    bool pressed = false, dragging = false;
};
} // namespace scui

//==============================================================================
class SoulChordEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SoulChordEditor (SoulChordProcessor&);
    ~SoulChordEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void populateProgressions (int genre);

    struct LabeledCombo
    {
        juce::Label label;
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };
    struct Knob
    {
        juce::Label label, valueLabel;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void setupCombo (LabeledCombo& c, const char* labelText, const char* paramId, bool attach = true);
    void setupKnob (Knob& k, const char* labelText, const char* paramId, bool showText);
    void placeCombo (LabeledCombo& c, int x, int y, int w);
    void placeKnob (Knob& k, int x, int y, int w);

    SoulChordProcessor& proc;
    scui::Look look;

    juce::TextButton modeSeq, modeTrig, previewBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> previewAtt;

    LabeledCombo genre, key, scale, prog, length;
    juce::Label customLabel, helpLabel, errorLabel;
    juce::TextEditor customEdit;

    Knob color, octave;
    LabeledCombo voicing, inversion;
    juce::ToggleButton voiceLead;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> voiceLeadAtt;

    LabeledCombo bass, pass, where;
    LabeledCombo rhythm, sound;
    Knob roll, swing, chordVol, bassVol;

    scui::ChordStrip strip;
    scui::KeyboardView keyboard;
    scui::DragMidiButton dragChords, dragBass, dragBoth;
    juce::Label statusLabel;
    juce::TooltipWindow tooltips { this, 400 };

    int shownGenre = -1;
    int shownProg = -1;
    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoulChordEditor)
};
