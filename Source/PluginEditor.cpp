#include "PluginEditor.h"

using namespace juce;
using scui::col::accent;
using scui::col::dim;
using scui::col::panel;
using scui::col::panel2;

static String vi (const char* s) { return String::fromUTF8 (s); }
static Font uiFont (float size, bool bold = false) { return Font (FontOptions (size, bold ? Font::bold : Font::plain)); }

//==============================================================================
namespace scui
{
Look::Look()
{
    setColour (ResizableWindow::backgroundColourId, col::bg);
    setColour (ComboBox::backgroundColourId, col::panel2);
    setColour (ComboBox::outlineColourId, col::panel2.brighter (0.15f));
    setColour (ComboBox::textColourId, col::text);
    setColour (ComboBox::arrowColourId, col::dim);
    setColour (PopupMenu::backgroundColourId, col::panel);
    setColour (PopupMenu::textColourId, col::text);
    setColour (PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (Slider::rotarySliderFillColourId, col::accent);
    setColour (Slider::rotarySliderOutlineColourId, col::panel2);
    setColour (Slider::thumbColourId, Colours::white);
    setColour (Slider::textBoxTextColourId, col::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (TextButton::buttonColourId, col::panel2);
    setColour (TextButton::buttonOnColourId, col::accent);
    setColour (TextButton::textColourOffId, col::text);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (ToggleButton::textColourId, col::text);
    setColour (ToggleButton::tickColourId, col::accent);
    setColour (ToggleButton::tickDisabledColourId, col::dim);
    setColour (Label::textColourId, col::text);
    setColour (TextEditor::backgroundColourId, col::panel2);
    setColour (TextEditor::textColourId, col::text);
    setColour (TextEditor::outlineColourId, col::panel2.brighter (0.15f));
    setColour (TextEditor::focusedOutlineColourId, col::accent);
    setColour (TooltipWindow::backgroundColourId, col::panel2);
    setColour (TooltipWindow::textColourId, col::text);
   #if JUCE_WINDOWS
    setDefaultSansSerifTypefaceName ("Segoe UI");
   #endif
}

void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&)
{
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (size, size);
    const float stroke = 4.0f;
    const float radius = size * 0.5f - stroke;
    const auto c = r.getCentre();
    const float angle = a0 + pos * (a1 - a0);

    Path track;
    track.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, a1, true);
    g.setColour (col::panel2);
    g.strokePath (track, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));

