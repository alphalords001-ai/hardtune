//  PluginEditor.h — interface du plugin.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "gui/HardTuneLookAndFeel.h"
#include "gui/PitchDisplay.h"

class HardTuneEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit HardTuneEditor (HardTuneProcessor&);
    ~HardTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label>  title;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void timerCallback() override;

    void setupKnob (Knob& k, const juce::String& paramId, const juce::String& text,
                    const juce::String& suffix);
    void buildKeyButtons();
    void buildCustomToggles();
    void applyPreset (int index);
    void refreshKeyButtonsFromParameter();

    HardTuneProcessor& proc;
    HardTuneLookAndFeel lnf;

    // --- en-tête ------------------------------------------------------------
    juce::ComboBox  presetBox;
    juce::TextButton tpainButton { "T-PAIN" };
    juce::TextButton bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;

    // --- tonalité / gamme ---------------------------------------------------
    std::array<std::unique_ptr<juce::TextButton>, 12> keyButtons;
    juce::ComboBox scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttach;

    std::array<std::unique_ptr<juce::ToggleButton>, 12> customToggles;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 12> customAttaches;
    juce::ToggleButton customOnButton { "CUSTOM" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> customOnAttach;

    // --- boutons rotatifs ---------------------------------------------------
    Knob kRetune, kAmount, kFormant, kMix, kTranspose, kOutput;

    // --- écran ---------------------------------------------------------------
    ht::PitchDisplay display;

    int lastKeyIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardTuneEditor)
};
