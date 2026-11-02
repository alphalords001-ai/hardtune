//  PitchDisplay.cpp
#include "PitchDisplay.h"

namespace ht
{

juce::String PitchDisplay::noteName (int midi)
{
    if (midi < 0) return "--";
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                     "F#", "G", "G#", "A", "A#", "B" };
    const int pc  = ((midi % 12) + 12) % 12;
    const int oct = midi / 12 - 1;
    return juce::String (names[pc]) + juce::String (oct);
}

void PitchDisplay::setState (float newHz, float newMidiFloat, int newTarget,
                             float newCents, bool newVoiced)
{
    if (juce::approximatelyEqual (hz, newHz)
        && target == newTarget
        && juce::approximatelyEqual (cents, newCents)
        && voiced == newVoiced)
        return;

    hz        = newHz;
    midiFloat = newMidiFloat;
    target    = newTarget;
    cents     = newCents;
    voiced    = newVoiced;
    repaint();
}

void PitchDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    g.setColour (Colours::panel);
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (area, 8.0f, 1.0f);

    auto r = getLocalBounds().reduced (14);

    // ---- en-tête -----------------------------------------------------------
    g.setColour (Colours::textDim);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("DETECTED", r.removeFromTop (16), juce::Justification::centredLeft, false);

    // ---- note détectée ------------------------------------------------------
    auto noteRow = r.removeFromTop (72);

    const int detectedNote = voiced ? (int) std::lround (midiFloat) : -1;

    g.setColour (voiced ? Colours::text : Colours::textDim.withAlpha (0.5f));
    g.setFont (uiFont (46.0f, true));
    g.drawText (voiced ? noteName (detectedNote) : "--", noteRow,
                juce::Justification::centredLeft, false);

    // note cible
    g.setFont (uiFont (12.0f, true));
    g.setColour (Colours::textDim);
    g.drawText ("TARGET", noteRow.removeFromRight (72).removeFromTop (18),
                juce::Justification::centredRight, false);

    g.setFont (uiFont (26.0f, true));
    g.setColour (voiced && target >= 0 ? Colours::accent : Colours::textDim.withAlpha (0.5f));
    g.drawText (target >= 0 ? noteName (target) : "--",
                noteRow.withTrimmedTop (14), juce::Justification::centredRight, false);

    // ---- aiguille de justesse (cents) --------------------------------------
    auto meter = r.removeFromTop (54).toFloat();
    const auto track = meter.reduced (2.0f, 0.0f).withSizeKeepingCentre (meter.getWidth() - 4.0f, 10.0f);

    g.setColour (Colours::panelLight);
    g.fillRoundedRectangle (track, 5.0f);

    // graduations
    for (int i = -2; i <= 2; ++i)
    {
        const float tx = track.getCentreX() + (float) i * track.getWidth() * 0.25f;
        g.setColour (i == 0 ? Colours::textDim : Colours::outline);
        g.fillRect (juce::Rectangle<float> (tx - 0.5f, track.getY() - 5.0f, 1.0f, 6.0f));
    }

    // zone "juste"
    g.setColour (Colours::accent.withAlpha (0.25f));
    g.fillRoundedRectangle (track.withSizeKeepingCentre (track.getWidth() * 0.10f, track.getHeight()), 5.0f);

    if (voiced)
    {
        const float clamped = juce::jlimit (-50.0f, 50.0f, cents);
        const float px = track.getCentreX() + (clamped / 50.0f) * (track.getWidth() * 0.5f - 6.0f);

        g.setColour (std::abs (clamped) < 5.0f ? Colours::accent : Colours::accentWarm);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f)
                           .withCentre ({ px, track.getCentreY() }));
    }

    g.setColour (Colours::textDim);
    g.setFont (uiFont (10.0f));
    g.drawText ("-50", meter.withTrimmedTop (18).removeFromLeft (26).toNearestInt(),
                juce::Justification::centredLeft, false);
    g.drawText ("+50", meter.withTrimmedTop (18).removeFromRight (26).toNearestInt(),
                juce::Justification::centredRight, false);

    // ---- valeur texte -------------------------------------------------------
    auto bottom = r.removeFromTop (46).toFloat();

    g.setColour (Colours::textDim);
    g.setFont (uiFont (10.0f, true));
    g.drawText ("Hz", bottom.removeFromTop (14).toNearestInt(), juce::Justification::centredLeft, false);

    g.setColour (voiced ? Colours::text : Colours::textDim.withAlpha (0.5f));
    g.setFont (uiFont (19.0f, true));
    g.drawText (voiced ? juce::String (hz, 1) : "--",
                bottom.removeFromTop (26).toNearestInt(), juce::Justification::centredLeft, false);

    if (voiced)
    {
        g.setColour (Colours::textDim);
        g.setFont (uiFont (13.0f));
        g.drawText (juce::String (cents, 0) + " cents",
                    bottom.withHeight (26.0f).toNearestInt(), juce::Justification::centredRight, false);
    }

    // ---- témoin de voisement ----------------------------------------------
    const auto led = juce::Rectangle<float> (getWidth() - 22.0f, 12.0f, 8.0f, 8.0f);
    g.setColour (voiced ? Colours::accent : Colours::outline);
    g.fillEllipse (led);
}

} // namespace ht