    Path val;
    val.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, angle, true);
    g.setColour (col::accent);
    g.strokePath (val, PathStrokeType (stroke, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (col::panel.brighter (0.1f));
    g.fillEllipse (r.reduced (stroke * 2.2f));
    const auto tip = c.getPointOnCircumference (radius - stroke * 2.5f, angle);
    g.setColour (Colours::white);
    g.drawLine (Line<float> (c.getPointOnCircumference (radius * 0.25f, angle), tip), 2.2f);
}

//==============================================================================
void ChordStrip::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (area, 8.0f);
    area = area.reduced (6.0f);

    const auto P = proc.getPerformance();
    if (P == nullptr) return;
    const int mode = (int) std::lround (proc.getParam ("mode"));

    if (mode == SoulChordProcessor::ModeSequencer)
    {
        const double L = jmax (0.001, P->loopBeats);
        const int cur = proc.currentStep.load();
        for (size_t i = 0; i < P->steps.size(); ++i)
        {
            const auto& st = P->steps[i];
            auto box = Rectangle<float> (area.getX() + (float) (st.start / L) * area.getWidth(), area.getY(),
                                         (float) (st.len / L) * area.getWidth(), area.getHeight()).reduced (2.0f, 0.0f);
            const bool isCur = (int) i == cur;
            g.setColour (isCur ? col::accent : (st.passing ? col::pass : col::panel2));
            g.fillRoundedRectangle (box, 6.0f);

            const bool narrow = box.getWidth() < 64.0f;
            g.setColour (Colours::white);
            g.setFont (uiFont (narrow ? 12.0f : 17.0f, true));
            g.drawFittedText (String::fromUTF8 (st.name.c_str()), box.withTrimmedBottom (box.getHeight() * 0.42f).toNearestInt(),
                              Justification::centredBottom, 1, 0.6f);
            g.setColour (isCur ? Colours::white : col::dim);
            g.setFont (uiFont (narrow ? 10.0f : 13.0f));
            g.drawFittedText (String::fromUTF8 (st.roman.c_str()), box.withTrimmedTop (box.getHeight() * 0.6f).toNearestInt(),
                              Justification::centredTop, 1, 0.6f);
        }
        if (! proc.transportRunning.load() && proc.getParam ("preview") < 0.5f)
        {
            g.setColour (col::dim);
            g.setFont (uiFont (11.0f));
            g.drawText (vi ("Bấm Play trong FL (hoặc nút Nghe thử) để chạy vòng"), area.withTrimmedTop (area.getHeight() - 13.0f),
                        Justification::centredRight);
        }
    }
    else
    {
        const int key = (int) std::lround (proc.getParam ("key"));
        const int scale = (int) std::lround (proc.getParam ("scale"));
        const int heldMask = proc.heldRelMask.load();
        const float w = area.getWidth() / 12.0f;
        for (int rel = 0; rel < 12; ++rel)
        {
            auto box = Rectangle<float> (area.getX() + rel * w, area.getY(), w, area.getHeight()).reduced (2.0f, 0.0f);
            const bool inScale = sc::scaleContains (scale, rel);
            const bool isHeld = (heldMask >> rel) & 1;
            g.setColour (isHeld ? col::accent : (inScale ? col::panel2 : col::pass.darker (0.35f)));
            g.fillRoundedRectangle (box, 6.0f);

            g.setColour (col::dim);
            g.setFont (uiFont (11.0f));
            g.drawText (vi ("phím ") + sc::noteName (key + rel, sc::preferFlats (key, scale)), box.removeFromTop (16.0f),
                        Justification::centred);
            g.setColour (Colours::white);
            g.setFont (uiFont (14.0f, true));
            g.drawFittedText (String::fromUTF8 (P->triggerNames[rel].c_str()), box.removeFromTop (box.getHeight() * 0.55f).toNearestInt(),
                              Justification::centred, 1, 0.6f);
            g.setColour (isHeld ? Colours::white : col::dim);
            g.setFont (uiFont (11.0f));
            g.drawFittedText (String::fromUTF8 (P->triggerRoman[rel].c_str()), box.toNearestInt(), Justification::centredTop, 1, 0.6f);
        }
    }
}

//==============================================================================
void KeyboardView::paint (Graphics& g)
{
    const int lo = 36, hi = 96;   // C2 .. C7
    auto area = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (area, 8.0f);
    area = area.reduced (6.0f, 5.0f);

    auto isBlack = [] (int n) { const int p = n % 12; return p == 1 || p == 3 || p == 6 || p == 8 || p == 10; };
    auto sounding = [this] (int n, bool bass)
    {
        const std::atomic<uint64_t>* arr = bass ? proc.soundingBass : proc.soundingChord;
        return ((arr[n >> 6].load() >> (n & 63)) & 1) != 0;
    };

    int whites = 0;
    for (int n = lo; n <= hi; ++n) if (! isBlack (n)) ++whites;
    const float ww = area.getWidth() / (float) whites;

    std::vector<float> xOf (128, 0.0f);
    int wi = 0;
    for (int n = lo; n <= hi; ++n)
    {
        if (isBlack (n)) continue;
        auto r = Rectangle<float> (area.getX() + wi * ww, area.getY(), ww, area.getHeight()).reduced (0.5f, 0.0f);
        xOf[(size_t) n] = r.getX();
        Colour c = Colour (0xffdedbe8);
        if (sounding (n, true)) c = col::bass;
        if (sounding (n, false)) c = col::accent;
        g.setColour (c);
        g.fillRoundedRectangle (r, 2.0f);
        if (n % 12 == 0)
        {
            g.setColour (Colour (0xff6b6780));
            g.setFont (uiFont (9.0f));
            g.drawText ("C" + String (n / 12 - 1), r.removeFromBottom (12.0f), Justification::centred);
        }
        ++wi;
    }
    for (int n = lo; n <= hi; ++n)
    {
        if (! isBlack (n)) continue;
        const float x = xOf[(size_t) (n - 1)] + ww * 0.68f;
        auto r = Rectangle<float> (x, area.getY(), ww * 0.64f, area.getHeight() * 0.6f);
        Colour c = Colour (0xff1b1a22);
        if (sounding (n, true)) c = col::bass;
        if (sounding (n, false)) c = col::accent;
        g.setColour (c);
        g.fillRoundedRectangle (r, 2.0f);
    }
}

