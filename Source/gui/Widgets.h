//  Widgets.h — tous les composants dessinés à la main de HardTune.
//
//  Tout est dessiné en code (aucune image) pour que l'interface reste identique
//  sur Windows et Linux, et pour que le mode Light/Dark soit un simple
//  changement de palette.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "Theme.h"

namespace ht
{

// ---------------------------------------------------------------------------
//  Bouton rond (270°, sans aiguille, ventre sombre)
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
        const float sw = juce::jmax (5.0f, d * 0.105f);
        const float R  = d * 0.5f - sw * 0.5f - 1.0f;
        const auto  c  = b.getCentre();

        // piste 270°
        juce::Path track;
        track.addCentredArc (c.x, c.y, R, R, 0.0f,
                             juce::degreesToRadians (-135.0f),
                             juce::degreesToRadians (270.0f), true);
        g.setColour (pal.ringTrack);
        g.strokePath (track, juce::PathStrokeType (sw, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::butt));

        // arc de valeur
        const double range = juce::jmax (1.0e-9, getMaximum() - getMinimum());
        const double frac  = juce::jlimit (0.0, 1.0, (getValue() - getMinimum()) / range);
        if (frac > 0.001)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, R, R, 0.0f,
                               juce::degreesToRadians (-135.0f),
                               juce::degreesToRadians ((float) (270.0 * frac)), true);
            juce::ColourGradient grad (pal.accentLight, c.x - R, c.y - R,
                                       pal.accentDark,  c.x + R, c.y + R, false);
            g.setGradientFill (grad);
            g.strokePath (val, juce::PathStrokeType (sw, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::butt));
        }

        // ventre du bouton (sombre, comme validé sur la maquette)
        const float Rf = R - sw * 0.5f - 2.0f;
        auto face = juce::Rectangle<float> (Rf * 2.0f, Rf * 2.0f).withCentre (c);
        juce::ColourGradient fg (pal.knobFace1, c.x - Rf * 0.35f, c.y - Rf * 0.45f,
                                 pal.knobFace2, c.x + Rf * 0.35f, c.y + Rf * 0.55f, true);
        g.setGradientFill (fg);
        g.fillEllipse (face);
        g.setColour (pal.knobEdge);
        g.drawEllipse (face, 1.0f);
    }

private:
    Palette pal = lightPalette();
};

// ---------------------------------------------------------------------------
//  Barre coulissante (FORMANT, TRANSPOSE) — valeur -1 .. +1, repère au centre
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

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float h = 12.0f;
        auto track = juce::Rectangle<float> (b.getX(), b.getCentreY() - h * 0.5f,
                                             b.getWidth(), h);

        g.setColour (pal.barTrack);
        g.fillRoundedRectangle (track, h * 0.5f);

        // repère du milieu
        g.setColour (pal.accent.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (track.getCentreX(), track.getY() + 2.0f, 1.0f, h - 4.0f));

        const double frac = juce::jlimit (0.0, 1.0, (getValue() - getMinimum())
                                                    / juce::jmax (1.0e-9, getMaximum() - getMinimum()));
        const float x = track.getX() + track.getWidth() * (float) frac;
        const float mid = track.getCentreX();

        juce::Rectangle<float> fill (juce::jmin (mid, x), track.getY(),
                                     std::abs (x - mid), h);
        if (fill.getWidth() > 0.5f)
        {
            juce::ColourGradient grad (pal.barFill1, fill.getX(), 0.0f,
                                       pal.barFill2, fill.getRight(), 0.0f, false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill, h * 0.5f);
        }

        // poignée
        const float kd = 22.0f;
        juce::Rectangle<float> knob (kd, kd);
        knob.setCentre (x, track.getCentreY());
        g.setColour (juce::Colours::white);
        g.fillEllipse (knob);
        g.setColour (pal.accentDark);
        g.drawEllipse (knob, 1.6f);
    }

private:
    Palette pal = lightPalette();
};

// ---------------------------------------------------------------------------
//  Interrupteur à glissière (BYPASS, LIGHT/DARK, PITCH CORRECTION)
// ---------------------------------------------------------------------------
class PillSwitch : public juce::Button
{
public:
    explicit PillSwitch (const juce::String& name) : juce::Button (name)
    {
        setClickingTogglesState (true);
    }

    void setPalette (const Palette& p) { pal = p; repaint(); }
    void setCompact (bool c) { compact = c; resized(); }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        const float r = b.getHeight() * 0.5f;

