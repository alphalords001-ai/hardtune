//  PluginEditor.cpp — interface paysage (maquette 1408 x 768).
//
//  Toutes les coordonnees de ce fichier sont ecrites dans le repere de la
//  maquette (1408 x 768). place() les convertit vers la taille reelle de la
//  fenetre, et paint() dessine sous la meme echelle : l'interface peut donc
//  etre redimensionnee sans se disloquer.
#include "PluginEditor.h"

#include "Diag.h"

#include <array>
#include <cmath>

namespace
{
constexpr int W = HardTuneEditor::kDesignW;
constexpr int H = HardTuneEditor::kDesignH;

juce::String fmtMs (float v)  { return juce::String (v, 0) + " ms"; }
juce::String fmtPct (float v) { return juce::String (v, 0) + " %"; }
juce::String fmtHz (float v)  { return juce::String (v, 1) + " Hz"; }
juce::String fmtSt (int v)    { return juce::String (v) + " st"; }

//  Les pics descendent tres bas au silence (-120 dB) : on borne l'affichage
//  pour que le texte ne sorte jamais de sa case.
juce::String fmtPeak (float db)
{
    return "PEAK " + juce::String ((int) std::lround (juce::jmax (-99.0f, db))) + " dB";
}
}

// ---------------------------------------------------------------------------
HardTuneEditor::HardTuneEditor (HardTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    ht::diag::markUiThread();

    auto& apvts = proc.apvts;

    // ---- en-tete ------------------------------------------------------------
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("ALPHA-TUNE", juce::dontSendNotification);
    titleLabel.setFont (ht::font (34.0f, true));

    cap (keyCap,   "KEY",   19);
    cap (scaleCap, "SCALE", 19);

    addAndMakeVisible (keyBox);
    keyBox.addItemList (apvts.getParameter (HardTuneProcessor::kKey)->getAllValueStrings(), 1);
    keyAtt = std::make_unique<ComboAtt> (apvts, HardTuneProcessor::kKey, keyBox);

    addAndMakeVisible (scaleBox);
    scaleBox.addItemList (apvts.getParameter (HardTuneProcessor::kScale)->getAllValueStrings(), 1);
    scaleAtt = std::make_unique<ComboAtt> (apvts, HardTuneProcessor::kScale, scaleBox);

    //  LIGHT et DARK entourent l'interrupteur : le mot du mode actif passe en
    //  bleu (voir applyTheme()). Allume = mode sombre, comme le parametre.
    cap (lightCap,  "LIGHT",  19);
    cap (darkCap,   "DARK",   19);
    cap (bypassCap, "BYPASS", 19);

    addAndMakeVisible (lightSwitch);
    lightAtt = std::make_unique<ButtonAtt> (apvts, HardTuneProcessor::kDarkMode, lightSwitch);

    addAndMakeVisible (bypassSwitch);
    bypassAtt = std::make_unique<ButtonAtt> (apvts, HardTuneProcessor::kBypass, bypassSwitch);

    // ---- clavier ------------------------------------------------------------
    addAndMakeVisible (keys);
    cap (keysCap, "KEYBOARD \xc2\xb7 SCALE", 19);

    // ---- waveform -----------------------------------------------------------
    cap (waveCap, "WAVEFORM \xc2\xb7 PITCH", 19);
    addAndMakeVisible (scope);

    // ---- RETUNE SPEED -------------------------------------------------------
    auto setupKnob = [&] (ht::Knob& k, const char* id)
    {
        addAndMakeVisible (k);
        k.setLookAndFeel (nullptr);
        return std::make_unique<SliderAtt> (apvts, id, k);
    };
    auto styleValue = [&] (juce::Label& v)
    {
        addAndMakeVisible (v);
        v.setFont (ht::font (17.0f, true));
        v.setColour (juce::Label::textColourId, juce::Colour (0xff0e527d));
    };
    auto capVal = [&] (juce::Label& c, juce::Label& v, const juce::String& t)
    {
        cap (c, t, 15);
        styleValue (v);
    };

    cap (retuneCap, "RETUNE SPEED", 17);
    retuneKnob.setName ("retune");
    retuneAtt = setupKnob (retuneKnob, HardTuneProcessor::kRetune);
    styleValue (retuneVal);
    retuneVal.setText (fmtMs (rawValue (HardTuneProcessor::kRetune, 0.0f)),
                       juce::dontSendNotification);

    capVal (humanizeCap, humanizeVal, "HUMANIZE");
    humanizeAtt = setupKnob (humanizeKnob, HardTuneProcessor::kHumanize);

    //  FLEX-TUNE est le dosage de la correction : c'est le parametre `amount`.
    //  En v0.6 il etait eteint et affichait la valeur de HUMANIZE.
    capVal (flexCap, flexVal, "FLEX-TUNE");
    flexAtt = setupKnob (flexKnob, HardTuneProcessor::kAmount);

    // ---- CORRECTION ---------------------------------------------------------
    cap (corrCap, "CORRECTION", 17);
    capVal (vibratoCap, vibratoVal, "VIBRATO");
    vibratoAtt = setupKnob (vibratoKnob, HardTuneProcessor::kVibrato);
    capVal (rateCap, rateVal, "RATE");
    rateAtt = setupKnob (rateKnob, HardTuneProcessor::kVibRate);

    // ---- carte du bas -------------------------------------------------------
    cap (inCap,  "INPUT",  17);
    cap (outCap, "OUTPUT", 17);

    for (auto* l : { &inPeak, &outPeak })
    {
        addAndMakeVisible (*l);
        l->setFont (ht::font (13.0f));
        l->setColour (juce::Label::textColourId, juce::Colour (0xff33505e));
        l->setJustificationType (juce::Justification::centredRight);
    }
    inPeak.setText  (fmtPeak (-99.0f), juce::dontSendNotification);
    outPeak.setText (fmtPeak (-99.0f), juce::dontSendNotification);

    addAndMakeVisible (meterIn);
    addAndMakeVisible (meterOut);
    addAndMakeVisible (gauge);

    //  FORMANT a ete retire : le parametre existe encore dans le moteur mais
    //  n'a aucun effet audio. TRANSPOSE recupere toute la largeur.
    cap (transposeCap, "TRANSPOSE", 17);
    addAndMakeVisible (transposeBar);
    transposeAtt = std::make_unique<SliderAtt> (apvts, HardTuneProcessor::kTranspose, transposeBar);
    styleValue (transposeVal);

    // ---- fenetre ------------------------------------------------------------
    //  Sur un ecran plus petit que la maquette, on ouvre la fenetre a une
    //  echelle reduite plutot que de la faire depasser de l'ecran.
    uiScale = 1.0f;
    if (auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto ua = disp->userArea;
        const float kH = ((float) ua.getHeight() - 120.0f) / (float) H;
        const float kW = ((float) ua.getWidth()  -  80.0f) / (float) W;
        uiScale = juce::jlimit (0.55f, 1.0f, juce::jmin (kH, kW));
    }

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (W * 0.55f), juce::roundToInt (H * 0.55f), W * 2, H * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) W / (double) H);

    setSize (juce::roundToInt (W * uiScale), juce::roundToInt (H * uiScale));

    applyTheme();
    startTimerHz (30);
}

