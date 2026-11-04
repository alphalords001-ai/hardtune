//  PluginEditor.h — interface paysage (maquette 1408 x 768).
//
//  v0.9 : reprise de la maquette v0.6 (grande interface paysage) avec les
//  corrections de la v0.8.3 :
//    - FLEX-TUNE est cable sur le parametre `amount` (il etait eteint et
//      affichait la valeur de HUMANIZE) ;
//    - le clavier est pilote par tune::NoteMapper::scaleMask() (les 9 gammes),
//      et la note jouee est surlignee ;
//    - FORMANT (parametre inerte) est retire de l'interface ;
//    - LIGHT / DARK entourent l'interrupteur, comme en v0.8.3 ;
//    - la fenetre est mise a l'echelle au lieu de deborder de l'ecran.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "gui/Theme.h"
#include "gui/Widgets.h"

class HardTuneEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    //  Taille de la maquette. Toutes les positions de ce fichier sont ecrites
    //  dans ce repere, puis converties par place().
    static constexpr int kDesignW = 1408;
    static constexpr int kDesignH = 768;

    explicit HardTuneEditor (HardTuneProcessor&);
    ~HardTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyTheme();
    float rawValue (const char* id, float fallback = 0.0f) const;
    juce::Label& cap (juce::Label&, const juce::String&, int height = 19,
                      juce::Colour c = juce::Colour (0xff0f1b23));

    //  Place un composant en coordonnees MAQUETTE ; la conversion vers la
    //  taille reelle de la fenetre se fait ici, et nulle part ailleurs.
    void place (juce::Component&, int x, int y, int w, int h);

    HardTuneProcessor& proc;
    ht::Palette pal;
    bool lastDarkState = false;
    float uiScale = 1.0f;

    // ---- en-tete (24,18,1360x60) -------------------------------------------
    juce::Label  titleLabel, keyCap, scaleCap, lightCap, darkCap, bypassCap;
    juce::ComboBox keyBox, scaleBox;
    ht::PillSwitch lightSwitch { "light" }, bypassSwitch { "bypass" };

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

    // ---- carte du bas (797,500,585x235) : vumetres, cadran, transpose ------
    juce::Label  inCap, outCap, inPeak, outPeak, transposeCap, transposeVal;
    ht::LevelMeter meterIn, meterOut;
    ht::Gauge    gauge;
    ht::BarControl transposeBar;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAtt> retuneAtt, humanizeAtt, flexAtt, vibratoAtt, rateAtt,
                               transposeAtt;
    std::unique_ptr<ButtonAtt> lightAtt, bypassAtt;
    std::unique_ptr<ComboAtt>  keyAtt, scaleAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardTuneEditor)
};
