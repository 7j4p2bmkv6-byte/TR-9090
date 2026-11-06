#include "PluginEditor.h"

namespace col
{
    const juce::Colour panel (0xffdcd9d2), header (0xffcfccc4), orange (0xffe8641e), dark (0xff2a2a2a),
                       text (0xff333333), led (0xffff3b1f), key (0xfff1efe9);
}
static const char* instNames[kRows] = { "BD","SD","LT","MT","HT","RIM","CLAP","CH","OH","CRASH","RIDE","ACCENT" };

// ---------------- Look & feel ----------------
KnobLnF::KnobLnF()
{
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::ComboBox::backgroundColourId, col::key);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff999999));
    setColour (juce::ComboBox::arrowColourId, col::orange);
    setColour (juce::PopupMenu::backgroundColourId, col::key);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::orange);
}

void KnobLnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5);
    const float d = std::min (r.getWidth(), r.getHeight());
    auto c = r.withSizeKeepingCentre (d, d);
    const auto ctr = c.getCentre();
    g.setColour (juce::Colour (0xff666666));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / 10.f, r1 = d * 0.5f + 1, r2 = d * 0.5f + 4;
        g.drawLine (ctr.x + r1 * std::sin (a), ctr.y - r1 * std::cos (a), ctr.x + r2 * std::sin (a), ctr.y - r2 * std::cos (a), 1.f);
    }
    auto k = c.reduced (d * 0.06f);
    g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillEllipse (k.translated (1.5f, 2.f));
    g.setColour (col::dark); g.fillEllipse (k);
    g.setColour (juce::Colour (0xff474747)); g.fillEllipse (k.reduced (k.getWidth() * 0.16f));
    const float ang = a0 + pos * (a1 - a0), rr = k.getWidth() * 0.46f;
    g.setColour (juce::Colours::white);
    g.drawLine (ctr.x + rr * 0.35f * std::sin (ang), ctr.y - rr * 0.35f * std::cos (ang),
                ctr.x + rr * std::sin (ang), ctr.y - rr * std::cos (ang), 2.5f);
}

void KnobLnF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1);
    g.setColour (juce::Colours::black.withAlpha (0.2f)); g.fillRoundedRectangle (r.translated (0, 2), 3);
    g.setColour (down ? juce::Colour (0xffd8d3c6) : over ? juce::Colour (0xfffaf8f2) : col::key);
    g.fillRoundedRectangle (r, 3);
    g.setColour (juce::Colour (0xff9a968c)); g.drawRoundedRectangle (r, 3, 1);
}

// ---------------- Editor ----------------
juce::Slider* TR9090Editor::makeKnob (const juce::String& id)
{
    Knob k;
    k.s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
    k.s->setLookAndFeel (&lnf);
    k.s->setPopupDisplayEnabled (true, false, this);
    addAndMakeVisible (*k.s);
    k.a = std::make_unique<SA> (proc.apvts, id, *k.s);
    knobs.push_back (std::move (k));
    return knobs.back().s.get();
}