HardTuneEditor::~HardTuneEditor() { stopTimer(); }

// ---------------------------------------------------------------------------
float HardTuneEditor::rawValue (const char* id, float fallback) const
{
    //  getRawParameterValue peut renvoyer nullptr (identifiant absent, etat
    //  restaure d'une autre version) : le dereferencer donnerait exactement
    //  "Read of address 0x0".
    if (auto* p = proc.apvts.getRawParameterValue (id))
        return p->load();

    return fallback;
}

juce::Label& HardTuneEditor::cap (juce::Label& l, const juce::String& t, int height,
                                  juce::Colour c)
{
    addAndMakeVisible (l);
    l.setText (t, juce::dontSendNotification);
    l.setFont (ht::font ((float) height));
    l.setColour (juce::Label::textColourId, c);
    return l;
}

void HardTuneEditor::place (juce::Component& c, int x, int y, int w, int h)
{
    c.setBounds (juce::roundToInt ((float) x * uiScale), juce::roundToInt ((float) y * uiScale),
                 juce::roundToInt ((float) w * uiScale), juce::roundToInt ((float) h * uiScale));
}

// ---------------------------------------------------------------------------
void HardTuneEditor::applyTheme()
{
    const bool dark = rawValue (HardTuneProcessor::kDarkMode) > 0.5f;
    pal = dark ? ht::darkPalette() : ht::lightPalette();
    setOpaque (true);

    for (auto* k : { &retuneKnob, &humanizeKnob, &flexKnob, &vibratoKnob, &rateKnob })
        k->setPalette (pal);

    transposeBar.setPalette (pal);
    lightSwitch.setPalette (pal);
    bypassSwitch.setPalette (pal);
    scope.setPalette (pal);
    gauge.setPalette (pal);
    meterIn.setPalette (pal);
    meterOut.setPalette (pal);

    //  En-tete : le mot du mode actif est en bleu, l'autre en gris.
    lightCap.setColour (juce::Label::textColourId, dark ? pal.inkSoft    : pal.accentDark);
    darkCap .setColour (juce::Label::textColourId, dark ? pal.accentDark : pal.inkSoft);

    titleLabel.setColour (juce::Label::textColourId, pal.accentDark);

    for (auto* l : { &keyCap, &scaleCap, &bypassCap, &keysCap, &waveCap, &corrCap,
                     &retuneCap, &humanizeCap, &flexCap, &vibratoCap, &rateCap,
                     &inCap, &outCap, &transposeCap })
        l->setColour (juce::Label::textColourId, pal.inkSoft);

    for (auto* l : { &inPeak, &outPeak })
        l->setColour (juce::Label::textColourId, pal.inkSoft);

    for (auto* l : { &retuneVal, &humanizeVal, &flexVal, &vibratoVal, &rateVal,
                     &transposeVal })
        l->setColour (juce::Label::textColourId, pal.accentDark);

    repaint();
}