        const bool on = getToggleState();
        if (on)
        {
            juce::ColourGradient grad (pal.accent, 0.0f, b.getY(),
                                       pal.accentDark, 0.0f, b.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (b, r);
            g.setColour (pal.accentDark);
            g.drawRoundedRectangle (b, r, 1.0f);
        }
        else
        {
            g.setColour (pal.dark ? pal.card1.brighter (0.10f) : juce::Colour (0xffdfe9f1));
            g.fillRoundedRectangle (b, r);
            g.setColour (pal.dark ? pal.cardEdge : juce::Colour (0xffc9d8e4));
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
    bool compact = false;
};

// ---------------------------------------------------------------------------
//  Le piano : 12 touches de même taille. Couleurs FIXES (jamais de thème).
// ---------------------------------------------------------------------------
class KeyStrip : public juce::Component
{
public:
    void setState (int targetMidi, bool playing, int keyIndex, unsigned scaleMask)
    {
        target = targetMidi;
        voiced = playing;
        key    = keyIndex;
        mask   = scaleMask;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const int n = 12;
        const float w = r.getWidth() / (float) n;

        auto isBlack = [] (int c)
        {
            return c == 1 || c == 3 || c == 6 || c == 8 || c == 10;
        };

        for (int i = 0; i < n; ++i)
        {
            juce::Rectangle<float> keyRect (r.getX() + w * (float) i + 1.0f, r.getY(),
                                            w - 2.0f, r.getHeight());
            const bool isPlaying = voiced && target >= 0
                                   && ((target - key) % 12 + 12) % 12 == i;
            const bool inScale = ((mask >> ((i - key + 12) % 12)) & 1u) != 0u;

            juce::Colour c1, c2, txt;
            if (isPlaying)      { c1 = juce::Colour (0xffa6e2fa); c2 = juce::Colour (0xff4aa3d8); txt = juce::Colours::white; }
            else if (isBlack (i)) { c1 = juce::Colour (0xff3d586f); c2 = juce::Colour (0xff22374b); txt = juce::Colour (0xffdbeaf5); }
            else                { c1 = juce::Colours::white;        c2 = juce::Colour (0xffeef7fd); txt = juce::Colour (0xff5b8098); }

            juce::ColourGradient grad (c1, 0.0f, keyRect.getY(), c2, 0.0f, keyRect.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (keyRect, 5.0f);
            g.setColour (juce::Colour (0xffcfe2f0));
            g.drawRoundedRectangle (keyRect, 5.0f, 1.0f);

            g.setColour (txt);
            g.setFont (font (16.5f));
            g.drawText (juce::String (nameFor (i)),
                        keyRect.withTrimmedBottom (30.0f).toNearestInt(),
                        juce::Justification::centredBottom, false);

            if (inScale && ! isPlaying)
            {
                g.setColour (juce::Colour (0xff5fb6e2).withAlpha (0.85f));
                g.fillEllipse (keyRect.getCentreX() - 4.6f, keyRect.getBottom() - 22.0f, 9.2f, 9.2f);
            }
        }
    }

    static juce::String nameFor (int i)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((i % 12) + 12) % 12]);
    }

private:
    int target = -1, key = 0;
    unsigned mask = 0xfffu;
    bool voiced = false;
};

// ---------------------------------------------------------------------------
//  Le grand cadran (Ø350) : arc, graduations, valeur au centre
// ---------------------------------------------------------------------------
class Gauge : public juce::Component
{
public:
    void setPalette (const Palette& p) { pal = p; repaint(); }

