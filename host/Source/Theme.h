#pragma once
// Farben und Schriften des Panels — übernommen aus design/panel-prototype.html
// (CSS-Variablen). Eigene Zuordnung, keine Original-Panelgrafik (F3).

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
    // Panel — immer dunkel. Ein Hardware-Frontpanel hat keinen Light Mode.
    const juce::Colour panel      { 0xff212429 };
    const juce::Colour raised     { 0xff2b2f36 };
    const juce::Colour rail       { 0xff282c33 };
    const juce::Colour sunk       { 0xff15171b };
    const juce::Colour edge       { 0xff3a3f47 };
    const juce::Colour silk       { 0xffe6e2d9 };
    const juce::Colour silkDim    { 0xff8b919c };
    const juce::Colour cap        { 0xffc8c4bb };
    const juce::Colour capEdge    { 0xff9a968e };
    const juce::Colour ink        { 0xff1a1c20 };
    const juce::Colour toneOn     { 0xffe8c05a };
    const juce::Colour focus      { 0xff6ea8ff };

    // LCD — grün hinterleuchtet, dunkle Zeichen
    const juce::Colour lcdLit     { 0xffa8c261 };
    const juce::Colour lcdLit2    { 0xff8da849 };
    const juce::Colour lcdInk     { 0xff161c0e };
    const juce::Colour lcdInkDim  { 0xff3f4a2c };

    // Sektionsfarben
    const juce::Colour wg     { 0xffd9a441 };
    const juce::Colour pitch  { 0xff5aa9d6 };
    const juce::Colour tvf    { 0xff6cc47a };
    const juce::Colour tva    { 0xffd4708f };
    const juce::Colour lfo    { 0xff9d86d9 };
    const juce::Colour ctrl   { 0xff7f8894 };
    const juce::Colour x990   { 0xffe0603f };
    const juce::Colour basics { 0xffc9cbcf };
    const juce::Colour fxA    { 0xffc98b4a };   // Effektgruppe A (Distortion … Enhancer)
    const juce::Colour fxB    { 0xff4fb3a9 };   // Effektgruppe B (Chorus, Delay, Reverb)

    inline juce::Colour accent (const juce::String& group)
    {
        if (group == "WG") return wg;
        if (group == "PITCH ENV") return pitch;
        if (group == "TVF" || group == "TVF ENV") return tvf;
        if (group == "TVA" || group == "TVA ENV") return tva;
        if (group == "LFO 1" || group == "LFO 2") return lfo;
        if (group == "990" || group == "VELOCITY" || group == "STRUCTURE") return x990;
        if (group == "BASICS" || group == "KEY FX") return basics;
        if (group == "GROUP A" || group == "DISTORTION" || group == "PHASER" || group == "SPECTRUM" || group == "ENHANCER") return fxA;
        if (group == "GROUP B" || group == "CHORUS" || group == "DELAY" || group == "REVERB") return fxB;
        if (group == "EQ" || group == "KEY RANGE") return tva.withSaturation (0.25f);
        return ctrl;   // TONE COMMON, CTRL 1/2, FX CTRL
    }

    inline juce::Font label (float h = 11.0f)  { return juce::Font (juce::FontOptions (h, juce::Font::bold)); }
    inline juce::Font mono (float h = 11.0f)   { return juce::Font (juce::FontOptions ("Menlo", h, juce::Font::plain)); }
    inline juce::Font monoBold (float h)       { return juce::Font (juce::FontOptions ("Menlo", h, juce::Font::bold)); }

    /** Look & Feel: dunkle Knöpfe/Listen wie im Prototyp. */
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::ResizableWindow::backgroundColourId, panel);
            setColour (juce::TextButton::buttonColourId, sunk);
            setColour (juce::TextButton::buttonOnColourId, silk);
            setColour (juce::TextButton::textColourOffId, silkDim);
            setColour (juce::TextButton::textColourOnId, ink);
            setColour (juce::ComboBox::backgroundColourId, sunk);
            setColour (juce::ComboBox::outlineColourId, edge);
            setColour (juce::ComboBox::textColourId, silk);
            setColour (juce::ComboBox::arrowColourId, silkDim);
            setColour (juce::PopupMenu::backgroundColourId, raised);
            setColour (juce::PopupMenu::textColourId, silk);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, edge);
            setColour (juce::Label::textColourId, silk);
            setColour (juce::TabbedButtonBar::tabOutlineColourId, edge);
            setColour (juce::TabbedButtonBar::frontOutlineColourId, edge);
            setColour (juce::TabbedComponent::backgroundColourId, panel);
            setColour (juce::TabbedComponent::outlineColourId, edge);
            setColour (juce::ToggleButton::textColourId, silkDim);
            setColour (juce::ToggleButton::tickColourId, toneOn);
            setColour (juce::TextEditor::backgroundColourId, sunk);
            setColour (juce::TextEditor::outlineColourId, edge);
            setColour (juce::TextEditor::textColourId, silk);
            setColour (juce::ListBox::backgroundColourId, panel);
            setColour (juce::ScrollBar::thumbColourId, edge);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int h) override { return label (juce::jmin (12.0f, (float) h * 0.55f)); }
        juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (11.0f); }
        juce::Font getPopupMenuFont() override { return mono (12.0f); }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : bg;
            if (down) c = c.brighter (0.1f);
            g.setColour (c);
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (over ? juce::Colour (0xff525862) : edge);
            g.drawRoundedRectangle (r, 2.0f, 1.0f);
        }

        void drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool) override
        {
            auto r = b.getLocalBounds().toFloat();
            bool front = b.isFrontTab();
            g.setColour (front ? raised : panel);
            g.fillRect (r);
            if (front) { g.setColour (toneOn); g.fillRect (r.removeFromBottom (2.0f)); }
            g.setColour (front ? silk : (over ? silk : silkDim));
            g.setFont (label (12.0f));
            g.drawText (b.getButtonText().toUpperCase(), b.getLocalBounds(), juce::Justification::centred);
        }
    };
}