// ---------------------------------------------------------------------------
void HardTuneEditor::paint (juce::Graphics& g)
{
    //  Tout le dessin est ecrit en coordonnees maquette, puis mis a l'echelle.
    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::scale (uiScale));

    // fond aqua degrade (mesure sur la reference)
    juce::ColourGradient bg (juce::Colour (0xffc9e8f2), 0.0f, 0.0f,
                             juce::Colour (0xff84b0ce), 0.0f, (float) H, false);
    g.setGradientFill (bg);
    g.fillRect (0, 0, W, H);

    auto card = [&] (int x, int y, int w, int h)
    {
        juce::ColourGradient cg (pal.card1, 0.0f, (float) y,
                                 pal.card2, 0.0f, (float) (y + h), false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) x, (float) y,
                                                        (float) w, (float) h), 9.0f);
        g.setColour (pal.cardEdge);
        g.drawRoundedRectangle (juce::Rectangle<float> ((float) x, (float) y,
                                                        (float) w, (float) h), 9.0f, 1.0f);
    };

    card (24, 18, 1360, 60);     // en-tete
    card (24, 95, 1360, 150);    // clavier
    card (24, 265, 758, 470);    // waveform
    card (797, 265, 328, 217);   // retune
    card (1142, 265, 240, 217);  // correction
    card (797, 500, 585, 235);   // carte du bas

    // bandeau degrade a gauche de l'en-tete
    juce::ColourGradient strip (juce::Colour (0xff4fc0e8), 0.0f, 18.0f,
                                juce::Colour (0xff14618f), 0.0f, 78.0f, false);
    g.setGradientFill (strip);
    g.fillRoundedRectangle (juce::Rectangle<float> (24.0f, 18.0f, 6.0f, 60.0f), 3.0f);

    // separateur vertical de l'en-tete
    g.setColour (pal.cardEdge);
    g.drawLine (300.0f, 26.0f, 300.0f, 70.0f, 1.0f);
}

// ---------------------------------------------------------------------------
void HardTuneEditor::resized()
{
    const float s = juce::jmin ((float) getWidth()  / (float) W,
                                (float) getHeight() / (float) H);
    if (s > 0.1f)
        uiScale = s;

    // en-tete
    place (titleLabel,   44, 24, 240, 44);
    place (keyCap,      322, 32,  60, 26);
    place (keyBox,      378, 26,  52, 34);
    place (scaleCap,    470, 32,  70, 26);
    place (scaleBox,    552, 26, 126, 34);
    place (lightCap,   1000, 30,  56, 26);
    place (lightSwitch,1060, 25,  46, 28);
    place (darkCap,    1112, 30,  54, 26);
    place (bypassCap,  1186, 30,  70, 26);
    place (bypassSwitch, 1264, 25, 46, 28);

    // clavier
    place (keys,      36, 106, 1348, 106);
    place (keysCap,   40, 218, 300, 24);

    // waveform
    place (waveCap,   56, 274, 300, 26);
    place (scope,     40, 309, 731, 417);

    // retune
    place (retuneCap,   817, 272, 200, 24);
    place (retuneKnob,  887, 300, 120, 120);
    place (retuneVal,   872, 424, 150, 30);
    place (humanizeKnob, 812, 300, 60, 60);
    place (humanizeVal,  876, 331, 70, 22);
    place (humanizeCap,  810, 452, 100, 20);
    place (flexKnob,    1016, 300, 60, 60);
    place (flexVal,     1079, 331, 46, 22);
    place (flexCap,     1010, 452, 100, 20);

    // correction
    place (corrCap,     1162, 272, 200, 24);
    place (vibratoKnob, 1156, 300, 60, 60);
    place (vibratoVal,  1220, 331, 70, 22);
    place (vibratoCap,  1152, 452, 90, 20);
    place (rateKnob,    1272, 300, 60, 60);
    place (rateVal,     1332, 333, 50, 22);
    place (rateCap,     1272, 452, 60, 20);

    // carte du bas
    place (inCap,    834, 508, 80, 22);
    place (meterIn,  834, 536, 70, 112);
    place (inPeak,   906, 541, 86, 40);
    place (outCap,  1300, 508, 70, 22);
    place (meterOut,1300, 536, 70, 112);
    place (outPeak, 1196, 549, 94, 40);
    place (gauge,    993, 510, 190, 190);
    place (transposeCap, 820, 680, 140, 22);
    place (transposeBar, 820, 706, 240, 22);
    place (transposeVal, 1075, 680, 60, 22);
}

