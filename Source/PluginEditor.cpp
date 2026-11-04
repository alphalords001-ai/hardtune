//  PluginEditor.cpp — l'interface, dessinée à la main.
#include "PluginEditor.h"
#include "Diag.h"

using namespace juce;

namespace
{
    // Compact layout: 820 x 600 instead of 960 x 1050, so the window fits on a
    // 1366 x 768 laptop and stays in the usual range for an audio plug-in.
    constexpr int kWindowW = 820;
    constexpr int kWindowH = 600;

    constexpr int kHeaderH   = 44;
    constexpr int kKeysH     = 76;
    constexpr int kRightW    = 292;
    constexpr int kGaugeH    = 292;
    constexpr int kCard1H    = 190;
    constexpr int kCard2H    = 130;
    constexpr int kGap       = 8;
    constexpr int kMargin    = 10;

    String noteNameWithOctave (int midi)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };
        const int pc  = ((midi % 12) + 12) % 12;
        const int oct = midi / 12 - 2;
        return String (names[pc]) + String (oct);
    }

    String signedInt (int v) { return (v > 0 ? "+" : "") + String (v); }
}

// ---------------------------------------------------------------------------
HardTuneEditor::HardTuneEditor (HardTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p), pal (ht::lightPalette())
{
    //  Le fil qui construit l'editeur est le fil d'interface : lui seul a le
    //  droit d'ecrire le journal de diagnostic.
    ht::diag::markUiThread();

    //  On retire toute l'interface de l'arbre d'accessibilite Windows (UIA).
    //  Nos libelles changent 30 fois par seconde : chaque changement notifie
    //  les clients UIA (overlays NVIDIA / Discord / Razer, Narrator...), qui
    //  interrogeaient le plugin par messages de fenetre. C'est exactement la
    //  pile du plantage (FLEngine -> USER32 -> HardTune). Un plugin de musique
    //  n'a rien a faire dans cet arbre : setAccessible(false) couvre aussi
    //  tous les enfants.
    setAccessible (false);

    // ---- en-tête -----------------------------------------------------------
    addAndMakeVisible (titleLabel);
    makeLabel (titleLabel, "ALPHA-TUNE", 15.0f, pal.accentDark, true);
    titleLabel.setFont (ht::font (15.0f, true).withExtraKerningFactor (0.10f));

    addAndMakeVisible (keyCaption);
    makeCaption (keyCaption, "KEY", 9.5f);
    addAndMakeVisible (scaleCaption);
    makeCaption (scaleCaption, "SCALE", 9.5f);
    addAndMakeVisible (lightCaption);
    makeCaption (lightCaption, "LIGHT", 9.0f);
    addAndMakeVisible (darkCaption);
    makeCaption (darkCaption, "DARK", 9.0f);
    addAndMakeVisible (bypassCaption);
    makeCaption (bypassCaption, "BYPASS", 9.0f);

    addAndMakeVisible (keyBox);
    keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    keyBox.setJustificationType (Justification::centred);

    addAndMakeVisible (scaleBox);
    StringArray scales;
    for (int i = 0; i < (int) tune::NoteMapper::NumScales; ++i)
        scales.add (tune::NoteMapper::scaleName ((tune::NoteMapper::Scale) i));
    scales.add ("Custom");
    scaleBox.addItemList (scales, 1);

    addAndMakeVisible (darkSwitch);
    addAndMakeVisible (bypassSwitch);

    // ---- clavier / oscilloscope -------------------------------------------
    addAndMakeVisible (keys);

    addAndMakeVisible (scopeCaption);
    makeCaption (scopeCaption, "WAVEFORM · PITCH", 9.5f);
    addAndMakeVisible (scope);

    // ---- colonne de droite --------------------------------------------------
    addAndMakeVisible (retuneCaption);
    makeCaption (retuneCaption, "RETUNE SPEED", 10.0f);
    addAndMakeVisible (retuneKnob);
    retuneKnob.setPalette (pal);

    addAndMakeVisible (retuneValue);
    makeLabel (retuneValue, "75 ms", 13.0f, pal.accentDark);
    retuneValue.setJustificationType (Justification::centred);
    retuneValue.setColour (Label::backgroundColourId, pal.card2);
    retuneValue.setColour (Label::outlineColourId, pal.cardEdge);

    addAndMakeVisible (humanizeCaption);
    makeCaption (humanizeCaption, "HUMANIZE", 9.5f);
    addAndMakeVisible (humanizeKnob);
    humanizeKnob.setPalette (pal);
    addAndMakeVisible (humanizeValue);
    makeLabel (humanizeValue, "32 %", 11.0f, pal.ink);
    humanizeValue.setJustificationType (Justification::centred);

    addAndMakeVisible (flexCaption);
    makeCaption (flexCaption, "FLEX-TUNE", 9.5f);
    addAndMakeVisible (flexKnob);
    flexKnob.setPalette (pal);
    addAndMakeVisible (flexValue);
    makeLabel (flexValue, "40 %", 11.0f, pal.ink);
    flexValue.setJustificationType (Justification::centred);

    addAndMakeVisible (vibCaption);
    makeCaption (vibCaption, "VIBRATO", 9.5f);
    addAndMakeVisible (vibratoKnob);
    vibratoKnob.setPalette (pal);
    addAndMakeVisible (vibValue);
    makeLabel (vibValue, "20 %", 11.0f, pal.ink);
    vibValue.setJustificationType (Justification::centred);

    addAndMakeVisible (rateCaption);
    makeCaption (rateCaption, "RATE", 9.5f);
    addAndMakeVisible (rateKnob);
    rateKnob.setPalette (pal);
    addAndMakeVisible (rateValue);
    makeLabel (rateValue, "6 Hz", 11.0f, pal.ink);
    rateValue.setJustificationType (Justification::centred);

    addAndMakeVisible (meterIn);
    meterIn.setTitle ("Input");
    meterIn.setPalette (pal);
    addAndMakeVisible (meterOut);
    meterOut.setTitle ("Output");
    meterOut.setPalette (pal);

    // ---- carte du grand cadran ---------------------------------------------
    addAndMakeVisible (gauge);
    gauge.setPalette (pal);

    addAndMakeVisible (noteLabel);
    makeLabel (noteLabel, "C3#", 15.0f, pal.accentDark, true);
    noteLabel.setFont (ht::font (15.0f, true).withExtraKerningFactor (0.14f));
    noteLabel.setJustificationType (Justification::centredRight);

    addAndMakeVisible (pitchCaption);
    makeCaption (pitchCaption, "PITCH", 9.5f);
    addAndMakeVisible (targetCaption);
    makeCaption (targetCaption, "target C3", 9.5f);

    //  v0.8.0 : le moteur Live est une ligne à retard — il ne peut pas déplacer
    //  les formants indépendamment de la hauteur. Le réglage reste présent pour
    //  ne pas invalider les presets existants, mais il n'a plus d'effet : on le
    //  montre inactif plutôt que de laisser croire à un contrôle.
    addAndMakeVisible (formantCaption);
    makeCaption (formantCaption, "FORMANT (INACTIF)", 9.5f);
    addAndMakeVisible (formantValue);
    makeLabel (formantValue, "\u2014", 11.0f, pal.inkFaint);
    formantValue.setJustificationType (Justification::centredRight);
    addAndMakeVisible (formantBar);
    formantBar.setPalette (pal);
    formantBar.setAlpha (0.35f);
    formantBar.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (transposeCaption);
    makeCaption (transposeCaption, "TRANSPOSE", 9.5f);
    addAndMakeVisible (transposeValue);
    makeLabel (transposeValue, "0 st", 11.0f, pal.ink);
    transposeValue.setJustificationType (Justification::centredRight);
    addAndMakeVisible (transposeBar);
    transposeBar.setPalette (pal);

    // ---- liaisons avec les paramètres --------------------------------------
    auto& st = proc.apvts;
    retuneAtt    = std::make_unique<SliderAtt> (st, HardTuneProcessor::kRetune,   retuneKnob);
    humanizeAtt  = std::make_unique<SliderAtt> (st, HardTuneProcessor::kHumanize, humanizeKnob);
    flexAtt      = std::make_unique<SliderAtt> (st, HardTuneProcessor::kAmount,   flexKnob);
    vibratoAtt   = std::make_unique<SliderAtt> (st, HardTuneProcessor::kVibrato,  vibratoKnob);
    rateAtt      = std::make_unique<SliderAtt> (st, HardTuneProcessor::kVibRate,  rateKnob);
    formantAtt   = std::make_unique<SliderAtt> (st, HardTuneProcessor::kFormant,  formantBar);
    transposeAtt = std::make_unique<SliderAtt> (st, HardTuneProcessor::kTranspose, transposeBar);

    darkAtt   = std::make_unique<ButtonAtt> (st, HardTuneProcessor::kDarkMode,   darkSwitch);
    bypassAtt = std::make_unique<ButtonAtt> (st, HardTuneProcessor::kBypass,     bypassSwitch);

    keyAtt   = std::make_unique<ComboAtt> (st, HardTuneProcessor::kKey,   keyBox);
    scaleAtt = std::make_unique<ComboAtt> (st, HardTuneProcessor::kScale, scaleBox);

    // mini-interrupteurs de l'en-tête
    darkSwitch.setCompact (true);
    bypassSwitch.setCompact (true);

    applyTheme();

    // Adapte la fenetre a l'ecran : 820x600 ne tient pas sur tous les ecrans
    // (1080p avec barres, portables, mise a l'echelle 125-150 %).
    {
        double scale = 1.0;

        if (auto* d = Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const auto area = d->userBounds;   // zone utile, en points logiques
            const double sx = (double) area.getWidth()  * 0.90 / (double) kWindowW;
            const double sy = (double) area.getHeight() * 0.85 / (double) kWindowH;
            scale = jlimit (0.50, 1.0, jmin (sx, sy));
        }

        setScaleFactor ((float) scale);
    }

    setSize (kWindowW, kWindowH);
    lastDark = darkSwitch.getToggleState();
    startTimerHz (30);

    ht::diag::log ("editeur : construction terminee");
}

