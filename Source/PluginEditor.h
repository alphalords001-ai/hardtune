//  PluginEditor.h — l'interface du plugin, fidèle à la maquette validée.
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

    /** Corps du timer : appelé sous try/catch, ne doit jamais laisser passer d'exception. */
    void refreshFromProcessor();

    /** Lecture tolérante d'un paramètre (getRawParameterValue peut renvoyer nullptr). */
    float paramValue (const char* id, float fallback = 0.0f) const;

    void applyTheme();
    void drawCard (juce::Graphics&, juce::Rectangle<int>) const;

    juce::Label& makeLabel (juce::Label&, const juce::String& text, float height,
                            juce::Colour colour, bool bold = false);
    juce::Label& makeCaption (juce::Label&, const juce::String& text, float height);

    HardTuneProcessor& proc;
    ht::Palette pal;

    /** Dernier theme applique. Membre et non `static` : un static dans une
        fonction membre est partage par TOUTES les instances du plugin. */
    bool lastDark = false;

    // ---- en-tête -----------------------------------------------------------
    juce::Label  titleLabel, keyCaption, scaleCaption, lightCaption, darkCaption, bypassCaption;
    juce::ComboBox keyBox, scaleBox;
    ht::PillSwitch darkSwitch { "dark" }, bypassSwitch { "bypass" };

    // ---- clavier + oscilloscope --------------------------------------------
    ht::KeyStrip keys;
    ht::Scope    scope;
    juce::Label  scopeCaption;

    // ---- colonne de droite --------------------------------------------------
    juce::Label  retuneCaption, retuneValue;
    ht::Knob     retuneKnob;                 // Ø92
    juce::Label  humanizeCaption, humanizeValue, flexCaption, flexValue;
    ht::Knob     humanizeKnob, flexKnob;     // Ø68
    juce::Label  vibCaption, vibValue, rateCaption, rateValue;
    ht::Knob     vibratoKnob, rateKnob;
    ht::LevelMeter meterIn, meterOut;

    // ---- carte du grand cadran ---------------------------------------------
    ht::Gauge    gauge;
    juce::Label  noteLabel, pitchCaption, targetCaption;
    juce::Label  formantCaption, formantValue, transposeCaption, transposeValue;
    ht::BarControl formantBar, transposeBar;

    // ---- pièces jointes aux paramètres -------------------------------------
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAtt> retuneAtt, humanizeAtt, flexAtt, vibratoAtt, rateAtt,
                               formantAtt, transposeAtt;
    std::unique_ptr<ButtonAtt> darkAtt, bypassAtt;
    std::unique_ptr<ComboAtt>  keyAtt, scaleAtt;

    // rectangles des cartes (dessinées dans paint)
    juce::Rectangle<int> rHeader, rKeys, rScope, rRetuneCard, rCorrCard, rMetersCard,
                         rGaugeCard, rGaugeInner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardTuneEditor)
};