    void setData (float cents, bool isVoiced, int targetNote)
    {
        if (std::abs (cents - value) > 0.05f || isVoiced != voiced || targetNote != target)
        {
            value = cents; voiced = isVoiced; target = targetNote;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float d  = juce::jmin (b.getWidth(), b.getHeight());
        const float sw = d * 0.085f;
        const float R  = d * 0.5f - sw * 0.5f - 16.0f;
        const auto  c  = b.getCentre();

        // anneau pointillé « technique »
        juce::Path dots;
        dots.addCentredArc (c.x, c.y, R + sw * 0.5f + 12.0f, R + sw * 0.5f + 12.0f, 0.0f,
                            juce::degreesToRadians (-120.0f),
                            juce::degreesToRadians (240.0f), true);
        juce::PathStrokeType ps (1.4f);
        juce::Path dashed;
        const float dashLengths[2] = { 2.0f, 11.0f };
        ps.createDashedStroke (dashed, dots, dashLengths, 2);
        g.setColour (pal.accent.withAlpha (0.45f));
        g.fillPath (dashed);

        // ventre
        auto face = juce::Rectangle<float> (R - sw * 0.5f - 3.0f, R - sw * 0.5f - 3.0f);
        face = face.withSizeKeepingCentre (face.getWidth() * 2.0f, face.getHeight() * 2.0f).withCentre (c);
        juce::ColourGradient fg (pal.dialFace1, c.x, c.y - face.getHeight() * 0.4f,
                                 pal.dialFace2, c.x, c.y + face.getHeight() * 0.6f, false);
        g.setGradientFill (fg);
        g.fillEllipse (face);

        // piste
        juce::Path track;
        track.addCentredArc (c.x, c.y, R, R, 0.0f, juce::degreesToRadians (-120.0f),
                             juce::degreesToRadians (240.0f), true);
        g.setColour (pal.ringTrack);
        g.strokePath (track, juce::PathStrokeType (sw, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::butt));

        // arc de valeur : 0 cent = en haut, ±50 cents = ±120°
        const float clamped = juce::jlimit (-50.0f, 50.0f, value);
        const float angle   = clamped / 50.0f * 120.0f;
        if (std::abs (angle) > 0.4f)
        {
            juce::Path arc;
            const float from = juce::jmin (0.0f, angle);
            const float to   = juce::jmax (0.0f, angle);
            arc.addCentredArc (c.x, c.y, R, R, 0.0f,
                               juce::degreesToRadians (from - 90.0f),
                               juce::degreesToRadians (to - from), true);
            juce::ColourGradient grad (pal.accentLight, c.x - R, c.y - R,
                                       pal.accentDark,  c.x + R, c.y + R, false);
            g.setGradientFill (grad);
            g.strokePath (arc, juce::PathStrokeType (sw, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::butt));
        }

        // graduations
        for (int k = -50; k <= 50; k += 10)
        {
            const bool major = (k % 50) == 0;
            const float a = juce::degreesToRadians ((float) k / 50.0f * 120.0f - 90.0f);
            const float r1 = R - sw * 0.5f - 2.0f;
            const float r2 = r1 - (major ? 10.0f : 6.0f);
            g.setColour (major ? pal.ink : pal.inkFaint.withAlpha (0.75f));
            g.drawLine (c.x + std::cos (a) * r1, c.y + std::sin (a) * r1,
                        c.x + std::cos (a) * r2, c.y + std::sin (a) * r2, major ? 2.2f : 1.4f);
        }

        // valeur au centre
        g.setColour (pal.ink);
        const int  shown = (int) std::lround (value);
        g.setFont (font (d * 0.165f));
        g.drawText ((shown > 0 ? "+" : "") + juce::String (shown),
                    getLocalBounds().reduced (0, (int) (d * 0.06f)),
                    juce::Justification::centred, false);
        g.setFont (font (d * 0.068f));
        g.setColour (pal.inkSoft);
        g.drawText (juce::String::fromUTF8 ("\xc2\xa2"),
                    getLocalBounds().withTrimmedTop ((int) (d * 0.30f)),
                    juce::Justification::centredTop, false);
    }

private:
    Palette pal = lightPalette();
    float value = 0.0f;
    bool  voiced = false;
    int   target = -1;
};

// ---------------------------------------------------------------------------
//  Forme d'onde + courbe de hauteur
// ---------------------------------------------------------------------------
class Scope : public juce::Component
{
public:
    void setPalette (const Palette& p) { pal = p; repaint(); }

    void setData (const float* s, int n, const float* cts, int nc)
    {
        const int keep = juce::jmin (n, (int) samples.size());
        std::copy (s + (n - keep), s + n, samples.begin());
        const int keepC = juce::jmin (nc, (int) cents.size());
        std::copy (cts + (nc - keepC), cts + nc, cents.begin());
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (pal.dark ? pal.card2 : juce::Colour (0xfff7fbfe));
        g.fillRoundedRectangle (b, 9.0f);
        g.setColour (pal.cardEdge.withAlpha (0.8f));
        g.drawRoundedRectangle (b, 9.0f, 1.0f);

        // grille
        g.setColour (pal.dark ? juce::Colour (0xff20303c) : juce::Colour (0xffeaf3fa));
        for (int i = 1; i < 6; ++i)
            g.drawVerticalLine ((int) (b.getX() + b.getWidth() * i / 6.0f), b.getY() + 2, b.getBottom() - 2);
        for (int i = 1; i < 4; ++i)
            g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * i / 4.0f), b.getX() + 2, b.getRight() - 2);