HardTuneEditor::~HardTuneEditor()
{
    // Le minuteur doit etre arrete avant que l'editeur ne disparaisse : sinon
    // un rappel peut arriver pendant/pres la destruction (acces a un objet mort).
    stopTimer();

    ht::diag::log ("editeur : destruction");
}

// ---------------------------------------------------------------------------
Label& HardTuneEditor::makeLabel (Label& l, const String& text, float height,
                                  Colour colour, bool bold)
{
    l.setText (text, dontSendNotification);
    l.setFont (ht::font (height, bold).withExtraKerningFactor (0.06f));
    l.setColour (Label::textColourId, colour);
    l.setJustificationType (Justification::centredLeft);
    l.setInterceptsMouseClicks (false, false);
    return l;
}

Label& HardTuneEditor::makeCaption (Label& l, const String& text, float height)
{
    l.setText (text.toUpperCase(), dontSendNotification);
    l.setFont (ht::font (height, false).withExtraKerningFactor (0.18f));
    l.setColour (Label::textColourId, pal.inkSoft);
    l.setJustificationType (Justification::centredLeft);
    l.setInterceptsMouseClicks (false, false);
    return l;
}

// ---------------------------------------------------------------------------
void HardTuneEditor::applyTheme()
{
    const bool dark = darkSwitch.getToggleState();
    pal = dark ? ht::darkPalette() : ht::lightPalette();

    for (Label* l : { &keyCaption, &scaleCaption, &bypassCaption })
        l->setColour (Label::textColourId, pal.inkSoft);
    lightCaption.setColour (Label::textColourId, dark ? pal.inkSoft : pal.accentDark);
    darkCaption .setColour (Label::textColourId, dark ? pal.accent : pal.inkSoft);

    titleLabel.setColour (Label::textColourId, pal.accentDark);
    noteLabel.setColour (Label::textColourId, pal.accentDark);

    for (Label* l : { &scopeCaption, &retuneCaption, &humanizeCaption, &flexCaption,
                      &vibCaption, &rateCaption, &pitchCaption,
                      &targetCaption, &formantCaption, &transposeCaption })
        l->setColour (Label::textColourId, pal.inkSoft);

    for (Label* l : { &humanizeValue, &flexValue, &vibValue, &rateValue,
                      &formantValue, &transposeValue })
        l->setColour (Label::textColourId, pal.ink);

    retuneValue.setColour (Label::textColourId, pal.accentDark);
    retuneValue.setColour (Label::backgroundColourId, pal.card2);
    retuneValue.setColour (Label::outlineColourId, pal.cardEdge);

    for (ComboBox* c : { &keyBox, &scaleBox })
    {
        c->setColour (ComboBox::backgroundColourId, pal.card2);
        c->setColour (ComboBox::textColourId, pal.accentDark);
        c->setColour (ComboBox::outlineColourId, pal.cardEdge);
        c->setColour (ComboBox::arrowColourId, pal.inkSoft);
        c->setColour (ComboBox::focusedOutlineColourId, pal.accent);
    }

    darkSwitch.setPalette (pal);
    bypassSwitch.setPalette (pal);
    retuneKnob.setPalette (pal);
    humanizeKnob.setPalette (pal);
    flexKnob.setPalette (pal);
    vibratoKnob.setPalette (pal);
    rateKnob.setPalette (pal);
    formantBar.setPalette (pal);
    transposeBar.setPalette (pal);
    gauge.setPalette (pal);
    scope.setPalette (pal);
    meterIn.setPalette (pal);
    meterOut.setPalette (pal);

    repaint();
}