//==============================================================================
void DragMidiButton::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (pressed ? col::accent : (isMouseOver() ? col::panel2.brighter (0.12f) : col::panel2));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (col::accent.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 7.0f, 1.2f);
    g.setColour (Colours::white);
    g.setFont (uiFont (14.0f, true));
    g.drawText (text, r, Justification::centred);
}

void DragMidiButton::mouseDown (const MouseEvent&)
{
    pressed = true;
    dragging = false;
    repaint();
}

void DragMidiButton::mouseDrag (const MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5) return;
    dragging = true;
    const File f = makeFile();
    SafePointer<DragMidiButton> safe (this);
    DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [safe]
    {
        if (safe != nullptr) { safe->pressed = false; safe->repaint(); }
    });
}

void DragMidiButton::mouseUp (const MouseEvent&)
{
    if (! dragging)
    {
        // a plain click saves a copy into Documents/SoulChord
        const File f = makeFile();
        auto dir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("SoulChord");
        dir.createDirectory();
        auto dest = dir.getNonexistentChildFile (f.getFileNameWithoutExtension(), ".mid");
        if (f.copyFileTo (dest) && onStatus)
            onStatus (vi ("Đã lưu: ") + dest.getFullPathName());
    }
    pressed = false;
    dragging = false;
    repaint();
}
} // namespace scui

