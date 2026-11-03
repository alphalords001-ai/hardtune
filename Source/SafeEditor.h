//  SafeEditor.h — interface minimale de secours.
//
//  Objectif double :
//    1. donner tout de suite un plugin utilisable (aucun dessin maison, aucun
//       timer de rendu, uniquement des composants JUCE standard) ;
//    2. servir de test de bissection : si le plugin tient avec CETTE interface
//       mais tombe avec l'interface complète, le défaut est dans notre dessin
//       (Widgets.h / PluginEditor.cpp) et pas dans le moteur audio.
//
//  L'interface complète se réactive en créant le fichier
//  %TEMP%\hardtune-ui-complete.txt  (et en le supprimant pour revenir ici).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "Diag.h"

class HardTuneSafeEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit HardTuneSafeEditor (HardTuneProcessor& p)
        : juce::AudioProcessorEditor (&p), proc (p)
    {
        ht::diag::markUiThread();
        setAccessible (false);

        title.setText ("HardTune - interface minimale", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions (16.0f)));
        title.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (title);

        hint.setText ("interface complete desactivee : creer %TEMP%\\hardtune-ui-complete.txt puis recharger le plugin",
                      juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        hint.setFont (juce::Font (juce::FontOptions (11.0f)));
        addAndMakeVisible (hint);

        keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
        addAndMakeVisible (keyBox);
        keyAtt = std::make_unique<ComboAtt> (proc.apvts, HardTuneProcessor::kKey, keyBox);

        juce::StringArray scales;

        for (int i = 0; i < (int) tune::NoteMapper::NumScales; ++i)
            scales.add (tune::NoteMapper::scaleName ((tune::NoteMapper::Scale) i));

        scales.add ("Custom");
        scaleBox.addItemList (scales, 1);
        addAndMakeVisible (scaleBox);
        scaleAtt = std::make_unique<ComboAtt> (proc.apvts, HardTuneProcessor::kScale, scaleBox);

        retune.setSliderStyle (juce::Slider::LinearHorizontal);
        retune.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
        addAndMakeVisible (retune);
        retuneAtt = std::make_unique<SliderAtt> (proc.apvts, HardTuneProcessor::kRetune, retune);

        bypass.setButtonText ("Bypass");
        addAndMakeVisible (bypass);
        bypassAtt = std::make_unique<ButtonAtt> (proc.apvts, HardTuneProcessor::kBypass, bypass);

        note.setText ("--", juce::dontSendNotification);
        note.setFont (juce::Font (juce::FontOptions (22.0f)));
        note.setColour (juce::Label::textColourId, juce::Colours::white);
        note.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (note);

        setSize (520, 240);
        startTimerHz (10);

        ht::diag::log ("editeur minimal : construction terminee");
    }

    ~HardTuneSafeEditor() override
    {
        ht::diag::log ("editeur minimal : destruction");
    }

    void paint (juce::Graphics& g) override
    {
        ht::diag::stage ("paint:minimal");

        g.fillAll (juce::Colour (0xff1a2733));
    }

    void resized() override
    {
        ht::diag::stage ("resized:minimal");

        auto r = getLocalBounds().reduced (12);
        title.setBounds (r.removeFromTop (24));
        hint.setBounds (r.removeFromTop (16));
        r.removeFromTop (10);

        auto row = r.removeFromTop (28);
        row.removeFromLeft (60);
        keyBox.setBounds (row.removeFromLeft (80));
        row.removeFromLeft (12);
        scaleBox.setBounds (row.removeFromLeft (160));
        row.removeFromLeft (12);
        bypass.setBounds (row.removeFromLeft (80));

        r.removeFromTop (10);
        retune.setBounds (r.removeFromTop (28));

        r.removeFromTop (8);
        note.setBounds (r);
    }

private:
    void timerCallback() override
    {
        ht::diag::stage ("timer:minimal");

        const int target = proc.getTargetNote();

        if (target < 0)
        {
            note.setText ("--", juce::dontSendNotification);
            return;
        }

        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };
        const int pc  = ((target % 12) + 12) % 12;
        const int oct = target / 12 - 2;

        note.setText (juce::String (names[pc]) + juce::String (oct)
                          + "   (" + juce::String (proc.getCentsError(), 1) + " cents)",
                      juce::dontSendNotification);
    }

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    HardTuneProcessor& proc;

    juce::Label    title, hint, note;
    juce::ComboBox keyBox, scaleBox;
    juce::Slider   retune;
    juce::ToggleButton bypass;

    std::unique_ptr<ComboAtt>  keyAtt, scaleAtt;
    std::unique_ptr<SliderAtt> retuneAtt;
    std::unique_ptr<ButtonAtt> bypassAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardTuneSafeEditor)
};
