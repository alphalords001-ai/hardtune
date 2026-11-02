//  PluginEditor.cpp
#include "PluginEditor.h"

using namespace juce;

namespace
{
    struct FactoryPreset
    {
        const char* name;
        float retuneMs, amount, formant, mix, output;
        int   transpose;
        int   scale;
    };

    // Presets d'usine (le premier est le son signature)
    const FactoryPreset factoryPresets[] =
    {
        { "T-PAIN",  0.0f,   100.0f,  15.0f, 100.0f, 0.0f, 0, (int) tune::NoteMapper::Chromatic },
        { "TRAP",    0.0f,   100.0f,  30.0f, 100.0f, 0.0f, 0, (int) tune::NoteMapper::MinorNatural },
        { "RAP",     12.0f,  100.0f,  10.0f, 100.0f, 0.0f, 0, (int) tune::NoteMapper::MinorNatural },
        { "NATUREL", 350.0f,  75.0f,   0.0f, 100.0f, 0.0f, 0, (int) tune::NoteMapper::Major },
        { "CHOEUR",  220.0f,  60.0f,  -5.0f, 100.0f, 0.0f, 0, (int) tune::NoteMapper::Major },
        { "INIT",    20.0f,  100.0f,   0.0f, 100.0f, 0.0f, 0, (int) tune::NoteMapper::Major }
    };

    constexpr int kNumPresets = (int) (sizeof (factoryPresets) / sizeof (factoryPresets[0]));
}

// ---------------------------------------------------------------------------
HardTuneEditor::HardTuneEditor (HardTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    // ---- presets ------------------------------------------------------------
    auto presetNames = StringArray();
    for (int i = 0; i < kNumPresets; ++i)
        presetNames.add (factoryPresets[i].name);
    for (int i = 0; i < kNumPresets; ++i)
        presetBox.addItem (presetNames[i], i + 1);
    presetBox.setTextWhenNothingSelected ("PRESETS");
    presetBox.onChange = [this] { applyPreset (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    // ---- bouton T-PAIN ------------------------------------------------------
    tpainButton.setColour (TextButton::buttonColourId, ht::Colours::accentWarm);
    tpainButton.setColour (TextButton::textColourOffId, Colours::white);
    tpainButton.onClick = [this] { applyPreset (0); presetBox.setSelectedId (1, dontSendNotification); };
    addAndMakeVisible (tpainButton);

    // ---- bypass -------------------------------------------------------------
    bypassButton.setClickingTogglesState (true);
    bypassAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, HardTuneProcessor::kBypass, bypassButton);
    addAndMakeVisible (bypassButton);

    // ---- tonalité / gammes --------------------------------------------------
    buildKeyButtons();
    buildCustomToggles();

    scaleAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, HardTuneProcessor::kScale, scaleBox);
    addAndMakeVisible (scaleBox);
    addAndMakeVisible (customOnButton);
    customOnAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, HardTuneProcessor::kCustomOn, customOnButton);

    // ---- boutons rotatifs ---------------------------------------------------
    setupKnob (kRetune,    HardTuneProcessor::kRetune,    "RETUNE",    " ms");
    setupKnob (kAmount,    HardTuneProcessor::kAmount,    "AMOUNT",    " %");
    setupKnob (kFormant,   HardTuneProcessor::kFormant,   "FORMANT",   " %");
    setupKnob (kMix,       HardTuneProcessor::kMix,       "MIX",       " %");
    setupKnob (kTranspose, HardTuneProcessor::kTranspose, "TRANSPOSE", " st");
    setupKnob (kOutput,    HardTuneProcessor::kOutput,    "OUTPUT",    " dB");

    addAndMakeVisible (display);

    setResizable (true, true);
    setResizeLimits (760, 480, 1400, 900);
    getConstrainer()->setFixedAspectRatio (880.0 / 540.0);
    setSize (880, 540);

    startTimerHz (30);
}

HardTuneEditor::~HardTuneEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------------------
void HardTuneEditor::setupKnob (Knob& k, const String& paramId, const String& text,
                                const String& suffix)
{
    k.slider = std::make_unique<Slider>();
    k.slider->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider->setTextBoxStyle (Slider::TextBoxBelow, false, 68, 15);
    k.slider->setColour (Slider::textBoxTextColourId, ht::Colours::text);
    k.slider->setTextValueSuffix (suffix);
    k.slider->setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (*k.slider);

    k.title = std::make_unique<Label>();
    k.title->setText (text, dontSendNotification);
    k.title->setFont (ht::uiFont (11.0f, true));
    k.title->setColour (Label::textColourId, ht::Colours::textDim);
    k.title->setJustificationType (Justification::centred);
    addAndMakeVisible (*k.title);

    k.attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, paramId, *k.slider);
}

