//  HardTuneLookAndFeel.cpp
#include "HardTuneLookAndFeel.h"

using namespace juce;

HardTuneLookAndFeel::HardTuneLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, ht::Colours::background);
    setColour (Slider::rotarySliderFillColourId,    ht::Colours::accent);
    setColour (Slider::rotarySliderOutlineColourId, ht::Colours::outline);
    setColour (Slider::thumbColourId,               ht::Colours::accent);
    setColour (Slider::textBoxTextColourId,         ht::Colours::text);
    setColour (Slider::textBoxOutlineColourId,      Colours::transparentBlack);
    setColour (Label::textColourId,                 ht::Colours::text);
    setColour (ComboBox::backgroundColourId,        ht::Colours::panelLight);
    setColour (ComboBox::textColourId,              ht::Colours::text);
    setColour (ComboBox::outlineColourId,           ht::Colours::outline);
    setColour (ComboBox::arrowColourId,             ht::Colours::accent);
    setColour (PopupMenu::backgroundColourId,       ht::Colours::panel);
    setColour (PopupMenu::textColourId,             ht::Colours::text);
    setColour (PopupMenu::highlightedBackgroundColourId, ht::Colours::accentDim);
    setColour (PopupMenu::highlightedTextColourId,  Colours::white);
    setColour (TextButton::buttonColourId,          ht::Colours::panelLight);
    setColour (TextButton::textColourOffId,         ht::Colours::text);
    setColour (TextButton::textColourOnId,          ht::Colours::background);
}

void HardTuneLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle,
                                            float rotaryEndAngle, Slider& slider)
{
    const auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto lineW  = jmax (3.0f, radius * 0.11f);

    // piste
    {
        Path p;
        p.addCentredArc (centre.x, centre.y, radius - lineW * 0.6f, radius - lineW * 0.6f,
                         0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (ht::Colours::outline);
        g.strokePath (p, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // valeur
    {
        Path p;
        p.addCentredArc (centre.x, centre.y, radius - lineW * 0.6f, radius - lineW * 0.6f,
                         0.0f, rotaryStartAngle, angle, true);
        g.setColour (slider.isEnabled() ? ht::Colours::accent : ht::Colours::textDim);
        g.strokePath (p, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // corps du bouton
    const auto bodyR = radius - lineW * 2.1f;
    if (bodyR > 2.0f)
    {
        const auto body = Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);

        ColourGradient grad (ht::Colours::panelLight.brighter (0.10f), body.getTopLeft(),
                             ht::Colours::panel, body.getBottomRight(), false);
        g.setGradientFill (grad);
        g.fillEllipse (body);

        g.setColour (ht::Colours::outline);
        g.drawEllipse (body, 1.0f);

        // index
        Path pointer;
        pointer.addRoundedRectangle (-lineW * 0.28f, -bodyR + lineW * 0.6f,
                                     lineW * 0.56f, bodyR * 0.42f, lineW * 0.28f);
        g.setColour (ht::Colours::accent);
        g.fillPath (pointer, AffineTransform::rotation (angle).translated (centre));
    }
}

void HardTuneLookAndFeel::drawComboBox (Graphics& g, int width, int height, bool,
                                        int, int, int, int, ComboBox& box)
{
    const auto bounds = Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (ht::Colours::panelLight);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (box.isEnabled() ? ht::Colours::outline : ht::Colours::outline.darker (0.4f));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

    // flèche
    Path arrow;
    const auto ax = (float) width - 14.0f;
    const auto ay = (float) height * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 3.0f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (ht::Colours::accent);
    g.strokePath (arrow, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

void HardTuneLookAndFeel::drawButtonBackground (Graphics& g, Button& button,
                                                const Colour&, bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    Colour base = on ? ht::Colours::accent : ht::Colours::panelLight;
    if (down)        base = base.darker (0.18f);
    else if (highlighted) base = base.brighter (0.10f);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, 5.0f);

    g.setColour (on ? ht::Colours::accent.brighter (0.2f) : ht::Colours::outline);
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
}

void HardTuneLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& button,
                                            bool highlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    g.setColour (on ? ht::Colours::accent
                    : (highlighted ? ht::Colours::panelLight.brighter (0.12f)
                                   : ht::Colours::panelLight));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (on ? ht::Colours::accent : ht::Colours::outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    g.setColour (on ? ht::Colours::background : ht::Colours::text);
    g.setFont (ht::uiFont (jmin (15.0f, bounds.getHeight() * 0.55f), true));
    g.drawText (button.getButtonText(), button.getLocalBounds(), Justification::centred, false);
}

void HardTuneLookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area,
                                             bool isSeparator, bool isActive, bool isHighlighted,
                                             bool isTicked, bool hasSubMenu, const String& text,
                                             const String& shortcutKeyText,
                                             const Drawable* icon, const Colour*)
{
    if (isSeparator)
    {
        auto r = area.reduced (5, 0);
        g.setColour (ht::Colours::outline);
        g.fillRect (r.removeFromTop (1));
        return;
    }

    const auto r = area.reduced (8, 0);

    if (isHighlighted && isActive)
    {
        g.setColour (ht::Colours::accentDim);
        g.fillRoundedRectangle (area.toFloat().reduced (2.0f), 3.0f);
    }

    g.setColour (isActive ? (isHighlighted ? Colours::white : ht::Colours::text)
                          : ht::Colours::textDim);
    g.setFont (ht::uiFont (14.0f, isHighlighted));
    g.drawFittedText (text, r, Justification::centredLeft, 1);

    if (isTicked)
    {
        g.setColour (ht::Colours::accent);
        g.setFont (ht::uiFont (14.0f, true));
        g.drawFittedText (String::charToString ((juce_wchar) 0x2713), r, Justification::centredRight, 1);
    }

    juce::ignoreUnused (hasSubMenu, shortcutKeyText, icon);
}

Font HardTuneLookAndFeel::getComboBoxFont (ComboBox&)       { return ht::uiFont (14.0f); }
Font HardTuneLookAndFeel::getPopupMenuFont()                { return ht::uiFont (14.0f); }
Font HardTuneLookAndFeel::getLabelFont (Label& label)       { return label.getFont(); }
