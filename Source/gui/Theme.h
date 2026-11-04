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
    // Une hauteur nulle (composant pas encore dimensionné) ferait planter
    // le moteur de texte : on la borne toujours.
    const float h = juce::jlimit (1.0f, 400.0f, height);
    //  Segoe UI : la seule police presente partout sous Windows, et celle de la
    //  reference. Sans nom explicite, JUCE prend sa police par defaut, plus
    //  large, et les libelles debordent de leurs cases.
    return juce::Font (juce::FontOptions ("Segoe UI", h,
                                          bold ? juce::Font::bold : juce::Font::plain));
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
    juce::Colour index;                      // index du bouton
    bool dark = false;
};

inline Palette lightPalette()
{
    Palette p;
    // valeurs mesurées sur l'image de référence
    p.win1        = juce::Colour (0xffc9e8f2);
    p.win2        = juce::Colour (0xff84b0ce);
    p.card1       = juce::Colour (0xfff0fbfb);
    p.card2       = juce::Colour (0xffeaf4f8);
    p.cardEdge    = juce::Colour (0xff96c3cd);
    p.ink         = juce::Colour (0xff0f1b23);
    p.inkSoft     = juce::Colour (0xff33505e);
    p.inkFaint    = juce::Colour (0xff8fa3ae);
    p.accent      = juce::Colour (0xff2fa6d4);
    p.accentDark  = juce::Colour (0xff14618f);
    p.accentLight = juce::Colour (0xff8fe6f2);
    p.ringTrack   = juce::Colour (0xffd5dee5);
    p.dialFace1   = juce::Colour (0xfff0fbfb);
    p.dialFace2   = juce::Colour (0xffeaf4f8);
    p.knobFace1   = juce::Colour (0xff39444c);
    p.knobFace2   = juce::Colour (0xff232c33);
    p.knobEdge    = juce::Colour (0xff141b20);
    p.barTrack    = juce::Colour (0xffdde5ea);
    p.barFill1    = juce::Colour (0xff3fb3e0);
    p.barFill2    = juce::Colour (0xff14618f);
    p.meterTrack  = juce::Colour (0xffc9ced3);
    p.glow        = juce::Colour (0xff2fa6d4).withAlpha (0.35f);
    p.index       = juce::Colour (0xff6ee0f5);
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
    p.index       = juce::Colour (0xff6ee0f5);
    p.dark        = true;
    return p;
}

/** Palette du clavier : elle ne change JAMAIS. */
inline Palette keyboardPalette() { return lightPalette(); }

} // namespace ht
