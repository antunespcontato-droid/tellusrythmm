#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include "../Engine/Engine.h"
#include "../Engine/DrumSynth.h"

class PsyProcessor : public juce::AudioProcessor
{
public:
    PsyProcessor();
    ~PsyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---------- parâmetros ----------
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const ps::ParamDef& def (int index);
    float getParam (int index) const { return raw[(size_t) index]->load(); }
    void setParamReal (int index, float realValue);          // thread de mensagens
    void resetAllParamsToDefault();

    // ---------- padrões (UI <-> áudio sem bloquear o áudio) ----------
    ps::Pattern copyPattern (int which);                        // -1 = principal, 0..7 = lanes
    void editPattern (int which, const std::function<void (ps::Pattern&)>& fn);
    ps::PatternBank copyBank();
    void setBank (const ps::PatternBank& b);
    void requestSeqReset() { seqResetRequested.store (true); }

    // ---------- presets ----------
    static juce::File presetDirectory();
    juce::StringArray listUserPresets() const;
    bool savePreset (const juce::String& name);
    bool loadPresetFile (const juce::File& f);
    static juce::StringArray factoryPresetNames();
    void loadFactoryPreset (int index);
    juce::String currentPresetName { "Init" };

    // ---------- feedback para a UI ----------
    std::atomic<int> uiMainStep { -1 };
    std::array<std::atomic<int>, ps::kNumLanes> uiLaneStep;
    std::atomic<bool> uiRunning { false };
    std::atomic<int> uiLoopState { 0 };
    std::atomic<float> uiBpm { 120.f };

private:
    std::array<std::atomic<float>*, (size_t) ps::kNumParams> raw {};
    float P[ps::kNumParams] {};

    ps::Engine* engine = nullptr;               // alocado em prepare (fora do áudio)
    std::unique_ptr<ps::Engine> engineOwner;
    std::unique_ptr<ps::DrumSynth> synth;
    std::unique_ptr<ps::OutBuffer> outBuf;
    juce::MidiBuffer midiOut;

    juce::SpinLock bankLock;
    ps::PatternBank editBank;
    std::atomic<bool> bankDirty { false };
    std::atomic<bool> seqResetRequested { false };

    double sr = 44100.0;
    bool prepared = false;

    void writeBankToState (juce::ValueTree& st) const;
    bool readBankFromState (const juce::ValueTree& st);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsyProcessor)
};
