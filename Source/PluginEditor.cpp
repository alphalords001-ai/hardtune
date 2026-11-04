//  PluginEditor.cpp — interface paysage 1408x768 fidèle à la maquette.
#include "PluginEditor.h"

#include <array>
#include <cmath>

namespace
{
constexpr int W = 1408, H = 768;

juce::String fmtMs (float v)  { return juce::String (v, 0) + " ms"; }
juce::String fmtPct (float v) { return juce::String (v, 0) + "%"; }
juce::String fmtHz (float v)  { return juce::String (v, 1) + " Hz"; }
juce::String fmtSt (int v)    { return juce::String (v) + " st"; }
juce::String fmtDb (float v)  { return juce::String (v, 1) + " dB"; }
}

HardTuneEditor::HardTuneEditor (HardTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (W, H);

    auto& apvts = proc.apvts;

    // ---- en-tête ------------------------------------------------------------
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("ALPHA-TUNE", juce::dontSendNotification);
    titleLabel.setFont (ht::font (34.0f, true));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff0e527d));

    addAndMakeVisible (keyCap);   keyCap.setText ("KEY", juce::dontSendNotification);
    addAndMakeVisible (scaleCap); scaleCap.setText ("SCALE", juce::dontSendNotification);
    for (auto* l : { &keyCap, &scaleCap })
    {
        l->setFont (ht::font (19.0f));
        l->setColour (juce::Label::textColourId, juce::Colour (0xff0f1b23));
    }

    addAndMakeVisible (keyBox);
    keyBox.addItemList (apvts.getParameter (HardTuneProcessor::kKey)->getAllValueStrings(), 1);
    keyBox.setSelectedId (1, juce::dontSendNotification);
    keyAtt = std::make_unique<ComboAtt> (apvts, HardTuneProcessor::kKey, keyBox);

    addAndMakeVisible (scaleBox);
    scaleBox.addItemList (apvts.getParameter (HardTuneProcessor::kScale)->getAllValueStrings(), 1);
    scaleBox.setSelectedId (1, juce::dontSendNotification);
    scaleAtt = std::make_unique<ComboAtt> (apvts, HardTuneProcessor::kScale, scaleBox);

    addAndMakeVisible (lightCap);  lightCap.setText ("LIGHT", juce::dontSendNotification);
    addAndMakeVisible (bypassCap); bypassCap.setText ("BYPASS", juce::dontSendNotification);
    for (auto* l : { &lightCap, &bypassCap })
    {
        l->setFont (ht::font (19.0f));
        l->setColour (juce::Label::textColourId, juce::Colour (0xff0f1b23));
    }
    lightCap.setColour (juce::Label::textColourId, juce::Colour (0xff0e527d));

    addAndMakeVisible (lightSwitch);
    lightSwitch.setToggleState (true, juce::dontSendNotification); // LIGHT allumé = mode clair
    lightSwitch.onClick = [this]
    {
        const bool dark = proc.apvts.getRawParameterValue (HardTuneProcessor::kDarkMode)->load() > 0.5f;
        if (auto* pm = dynamic_cast<juce::AudioParameterBool*> (proc.apvts.getParameter (HardTuneProcessor::kDarkMode)))
            pm->setValueNotifyingHost (! dark); // LIGHT allumé = PAS de mode sombre
    };

    addAndMakeVisible (bypassSwitch);
    bypassAtt = std::make_unique<ButtonAtt> (apvts, HardTuneProcessor::kBypass, bypassSwitch);

    // bouton de recadrage : petit (0.75) / normal (1.0) / grand (1.25)
    addAndMakeVisible (sizeButton);
    sizeButton.setButtonText (juce::String::fromUTF8 ("\xe2\x9f\xb2")); // ⤢
    sizeButton.setTooltip ("Taille : petit / normal / grand");
    sizeButton.onClick = [this] { cycleSize(); };
    sizeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffa9ddf5));
    sizeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0e527d));

    // ---- clavier ------------------------------------------------------------
    addAndMakeVisible (keys);
    addAndMakeVisible (keysCap);
    keysCap.setText ("KEYBOARD \xc2\xb7 SCALE", juce::dontSendNotification);
    keysCap.setFont (ht::font (19.0f));
    keysCap.setColour (juce::Label::textColourId, juce::Colour (0xff0f1b23));

    // ---- waveform -----------------------------------------------------------
    addAndMakeVisible (waveCap);
    waveCap.setText ("WAVEFORM \xc2\xb7 PITCH", juce::dontSendNotification);
    waveCap.setFont (ht::font (19.0f));
    waveCap.setColour (juce::Label::textColourId, juce::Colour (0xff0f1b23));
    addAndMakeVisible (scope);

    // ---- RETUNE SPEED -------------------------------------------------------
    auto setupKnob = [&] (ht::Knob& k, const char* id)
    {
        addAndMakeVisible (k);
        k.setLookAndFeel (nullptr);
        return std::make_unique<SliderAtt> (apvts, id, k);
    };
    auto capVal = [&] (juce::Label& c, juce::Label& v, const char* t)
    {
        addAndMakeVisible (c); c.setText (t, juce::dontSendNotification);
        c.setFont (ht::font (15.0f));
        c.setColour (juce::Label::textColourId, juce::Colour (0xff33505e));
        addAndMakeVisible (v);
        v.setFont (ht::font (17.0f, true));
        v.setColour (juce::Label::textColourId, juce::Colour (0xff0e527d));
    };

    cap (retuneCap, "RETUNE SPEED", 817, 279, 22);
    retuneKnob.setName ("retune");
    retuneAtt = setupKnob (retuneKnob, HardTuneProcessor::kRetune);
    capVal (retuneVal, retuneVal, "");
    retuneVal.setText (fmtMs (rawValue (HardTuneProcessor::kRetune, 75.0f)), juce::dontSendNotification);

    capVal (humanizeCap, humanizeVal, "HUMANIZE");
    humanizeAtt = setupKnob (humanizeKnob, HardTuneProcessor::kHumanize);
    capVal (flexCap, flexVal, "FLEX-TUNE");
    flexAtt = setupKnob (flexKnob, HardTuneProcessor::kAmount); // FLEX-TUNE = dosage de la correction

    // ---- CORRECTION ---------------------------------------------------------
    cap (corrCap, "CORRECTION", 1162, 279, 22);
    capVal (vibratoCap, vibratoVal, "VIBRATO");
    vibratoAtt = setupKnob (vibratoKnob, HardTuneProcessor::kVibrato);
    capVal (rateCap, rateVal, "RATE");
    rateAtt = setupKnob (rateKnob, HardTuneProcessor::kVibRate);

    // ---- carte du bas -------------------------------------------------------
    cap (inCap, "INPUT", 834, 516);
    cap (inPeak, "", 910, 546);
    inPeak.setText ("PEAK " + fmtDb (-6.2f), juce::dontSendNotification);
    cap (outCap, "OUTPUT", 1300, 516);
    cap (outPeak, "", 1208, 556);
    outPeak.setText ("PEAK " + fmtDb (-5.8f), juce::dontSendNotification);
    addAndMakeVisible (meterIn);
    addAndMakeVisible (meterOut);
    addAndMakeVisible (gauge);

    cap (formantCap, "FORMANT (INACTIF)", 820, 686, 17);
    formantCap.setColour (juce::Label::textColourId, juce::Colour (0xff9aa7ae));
    addAndMakeVisible (formantBar);
    formantAtt = std::make_unique<SliderAtt> (apvts, HardTuneProcessor::kFormant, formantBar);
    formantBar.setEnabled2 (false);

    cap (transposeCap, "TRANSPOSE", 1180, 686);
    capVal (transposeVal, transposeVal, "");
    transposeVal.setText (fmtSt (0), juce::dontSendNotification);
    transposeVal.setFont (ht::font (17.0f, true));
    transposeVal.setColour (juce::Label::textColourId, juce::Colour (0xff0e527d));
    addAndMakeVisible (transposeBar);
    transposeAtt = std::make_unique<SliderAtt> (apvts, HardTuneProcessor::kTranspose, transposeBar);

    // taille fixe 1408x768 ; le bouton ⤢ applique une vraie échelle (transform)
    setResizable (false, false);
    setSize (W, H);

    applyTheme();
    startTimerHz (30);
}