//==============================================================================
SoulChordEditor::SoulChordEditor (SoulChordProcessor& p)
    : AudioProcessorEditor (&p), proc (p), strip (p), keyboard (p),
      dragChords (vi ("Kéo hợp âm vào FL"), [&p] { return p.writeMidiFile (0); }),
      dragBass (vi ("Kéo bass vào FL"), [&p] { return p.writeMidiFile (1); }),
      dragBoth (vi ("Kéo cả hai vào FL"), [&p] { return p.writeMidiFile (2); })
{
    setLookAndFeel (&look);

    // ---- header
    modeSeq.setButtonText (vi ("Tự chạy theo tempo"));
    modeTrig.setButtonText (vi ("Mỗi phím 1 hợp âm"));
    for (auto* b : { &modeSeq, &modeTrig })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    modeSeq.onClick = [this] { proc.setParam ("mode", 0.0f); };
    modeTrig.onClick = [this] { proc.setParam ("mode", 1.0f); };
    modeSeq.setTooltip (vi ("Bấm Play trong FL Studio, plugin tự đánh cả vòng theo tempo"));
    modeTrig.setTooltip (vi ("Mỗi phím trên bàn phím / Piano roll = một hợp âm hoàn chỉnh của giọng đang chọn"));

    previewBtn.setButtonText (vi ("Nghe thử"));
    previewBtn.setClickingTogglesState (true);
    previewBtn.setTooltip (vi ("Chạy vòng hợp âm ngay cả khi FL đang dừng"));
    addAndMakeVisible (previewBtn);
    previewAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "preview", previewBtn);

    // ---- key & progression
    setupCombo (genre, "Thể loại", "genre", false);
    genre.box.onChange = [this] { proc.applyGenreDefaults (genre.box.getSelectedItemIndex()); };
    setupCombo (key, "Giọng", "key");
    setupCombo (scale, "Thang âm", "scale");
    setupCombo (prog, "Vòng hòa âm", "prog", false);
    prog.box.onChange = [this]
    {
        const int idx = prog.box.getSelectedId() - 1;
        if (idx < 0) return;
        proc.setParam ("prog", (float) idx);
        if (idx < sc::customProgressionIndex())
        {
            const auto& pre = sc::progressionLibrary()[(size_t) idx];
            proc.setParam ("scale", (float) pre.scale);
            customEdit.setText (String::fromUTF8 (pre.text), dontSendNotification);
        }
        else
        {
            customEdit.setText (proc.getCustomProgression(), dontSendNotification);
            customEdit.grabKeyboardFocus();
        }
    };
    setupCombo (length, "Độ dài mỗi hợp âm", "length");

    customLabel.setText (vi ("Ký hiệu vòng:"), dontSendNotification);
    customLabel.setFont (uiFont (13.0f, true));
    addAndMakeVisible (customLabel);
    customEdit.setFont (uiFont (16.0f, true));
    customEdit.setJustification (Justification::centredLeft);
    customEdit.setTooltip (vi ("Gõ bậc hợp âm để tự tạo vòng. Sửa ở đây sẽ tự chuyển sang chế độ Tự nhập."));
    customEdit.onTextChange = [this]
    {
        proc.setCustomProgression (customEdit.getText());
        if ((int) std::lround (proc.getParam ("prog")) != sc::customProgressionIndex())
            proc.setParam ("prog", (float) sc::customProgressionIndex());
    };
    addAndMakeVisible (customEdit);
    helpLabel.setText (vi ("1-7 = bậc   b/# = giáng/thăng   m M 7 o s2 s4 = loại hợp âm   /1 /2 /3 = bass nốt 3/5/7   *2 = dài gấp đôi"),
                       dontSendNotification);
    helpLabel.setFont (uiFont (11.5f));
    helpLabel.setColour (Label::textColourId, dim);
    addAndMakeVisible (helpLabel);
    errorLabel.setFont (uiFont (12.0f, true));
    errorLabel.setColour (Label::textColourId, scui::col::error);
    errorLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (errorLabel);

    // ---- harmony
    setupKnob (color, "Màu hòa âm", "color", false);
    color.slider.setTooltip (vi ("Vặn phải để hợp âm \"đậm\" dần: Triad -> add9 -> 7 -> 9 -> 11/13 (chất R&B, neo-soul)"));
    setupCombo (voicing, "Kiểu voicing", "voicing");
    setupCombo (inversion, "Thế đảo", "inversion");
    voiceLead.setButtonText (vi ("Đi giọng mượt"));
    voiceLead.setTooltip (vi ("Tự chọn thế đảo để các nốt di chuyển gần nhau nhất giữa các hợp âm"));
    addAndMakeVisible (voiceLead);
    voiceLeadAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "voicelead", voiceLead);
    setupKnob (octave, "Quãng tám", "octave", false);

    // ---- bass & passing
    setupCombo (bass, "Bass", "bass");
    setupCombo (pass, "Passing chord", "pass");
    setupCombo (where, "Chèn passing ở", "where");

    // ---- rhythm & sound
    setupCombo (rhythm, "Nhịp điệu", "rhythm");
    setupKnob (roll, "Rải hợp âm", "roll", true);
    setupKnob (swing, "Swing", "swing", true);
    setupCombo (sound, "Tiếng", "sound");
    setupKnob (chordVol, "Âm lượng hợp âm", "chordvol", true);
    setupKnob (bassVol, "Âm lượng bass", "bassvol", true);
    roll.slider.textFromValueFunction = [] (double v) { return String (roundToInt (v)) + " ms"; };
    swing.slider.textFromValueFunction = [] (double v) { return String (roundToInt (v * 100.0)) + "%"; };
    chordVol.slider.textFromValueFunction = [] (double v) { return String (roundToInt (v * 100.0)) + "%"; };
    bassVol.slider.textFromValueFunction = chordVol.slider.textFromValueFunction;
    for (auto* k : { &roll, &swing, &chordVol, &bassVol }) k->slider.updateText();

    // ---- display + drag
    addAndMakeVisible (strip);
    addAndMakeVisible (keyboard);
    for (auto* d : { &dragChords, &dragBass, &dragBoth })
    {
        d->setTooltip (vi ("Giữ chuột và kéo thả vào Piano roll / Playlist của FL. Bấm 1 lần để lưu file .mid vào Documents\\SoulChord"));
        d->onStatus = [this] (const String& s) { statusText = s; };
        addAndMakeVisible (*d);
    }
    statusLabel.setFont (uiFont (11.5f));
    statusLabel.setColour (Label::textColourId, dim);
    addAndMakeVisible (statusLabel);

    setSize (1000, 676);
    timerCallback();
    startTimerHz (24);
}

