//  PluginEditor.cpp — ALPHA-TUNE, dessin 1408x768 (SPEC-INTERFACE-v0.6).
#include "PluginEditor.h"

#include <array>
#include <cmath>

namespace
{
constexpr int W = 1408, H = 768;

const juce::Colour kInk      (0xff0f1b23);
const juce::Colour kInkSoft  (0xff33505e);
const juce::Colour kBlue     (0xff0e527d);   // LIGHT, titre
const juce::Colour kValue    (0xff0e4d72);   // valeurs (18 px gras)

juce::String fmtMs (float v)  { return juce::String (v, 0) + " ms"; }
juce::String fmtPct (float v) { return juce::String (v, 0) + "%"; }
juce::String fmtHz (float v)  { return juce::String (v, 1) + " Hz"; }
juce::String fmtSt (int v)    { return juce::String (v) + " st"; }
juce::String fmtDb (float v)  { return juce::String (v, 1) + " dB"; }

/** (x,y) = haut-gauche de la boite de 140 px centree sur x (regle 10). */
juce::Rectangle<int> box140 (int cx, int y, int h = 22)
{
    return { cx - 70, y, 140, h };
}
}

HardTuneEditor::HardTuneEditor (HardTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    auto& apvts = proc.apvts;

    //  le canevas porte le dessin et la mise a l'echelle
    canvas.onPaint   = [this] (juce::Graphics& g) { paintCanvas (g); };
    canvas.onResized = [this] { layoutCanvas(); };
    addAndMakeVisible (canvas);

    // ---- en-tete -----------------------------------------------------------
    label (titleLabel, "ALPHA-TUNE", 34.0f, kBlue, true);

    label (keyCap,   "KEY",   19.0f, kInk);
    label (scaleCap, "SCALE", 19.0f, kInk);

    canvas.addAndMakeVisible (keyBox);
    keyBox.setLookAndFeel (&pillLnf);
    keyBox.addItemList (apvts.getParameter (HardTuneProcessor::kKey)->getAllValueStrings(), 1);
    keyBox.setSelectedId (1, juce::dontSendNotification);
    keyAtt = std::make_unique<ComboAtt> (apvts, HardTuneProcessor::kKey, keyBox);

    canvas.addAndMakeVisible (scaleBox);
    scaleBox.setLookAndFeel (&pillLnf);
    scaleBox.addItemList (apvts.getParameter (HardTuneProcessor::kScale)->getAllValueStrings(), 1);
    scaleBox.setSelectedId (1, juce::dontSendNotification);
    scaleAtt = std::make_unique<ComboAtt> (apvts, HardTuneProcessor::kScale, scaleBox);

    label (lightCap,  "LIGHT",  19.0f, kBlue);
    label (bypassCap, "BYPASS", 19.0f, kInk);

    canvas.addAndMakeVisible (lightSwitch);
    lightSwitch.setToggleState (true, juce::dontSendNotification);   // LIGHT allume = clair
    lightSwitch.onClick = [this]
    {
        const bool dark = proc.apvts.getRawParameterValue (HardTuneProcessor::kDarkMode)->load() > 0.5f;
        if (auto* pm = dynamic_cast<juce::AudioParameterBool*> (proc.apvts.getParameter (HardTuneProcessor::kDarkMode)))
            pm->setValueNotifyingHost (! dark);
    };

    canvas.addAndMakeVisible (bypassSwitch);
    bypassAtt = std::make_unique<ButtonAtt> (apvts, HardTuneProcessor::kBypass, bypassSwitch);

    canvas.addAndMakeVisible (sizeButton);
    sizeButton.setButtonText (juce::String::fromUTF8 ("\xe2\x9f\xb2"));   // ⤢
    sizeButton.setTooltip ("Size : 0.6 / 1.0 / 1.5");
    sizeButton.onClick = [this] { cycleSize(); };
    sizeButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xffa9ddf5));
    sizeButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffc4e9fa));
    sizeButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff0e527d));
    sizeButton.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff0e527d));

    // ---- clavier -----------------------------------------------------------
    canvas.addAndMakeVisible (keys);
    label (keysCap, "KEYBOARD " + juce::String::fromUTF8 ("\xc2\xb7") + " SCALE", 19.0f, kInk);

    // ---- waveform ----------------------------------------------------------
    label (waveCap, "WAVEFORM " + juce::String::fromUTF8 ("\xc2\xb7") + " PITCH", 22.0f, kInk);
    canvas.addAndMakeVisible (scope);

    // ---- RETUNE SPEED ------------------------------------------------------
    auto setupKnob = [&] (ht::Knob& k, const char* id)
    {
        canvas.addAndMakeVisible (k);
        return std::make_unique<SliderAtt> (apvts, id, k);
    };

    label (retuneCap, "RETUNE SPEED", 22.0f, kInk);
    retuneKnob.setName ("retune");
    retuneAtt = setupKnob (retuneKnob, HardTuneProcessor::kRetune);
    //  valeur unique, dans sa boite (1010,272,96x30) — regle (a)
    label (retuneVal, fmtMs (0.0f), 18.0f, kValue, true);
    label (retuneSub, "SPEED", 18.0f, kInkSoft);

    label (humanizeCap, "HUMANIZE", 18.0f, kInkSoft);
    label (humanizeVal, "0%", 16.0f, kValue);
    humanizeAtt = setupKnob (humanizeKnob, HardTuneProcessor::kHumanize);

    label (flexCap, "FLEX-TUNE", 18.0f, kInkSoft);
    label (flexVal, "100%", 16.0f, kValue);
    flexAtt = setupKnob (flexKnob, HardTuneProcessor::kAmount);

    // ---- CORRECTION --------------------------------------------------------
    label (corrCap, "CORRECTION", 22.0f, kInk);
    label (vibratoCap, "VIBRATO", 18.0f, kInkSoft);
    label (vibratoVal, "0%", 16.0f, kValue);
    vibratoAtt = setupKnob (vibratoKnob, HardTuneProcessor::kVibrato);

    label (rateCap, "RATE", 18.0f, kInkSoft);
    label (rateVal, "6.0 Hz", 16.0f, kValue);
    rateAtt = setupKnob (rateKnob, HardTuneProcessor::kVibRate);

    // ---- carte du bas ------------------------------------------------------
    label (inCap,  "INPUT",  16.0f, kInkSoft);
    label (inCap2, "INPUT",  16.0f, kInkSoft);
    label (inPeak, "PEAK", 13.0f, kInkSoft);
    label (inPeakVal, juce::String::fromUTF8 ("\xe2\x97\x84 ") + fmtDb (-6.2f), 13.0f, kInkSoft);

    label (outCap,  "OUTPUT", 16.0f, kInkSoft);
    label (outCap2, "OUTPUT", 16.0f, kInkSoft);
    label (outPeak, "PEAK", 13.0f, kInkSoft);
    label (outPeakVal, fmtDb (-5.8f) + " " + juce::String::fromUTF8 ("\xe2\x96\xba"), 13.0f, kInkSoft);

    canvas.addAndMakeVisible (meterIn);
    canvas.addAndMakeVisible (meterOut);
    canvas.addAndMakeVisible (gauge);

    label (formantCap, "FORMANT (INACTIF)", 16.0f, juce::Colour (0xff9aa7ae));
    canvas.addAndMakeVisible (formantBar);
    formantAtt = std::make_unique<SliderAtt> (apvts, HardTuneProcessor::kFormant, formantBar);
    formantBar.setEnabled2 (false);   // grise, non cliquable (§6)

    label (transposeCap, "TRANSPOSE", 16.0f, kInkSoft);
    label (transposeVal, "0 st", 16.0f, kValue, true);
    canvas.addAndMakeVisible (transposeBar);
    transposeAtt = std::make_unique<SliderAtt> (apvts, HardTuneProcessor::kTranspose, transposeBar);

    setResizable (false, false);
    lastScopeTotal = proc.getScopeTotal();
    applyScale (0.6);          // taille par defaut : x0.6 -> 845x461
    applyTheme();
    startTimerHz (30);
}

