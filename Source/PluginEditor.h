//  PluginEditor.h — ALPHA-TUNE, interface 1408x768 (SPEC-INTERFACE-v0.6).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "gui/Theme.h"
#include "gui/Widgets.h"

/**  Canevas de dessin 1408x768.
 *   Tout l'habillage vit ici, et c'est CE composant qui porte la mise a
 *   l'echelle. Mettre le transform sur l'editeur lui-meme ne marchait pas :
 *   l'hote affichait la fenetre reduite sans reduire le dessin (on ne voyait
 *   que le coin haut-gauche). Un enfant transforme, lui, est toujours respecte. */
class HardTuneCanvas : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> onPaint;
    std::function<void()> onResized;

    void paint (juce::Graphics& g) override  { if (onPaint)   onPaint (g); }
    void resized() override                  { if (onResized) onResized(); }
};

class HardTuneEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit HardTuneEditor (HardTuneProcessor&);
    ~HardTuneEditor() override;

    void paint (juce::Graphics&) override;     // fond de fenetre
    void resized() override;                   // place et echelle le canevas
    void paintCanvas (juce::Graphics&);        // tout le dessin, en 1408x768
    void layoutCanvas();                       // coordonnees fixes du canevas

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
    HardTuneCanvas canvas;
    double currentScale = 1.0;
    ht::Palette pal;
    bool lastDarkState = false;
    int  sizeIdx = 0;
    juce::int64 lastScopeTotal = 0;

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