SoulChordEditor::~SoulChordEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SoulChordEditor::setupCombo (LabeledCombo& c, const char* labelText, const char* paramId, bool attach)
{
    c.label.setText (vi (labelText), dontSendNotification);
    c.label.setFont (uiFont (12.5f));
    c.label.setColour (Label::textColourId, dim);
    addAndMakeVisible (c.label);
    addAndMakeVisible (c.box);

    if (auto* choice = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        c.box.addItemList (choice->choices, 1);

    if (attach)
        c.att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, c.box);
}

void SoulChordEditor::setupKnob (Knob& k, const char* labelText, const char* paramId, bool showText)
{
    k.label.setText (vi (labelText), dontSendNotification);
    k.label.setFont (uiFont (12.5f));
    k.label.setColour (Label::textColourId, dim);
    k.label.setJustificationType (Justification::centred);
    addAndMakeVisible (k.label);

    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (showText ? Slider::TextBoxBelow : Slider::NoTextBox, false, 70, 16);
    k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k.slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    addAndMakeVisible (k.slider);
    k.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);

    if (! showText)
    {
        k.valueLabel.setFont (uiFont (13.0f, true));
        k.valueLabel.setJustificationType (Justification::centred);
        addAndMakeVisible (k.valueLabel);
    }
}

void SoulChordEditor::placeCombo (LabeledCombo& c, int x, int y, int w)
{
    c.label.setBounds (x, y, w, 16);
    c.box.setBounds (x, y + 18, w, 28);
}

void SoulChordEditor::placeKnob (Knob& k, int x, int y, int w)
{
    k.label.setBounds (x, y, w, 16);
    const bool hasText = k.slider.getTextBoxPosition() != Slider::NoTextBox;
    k.slider.setBounds (x, y + 16, w, hasText ? 70 : 54);
    if (! hasText) k.valueLabel.setBounds (x, y + 70, w, 16);
}

void SoulChordEditor::populateProgressions (int g)
{
    prog.box.clear (dontSendNotification);
    const auto& lib = sc::progressionLibrary();
    for (int i = 0; i < (int) lib.size(); ++i)
        if (lib[(size_t) i].genre == g)
            prog.box.addItem (String::fromUTF8 (lib[(size_t) i].name), i + 1);
    prog.box.addSeparator();
    prog.box.addItem (vi ("Tự nhập / sửa vòng..."), sc::customProgressionIndex() + 1);
    shownGenre = g;
    shownProg = -1;
}

//==============================================================================
void SoulChordEditor::paint (Graphics& g)
{
    g.fillAll (scui::col::bg);

    g.setColour (Colours::white);
    g.setFont (uiFont (26.0f, true));
    g.drawText ("SoulChord", 16, 10, 200, 32, Justification::centredLeft);
    g.setColour (dim);
    g.setFont (uiFont (12.0f));
    g.drawText (vi ("Pop - R&B - Hip hop chord generator"), 16, 38, 300, 16, Justification::centredLeft);

    auto section = [&g] (Rectangle<int> r, const char* title)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (accent);
        g.setFont (uiFont (13.0f, true));
        g.drawText (vi (title), r.getX() + 12, r.getY() + 6, r.getWidth() - 24, 16, Justification::centredLeft);
    };
    section ({ 12, 66, 976, 138 }, "GIỌNG & VÒNG HÒA ÂM");
    section ({ 12, 212, 600, 132 }, "HÒA ÂM & VOICING");
    section ({ 620, 212, 368, 132 }, "BASS & PASSING CHORD");
    section ({ 12, 352, 976, 112 }, "NHỊP & TIẾNG");
}