HardTuneEditor::~HardTuneEditor()
{
    stopTimer();
    keyBox.setLookAndFeel (nullptr);
    scaleBox.setLookAndFeel (nullptr);
}

float HardTuneEditor::rawValue (const char* id, float fb) const
{
    if (auto* p = proc.apvts.getRawParameterValue (id))
        return p->load();

    return fb;
}

juce::Label& HardTuneEditor::label (juce::Label& l, const juce::String& t, float size,
                                    juce::Colour c, bool bold, float dx)
{
    juce::ignoreUnused (dx);
    canvas.addAndMakeVisible (l);
    l.setText (t, juce::dontSendNotification);
    l.setFont (ht::font (size, bold));
    l.setColour (juce::Label::textColourId, c);
    l.setJustificationType (juce::Justification::centred);
    l.setInterceptsMouseClicks (false, false);
    return l;
}

void HardTuneEditor::applyScale (double factor)
{
    currentScale = factor;
    setSize ((int) (W * factor), (int) (H * factor));   // resized() place le canevas
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

    keys.setPalette (pal);
    scope.setPalette (pal);
    gauge.setPalette (pal);
    meterIn.setPalette (pal);
    meterOut.setPalette (pal);

    const juce::Colour ink  = dark ? pal.ink : kInk;
    const juce::Colour soft = dark ? pal.inkSoft : kInkSoft;
    const juce::Colour blue = dark ? pal.accentLight : kBlue;
    const juce::Colour val  = dark ? pal.accentLight : kValue;

    titleLabel.setColour (juce::Label::textColourId, blue);
    for (auto* l : { &keyCap, &scaleCap, &bypassCap, &retuneCap, &corrCap, &waveCap, &keysCap })
        l->setColour (juce::Label::textColourId, ink);
    lightCap.setColour (juce::Label::textColourId, blue);

    for (auto* l : { &retuneSub, &humanizeCap, &flexCap, &vibratoCap, &rateCap,
                     &inCap, &inCap2, &outCap, &outCap2, &transposeCap })
        l->setColour (juce::Label::textColourId, soft);
    for (auto* l : { &inPeak, &outPeak, &inPeakVal, &outPeakVal })
        l->setColour (juce::Label::textColourId, soft);

    for (auto* l : { &retuneVal, &humanizeVal, &flexVal, &vibratoVal, &rateVal, &transposeVal })
        l->setColour (juce::Label::textColourId, val);

    formantCap.setColour (juce::Label::textColourId, pal.inkFaint);
    formantBar.setAlpha (0.75f);

    //  listes deroulantes + bouton de taille : eux aussi bicolores
    pillLnf.setDark (dark);
    keyBox.setLookAndFeel (&pillLnf);
    scaleBox.setLookAndFeel (&pillLnf);
    sizeButton.setColour (juce::TextButton::buttonColourId,
                          dark ? juce::Colour (0xff1f3341) : juce::Colour (0xffa9ddf5));
    sizeButton.setColour (juce::TextButton::buttonOnColourId,
                          dark ? juce::Colour (0xff2a4456) : juce::Colour (0xffc4e9fa));
    sizeButton.setColour (juce::TextButton::textColourOffId,
                          dark ? juce::Colour (0xffbfe6f8) : juce::Colour (0xff0e527d));
    sizeButton.setColour (juce::TextButton::textColourOnId,
                          dark ? juce::Colour (0xffbfe6f8) : juce::Colour (0xff0e527d));

    repaint();
}

