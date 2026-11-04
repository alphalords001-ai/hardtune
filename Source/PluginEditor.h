//  PluginEditor.h — ALPHA-TUNE, interface 1408x768 (SPEC-INTERFACE-v0.6).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "gui/Theme.h"
#include "gui/Widgets.h"

class HardTuneEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit HardTuneEditor (HardTuneProcessor&);
    ~HardTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void cycleSize();
    void applyScale (double factor);
    void applyTheme();
    float rawValue (const char* id, float fallback = 0.0f) const;

    /** Libelle (x,y) = haut-gauche d'une boite de 140 px centree sur x. */
    juce::Label& label (juce::Label&, const juce::String& text, float size,
                        juce::Colour colour, bool bold = false, float dx = 0.0f);

    HardTuneProcessor& proc;
    ht::Palette pal;
    bool lastDarkState = false;
    int  sizeIdx = 0;

    // ---- en-tete (24,18,1360x60) -------------------------------------------
    juce::Label  titleLabel, keyCap, scaleCap, lightCap, bypassCap;
    ht::PillLookAndFeel pillLnf;          // declare AVANT les combos (duree de vie)
    juce::ComboBox keyBox, scaleBox;
    ht::PillSwitch lightSwitch { "light" }, bypassSwitch { "bypass" };
    juce::TextButton sizeButton { "size" };

    // ---- clavier (24,95,1360x150) ------------------------------------------
    ht::KeyStrip keys;
    juce::Label  keysCap;

    // ---- WAVEFORM · PITCH (24,265,758x470) ---------------------------------
    juce::Label  waveCap;
    ht::Scope    scope;

    // ---- RETUNE SPEED (797,265,328x217) ------------------------------------
    juce::Label  retuneCap, retuneVal, retuneSub;
    juce::Label  humanizeCap, humanizeVal, flexCap, flexVal;
    ht::Knob     retuneKnob, humanizeKnob, flexKnob;

    // ---- CORRECTION (1142,265,240x217) -------------------------------------
    juce::Label  corrCap, vibratoCap, vibratoVal, rateCap, rateVal;
    ht::Knob     vibratoKnob, rateKnob;

    // ---- carte du bas (797,500,585x235) ------------------------------------
    juce::Label  inCap, inCap2, outCap, outCap2, inPeak, inPeakVal, outPeak, outPeakVal;
    juce::Label  formantCap, transposeCap, transposeVal;
    ht::LevelMeter meterIn, meterOut;
    ht::Gauge    gauge;
    ht::BarControl formantBar, transposeBar;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAtt> retuneAtt, humanizeAtt, flexAtt, vibratoAtt, rateAtt,
                               formantAtt, transposeAtt;
    std::unique_ptr<ButtonAtt> bypassAtt;
    std::unique_ptr<ComboAtt>  keyAtt, scaleAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardTuneEditor)
};
