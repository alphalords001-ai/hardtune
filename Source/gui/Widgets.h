//  Widgets.h — composants dessinés à la main de HardTune (interface paysage 1408x768,
//  fidèle à la maquette validée pixel par pixel contre la référence).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "Theme.h"

namespace ht
{

// ---------------------------------------------------------------------------
//  Bouton rond : anneau 270° (quart du bas ouvert), ventre charbon, index à l'arc.
// ---------------------------------------------------------------------------
class Knob : public juce::Slider
{
public:
    Knob()
    {
        setSliderStyle (juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRange (0.0, 1.0, 0.0);
        setDoubleClickReturnValue (true, 0.5);
    }

    void setPalette (const Palette& p) { pal = p; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float d  = juce::jmin (b.getWidth(), b.getHeight());
        if (d < 24.0f) return;
        const float ring = d * 0.115f;
        const float R    = d * 0.5f;
        const float bodyR = R - ring * 0.5f - 1.0f;
        const auto  c  = b.getCentre();

        const double range = juce::jmax (1.0e-9, getMaximum() - getMinimum());
        const double frac  = juce::jlimit (0.0, 1.0, (getValue() - getMinimum()) / range);

        // piste 270° : de 135° à 405° (0° = droite, positif vers le bas)
        juce::Path track;
        track.addCentredArc (c.x, c.y, R, R, 0.0f,
                             juce::degreesToRadians (135.0f),
                             juce::degreesToRadians (405.0f), true);
        juce::ColourGradient trackGrad (juce::Colour (0xffe7edf1), c.x - R, c.y - R,
                                        juce::Colour (0xff9fabb4), c.x + R, c.y + R, false);
        trackGrad.addColour (0.5, juce::Colour (0xffc3cdd5));
        g.setGradientFill (trackGrad);
        g.strokePath (track, juce::PathStrokeType (ring, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::butt));

        // arc de valeur
        const float a1 = 135.0f + 270.0f * (float) frac;
        if (frac > 0.001)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, R, R, 0.0f,
                               juce::degreesToRadians (135.0f),
                               juce::degreesToRadians (a1), true);
            juce::ColourGradient grad (pal.accentLight, c.x - R, c.y - R,
                                       pal.accentDark,  c.x + R, c.y + R, false);
            g.setGradientFill (grad);
            g.strokePath (val, juce::PathStrokeType (ring, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::butt));
        }

        // ventre charbon
        auto face = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
        juce::ColourGradient fg (pal.knobFace1, c.x - bodyR * 0.35f, c.y - bodyR * 0.45f,
                                 pal.knobFace2, c.x + bodyR * 0.35f, c.y + bodyR * 0.55f, true);
        g.setGradientFill (fg);
        g.fillEllipse (face);
        g.setColour (pal.knobEdge);
        g.drawEllipse (face, 1.0f);

        // index à la fin de l'arc
        const float ar = juce::degreesToRadians (a1);
        const float ix = c.x + std::cos (ar) * R;
        const float iy = c.y + std::sin (ar) * R;
        g.setColour (pal.index);
        g.fillEllipse (ix - ring * 0.30f, iy - ring * 0.30f, ring * 0.60f, ring * 0.60f);
    }

private:
    Palette pal = lightPalette();
};

// ---------------------------------------------------------------------------
//  Barre coulissante horizontale (FORMANT grisée, TRANSPOSE bleue).
// ---------------------------------------------------------------------------
class BarControl : public juce::Slider
{
public:
    BarControl()
    {
        setSliderStyle (juce::Slider::LinearHorizontal);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRange (-1.0, 1.0, 0.0);
        setDoubleClickReturnValue (true, 0.0);
    }

