//  Theme.h — les deux palettes du plugin (Light / Dark).
//
//  Règle absolue : le CLAVIER (le « piano ») garde TOUJOURS la palette claire,
//  même quand le mode sombre est actif. C'est la seule exception.
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace ht
{

/** Police unique de l'interface (type Gill Sans / sans-serif, jamais grasse). */
inline juce::Font font (float height, bool bold = false)
{
    // Une hauteur <= 0 fait renvoyer un "typeface" nul par JUCE, et dessiner du
    // texte avec un typeface nul est un dereferencement de pointeur nul
    // ("Read of address 0x0") : on borne toujours.
    return juce::Font (juce::FontOptions (juce::jlimit (5.0f, 400.0f, height))
                           .withStyle (bold ? "Bold" : "Regular"));
}

struct Palette
{
    juce::Colour win1, win2;                 // fond de la fenêtre
    juce::Colour card1, card2, cardEdge;     // cartes
    juce::Colour ink, inkSoft, inkFaint;     // textes
    juce::Colour accent, accentDark, accentLight;
    juce::Colour ringTrack, dialFace1, dialFace2;
    juce::Colour knobFace1, knobFace2, knobEdge;
    juce::Colour barTrack, barFill1, barFill2;
    juce::Colour meterTrack;
    juce::Colour glow;                       // halo du grand cadran
    bool dark = false;
};

inline Palette lightPalette()
{
    Palette p;
    p.win1        = juce::Colour (0xfff7fcff);
    p.win2        = juce::Colour (0xffdcebf7);
    p.card1       = juce::Colour (0xffffffff);
    p.card2       = juce::Colour (0xfff2f9ff);
    p.cardEdge    = juce::Colour (0xffd3e5f2);
    p.ink         = juce::Colour (0xff2b4a63);
    p.inkSoft     = juce::Colour (0xff7d9bb0);
    p.inkFaint    = juce::Colour (0xffa8c2d4);
    p.accent      = juce::Colour (0xff3f9ed8);
    p.accentDark  = juce::Colour (0xff2f7fb8);
    p.accentLight = juce::Colour (0xff8fd0f0);
    p.ringTrack   = juce::Colour (0xffc2dcee);
    p.dialFace1   = juce::Colour (0xfffdfeff);
    p.dialFace2   = juce::Colour (0xffd5e9f8);
    p.knobFace1   = juce::Colour (0xff5e6975);
    p.knobFace2   = juce::Colour (0xff3b454e);
    p.knobEdge    = juce::Colour (0xff2c353d);
    p.barTrack    = juce::Colour (0xffdcecf8);
    p.barFill1    = juce::Colour (0xff8fd8f8);
    p.barFill2    = juce::Colour (0xff2f9bd8);
    p.meterTrack  = juce::Colour (0xffe7f0f7);
    p.glow        = juce::Colour (0xff3f9ed8).withAlpha (0.30f);
    p.dark        = false;
    return p;
}

inline Palette darkPalette()
{
    Palette p;
    p.win1        = juce::Colour (0xff13202b);
    p.win2        = juce::Colour (0xff0a1219);
    p.card1       = juce::Colour (0xff1a2733);
    p.card2       = juce::Colour (0xff131e28);
    p.cardEdge    = juce::Colour (0xff24394a);
    p.ink         = juce::Colour (0xffdbeaf5);
    p.inkSoft     = juce::Colour (0xff82a2b8);
    p.inkFaint    = juce::Colour (0xff54707f);
    p.accent      = juce::Colour (0xff54b2e8);
    p.accentDark  = juce::Colour (0xff2f8dc9);
    p.accentLight = juce::Colour (0xff8fd0f0);
    p.ringTrack   = juce::Colour (0xff2b4050);
    p.dialFace1   = juce::Colour (0xff22303c);
    p.dialFace2   = juce::Colour (0xff111a22);
    p.knobFace1   = juce::Colour (0xff4a5862);
    p.knobFace2   = juce::Colour (0xff2b343c);
    p.knobEdge    = juce::Colour (0xff6f8fa5);
    p.barTrack    = juce::Colour (0xff26353f);
    p.barFill1    = juce::Colour (0xff8fd8f8);
    p.barFill2    = juce::Colour (0xff2f9bd8);
    p.meterTrack  = juce::Colour (0xff22303c);
    p.glow        = juce::Colour (0xff54b2e8).withAlpha (0.45f);
    p.dark        = true;
    return p;
}

/** Palette du clavier : elle ne change JAMAIS. */
inline Palette keyboardPalette() { return lightPalette(); }

} // namespace ht