// ---------------------------------------------------------------------------
void HardTuneEditor::buildKeyButtons()
{
    for (int i = 0; i < 12; ++i)
    {
        auto b = std::make_unique<TextButton> (tune::NoteMapper::noteName (i));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (0x1701);
        b->onClick = [this, i]
        {
            proc.setParamValue (HardTuneProcessor::kKey, (float) i);
            lastKeyIndex = i;
        };
        addAndMakeVisible (*b);
        keyButtons[(size_t) i] = std::move (b);
    }
}

void HardTuneEditor::buildCustomToggles()
{
    for (int i = 0; i < 12; ++i)
    {
        auto t = std::make_unique<ToggleButton> (tune::NoteMapper::noteName (i));
        const String id = String (HardTuneProcessor::kNotePrefix) + String (i);
        auto att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (
            proc.apvts, id, *t);
        addAndMakeVisible (*t);
        customToggles[(size_t) i] = std::move (t);
        customAttaches[(size_t) i] = std::move (att);
    }
}

// ---------------------------------------------------------------------------
void HardTuneEditor::applyPreset (int index)
{
    if (index < 0 || index >= kNumPresets)
        return;

    const auto& p = factoryPresets[index];

    proc.setParamValue (HardTuneProcessor::kRetune,    p.retuneMs);
    proc.setParamValue (HardTuneProcessor::kAmount,    p.amount);
    proc.setParamValue (HardTuneProcessor::kFormant,   p.formant);
    proc.setParamValue (HardTuneProcessor::kMix,       p.mix);
    proc.setParamValue (HardTuneProcessor::kOutput,    p.output);
    proc.setParamValue (HardTuneProcessor::kTranspose, (float) p.transpose);
    proc.setParamValue (HardTuneProcessor::kScale,     (float) p.scale);
    proc.setParamValue (HardTuneProcessor::kCustomOn,  0.0f);

    scaleBox.setSelectedId (p.scale + 1, dontSendNotification);
    customOnButton.setToggleState (false, dontSendNotification);
}

void HardTuneEditor::refreshKeyButtonsFromParameter()
{
    const int idx = (int) std::lround ((double) proc.apvts.getRawParameterValue (HardTuneProcessor::kKey)->load());
    if (idx == lastKeyIndex)
        return;

    lastKeyIndex = idx;
    for (int i = 0; i < 12; ++i)
        if (auto* b = keyButtons[(size_t) i].get())
            b->setToggleState (i == idx, dontSendNotification);
}

// ---------------------------------------------------------------------------
void HardTuneEditor::timerCallback()
{
    refreshKeyButtonsFromParameter();

    display.setState (proc.getDetectedHz(), proc.getDetectedMidi(),
                      proc.getTargetNote(), proc.getCentsError(), proc.isVoiced());
}

// ---------------------------------------------------------------------------
void HardTuneEditor::paint (Graphics& g)
{
    g.fillAll (ht::Colours::background);

    auto area = getLocalBounds().toFloat();

    // bandeau supérieur
    auto header = area.removeFromTop (58.0f);
    {
        ColourGradient grad (ht::Colours::panelLight, header.getTopLeft(),
                             ht::Colours::background, header.getBottomLeft(), false);
        g.setGradientFill (grad);
        g.fillRect (header);
        g.setColour (ht::Colours::outline);
        g.fillRect (header.removeFromBottom (1.0f));
    }

    // titre
    g.setColour (ht::Colours::accent);
    g.setFont (ht::uiFont (24.0f, true));
    g.drawText ("HARDTUNE", 18, 12, 240, 34, Justification::centredLeft, false);

    g.setColour (ht::Colours::textDim);
    g.setFont (ht::uiFont (10.0f));
    g.drawText ("real-time pitch correction", 19, 38, 240, 14, Justification::centredLeft, false);

    // panneaux
    auto body = getLocalBounds().reduced (14);
    body.removeFromTop (58 - 14);

    const int leftW  = (int) (getWidth() * 0.215f);
    const int rightW = (int) (getWidth() * 0.265f);

    auto leftPanel  = body.removeFromLeft (leftW).toFloat();
    auto rightPanel = body.removeFromRight (rightW).toFloat();

    for (auto* panel : { &leftPanel, &rightPanel })
    {
        g.setColour (ht::Colours::panel);
        g.fillRoundedRectangle (panel->reduced (4.0f), 8.0f);
        g.setColour (ht::Colours::outline);
        g.drawRoundedRectangle (panel->reduced (4.0f), 8.0f, 1.0f);
    }

    // pied de page
    g.setColour (ht::Colours::textDim);
    g.setFont (ht::uiFont (10.0f));
    g.drawText ("latence " + String (proc.getLatencyMs()) + " ms   ·   open source (GPLv3)",
                getLocalBounds().removeFromBottom (18).reduced (18, 0),
                Justification::centredLeft, false);

    // libellés de section
    g.setColour (ht::Colours::textDim);
    g.setFont (ht::uiFont (11.0f, true));
    auto lp = leftPanel.toNearestInt().reduced (14);
    g.drawText ("KEY", lp.removeFromTop (18), Justification::centredLeft, false);
}

