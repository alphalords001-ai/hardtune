//  PitchDisplay.h — écran de contrôle : note détectée, écart en cents, note cible.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "HardTuneLookAndFeel.h"

namespace ht
{

class PitchDisplay : public juce::Component
{
public:
    PitchDisplay() = default;

    void setState (float hz, float midiFloat, int targetNote, float cents, bool voiced);

    void paint (juce::Graphics&) override;

private:
    float hz        = 0.0f;
    float midiFloat = 0.0f;
    int   target    = -1;
    float cents     = 0.0f;
    bool  voiced    = false;

    static juce::String noteName (int midi);
};

} // namespace ht