void HardTuneEditor::paint (juce::Graphics& g)
{
    //  Repli : si le canevas n'est pas encore place, on evite un trou noir.
    g.fillAll (pal.win2);
}

void HardTuneEditor::paintCanvas (juce::Graphics& g)
{
    // fond page : #c9e8f2 -> #a3d3e4 -> #84b0ce
    const juce::Colour top    = pal.win1;
    const juce::Colour bottom = pal.win2;
    juce::ColourGradient bg (top, 0.0f, 0.0f, bottom, 0.0f, (float) H, false);
    bg.addColour (0.5, top.interpolatedWith (bottom, 0.5f));
    g.setGradientFill (bg);
    g.fillAll();

    auto card = [&] (int x, int y, int w, int h)
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        juce::ColourGradient cg (pal.card1, 0.0f, (float) y, pal.card2, 0.0f, (float) (y + h), false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (pal.cardEdge);
        g.drawRoundedRectangle (r, 9.0f, 1.0f);
    };

    card (24, 18, 1360, 60);     // en-tete
    card (24, 95, 1360, 150);    // clavier
    card (24, 265, 758, 470);    // waveform
    card (797, 265, 328, 217);   // retune speed
    card (1142, 265, 240, 217);  // correction
    card (797, 500, 585, 235);   // carte du bas

    // bandeau degrade de l'en-tete
    juce::ColourGradient strip (juce::Colour (0xff4fc0e8), 0.0f, 18.0f,
                                 juce::Colour (0xff14618f), 0.0f, 78.0f, false);
    g.setGradientFill (strip);
    g.fillRoundedRectangle (juce::Rectangle<float> (24.0f, 18.0f, 6.0f, 60.0f), 3.0f);

    // separateur vertical de l'en-tete
    g.setColour (pal.cardEdge);
    g.drawLine (300.0f, 26.0f, 300.0f, 70.0f, 1.0f);

    // boite de la valeur RETUNE (une seule fois, en haut a droite)
    auto rb = juce::Rectangle<float> (1010.0f, 272.0f, 96.0f, 30.0f);
    juce::ColourGradient rbg (juce::Colour (0xffbfe6f8), 0.0f, rb.getY(),
                              juce::Colour (0xffa5d9f4), 0.0f, rb.getBottom(), false);
    g.setGradientFill (rbg);
    g.fillRoundedRectangle (rb, 7.0f);
    g.setColour (juce::Colour (0xff9dc9e2));
    g.drawRoundedRectangle (rb, 7.0f, 1.0f);
}

