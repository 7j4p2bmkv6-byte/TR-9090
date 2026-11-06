#pragma once
#include "PluginProcessor.h"

struct KnobLnF : juce::LookAndFeel_V4
{
    KnobLnF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
};

class TR9090Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TR9090Editor (TR9090Processor&);
    ~TR9090Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void refreshKitBox();
    juce::Slider* makeKnob (const juce::String& id);

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    TR9090Processor& proc;
    KnobLnF lnf;

    struct Knob { std::unique_ptr<juce::Slider> s; std::unique_ptr<SA> a; juce::String label; };
    struct Group { juce::String name; int first = 0, count = 0; juce::Rectangle<int> area; };
    std::vector<Knob> knobs;
    std::vector<Group> groups;

    juce::Slider tempo, shuffle;
    std::unique_ptr<SA> tempoA, shuffleA;
    juce::TextButton startBtn { "START / STOP" }, writeBtn { "WRITE KIT" }, exportBtn { "EXPORT BANK" },
                     importBtn { "IMPORT BANK" }, copyBtn { "COPY" }, pasteBtn { "PASTE" }, clearBtn { "CLEAR PATTERN" };
    juce::ComboBox kitBox;
    std::unique_ptr<juce::FileChooser> chooser;

    int selRow = 0;
    juce::Rectangle<int> stepR[kSteps], patR[kPatterns], instR[kRows], displayR;
    juce::String clip; bool hasClip = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TR9090Editor)
};
