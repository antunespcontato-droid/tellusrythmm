#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class PsyLnF;
class PageBase;
class Splash;

class PsyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PsyEditor (PsyProcessor&);
    ~PsyEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPage (int i);
    void showPresetMenu();
    void stepPreset (int dir);
    void promptSave();
    void refreshPresetLabel();

    PsyProcessor& proc;
    std::unique_ptr<PsyLnF> lnf;
    juce::Image logo;

    juce::TextButton tabs[4];
    std::unique_ptr<PageBase> pages[4];
    int currentPage = 0;

    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" };
    juce::OwnedArray<juce::Component> headerControls;
    juce::Rectangle<int> logoArea, statusArea;
    std::unique_ptr<Splash> splash;
    juce::StringArray presetOrder;   // "F:n" ou "U:nome"

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsyEditor)
};