void HardTuneEditor::resized()
{
    //  Le canevas garde ses 1408x768 logiques et c'est SA transform qui
    //  reduit le dessin a la taille demandee.
    canvas.setBounds (0, 0, W, H);
    canvas.setTransform (juce::AffineTransform::scale ((float) currentScale));
}

void HardTuneEditor::layoutCanvas()
{
    // ---- en-tete -----------------------------------------------------------
    titleLabel .setBounds (44, 18, 260, 44);
    keyCap     .setBounds (322, 31, 60, 24);
    keyBox     .setBounds (378, 28, 52, 34);
    scaleCap   .setBounds (470, 31, 70, 24);
    scaleBox   .setBounds (552, 28, 126, 34);
    lightCap   .setBounds (1058, 32, 70, 24);
    lightSwitch.setBounds (1136, 27, 50, 28);
    bypassCap  .setBounds (1190, 32, 90, 24);
    bypassSwitch.setBounds (1248, 25, 50, 28);
    sizeButton .setBounds (1308, 25, 40, 28);

    // ---- clavier -----------------------------------------------------------
    keys   .setBounds (36, 106, 1348, 106);
    keysCap.setBounds (40, 212, 320, 24);

    // ---- waveform : cadre peint a (40,309,726x391) -------------------------
    waveCap.setBounds (43, 272, 340, 26);
    scope  .setBounds (24, 295, 758, 443 + 26);   // + 26 = place de l'axe

    // ---- RETUNE SPEED ------------------------------------------------------
    retuneCap  .setBounds (817, 270, 200, 26);
    retuneVal  .setBounds (1010, 272, 96, 30);
    humanizeKnob.setBounds (832, 336, 68, 68);    // centre (866,370)
    humanizeCap .setBounds (box140 (866, 402));
    humanizeVal .setBounds (box140 (866, 422, 20));
    retuneKnob  .setBounds (914, 301, 98, 98);    // centre (963,350)
    retuneSub   .setBounds (box140 (963, 396));
    flexKnob    .setBounds (1026, 336, 68, 68);   // centre (1060,370)
    flexCap     .setBounds (box140 (1060, 402));
    flexVal     .setBounds (box140 (1060, 422, 20));

    // ---- CORRECTION --------------------------------------------------------
    corrCap    .setBounds (1162, 270, 200, 26);
    vibratoKnob.setBounds (1160, 320, 72, 72);    // centre (1196,356)
    vibratoCap .setBounds (box140 (1196, 398));
    vibratoVal .setBounds (box140 (1196, 418, 20));
    rateKnob   .setBounds (1252, 320, 72, 72);    // centre (1288,356)
    rateCap    .setBounds (box140 (1288, 398));
    rateVal    .setBounds (box140 (1288, 418, 20));

    // ---- carte du bas ------------------------------------------------------
    inCap     .setBounds (817, 512, 90, 22);
    meterIn   .setBounds (834, 536, 70, 112);
    inCap2    .setBounds (834, 648, 90, 22);
    inPeak    .setBounds (890, 542, 90, 20);
    inPeakVal .setBounds (866, 566, 120, 20);

    outCap    .setBounds (1290, 512, 100, 22);
    meterOut  .setBounds (1300, 536, 70, 112);
    outCap2   .setBounds (1290, 648, 100, 22);
    outPeak   .setBounds (1180, 552, 90, 20);
    outPeakVal.setBounds (1166, 576, 120, 20);

    gauge     .setBounds (993, 505, 190, 190);    // centre (1088,600)
    formantCap.setBounds (820, 684, 210, 22);
    formantBar.setBounds (820, 700, 176, 26);
    transposeCap.setBounds (1180, 684, 150, 22);
    transposeBar.setBounds (1180, 700, 150, 26);
    transposeVal.setBounds (1338, 682, 60, 22);
}

