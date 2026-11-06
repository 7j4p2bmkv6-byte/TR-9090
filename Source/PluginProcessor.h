#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include "Voices.h"

constexpr int kRows = 12, kSteps = 16, kPatterns = 16, kVoices = 11, kAccentRow = 11, kUserKits = 8;

class TR9090Processor : public juce::AudioProcessor
{
public:
    TR9090Processor();
    ~TR9090Processor() override = default;

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "TR-9090"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static const juce::StringArray& soundParamIds();
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // ---- Sequencer (UI writes, audio reads) ----
    std::atomic<uint8_t> pattern[kPatterns][kRows][kSteps];
    std::atomic<int> currentPattern { 0 }, queuedPattern { -1 }, playingStep { -1 }, previewMask { 0 };
    std::atomic<bool> running { false };
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }

    juce::String encodePattern (int p) const;
    void decodePattern (int p, const juce::String& s);
    void loadFactoryPatterns();

    // ---- Sound memory bank (kits) ----
    int getNumFactoryKits() const;
    juce::String getFactoryKitName (int i) const;
    void loadFactoryKit (int i);
    juce::String userKitName[kUserKits];
    bool userKitUsed[kUserKits] {};
    std::vector<float> userKit[kUserKits];
    void storeUserKit (int slot, const juce::String& name);
    void loadUserKit (int slot);

    // ---- Bank file (patterns + user kits) ----
    std::unique_ptr<juce::XmlElement> bankToXml() const;
    void bankFromXml (const juce::XmlElement&);

private:
    void updateVoiceParams();
    void fireStep (int step);
    void trigger (int voice, float vel);
    static int noteToVoice (int note);
    void setSoundParam (const juce::String& id, float v);
    juce::File userKitFile() const;
    void loadUserKitsFromDisk();
    void saveUserKitsToDisk();

    std::array<std::atomic<float>*, 27> raw {};
    std::atomic<float>* pVolume = nullptr; std::atomic<float>* pTempo = nullptr; std::atomic<float>* pShuffle = nullptr;

    double sr = 44100.0, internalPpq = 0.0; bool wasPlaying = false;

    tr::Kick bd; tr::Snare sd; tr::Tom lt { 80.f, 130.f }, mt { 120.f, 180.f }, ht { 165.f, 250.f };
    tr::Rim rs; tr::Clap cp; tr::Hat ch { 0.03f, 0.18f }, oh { 0.15f, 1.0f };
    tr::Cymbal cr { 1.8f, 6500.f, 0.95f }, rd { 2.4f, 4800.f, 1.35f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TR9090Processor)
};