TR9090Editor::TR9090Editor (TR9090Processor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    struct GDef { const char* name; std::vector<std::pair<const char*, const char*>> k; };
    const std::vector<GDef> defs = {
        { "ACCENT",     { {"accent","TOTAL"} } },
        { "BASS DRUM",  { {"bd_tune","TUNE"},{"bd_level","LEVEL"},{"bd_attack","ATTACK"},{"bd_decay","DECAY"} } },
        { "SNARE DRUM", { {"sd_tune","TUNE"},{"sd_level","LEVEL"},{"sd_tone","TONE"},{"sd_snappy","SNAPPY"} } },
        { "LOW TOM",    { {"lt_tune","TUNE"},{"lt_level","LEVEL"},{"lt_decay","DECAY"} } },
        { "MID TOM",    { {"mt_tune","TUNE"},{"mt_level","LEVEL"},{"mt_decay","DECAY"} } },
        { "HI TOM",     { {"ht_tune","TUNE"},{"ht_level","LEVEL"},{"ht_decay","DECAY"} } },
        { "RIM/CLAP",   { {"rs_level","RIM"},{"cp_level","CLAP"} } },
        { "HI HAT",     { {"hh_level","LEVEL"},{"ch_decay","CH DEC"},{"oh_decay","OH DEC"} } },
        { "CRASH",      { {"cr_level","LEVEL"},{"cr_tune","TUNE"} } },
        { "RIDE",       { {"rd_level","LEVEL"},{"rd_tune","TUNE"} } },
        { "MASTER",     { {"volume","VOLUME"} } } };
    for (auto& d : defs)
    {
        Group gr; gr.name = d.name; gr.first = (int) knobs.size(); gr.count = (int) d.k.size();
        for (auto& [id, label] : d.k) { makeKnob (id); knobs.back().label = label; }
        groups.push_back (gr);
    }

    for (auto* s : { &tempo, &shuffle })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setLookAndFeel (&lnf);
        addAndMakeVisible (*s);
    }
    shuffle.setPopupDisplayEnabled (true, false, this);
    tempoA = std::make_unique<SA> (proc.apvts, "tempo", tempo);
    shuffleA = std::make_unique<SA> (proc.apvts, "shuffle", shuffle);

    for (auto* b : { &startBtn, &writeBtn, &exportBtn, &importBtn, &copyBtn, &pasteBtn, &clearBtn }) addAndMakeVisible (*b);
    addAndMakeVisible (kitBox);
    refreshKitBox();

    startBtn.onClick = [this] { proc.running = ! proc.running.load(); };

    kitBox.onChange = [this]
    {
        const int id = kitBox.getSelectedId();
        if (id >= 1 && id < 100) proc.loadFactoryKit (id - 1);
        else if (id >= 100) proc.loadUserKit (id - 100);
    };

    writeBtn.onClick = [this]
    {
        const int id = kitBox.getSelectedId();
        if (id < 100)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Write kit",
                "Select a USER slot in the kit list first, then press WRITE KIT to store the current sound.");
            return;
        }
        const int slot = id - 100;
        proc.storeUserKit (slot, "User " + juce::String (slot + 1) + " - " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M"));
        refreshKitBox();
        kitBox.setSelectedId (id, juce::dontSendNotification);
    };

    copyBtn.onClick  = [this] { clip = proc.encodePattern (proc.currentPattern); hasClip = true; };
    pasteBtn.onClick = [this] { if (hasClip) proc.decodePattern (proc.currentPattern, clip); };
    clearBtn.onClick = [this] { proc.decodePattern (proc.currentPattern, {}); };

    exportBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export TR-9090 bank",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("TR9090 Bank.xml"), "*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f != juce::File()) proc.bankToXml()->writeTo (f.withFileExtension ("xml"));
            });
    };

    importBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import TR-9090 bank",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                if (auto xml = juce::XmlDocument::parse (fc.getResult()))
                    if (xml->hasTagName ("TR9090Bank")) { proc.bankFromXml (*xml); refreshKitBox(); }
            });
    };

    setSize (1100, 530);
    startTimerHz (30);
}