    void setPalette (const Palette& p) { pal = p; repaint(); }
    void setEnabled2 (bool e) { enabled2 = e; setEnabled (e); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        if (b.getWidth() < 16.0f || b.getHeight() < 6.0f) return;
        const float h = 6.0f;
        auto track = juce::Rectangle<float> (b.getX(), b.getCentreY() - h * 0.5f,
                                             b.getWidth(), h);
        // piste toujours grise (comme la référence)
        g.setColour (enabled2 ? pal.barTrack : juce::Colour (0xffe3e9ec));
        g.fillRoundedRectangle (track, h * 0.5f);

        const double frac = juce::jlimit (0.0, 1.0, (getValue() - getMinimum())
                                                    / juce::jmax (1.0e-9, getMaximum() - getMinimum()));
        const float x = track.getX() + track.getWidth() * (float) frac;

        // poignée
        const float kd = 22.0f;
        juce::Rectangle<float> knob (kd, kd);
        knob.setCentre (x, track.getCentreY());
        if (enabled2)
        {
            juce::ColourGradient kg (juce::Colour (0xff3fb3e0), knob.getX(), knob.getY(),
                                     juce::Colour (0xff14618f), knob.getRight(), knob.getBottom(), false);
            g.setGradientFill (kg);
            g.fillEllipse (knob);
            g.setColour (juce::Colour (0xff0e5d8a));
            g.drawEllipse (knob, 1.0f);
        }
        else
        {
            juce::ColourGradient kg (juce::Colours::white, knob.getX(), knob.getY(),
                                     juce::Colour (0xffdfe8ee), knob.getRight(), knob.getBottom(), false);
            g.setGradientFill (kg);
            g.fillEllipse (knob);
            g.setColour (juce::Colour (0xffb9c4cb));
            g.drawEllipse (knob, 1.0f);
        }
    }

private:
    Palette pal = lightPalette();
    bool enabled2 = true;
};

// ---------------------------------------------------------------------------
//  Interrupteur à glissière (LIGHT, BYPASS).
// ---------------------------------------------------------------------------
class PillSwitch : public juce::Button
{
public:
    explicit PillSwitch (const juce::String& name) : juce::Button (name)
    {
        setClickingTogglesState (true);
    }

    void setPalette (const Palette& p) { pal = p; repaint(); }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        if (b.getWidth() < 8.0f || b.getHeight() < 6.0f) return;
        const float r = b.getHeight() * 0.5f;
        const bool on = getToggleState();
        if (on)
        {
            // bleu profond mesuré sur la référence : #066ac0
            juce::ColourGradient grad (juce::Colour (0xff0d78c8), 0.0f, b.getY(),
                                       juce::Colour (0xff055da6), 0.0f, b.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (b, r);
            g.setColour (juce::Colour (0xff044e8c));
            g.drawRoundedRectangle (b, r, 1.0f);
        }
        else
        {
            // gris mesuré sur la référence : #b3b8bc
            g.setColour (juce::Colour (0xffb3b8bc));
            g.fillRoundedRectangle (b, r);
            g.setColour (juce::Colour (0xff9aa4ab));
            g.drawRoundedRectangle (b, r, 1.0f);
        }
        if (over)
        {
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (b, r);
        }
        const float kd = b.getHeight() - 6.0f;
        juce::Rectangle<float> knob (kd, kd);
        knob.setCentre (on ? b.getRight() - kd * 0.5f - 3.0f : b.getX() + kd * 0.5f + 3.0f,
                        b.getCentreY());
        juce::ColourGradient kg (juce::Colours::white, knob.getX(), knob.getY(),
                                 juce::Colour (0xffdde8f0), knob.getRight(), knob.getBottom(), false);
        g.setGradientFill (kg);
        g.fillEllipse (knob);
    }

private:
    Palette pal = lightPalette();
};

// ---------------------------------------------------------------------------
//  Piano 3 octaves (21 blanches + noires), couleurs FIXES comme la référence :
//  blanches #fbfcfb, noires #000 avec fondu intérieur sur les côtés et en bas.
//  Les notes de la gamme sont surlignées (noires en bleu, bande translucide).
// ---------------------------------------------------------------------------
class KeyStrip : public juce::Component
{
public:
    void setScale (int keyIndex, unsigned scaleMask, bool minor)
    {
        key = keyIndex; mask = scaleMask; isMinor = minor;
        repaint();
    }

    void setPlaying (int targetMidi, bool voiced)
    {
        target = targetMidi; playing = voiced;
        repaint();
    }