HardTuneEditor::~HardTuneEditor() { stopTimer(); }

float HardTuneEditor::rawValue (const char* id, float fb) const
{
    if (auto* p = proc.apvts.getParameter (id))
        return p->getValue() * (p->getMaximum() - p->getMinimum()) + p->getMinimum();
    return fb;
}

juce::Label& HardTuneEditor::cap (juce::Label& l, const juce::String& t, int x, int y, int h,
                                  juce::Colour c)
{
    addAndMakeVisible (l);
    l.setText (t, juce::dontSendNotification);
    l.setFont (ht::font (h >= 19 ? 19.0f : 17.0f));
    l.setColour (juce::Label::textColourId, c);
    return l;
}

void HardTuneEditor::applyTheme()
{
    const bool dark = proc.apvts.getRawParameterValue (HardTuneProcessor::kDarkMode)->load() > 0.5f;
    pal = dark ? ht::darkPalette() : ht::lightPalette();
    setOpaque (true);
    for (auto* k : { &retuneKnob, &humanizeKnob, &flexKnob, &vibratoKnob, &rateKnob })
        k->setPalette (pal);
    for (auto* b : { &formantBar, &transposeBar })
        b->setPalette (pal);
    for (auto* s : { &lightSwitch, &bypassSwitch })
        s->setPalette (pal);
    scope.setPalette (pal);
    gauge.setPalette (pal);
    meterIn.setPalette (pal);
    meterOut.setPalette (pal);
    repaint();
}