TR9090Editor::~TR9090Editor()
{
    stopTimer();
    for (auto& k : knobs) k.s->setLookAndFeel (nullptr);
    tempo.setLookAndFeel (nullptr); shuffle.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TR9090Editor::refreshKitBox()
{
    const int sel = kitBox.getSelectedId();
    kitBox.clear (juce::dontSendNotification);
    kitBox.addSectionHeading ("FACTORY KITS");
    for (int i = 0; i < proc.getNumFactoryKits(); ++i) kitBox.addItem (proc.getFactoryKitName (i), i + 1);
    kitBox.addSectionHeading ("USER MEMORY");
    for (int s = 0; s < kUserKits; ++s)
        kitBox.addItem (proc.userKitUsed[s] ? proc.userKitName[s] : "User " + juce::String (s + 1) + " (empty)", 100 + s);
    kitBox.setTextWhenNothingSelected ("Sound memory...");
    if (sel > 0) kitBox.setSelectedId (sel, juce::dontSendNotification);
}

void TR9090Editor::resized()
{
    // knob groups
    int x = 16;
    const int colW = 54, knob = 48, top = 96;
    for (auto& g : groups)
    {
        const int cols = (g.count + 1) / 2;
        g.area = { x, 72, cols * colW + 8, 180 };
        for (int i = 0; i < g.count; ++i)
        {
            const int c = i % cols, r = i / cols;
            knobs[(size_t) (g.first + i)].s->setBounds (x + 4 + c * colW + (colW - knob) / 2, top + r * 76, knob, knob);
        }
        x += g.area.getWidth() + 6;
    }

    // transport / memory strip
    startBtn.setBounds (16, 282, 100, 56);
    displayR = { 126, 290, 80, 38 };
    tempo.setBounds (212, 268, 64, 64);
    shuffle.setBounds (284, 274, 54, 54);
    for (int i = 0; i < kPatterns; ++i) patR[i] = { 356 + (i % 8) * 34, 276 + (i / 8) * 32, 30, 26 };
    kitBox.setBounds (646, 278, 190, 26);
    writeBtn.setBounds (842, 278, 80, 26);
    exportBtn.setBounds (646, 312, 134, 26);
    importBtn.setBounds (786, 312, 136, 26);
    copyBtn.setBounds (932, 278, 74, 26);
    pasteBtn.setBounds (1010, 278, 74, 26);
    clearBtn.setBounds (932, 312, 152, 26);

    for (int i = 0; i < kRows; ++i) instR[i] = { 16 + i * 89, 372, 83, 28 };
    for (int i = 0; i < kSteps; ++i) stepR[i] = { 16 + i * 67 + (i / 4) * 2, 428, 58, 74 };
}

void TR9090Editor::paint (juce::Graphics& g)
{
    g.fillAll (col::panel);
    // header
    g.setColour (col::header); g.fillRect (0, 0, getWidth(), 62);
    g.setColour (col::orange); g.setFont (juce::FontOptions (16.f, juce::Font::bold));
    g.drawText ("LANDGREN AUDIO", 18, 6, 300, 18, juce::Justification::left);
    g.setColour (col::text); g.setFont (juce::FontOptions (40.f, juce::Font::bold));
    g.drawText ("TR-9090", 16, 20, 300, 40, juce::Justification::left);
    g.setFont (juce::FontOptions (22.f)); 
    g.drawText ("R H Y T H M   C O M P O S E R", getWidth() - 460, 26, 440, 30, juce::Justification::right);
    g.setColour (col::orange); g.fillRect (0, 62, getWidth(), 3);

    // knob groups
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    for (auto& gr : groups)
    {
        g.setColour (col::orange); g.fillRect (gr.area.getX(), gr.area.getY(), gr.area.getWidth(), 16);
        g.setColour (juce::Colours::white);
        g.drawText (gr.name, gr.area.getX(), gr.area.getY(), gr.area.getWidth(), 16, juce::Justification::centred);
        g.setColour (juce::Colour (0xffb8b4aa)); g.drawRect (gr.area, 1);
        g.setColour (col::text);
        for (int i = 0; i < gr.count; ++i)
        {
            auto b = knobs[(size_t) (gr.first + i)].s->getBounds();
            g.drawText (knobs[(size_t) (gr.first + i)].label, b.getX() - 6, b.getBottom(), b.getWidth() + 12, 14, juce::Justification::centred);
        }
    }

    // tempo display
    g.setColour (juce::Colour (0xff1a0806)); g.fillRoundedRectangle (displayR.toFloat(), 3);
    g.setColour (col::led); g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 24.f, juce::Font::bold));
    g.drawText (juce::String (tempo.getValue(), 1), displayR, juce::Justification::centred);
    g.setColour (col::text); g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("TEMPO", tempo.getX(), tempo.getBottom(), tempo.getWidth(), 14, juce::Justification::centred);
    g.drawText ("SHUFFLE " + juce::String ((int) shuffle.getValue()), shuffle.getX() - 10, shuffle.getBottom() + 4, shuffle.getWidth() + 20, 14, juce::Justification::centred);
    g.drawText ("PATTERN", 356, 260, 270, 14, juce::Justification::left);
    g.drawText (proc.isStandalone() ? "INTERNAL CLOCK" : "HOST SYNC / INT", 16, 262, 110, 14, juce::Justification::left);

    // start LED
    g.setColour (proc.running.load() ? col::led : juce::Colour (0xff5a2a22));
    g.fillEllipse ((float) startBtn.getRight() - 14, (float) startBtn.getY() + 6, 8, 8);

    // pattern buttons
    const int cur = proc.currentPattern.load(), q = proc.queuedPattern.load();
    for (int i = 0; i < kPatterns; ++i)
    {
        g.setColour (i == cur ? col::orange : col::key); g.fillRoundedRectangle (patR[i].toFloat(), 3);
        g.setColour (i == q ? col::led : juce::Colour (0xff9a968c)); g.drawRoundedRectangle (patR[i].toFloat(), 3, i == q ? 2.f : 1.f);
        g.setColour (i == cur ? juce::Colours::white : col::text);
        g.drawText (juce::String (i + 1), patR[i], juce::Justification::centred);
    }

    // instrument select
    g.drawText ("INSTRUMENT SELECT  (click = select + preview)", 16, 356, 400, 14, juce::Justification::left);
    for (int i = 0; i < kRows; ++i)
    {
        const bool s = (i == selRow);
        g.setColour (s ? col::orange : col::key); g.fillRoundedRectangle (instR[i].toFloat(), 3);
        g.setColour (juce::Colour (0xff9a968c)); g.drawRoundedRectangle (instR[i].toFloat(), 3, 1);
        g.setColour (s ? juce::Colours::white : col::text);
        g.drawText (instNames[i], instR[i], juce::Justification::centred);
    }

    // step keys
    g.setColour (col::dark); g.fillRect (10, 412, getWidth() - 20, 3);
    const int play = proc.playingStep.load();
    for (int i = 0; i < kSteps; ++i)
    {
        auto r = stepR[i].toFloat();
        const bool on = proc.pattern[cur][selRow][i].load() != 0;
        g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (r.translated (0, 3), 4);
        g.setColour (i == play ? juce::Colour (0xfffff0d8) : col::key); g.fillRoundedRectangle (r, 4);
        g.setColour (juce::Colour (0xff9a968c)); g.drawRoundedRectangle (r, 4, 1);
        auto led = juce::Rectangle<float> (r.getCentreX() - 6, r.getY() + 10, 12, 12);
        g.setColour (on ? col::led : juce::Colour (0xff6b3a32)); g.fillEllipse (led);
        if (on) { g.setColour (col::led.withAlpha (0.3f)); g.fillEllipse (led.expanded (4)); }
        if (i == play) { g.setColour (col::orange); g.drawRoundedRectangle (r.reduced (2), 4, 2.f); }
        g.setColour (col::text); g.setFont (juce::FontOptions (14.f, juce::Font::bold));
        g.drawText (juce::String (i + 1), r.withTrimmedTop (36).toNearestInt(), juce::Justification::centred);
    }
    g.setFont (juce::FontOptions (10.f));
    g.drawText ("Plugin: follows host transport. MIDI: GM drum map (36 BD, 38 SD, 41/45/48 toms, 37 RIM, 39 CLAP, 42 CH, 46 OH, 49 CRASH, 51 RIDE)",
                16, getHeight() - 20, getWidth() - 32, 14, juce::Justification::left);
}

void TR9090Editor::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    const int cur = proc.currentPattern.load();
    for (int i = 0; i < kSteps; ++i)
        if (stepR[i].contains (pt)) { auto& s = proc.pattern[cur][selRow][i]; s.store (s.load() ? 0 : 1); repaint(); return; }
    for (int i = 0; i < kRows; ++i)
        if (instR[i].contains (pt))
        {
            selRow = i;
            if (i < kVoices) proc.previewMask.fetch_or (1 << i);
            repaint(); return;
        }
    for (int i = 0; i < kPatterns; ++i)
        if (patR[i].contains (pt))
        {
            if (proc.running.load() || proc.playingStep.load() >= 0) proc.queuedPattern = i; // switch at bar end
            else proc.currentPattern = i;
            repaint(); return;
        }
}