// ---------------------------------------------------------------------------
void HardTuneEditor::drawCard (Graphics& g, Rectangle<int> r) const
{
    //  Rectangle degenere (mise en page pas encore faite) : rien a dessiner.
    if (r.getWidth() < 2 || r.getHeight() < 2)
        return;

    auto rf = r.toFloat();
    ColourGradient grad (pal.card1, rf.getX(), rf.getY(),
                         pal.card2, rf.getX(), rf.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (rf, 12.0f);
    g.setColour (pal.cardEdge);
    g.drawRoundedRectangle (rf.reduced (0.5f), 12.0f, 1.0f);
}

void HardTuneEditor::paint (Graphics& g)
{
    ht::diag::stage ("paint:editeur");

    //  Un hote peut peindre l'editeur avant sa mise en page (taille 1x1, 0x0).
    //  A cette taille, tous les rectangles deviennent degeneres et JUCE remplit
    //  des chemins impossibles : on ne dessine rien.
    if (getWidth() < 200 || getHeight() < 160)
    {
        g.fillAll (pal.win1);
        return;
    }

    try
    {
    auto b = getLocalBounds().toFloat();

    ColourGradient bg (pal.win1, b.getX(), b.getY(), pal.win2, b.getX(), b.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // en-tête
    g.setColour (pal.card1.withAlpha (0.92f));
    g.fillRect (rHeader);
    g.setColour (pal.cardEdge);
    g.drawHorizontalLine (rHeader.getBottom() - 1, 0.0f, (float) getWidth());

    for (auto* r : { &rKeys, &rScope, &rRetuneCard, &rCorrCard, &rMetersCard })
        drawCard (g, *r);

    // ---- carte du grand cadran : liseré lumineux + coins techniques --------
    {
        auto rf = rGaugeCard.toFloat();
        g.setColour (pal.glow.withAlpha (pal.dark ? 0.35f : 0.22f));
        g.drawRoundedRectangle (rf.expanded (3.0f), 15.0f, 3.0f);

        ColourGradient grad (pal.card1, rf.getCentreX(), rf.getY(),
                             pal.card2, rf.getCentreX(), rf.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (rf, 12.0f);
        g.setColour (pal.accent.withAlpha (0.55f));
        g.drawRoundedRectangle (rf.reduced (0.5f), 12.0f, 1.2f);

        const float L = 20.0f;
        g.setColour (pal.accent.withAlpha (0.55f));
        auto corner = [&] (float x, float y, float dx, float dy)
        {
            g.drawLine (x, y, x + L * dx, y, 2.0f);
            g.drawLine (x, y, x, y + L * dy, 2.0f);
        };
        corner (rf.getX() + 11.0f,  rf.getY() + 11.0f,  1.0f,  1.0f);
        corner (rf.getRight() - 11.0f, rf.getY() + 11.0f, -1.0f,  1.0f);
        corner (rf.getX() + 11.0f,  rf.getBottom() - 11.0f,  1.0f, -1.0f);
        corner (rf.getRight() - 11.0f, rf.getBottom() - 11.0f, -1.0f, -1.0f);
    }
    }
    catch (...)
    {
        ht::diag::logOnce ("EXCEPTION rattrapee dans paint:editeur");
    }
}

// ---------------------------------------------------------------------------
void HardTuneEditor::resized()
{
    ht::diag::stage ("resized");

    auto r = getLocalBounds();
    rHeader = r.removeFromTop (kHeaderH);

    // ---- en-tête -----------------------------------------------------------
    {
        auto h = rHeader.reduced (kMargin, 0);
        titleLabel.setBounds (h.removeFromLeft (148));
        h.removeFromLeft (6);

        keyCaption.setBounds (h.removeFromLeft (30).withSizeKeepingCentre (30, 12));
        keyBox.setBounds (h.removeFromLeft (54).withSizeKeepingCentre (54, 26));
        h.removeFromLeft (10);
        scaleCaption.setBounds (h.removeFromLeft (42).withSizeKeepingCentre (42, 12));
        scaleBox.setBounds (h.removeFromLeft (102).withSizeKeepingCentre (102, 26));

        auto rh = rHeader.reduced (kMargin, 0);
        bypassSwitch.setBounds (rh.removeFromRight (42).withSizeKeepingCentre (42, 20));
        rh.removeFromRight (6);
        bypassCaption.setBounds (rh.removeFromRight (50).withSizeKeepingCentre (50, 12));
        rh.removeFromRight (14);
        darkCaption.setBounds (rh.removeFromRight (34).withSizeKeepingCentre (34, 12));
        rh.removeFromRight (5);
        darkSwitch.setBounds (rh.removeFromRight (42).withSizeKeepingCentre (42, 20));
        rh.removeFromRight (5);
        lightCaption.setBounds (rh.removeFromRight (38).withSizeKeepingCentre (38, 12));
    }

    // ---- corps -------------------------------------------------------------
    auto body = r.reduced (kMargin);

    rKeys = body.removeFromTop (kKeysH);
    body.removeFromTop (kGap);
    keys.setBounds (rKeys.reduced (10, 10));

    auto right = body.removeFromRight (kRightW);
    body.removeFromRight (kGap);
    auto left = body;

    // carte du grand cadran (en bas à gauche)
    rGaugeCard = left.removeFromBottom (kGaugeH);
    left.removeFromBottom (kGap);
    rScope = left;

    // colonne de droite
    rRetuneCard = right.removeFromTop (kCard1H);
    right.removeFromTop (kGap);
    rCorrCard = right.removeFromTop (kCard2H);
    right.removeFromTop (kGap);
    rMetersCard = right;

    // ---- carte oscilloscope ------------------------------------------------
    {
        auto c = rScope.reduced (12, 10);
        scopeCaption.setBounds (c.removeFromTop (13));
        c.removeFromTop (4);
        scope.setBounds (c);
    }

    // ---- carte RETUNE SPEED / HUMANIZE / FLEX-TUNE -------------------------
    {
        auto c = rRetuneCard.reduced (10, 9);
        retuneCaption.setBounds (c.removeFromTop (12));
        c.removeFromTop (4);

        auto row = c.removeFromTop (66);
        retuneKnob.setBounds (row.removeFromLeft (74).withSizeKeepingCentre (64, 64));
        row.removeFromLeft (8);
        retuneValue.setBounds (row.withSizeKeepingCentre (juce::jmin (76, row.getWidth()), 30));

        c.removeFromTop (6);
        auto knobs = c;
        const int kw = knobs.getWidth() / 2;

        auto left1 = knobs.removeFromLeft (kw);
        humanizeCaption.setBounds (left1.removeFromTop (12));
        humanizeKnob.setBounds (left1.removeFromTop (50).withSizeKeepingCentre (46, 46));
        humanizeValue.setBounds (left1.removeFromTop (12));

        auto right1 = knobs;
        flexCaption.setBounds (right1.removeFromTop (12));
        flexKnob.setBounds (right1.removeFromTop (50).withSizeKeepingCentre (46, 46));
        flexValue.setBounds (right1.removeFromTop (12));
    }

    // ---- carte PITCH CORRECTION / VIBRATO / RATE ---------------------------
    {
        auto c = rCorrCard.reduced (10, 9);
        //  v0.8.1 (TEST) : plus de commutateur PITCH CORRECTION.
        (void) c.removeFromTop (24);

        c.removeFromTop (6);
        auto knobs = c;

        auto left1 = knobs.removeFromLeft (knobs.getWidth() / 2);
        vibCaption.setBounds (left1.removeFromTop (12));
        vibratoKnob.setBounds (left1.removeFromTop (48).withSizeKeepingCentre (44, 44));
        vibValue.setBounds (left1.removeFromTop (12));

        auto right1 = knobs;
        rateCaption.setBounds (right1.removeFromTop (12));
        rateKnob.setBounds (right1.removeFromTop (48).withSizeKeepingCentre (44, 44));
        rateValue.setBounds (right1.removeFromTop (12));
    }

    // ---- carte des vu-mètres ------------------------------------------------
    {
        auto c = rMetersCard.reduced (10, 8);
        meterIn.setBounds (c.removeFromLeft (c.getWidth() / 2));
        meterOut.setBounds (c);
    }

    // ---- carte du grand cadran ----------------------------------------------
    {
        auto c = rGaugeCard.reduced (12, 10);
        auto gaugeRow = c.removeFromTop (186);
        gauge.setBounds (gaugeRow.withSizeKeepingCentre (182, 182));

        c.removeFromTop (2);
        auto noteRow = c.removeFromTop (22);
        noteLabel.setBounds (noteRow.removeFromLeft (noteRow.getWidth() / 2));
        pitchCaption.setBounds (noteRow.removeFromLeft (44));
        targetCaption.setBounds (noteRow);

        c.removeFromTop (6);
        auto bars = c;
        auto b1 = bars.removeFromLeft (bars.getWidth() / 2 - 10);
        bars.removeFromLeft (20);
        auto b2 = bars;

        auto layoutBar = [] (Rectangle<int> area, Label& cap, Label& val, ht::BarControl& bar)
        {
            auto head = area.removeFromTop (14);
            cap.setBounds (head.removeFromLeft (100));
            val.setBounds (head);
            bar.setBounds (area.removeFromTop (26));   // la poignée Ø22 tient dans 26 px
        };
        layoutBar (b1, formantCaption, formantValue, formantBar);
        layoutBar (b2, transposeCaption, transposeValue, transposeBar);
    }
}

// ---------------------------------------------------------------------------
void HardTuneEditor::timerCallback()
{
    ht::diag::stage ("timer");

    // Le timer tourne sur le fil d'interface. Une exception ou un pointeur nul
    // ici fait tomber l'hote tout entier (le plugin tourne dans son processus) :
    // on ne laisse donc jamais rien s'echapper.
    try
    {
        refreshFromProcessor();
    }
    catch (...)
    {
        ht::diag::logOnce ("EXCEPTION rattrapee dans timerCallback");
    }

    // C'est ici, et seulement ici, que le journal est ecrit.
    ht::diag::flushStage();
}

// Lit un parametre sans jamais dereferencer un pointeur nul : getRawParameterValue
// peut renvoyer nullptr (identifiant absent, etat restaure d'une autre version,
// APVTS en cours de remplacement). C'etait le seul pointeur brut lu par
// l'interface, et un nullptr y donne exactement "Read of address 0x0".
float HardTuneEditor::paramValue (const char* id, float fallback) const
{
    if (auto* p = proc.apvts.getRawParameterValue (id))
        return p->load();

    return fallback;
}

void HardTuneEditor::refreshFromProcessor()
{
    // thème (interrupteur LIGHT / DARK)
    if (darkSwitch.getToggleState() != lastDark)
    {
        lastDark = darkSwitch.getToggleState();
        applyTheme();
    }

    // vu-mètres
    meterIn.setLevels (proc.getInputPeakDb(), proc.getInputRmsDb());
    meterOut.setLevels (proc.getOutputPeakDb(), proc.getOutputRmsDb());

    // cadran + note
    const float cents  = proc.getCentsError();
    const bool  voiced = proc.isVoiced();
    const int   target = proc.getTargetNote();

    gauge.setData (cents, voiced, target);
    if (target >= 0)
    {
        noteLabel.setText (noteNameWithOctave (target), dontSendNotification);
        targetCaption.setText ("target " + noteNameWithOctave (target), dontSendNotification);
    }

    // clavier
    const int keyIndex   = (int) paramValue (HardTuneProcessor::kKey);
    const int scaleIndex = (int) paramValue (HardTuneProcessor::kScale);
    unsigned mask = tune::NoteMapper::scaleMask (
        (tune::NoteMapper::Scale) jlimit (0, (int) tune::NoteMapper::NumScales - 1, scaleIndex));
    keys.setState (voiced ? target : -1, voiced, keyIndex, mask);

    // oscilloscope
    {
        float s[HardTuneProcessor::kScopeSize];
        float c[HardTuneProcessor::kCentsCount];
        proc.copyScope (s);
        proc.copyCents (c);

        const int nS = jmin (512, HardTuneProcessor::kScopeSize);
        const int nC = HardTuneProcessor::kCentsCount;
        scope.setData (s + (HardTuneProcessor::kScopeSize - nS), nS, c, nC);
    }

    // valeurs écrites à côté des boutons
    auto raw = [this] (const char* id) { return paramValue (id); };
    retuneValue.setText (String ((int) std::lround (raw (HardTuneProcessor::kRetune))) + " ms",
                         dontSendNotification);
    humanizeValue.setText (String ((int) std::lround (raw (HardTuneProcessor::kHumanize))) + " %",
                           dontSendNotification);
    flexValue.setText (String ((int) std::lround (raw (HardTuneProcessor::kAmount))) + " %",
                       dontSendNotification);
    vibValue.setText (String ((int) std::lround (raw (HardTuneProcessor::kVibrato))) + " %",
                      dontSendNotification);
    rateValue.setText (String (raw (HardTuneProcessor::kVibRate), 1) + " Hz", dontSendNotification);

    formantValue.setText (String::fromUTF8 ("\u2014"), dontSendNotification);
    const int tr = (int) std::lround (raw (HardTuneProcessor::kTranspose));
    transposeValue.setText (signedInt (tr) + " st", dontSendNotification);

}