void HardTuneEditor::timerCallback()
{
    try
    {
        const bool dark = proc.apvts.getRawParameterValue (HardTuneProcessor::kDarkMode)->load() > 0.5f;
        if (dark != lastDarkState) { lastDarkState = dark; applyTheme(); }
        lightSwitch.setToggleState (! dark, juce::dontSendNotification);

        retuneVal  .setText (fmtMs (rawValue (HardTuneProcessor::kRetune, 0.0f)), juce::dontSendNotification);
        humanizeVal.setText (fmtPct (rawValue (HardTuneProcessor::kHumanize, 0.0f)), juce::dontSendNotification);
        flexVal    .setText (fmtPct (rawValue (HardTuneProcessor::kAmount, 100.0f)), juce::dontSendNotification);
        vibratoVal .setText (fmtPct (rawValue (HardTuneProcessor::kVibrato, 0.0f)), juce::dontSendNotification);
        rateVal    .setText (fmtHz (rawValue (HardTuneProcessor::kVibRate, 6.0f)), juce::dontSendNotification);
        transposeVal.setText (fmtSt ((int) std::lround (rawValue (HardTuneProcessor::kTranspose, 0.0f))),
                              juce::dontSendNotification);

        // clavier : gamme reelle via le NoteMapper (source unique)
        const int keyIdx = (int) std::lround (rawValue (HardTuneProcessor::kKey, 0.0f));
        int scaleIdx = (int) std::lround (rawValue (HardTuneProcessor::kScale, 0.0f));
        if (scaleIdx < 0 || scaleIdx >= (int) tune::NoteMapper::NumScales)
            scaleIdx = (int) tune::NoteMapper::Chromatic;
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

        // waveform : on ne pousse QUE les points arrives depuis l'affichage
        // precedent (compteur monotone), a la vraie cadence du flux. La
        // fenetre fait donc 5 s reelles, meme si le timer derive.
        const juce::int64 total = proc.getScopeTotal();
        int fresh = (int) juce::jlimit<juce::int64> (0, (juce::int64) HardTuneProcessor::kScopeSize,
                                                    total - lastScopeTotal);
        lastScopeTotal = total;
        const int rate = (int) std::lround (proc.getScopePointRate());
        if (fresh > 0)
        {
            std::array<float, HardTuneProcessor::kScopeSize> buf {};
            proc.copyScope (buf.data());
            scope.pushBlock (buf.data() + (HardTuneProcessor::kScopeSize - fresh), fresh, rate);
        }
        else
        {
            //  pas d'audio : le temps avance quand meme (ligne plate), sinon la
            //  courbe se fige et l'axe 0s..5s devient faux pendant un silence.
            static const std::array<float, 2048> silence {};
            const int n = juce::jlimit (1, 2048, (int) std::lround (rate / 30.0));
            scope.pushBlock (silence.data(), n, rate);
        }

        // cadran
        std::array<float, 256> cts {};
        proc.copyCents (cts.data());
        const float cents = cts[(size_t) (proc.kCentsCount - 1)];
        gauge.setData (cents, tgt, minor);

        // vumetres + PEAK
        meterIn.setLevels (proc.getInputPeakDb(), proc.getInputRmsDb());
        meterOut.setLevels (proc.getOutputPeakDb(), proc.getOutputRmsDb());
        inPeakVal .setText (juce::String::fromUTF8 ("\xe2\x97\x84 ") + fmtDb (proc.getInputPeakDb()),
                            juce::dontSendNotification);
        outPeakVal.setText (fmtDb (proc.getOutputPeakDb()) + " " + juce::String::fromUTF8 ("\xe2\x96\xba"),
                            juce::dontSendNotification);
    }
    catch (...) { /* un widget ne doit jamais faire tomber l'hote */ }
}

void HardTuneEditor::cycleSize()
{
    // regle : 0.6 -> 1.0 -> 1.5 -> 0.6
    static const double factors[3] = { 0.6, 1.0, 1.5 };
    sizeIdx = (sizeIdx + 1) % 3;
    applyScale (factors[sizeIdx]);
}