        const float mid = b.getCentreY();
        const int   n   = (int) samples.size();

        // bande « zone juste »
        g.setColour (pal.accent.withAlpha (0.13f));
        g.fillRect (juce::Rectangle<float> (b.getX(), mid - b.getHeight() * 0.06f,
                                            b.getWidth(), b.getHeight() * 0.12f));

        // forme d'onde
        g.setColour (pal.accent.withAlpha (0.75f));
        const float stepX = b.getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float a = juce::jlimit (0.0f, 1.0f, std::abs (samples[(size_t) i])) * b.getHeight() * 0.44f;
            if (a < 0.7f) continue;
            g.drawVerticalLine ((int) (b.getX() + stepX * (float) i), mid - a, mid + a);
        }

        // courbe de hauteur (cents -> pixels)
        juce::Path curve;
        const int nc = (int) cents.size();
        bool started = false;
        for (int i = 0; i < nc; ++i)
        {
            const float x = b.getX() + b.getWidth() * (float) i / (float) juce::jmax (1, nc - 1);
            const float y = mid - juce::jlimit (-50.0f, 50.0f, cents[(size_t) i]) / 50.0f
                                  * b.getHeight() * 0.30f;
            if (! started) { curve.startNewSubPath (x, y); started = true; }
            else           curve.lineTo (x, y);
        }
        g.setColour (pal.accentDark);
        g.strokePath (curve, juce::PathStrokeType (2.4f,
                       juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    Palette pal = lightPalette();
    std::array<float, 512> samples {};
    std::array<float, 256> cents {};
};

// ---------------------------------------------------------------------------
//  Vu-mètre (Peak + RMS)
// ---------------------------------------------------------------------------
class LevelMeter : public juce::Component
{
public:
    void setPalette (const Palette& p) { pal = p; repaint(); }
    void setTitle (const juce::String& t) { title = t; repaint(); }

    void setLevels (float peakDb, float rmsDb)
    {
        peak = peakDb; rms = rmsDb;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        g.setColour (pal.inkSoft);
        g.setFont (font (11.0f, true));
        g.drawText (title.toUpperCase(), b.removeFromTop (16), juce::Justification::centred, false);

        auto foot = b.removeFromBottom (14);
        auto scaleArea = b.removeFromLeft (22);

        g.setFont (font (10.0f));
        g.setColour (pal.inkFaint);
        const char* labels[3] = { "0", "-12", "-24" };
        for (int i = 0; i < 3; ++i)
        {
            const int y = b.getY() + (int) (b.getHeight() * i / 2.0f);
            g.drawText (labels[i], scaleArea.getX(), y - 7, scaleArea.getWidth() - 3, 14,
                        juce::Justification::centredRight, false);
            juce::ignoreUnused (foot);
        }

        const int barW = 20, gap = 8;
        int x = b.getX() + 4;
        drawBar (g, juce::Rectangle<int> (x, b.getY(), barW, b.getHeight()), peak);
        x += barW + gap;
        drawBar (g, juce::Rectangle<int> (x, b.getY(), barW, b.getHeight()), rms);

        g.setColour (pal.inkFaint);
        g.setFont (font (10.0f));
        auto f = getLocalBounds().removeFromBottom (14);
        g.drawText ("PEAK", f.getX() + 22, f.getY(), 40, 14, juce::Justification::centredLeft, false);
        g.drawText ("RMS",  f.getX() + 22 + barW + gap, f.getY(), 40, 14, juce::Justification::centredLeft, false);
    }

private:
    void drawBar (juce::Graphics& g, juce::Rectangle<int> r, float db)
    {
        g.setColour (pal.meterTrack);
        g.fillRoundedRectangle (r.toFloat(), 5.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, (db + 24.0f) / 24.0f);
        if (norm > 0.01f)
        {
            auto fill = r.toFloat();
            fill = fill.withTop (fill.getBottom() - fill.getHeight() * norm);
            juce::ColourGradient grad (pal.barFill1, 0.0f, fill.getY(),
                                       pal.barFill2, 0.0f, fill.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill, 5.0f);
        }
    }

    Palette pal = lightPalette();
    juce::String title { "Input" };
    float peak = -100.0f, rms = -100.0f;
};

} // namespace ht