void HardTuneEditor::paint (juce::Graphics& g)
{
    // fond aqua dégradé (mesuré sur la référence)
    juce::ColourGradient bg (juce::Colour (0xffc9e8f2), 0, 0,
                             juce::Colour (0xff84b0ce), 0, (float) H, false);
    g.setGradientFill (bg);
    g.fillAll();

    auto card = [&] (int x, int y, int w, int h)
    {
        juce::ColourGradient cg (pal.card1, 0, (float) y, pal.card2, 0, (float) (y + h), false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h), 9.0f);
        g.setColour (pal.cardEdge);
        g.drawRoundedRectangle (juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h), 9.0f, 1.0f);
    };

    card (24, 18, 1360, 60);     // en-tête
    card (24, 95, 1360, 150);    // clavier
    card (24, 265, 758, 470);    // waveform
    card (797, 265, 328, 217);   // retune
    card (1142, 265, 240, 217);  // correction
    card (797, 500, 585, 235);   // carte du bas

    // bandeau dégradé à gauche de l'en-tête
    juce::ColourGradient strip (juce::Colour (0xff4fc0e8), 0, 18,
                                 juce::Colour (0xff14618f), 0, 78, false);
    g.setGradientFill (strip);
    g.fillRoundedRectangle (juce::Rectangle<float> (24, 18, 6, 60), 3.0f);
    // séparateur vertical de l'en-tête
    g.setColour (pal.cardEdge);
    g.drawLine (300.0f, 26.0f, 300.0f, 70.0f, 1.0f);
}

void HardTuneEditor::resized()
{
    // coordonnées fixes 1408x768 ; cycleSize() applique l'échelle via setTransform
    juce::ignoreUnused (getWidth(), getHeight());
    // en-tête
    titleLabel.setBounds (44, 24, 220, 44);
    keyCap.setBounds (322, 32, 60, 26);
    keyBox.setBounds (378, 26, 52, 34);
    scaleCap.setBounds (470, 32, 70, 26);
    scaleBox.setBounds (552, 26, 126, 34);
    lightCap.setBounds (1058, 30, 70, 26);
    lightSwitch.setBounds (1136, 25, 50, 28);
    bypassCap.setBounds (1190, 30, 90, 26);
    bypassSwitch.setBounds (1248, 25, 50, 28);
    sizeButton.setBounds (1308, 25, 40, 28);

    // clavier
    keys.setBounds (36, 106, 1348, 106);
    keysCap.setBounds (40, 218, 300, 24);

    // waveform
    waveCap.setBounds (43, 272, 300, 26);
    scope.setBounds (40, 309, 731, 391 + 26);

    // retune
    retuneCap.setBounds (817, 272, 200, 24);
    retuneKnob.setBounds (887, 300, 120, 120);
    retuneVal.setBounds (872, 424, 150, 30);
    humanizeCap.setBounds (810, 452, 100, 20);
    humanizeKnob.setBounds (812, 300, 60, 60);
    humanizeVal.setBounds (876, 330, 70, 22);
    flexCap.setBounds (1010, 452, 100, 20);
    flexKnob.setBounds (1016, 300, 60, 60);
    flexVal.setBounds (1080, 330, 70, 22);

    // correction
    corrCap.setBounds (1162, 272, 200, 24);
    vibratoCap.setBounds (1152, 452, 90, 20);
    vibratoKnob.setBounds (1156, 300, 60, 60);
    vibratoVal.setBounds (1220, 330, 70, 22);
    rateCap.setBounds (1272, 452, 60, 20);
    rateKnob.setBounds (1272, 300, 60, 60);
    rateVal.setBounds (1336, 330, 60, 22);

    // carte du bas
    inCap.setBounds (834, 508, 80, 22);
    meterIn.setBounds (834, 536, 70, 112);
    inPeak.setBounds (910, 540, 120, 40);
    outCap.setBounds (1290, 508, 90, 22);
    meterOut.setBounds (1300, 536, 70, 112);
    outPeak.setBounds (1200, 550, 120, 40);
    gauge.setBounds (993, 510, 190, 190);
    formantCap.setBounds (820, 680, 200, 22);
    formantBar.setBounds (820, 706, 176, 22);
    transposeCap.setBounds (1180, 680, 130, 22);
    transposeBar.setBounds (1180, 706, 150, 22);
    transposeVal.setBounds (1338, 680, 50, 22);
}