void SoulChordEditor::resized()
{
    modeSeq.setBounds (330, 16, 170, 32);
    modeTrig.setBounds (504, 16, 170, 32);
    previewBtn.setBounds (868, 16, 120, 32);

    int y = 92;
    placeCombo (genre, 26, y, 120);
    placeCombo (key, 156, y, 100);
    placeCombo (scale, 266, y, 160);
    placeCombo (prog, 436, y, 330);
    placeCombo (length, 776, y, 200);

    customLabel.setBounds (26, 146, 100, 26);
    customEdit.setBounds (126, 146, 420, 28);
    errorLabel.setBounds (556, 146, 420, 26);
    helpLabel.setBounds (26, 178, 950, 18);

    y = 236;
    placeKnob (color, 26, y, 90);
    placeCombo (voicing, 130, y + 4, 160);
    placeCombo (inversion, 300, y + 4, 110);
    voiceLead.setBounds (130, y + 62, 160, 24);
    placeKnob (octave, 500, y, 90);

    placeCombo (bass, 634, y + 4, 168);
    placeCombo (pass, 812, y + 4, 164);
    placeCombo (where, 634, y + 56, 168);

    y = 376;
    placeCombo (rhythm, 26, y + 6, 170);
    placeKnob (roll, 210, y, 90);
    placeKnob (swing, 310, y, 90);
    placeCombo (sound, 420, y + 6, 150);
    placeKnob (chordVol, 760, y, 100);
    placeKnob (bassVol, 870, y, 100);

    strip.setBounds (12, 472, 976, 84);
    keyboard.setBounds (12, 562, 976, 62);

    dragChords.setBounds (12, 632, 190, 34);
    dragBass.setBounds (210, 632, 170, 34);
    dragBoth.setBounds (388, 632, 190, 34);
    statusLabel.setBounds (590, 632, 398, 34);
}

void SoulChordEditor::timerCallback()
{
    const int mode = (int) std::lround (proc.getParam ("mode"));
    modeSeq.setToggleState (mode == 0, dontSendNotification);
    modeTrig.setToggleState (mode == 1, dontSendNotification);
    previewBtn.setEnabled (mode == 0);

    const int g = (int) std::lround (proc.getParam ("genre"));
    if (g != shownGenre) populateProgressions (g);
    genre.box.setSelectedItemIndex (g, dontSendNotification);

    const int pIdx = (int) std::lround (proc.getParam ("prog"));
    if (pIdx != shownProg)
    {
        prog.box.setSelectedId (pIdx + 1, dontSendNotification);
        if (! customEdit.hasKeyboardFocus (true))
        {
            const String t = pIdx < sc::customProgressionIndex()
                                 ? String::fromUTF8 (sc::progressionLibrary()[(size_t) pIdx].text)
                                 : proc.getCustomProgression();
            customEdit.setText (t, dontSendNotification);
        }
        shownProg = pIdx;
    }

    static const char* colorNames[5] = { "Triad", "add9", "7th", "9th", "11th / 13th" };
    color.valueLabel.setText (colorNames[jlimit (0, 4, (int) std::lround (proc.getParam ("color")))], dontSendNotification);
    const int oct = (int) std::lround (proc.getParam ("octave"));
    octave.valueLabel.setText (oct > 0 ? "+" + String (oct) : String (oct), dontSendNotification);

    const auto P = proc.getPerformance();
    errorLabel.setText (P != nullptr && ! P->error.empty() ? vi ("Lỗi: ") + String::fromUTF8 (P->error.c_str()) : String(),
                        dontSendNotification);

    const bool seqMode = mode == 0;
    rhythm.box.setEnabled (seqMode);
    length.box.setEnabled (seqMode);
    pass.box.setEnabled (seqMode);
    where.box.setEnabled (seqMode);
    swing.slider.setEnabled (seqMode);

    statusLabel.setText (statusText.isNotEmpty() ? statusText
                                                 : (seqMode ? vi ("Mẹo: kéo nút MIDI thả vào Piano roll để chỉnh tiếp bằng synth khác")
                                                            : vi ("Phím trắng = hợp âm trong giọng, phím đen = dominant phụ / mượn giọng")),
                         dontSendNotification);

    strip.repaint();
    keyboard.repaint();
}