// ---------------------------------------------------------------------------
void HardTuneEditor::resized()
{
    auto area = getLocalBounds();

    // ---- en-tête -----------------------------------------------------------
    auto header = area.removeFromTop (58).reduced (10, 12);
    header.removeFromLeft (250);            // place du logo (dessiné dans paint)

    bypassButton.setBounds (header.removeFromRight (84).reduced (2, 0));
    header.removeFromRight (8);
    tpainButton.setBounds (header.removeFromRight (84).reduced (2, 0));
    header.removeFromRight (12);
    presetBox.setBounds (header.removeFromRight (jmin (220, header.getWidth() - 10)).reduced (0, 0));

    area.reduce (14, 0);
    area.removeFromBottom (20);             // pied de page

    // ---- colonne gauche : tonalité + gamme ---------------------------------
    auto left = area.removeFromLeft ((int) (getWidth() * 0.215f)).reduced (10, 8);
    left.removeFromTop (20);                // libellé "KEY" (dessiné dans paint)

    const int bw = (left.getWidth() - 2 * 4) / 3;
    const int bh = 26;

    for (int row = 0; row < 4; ++row)
    {
        auto r = left.removeFromTop (bh + 4);
        for (int col = 0; col < 3; ++col)
        {
            const int idx = row * 3 + col;
            keyButtons[(size_t) idx]->setBounds (r.removeFromLeft (bw));
            r.removeFromLeft (4);
        }
    }

    left.removeFromTop (10);
    scaleBox.setBounds (left.removeFromTop (26));
    left.removeFromTop (8);
    customOnButton.setBounds (left.removeFromTop (24));
    left.removeFromTop (4);

    {
        const int tw = (left.getWidth() - 2 * 4) / 3;
        for (int row = 0; row < 4; ++row)
        {
            auto r = left.removeFromTop (22 + 3);
            for (int col = 0; col < 3; ++col)
            {
                const int idx = row * 3 + col;
                customToggles[(size_t) idx]->setBounds (r.removeFromLeft (tw));
                r.removeFromLeft (4);
            }
        }
    }

    // ---- colonne droite : écran --------------------------------------------
    auto right = area.removeFromRight ((int) (getWidth() * 0.265f)).reduced (10, 8);
    display.setBounds (right.removeFromTop (jmin (right.getHeight(), 300)));

    // ---- centre : boutons rotatifs -----------------------------------------
    auto centre = area.reduced (6, 8);

    auto bigRow = centre.removeFromTop ((int) (centre.getHeight() * 0.56f));
    auto smallRow = centre;

    const int colW = bigRow.getWidth() / 4;
    auto big1 = bigRow.removeFromLeft (colW);
    auto big2 = bigRow.removeFromLeft (colW);
    auto big3 = bigRow.removeFromLeft (colW);
    auto big4 = bigRow;

    auto placeKnob = [] (Knob& k, Rectangle<int> r)
    {
        k.title->setBounds (r.removeFromTop (16));
        k.slider->setBounds (r.reduced (4));
    };

    placeKnob (kRetune,  big1);
    placeKnob (kAmount,  big2);
    placeKnob (kFormant, big3);
    placeKnob (kMix,     big4);

    const int colW2 = smallRow.getWidth() / 4;
    auto s1 = smallRow.removeFromLeft (colW2);
    auto s2 = smallRow.removeFromLeft (colW2);
    s1 = s1.withTrimmedTop (10);
    s2 = s2.withTrimmedTop (10);

    placeKnob (kTranspose, s1.reduced (20, 0).withHeight (jmin (s1.getHeight(), 130)));
    placeKnob (kOutput,    s2.reduced (20, 0).withHeight (jmin (s2.getHeight(), 130)));
}