    static juce::String nameFor (int pc, bool minor)
    {
        static const char* sharp[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        static const char* flat [12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
        return juce::String ((minor ? flat : sharp)[((pc % 12) + 12) % 12]);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (r.getWidth() < 60.0f || r.getHeight() < 40.0f) return;
        const int nWhite = 21;
        const float w = r.getWidth() / (float) nWhite;
        const float blackH = r.getHeight() * (70.0f / 106.0f);
        const float blackW = w * 0.56f;
        static const int blackAfter[5] = { 0, 1, 3, 4, 5 };

        // blanches
        for (int i = 0; i < nWhite; ++i)
        {
            juce::Rectangle<float> kr (r.getX() + w * i, r.getY(), w, r.getHeight());
            g.setColour (juce::Colour (0xfffbfcfb));
            g.fillRect (kr);
            g.setColour (juce::Colour (0xff1d2a33));
            g.drawLine (kr.getRight(), kr.getY(), kr.getRight(), kr.getBottom(), 1.5f);
            g.drawLine (kr.getX(), kr.getBottom(), kr.getRight(), kr.getBottom(), 1.5f);
        }
        // bande translucide sur les blanches de la gamme
        for (int i = 0; i < nWhite; ++i)
        {
            const int pcWhite = whitePC (i);
            if (inScale (pcWhite))
            {
                juce::Rectangle<float> kr (r.getX() + w * i + 1, r.getY() + 1,
                                           w - 1, r.getHeight() - 1);
                g.setColour (juce::Colour (0x4c2aa6d8)); // rgba(42,166,216,.30)
                g.fillRect (kr);
            }
        }
        // noires
        for (int oct = 0; oct < 3; ++oct)
            for (int k = 0; k < 5; ++k)
            {
                const int i = oct * 7 + blackAfter[k];
                if (i + 1 >= nWhite) continue;
                const int pc = (whitePC (blackAfter[k]) + 1) % 12; // classe absolue de la noire
                const float x = r.getX() + (i + 1) * w - blackW * 0.5f;
                const bool hot = inScale (pc);
                //  `pc` est une classe ABSOLUE (voir inScale) : la note jouee doit
                //  l'etre aussi. Le decalage par `key` ne laissait surligner que
                //  la tonalite de do.
                const bool isPlaying = playing && target >= 0 && (target % 12) == pc;
                juce::Rectangle<float> kr (x, r.getY(), blackW, blackH);
                if (hot || isPlaying)
                {
                    juce::ColourGradient grad (juce::Colour (0xff8fe0f7), 0, kr.getY(),
                                               juce::Colour (0xff1f8ec5), 0, kr.getBottom(), false);
                    g.setGradientFill (grad);
                    g.fillRoundedRectangle (kr, 4.0f);
                }
                else
                {
                    // noir pur + fondu intérieur sur les côtés et en bas (mesuré sur la réf)
                    g.setColour (juce::Colours::black);
                    g.fillRect (kr);
                    juce::ColourGradient gl (juce::Colour (0x33ffffff), kr.getX(), 0,
                                             juce::Colours::transparentWhite, kr.getX() + 3.5f, 0, false);
                    g.setGradientFill (gl); g.fillRect (kr.withWidth (3.5f));
                    juce::ColourGradient gr (juce::Colours::transparentWhite, kr.getRight() - 3.5f, 0,
                                             juce::Colour (0x33ffffff), kr.getRight(), 0, false);
                    g.setGradientFill (gr); g.fillRect (kr.withTrimmedLeft (kr.getWidth() - 3.5f));
                    juce::ColourGradient gb (juce::Colours::transparentBlack, kr.getX(), kr.getBottom() - 4,
                                             juce::Colour (0x99ffffff), kr.getX(), kr.getBottom(), false);
                    g.setGradientFill (gb); g.fillRect (kr.withTrimmedTop (kr.getHeight() - 4));
                }
            }
    }

private:
    static int whitePC (int i) { static const int p[7] = {0,2,4,5,7,9,11}; return p[i % 7]; }
    bool inScale (int pc) const { return ((mask >> (((pc % 12) + 12) % 12)) & 1u) != 0u; } // masque absolu

    int key = 0;
    unsigned mask = 0xfffu;
    bool isMinor = false;
    int target = -1;
    bool playing = false;
};

// ---------------------------------------------------------------------------
//  Cadran à aiguille (jauge de pitch) — arc lumineux 142°..398°, 26 graduations.
// ---------------------------------------------------------------------------
class Gauge : public juce::Component
{
public:
    void setPalette (const Palette& p) { pal = p; repaint(); }

    void setData (float cents, int targetNote, bool minor)
    {
        value = cents; target = targetNote; isMinor = minor;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        if (d < 60.0f) return;
        const auto c = b.getCentre();
        const float th = d * 0.075f;
        const float R  = d * 0.5f - th * 0.5f - 2.0f;
        const float a0 = juce::degreesToRadians (142.0f);
        const float a1 = juce::degreesToRadians (398.0f);

        // halo lumineux
        {
            juce::Path halo;
            halo.addCentredArc (c.x, c.y, R, R, 0.0f, a0, a1, true);
            g.setColour (pal.glow);
            g.strokePath (halo, juce::PathStrokeType (th * 1.9f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
        // anneau dégradé
        {
            juce::Path ring;
            ring.addCentredArc (c.x, c.y, R, R, 0.0f, a0, a1, true);
            juce::ColourGradient grad (juce::Colour (0xff8fe6f2), c.x - R, c.y - R,
                                       juce::Colour (0xff14618f), c.x + R, c.y + R, false);
            g.setGradientFill (grad);
            g.strokePath (ring, juce::PathStrokeType (th, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::butt));
        }
        // graduations
        for (int i = 0; i <= 25; ++i)
        {
            const float a = a0 + (a1 - a0) * i / 25.0f;
            const bool major = (i % 5) == 0;
            const float r1 = R - th * 0.5f - 2.0f;
            const float r2 = r1 - (major ? d * 0.035f : d * 0.020f);
            g.setColour (major ? juce::Colour (0xff3a4a55) : juce::Colour (0xff8fa3ae));
            g.drawLine (c.x + std::cos (a) * r1, c.y + std::sin (a) * r1,
                        c.x + std::cos (a) * r2, c.y + std::sin (a) * r2, major ? 2.0f : 1.2f);
        }
        // aiguille : 0 = droite, positif vers le bas ; ±50 cents -> ±128°
        const float clamped = juce::jlimit (-50.0f, 50.0f, value);
        const float ang = juce::degreesToRadians (clamped / 50.0f * 128.0f);
        const float L = R - th * 0.95f;
        const float tail = L * 0.22f;
        g.setColour (juce::Colour (0x4015588a));
        g.drawLine (c.x - std::cos (ang) * tail, c.y - std::sin (ang) * tail,
                    c.x + std::cos (ang) * L, c.y + std::sin (ang) * L, d * 0.045f);
        g.setColour (juce::Colour (0xff17537f));
        g.drawLine (c.x - std::cos (ang) * tail, c.y - std::sin (ang) * tail,
                    c.x + std::cos (ang) * L, c.y + std::sin (ang) * L, d * 0.026f);
        // pivot charbon
        g.setColour (juce::Colour (0xff333d45));
        g.fillEllipse (c.x - d * 0.072f, c.y - d * 0.072f, d * 0.144f, d * 0.144f);
        g.setColour (juce::Colour (0x66ffffff));
        g.fillEllipse (c.x - d * 0.064f, c.y - d * 0.072f, d * 0.10f, d * 0.10f);

        // note (nom selon la gamme : # en majeur, b en mineur) + cents
        g.setColour (juce::Colour (0xff154f79));
        g.setFont (font (d * 0.132f, true));
        g.drawText (target >= 0 ? KeyStrip::nameFor (target % 12, isMinor)
                                 + juce::String (target / 12 - 1) : "--",
                    juce::Rectangle<float> (c.x - d * 0.30f, c.y + d * 0.16f, d * 0.60f, d * 0.20f),
                    juce::Justification::centred, false);
        g.setColour (juce::Colour (0xff16242e));
        g.setFont (font (13.0f));   // taille fixe imposee par la reference
        const int cv = (int) std::lround (value);
        g.drawText ((cv > 0 ? "+" : "") + juce::String (cv),
                    juce::Rectangle<float> (c.x + d * 0.10f, c.y - d * 0.06f, d * 0.28f, d * 0.14f),
                    juce::Justification::centredLeft, false);
    }

private:
    Palette pal = lightPalette();
    float value = 0.0f;
    int target = -1;
    bool isMinor = false;
};

// ---------------------------------------------------------------------------
//  WAVEFORM · PITCH : fenêtre de 5 secondes, axe 0s..5s, courbe de pitch.
//  Si le signal touche le plafond (saturation), la partie collée en haut
//  reste droite et passe au ROUGE (bascule nette, comme le CPU de FL Studio).
// ---------------------------------------------------------------------------
class Scope : public juce::Component
{
public:
    void setPalette (const Palette& p) { pal = p; repaint(); }

    /** `cols` = nombre de colonnes (largeur du cadre). Fenetre fixe de 5 s. */
    void pushBlock (const float* s, int n, int sampleRate)
    {
        const int cols = (int) colMin.size();
        const int perCol = juce::jmax (1, (int) (5.0 * sampleRate / cols));
        for (int i = 0; i < n; ++i)
        {
            const float a = s[i];
            colMin[(size_t) cursor] = pos == 0 ? a : juce::jmin (colMin[(size_t) cursor], a);
            colMax[(size_t) cursor] = pos == 0 ? a : juce::jmax (colMax[(size_t) cursor], a);
            if (++pos >= perCol)
            {
                pos = 0;
                cursor = (cursor + 1) % cols;
            }
        }
        repaint();
    }

    void resetWindow() { colMin.fill (0.0f); colMax.fill (0.0f); cursor = 0; pos = 0; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        if (b.getWidth() < 16.0f || b.getHeight() < 16.0f) return;

        auto frame = b.reduced (16.0f, 14.0f);
        frame.removeFromBottom (24.0f);   // place pour l'axe du temps
        const float mid = frame.getCentreY();
        const float amp = frame.getHeight() * 0.47f;
        const int   cols = (int) colMin.size();

        // fond : degrade horizontal + voile blanc vertical (haut et bas)
        {
            juce::ColourGradient bg (juce::Colour (0xffb5e7ea), frame.getX(), 0.0f,
                                     juce::Colour (0xffb7d0e6), frame.getRight(), 0.0f, false);
            bg.addColour (0.5, juce::Colour (0xffb5dbea));
            g.setGradientFill (bg);
            g.fillRoundedRectangle (frame, 8.0f);

            juce::ColourGradient ov (juce::Colours::white.withAlpha (0.78f), 0.0f, frame.getY(),
                                     juce::Colours::white.withAlpha (0.80f), 0.0f, frame.getBottom(), false);
            ov.addColour (0.45, juce::Colours::white.withAlpha (0.0f));
            g.setGradientFill (ov);
            g.fillRoundedRectangle (frame, 8.0f);
        }

        // grille
        g.setColour (juce::Colour (0xff465055).withAlpha (0.20f));
        for (int i = 1; i < 5; ++i)
            g.drawVerticalLine ((int) (frame.getX() + frame.getWidth() * i / 5.0f),
                                frame.getY(), frame.getBottom());
        g.setColour (juce::Colour (0xff537576).withAlpha (0.50f));
        g.drawHorizontalLine ((int) (frame.getY() + frame.getHeight() * 0.25f), frame.getX(), frame.getRight());
        g.drawHorizontalLine ((int) (frame.getY() + frame.getHeight() * 0.75f), frame.getX(), frame.getRight());
        g.setColour (juce::Colour (0xff3f6a7c));
        g.drawLine (frame.getX(), mid, frame.getRight(), mid, 1.1f);

        // courbe : UN point par colonne (la plus grande deviation)
        const float ceilY  = frame.getY() + 2.4f;
        const float floorY = frame.getBottom() - 2.4f;

        juce::Path curve, sat;
        bool started = false, satStarted = false;

        for (int i = 0; i < cols; ++i)
        {
            const float mn = colMin[(size_t) i], mx = colMax[(size_t) i];
            const float v  = std::abs (mx) >= std::abs (mn) ? mx : mn;
            const float x  = frame.getX() + frame.getWidth() * (i + 0.5f) / cols;
            const bool clipped = mx >= 0.999f || mn <= -0.999f;
            const float y = clipped ? (mx >= 0.999f ? ceilY : floorY) : (mid - v * amp);

            if (! started) { curve.startNewSubPath (x, y); started = true; }
            curve.lineTo (x, y);

            if (clipped)
            {
                if (! satStarted) { sat.startNewSubPath (x, y); satStarted = true; }
                else sat.lineTo (x, y);
            }
            else satStarted = false;
        }

        // saturation : partie collee au plafond, droite et rouge, avec halo
        if (satStarted)
        {
            g.setColour (juce::Colour (0xffd02a1e).withAlpha (0.28f));
            g.strokePath (sat, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            g.setColour (juce::Colour (0xffd02a1e));
            g.strokePath (sat, juce::PathStrokeType (3.3f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }

        g.setColour (juce::Colour (0xff22638f));
        g.strokePath (curve, juce::PathStrokeType (3.3f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0xff9dacae));
        g.drawRoundedRectangle (frame, 8.0f, 1.0f);

        // axe du temps
        g.setFont (font (15.0f));
        g.setColour (juce::Colour (0xff4a6875));
        for (int i = 0; i <= 5; ++i)
        {
            const float x = frame.getX() + frame.getWidth() * i / 5.0f;
            g.drawLine (x, frame.getBottom(), x, frame.getBottom() + 5.0f, 1.0f);
            auto just = i == 0 ? juce::Justification::left
                      : i == 5 ? juce::Justification::right
                               : juce::Justification::centred;
            g.drawText (juce::String (i) + "s",
                        juce::Rectangle<float> (x - 20.0f, frame.getBottom() + 7.0f, 40.0f, 16.0f),
                        just, false);
        }
    }

private:
    Palette pal = lightPalette();
    static constexpr int kCols = 731;
    std::array<float, kCols> colMin {}, colMax {};
    int cursor = 0;
    int pos = 0;
};

// ---------------------------------------------------------------------------
//  Vu-mètre segmenté (2 colonnes de 12 segments, remplissage depuis le bas,
//  bleu foncé en bas -> cyan vers le niveau, comme la référence).
// ---------------------------------------------------------------------------
class LevelMeter : public juce::Component
{
public:
    void setPalette (const Palette& p) { pal = p; repaint(); }

    void setLevels (float peakDb, float rmsDb)
    {
        peak = peakDb; rms = rmsDb;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        if (b.getWidth() < 24.0f || b.getHeight() < 40.0f) return;
        const int n = 12;
        const float segH = 8.0f, gap = 1.6f;
        const float colW = 30.0f, colGap = 10.0f;
        const float totalH = n * (segH + gap);
        const float y0 = b.getCentreY() - totalH * 0.5f;

        for (int k = 0; k < 2; ++k)
        {
            const float db = k == 0 ? peak : rms;
            const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
            const int filled = (int) (norm * n + 0.5f);
            const float cx = b.getX() + k * (colW + colGap);
            for (int i = 0; i < n; ++i)
            {
                const bool on = i >= n - filled;      // monte depuis le bas
                const float y = y0 + i * (segH + gap);
                juce::Colour col;
                if (on)
                {
                    const float t = (n - 1 - i) / (float) (n - 1); // 0 = bas
                    col = t > 0.82f ? juce::Colour (0xff1f8fe0)
                        : t > 0.62f ? juce::Colour (0xff4ec8e8)
                        : t > 0.34f ? juce::Colour (0xff1f6fb0)
                                    : juce::Colour (0xff17497e);
                }
                else col = juce::Colour (0xffc9ced3);
                g.setColour (col);
                g.fillRoundedRectangle (juce::Rectangle<float> (cx, y, colW, segH), segH * 0.5f);
            }
        }
    }

private:
    Palette pal = lightPalette();
    float peak = -100.0f, rms = -100.0f;
};

// ---------------------------------------------------------------------------
//  Listes deroulantes habillees en "pill" (KEY / SCALE).
// ---------------------------------------------------------------------------
class PillLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawComboBox (juce::Graphics& g, int w, int h, bool,
                       int, int, int, int, juce::ComboBox&) override
    {
        auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        juce::ColourGradient grad (juce::Colour (0xffbfe6f8), 0.0f, b.getY(),
                                   juce::Colour (0xffa5d9f4), 0.0f, b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b, 7.0f);
        g.setColour (juce::Colour (0xff9dc9e2));
        g.drawRoundedRectangle (b, 7.0f, 1.0f);

        // petit chevron, sinon rien n'indique que la pill s'ouvre
        juce::Path chev;
        const float cx = b.getRight() - 13.0f, cy = b.getCentreY();
        chev.startNewSubPath (cx - 4.0f, cy - 2.0f);
        chev.lineTo (cx, cy + 2.5f);
        chev.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (juce::Colour (0xff0e4d72));
        g.strokePath (chev, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (10, 0, box.getWidth() - 24, box.getHeight());
        label.setFont (font (19.0f, true));
        label.setColour (juce::Label::textColourId, juce::Colour (0xff0e4d72));
        label.setJustificationType (juce::Justification::centredLeft);
    }
};

} // namespace ht
