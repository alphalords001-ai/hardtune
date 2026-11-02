//  HardTuneLookAndFeel.h — identité visuelle du plugin (sombre / néon).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ht
{
    namespace Colours
    {
        const juce::Colour background   { 0xff10131a };
        const juce::Colour panel        { 0xff171b24 };
        const juce::Colour panelLight   { 0xff1f2430 };
        const juce::Colour accent       { 0xff2ef2a3 };   // vert néon
        const juce::Colour accentDim    { 0xff1a8f61 };
        const juce::Colour accentWarm   { 0xffff5f6d };   // rouge correction
        const juce::Colour text         { 0xffe8edf5 };
        const juce::Colour textDim      { 0xff8b94a7 };
        const juce::Colour outline      { 0xff2a3140 };
    }

    inline juce::Font uiFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height).withStyle (bold ? "Bold" : "Regular"));
    }
}

class HardTuneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HardTuneLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardTuneLookAndFeel)
};