// ---------------------------------------------------------------------------
void HardTuneEditor::timerCallback()
{
    ht::diag::stage ("timer");

    //  Le timer tourne sur le fil d'interface. Une exception ou un pointeur nul
    //  ici fait tomber l'hote tout entier (le plugin tourne dans son
    //  processus) : on ne laisse jamais rien s'echapper.
    try
    {
        const bool dark = rawValue (HardTuneProcessor::kDarkMode) > 0.5f;
        if (dark != lastDarkState) { lastDarkState = dark; applyTheme(); }

        // ---- valeurs affichees ---------------------------------------------
        retuneVal.setText (fmtMs (rawValue (HardTuneProcessor::kRetune, 0.0f)),
                           juce::dontSendNotification);
        humanizeVal.setText (fmtPct (rawValue (HardTuneProcessor::kHumanize)),
                             juce::dontSendNotification);
        //  FLEX-TUNE montre `amount` (et non plus `humanize`)
        flexVal.setText (fmtPct (rawValue (HardTuneProcessor::kAmount, 100.0f)),
                         juce::dontSendNotification);
        vibratoVal.setText (fmtPct (rawValue (HardTuneProcessor::kVibrato)),
                            juce::dontSendNotification);
        rateVal.setText (fmtHz (rawValue (HardTuneProcessor::kVibRate, 6.0f)),
                         juce::dontSendNotification);
        transposeVal.setText (fmtSt ((int) std::lround (rawValue (HardTuneProcessor::kTranspose))),
                              juce::dontSendNotification);

        // ---- etat musical ---------------------------------------------------
        const bool voiced = proc.isVoiced();
        const int  target = proc.getTargetNote();
        const int  keyIndex = (int) std::lround (rawValue (HardTuneProcessor::kKey));

        //  La gamme vient du MEME endroit que le moteur : plus de masque code
        //  a la main, et les 9 gammes du menu sont dessinees correctement.
        const int scaleIndex = juce::jlimit (0, (int) tune::NoteMapper::NumScales - 1,
                                             (int) std::lround (rawValue (HardTuneProcessor::kScale)));
        const auto scale = (tune::NoteMapper::Scale) scaleIndex;
        const bool minor = (scale == tune::NoteMapper::MinorNatural
                         || scale == tune::NoteMapper::MinorHarmonic);

        keys.setScale (keyIndex, tune::NoteMapper::scaleMask (scale), minor);
        keys.setPlaying (voiced ? target : -1, voiced);

        // ---- oscilloscope ---------------------------------------------------
        //  copyScope() ecrit TOUJOURS kScopeSize valeurs : un tampon plus petit
        //  deborderait (corruption de pile).
        std::array<float, (size_t) HardTuneProcessor::kScopeSize> scopeBuf {};
        proc.copyScope (scopeBuf.data());
        const int nS = juce::jmin (512, HardTuneProcessor::kScopeSize);
        scope.pushBlock (scopeBuf.data() + (HardTuneProcessor::kScopeSize - nS), nS,
                         (int) juce::jmax (8000.0, proc.getSampleRate()));

        // ---- cadran ---------------------------------------------------------
        gauge.setData (proc.getCentsError(), target, minor);

        // ---- vumetres -------------------------------------------------------
        meterIn.setLevels (proc.getInputPeakDb(), proc.getInputRmsDb());
        meterOut.setLevels (proc.getOutputPeakDb(), proc.getOutputRmsDb());
        inPeak.setText (fmtPeak (proc.getInputPeakDb()), juce::dontSendNotification);
        outPeak.setText (fmtPeak (proc.getOutputPeakDb()), juce::dontSendNotification);
    }
    catch (...)
    {
        ht::diag::logOnce ("EXCEPTION rattrapee dans timerCallback");
    }

    ht::diag::flushStage();
}
