//  PluginEditor.h — interface paysage 1408x768, fidèle à la maquette validée
//  contre l'image de référence (élément par élément).
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
    void applyTheme();
    float rawValue (const char* id, float fallback = 0.0f) const;
    void drawCard (juce::Graphics&, juce::Rectangle<int>) const;
    juce::Label& cap (juce::Label&, const juce::String&, int x, int y, int h = 19,
                      juce::Colour c = juce::Colour (0xff0f1b23));

    HardTuneProcessor& proc;
    ht::Palette pal;
    bool lastDarkState = false;

    // ---- en-tête (24,18,1360x60) -------------------------------------------
    juce::Label  titleLabel, keyCap, scaleCap, lightCap, bypassCap;
    juce::ComboBox keyBox, scaleBox;
    ht::PillSwitch lightSwitch { "light" }, bypassSwitch { "bypass" };
    juce::TextButton sizeButton { "size" };   // cycle : petit / normal / grand

    // ---- clavier (24,95,1360x150) ------------------------------------------
    ht::KeyStrip keys;
    juce::Label  keysCap;

    // ---- WAVEFORM · PITCH (24,265,758x470) ---------------------------------
    juce::Label  waveCap;
    ht::Scope    scope;

    // ---- RETUNE SPEED (797,265,328x217) ------------------------------------
    juce::Label  retuneCap, retuneVal, humanizeCap, humanizeVal, flexCap, flexVal;
    ht::Knob     retuneKnob, humanizeKnob, flexKnob;

    // ---- CORRECTION (1142,265,240x217) -------------------------------------
    juce::Label  corrCap, vibratoCap, vibratoVal, rateCap, rateVal;
    ht::Knob     vibratoKnob, rateKnob;

    // ---- carte du bas (797,500,585x235) : meters, cadran, barres -----------
    juce::Label  inCap, outCap, inPeak, outPeak, noteCap, formantCap, transposeCap, transposeVal;
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