void HardTuneEditor::timerCallback()
{
    try
    {
        const bool dark = proc.apvts.getRawParameterValue (HardTuneProcessor::kDarkMode)->load() > 0.5f;
        if (dark != lastDarkState) { lastDarkState = dark; applyTheme(); }
        lightSwitch.setToggleState (! dark, juce::dontSendNotification);

        // valeurs affichées
        retuneVal.setText (fmtMs (rawValue (HardTuneProcessor::kRetune, 0.0f)), juce::dontSendNotification);
        humanizeVal.setText (fmtPct (rawValue (HardTuneProcessor::kHumanize, 32.0f)), juce::dontSendNotification);
        flexVal.setText (fmtPct (rawValue (HardTuneProcessor::kAmount, 100.0f)), juce::dontSendNotification);
        vibratoVal.setText (fmtPct (rawValue (HardTuneProcessor::kVibrato, 20.0f)), juce::dontSendNotification);
        rateVal.setText (fmtHz (rawValue (HardTuneProcessor::kVibRate, 6.0f)), juce::dontSendNotification);
        transposeVal.setText (fmtSt ((int) rawValue (HardTuneProcessor::kTranspose, 0.0f)), juce::dontSendNotification);

        // clavier : gamme réelle via le NoteMapper (source unique), noms #/b selon la gamme
        const int keyIdx = (int) rawValue (HardTuneProcessor::kKey, 0.0f);
        int scaleIdx = (int) rawValue (HardTuneProcessor::kScale, 1.0f);
        if (scaleIdx >= (int) tune::NoteMapper::NumScales) scaleIdx = (int) tune::NoteMapper::Major;
        const auto sc = (tune::NoteMapper::Scale) scaleIdx;
        const unsigned mask = tune::NoteMapper::scaleMask (sc);
        const bool minor = sc == tune::NoteMapper::MinorNatural
                        || sc == tune::NoteMapper::MinorHarmonic
                        || sc == tune::NoteMapper::PentatonicMinor
                        || sc == tune::NoteMapper::Blues
                        || sc == tune::NoteMapper::Dorian;
        keys.setScale (keyIdx, mask, minor);
        const int tgt = proc.getTargetNote();
        keys.setPlaying (tgt, tgt >= 0);

        // waveform : fenêtre de 5 s
        std::array<float, HardTuneProcessor::kScopeSize> buf {};
        proc.copyScope (buf.data());
        scope.pushBlock (buf.data() + (HardTuneProcessor::kScopeSize - 512), 512,
                         (int) proc.getSampleRate());

        // cadran + note (nom selon la gamme)
        std::array<float, 256> cts {};
        proc.copyCents (cts.data());
        const float cents = cts[(size_t) (proc.kCentsCount - 1)];
        gauge.setData (cents, tgt, minor);

        // vu-mètres + PEAK
        meterIn.setLevels (proc.getInputPeakDb(), proc.getInputRmsDb());
        meterOut.setLevels (proc.getOutputPeakDb(), proc.getOutputRmsDb());
        inPeak.setText ("PEAK " + fmtDb (proc.getInputPeakDb()), juce::dontSendNotification);
        outPeak.setText ("PEAK " + fmtDb (proc.getOutputPeakDb()), juce::dontSendNotification);
    }
    catch (...) { /* un widget ne doit jamais faire tomber l'hôte */ }
}

void HardTuneEditor::cycleSize()
{
    // cycle : petit (0.75) -> normal (1.0) -> grand (1.5) -> petit ...
    static const double factors[3] = { 0.75, 1.0, 1.5 };
    static int idx = 1; // normal au départ
    idx = (idx + 1) % 3;
    const double f = factors[idx];
    setSize (int (W * f), int (H * f));
    setTransform (juce::AffineTransform::scale ((float) f)); // vraie échelle, layout intact
}
