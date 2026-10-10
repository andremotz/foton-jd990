#pragma once
// Bedienelemente des Panels (M4) — gezeichnet nach design/panel-prototype.html.
//  * Fader: vertikal, Wert oben, Beschriftung unten. Ziehen (⇧ = fein), Mausrad,
//    ↑↓ (⇧ ×10), Pos1/Ende, Doppelklick = Init-Wert.
//  * ParamControl: wählt je Parameterart Fader oder Auswahlliste und hält sich
//    über den Model-ChangeBroadcaster aktuell.

#include "Model.h"
#include "Theme.h"

/** Liegender Slider mit Tastatur: ↑/→ +1, ↓/← −1, mit ⇧ ×10 (Liste „Alle Parameter“). */
class KeySlider : public juce::Slider
{
public:
    KeySlider() { setWantsKeyboardFocus (true); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        int step = k.getModifiers().isShiftDown() ? 10 : 1;
        int code = k.getKeyCode();
        if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey) { setValue (getValue() + step); return true; }
        if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey) { setValue (getValue() - step); return true; }
        return false;
    }
};

class Fader : public juce::Component
{
public:
    Fader();
    void setRange (int min, int max, int init) { lo = min; hi = max; initValue = init; }
    void setValue (int v) { value = v; repaint(); }
    int getValue() const { return value; }
    void setText (const juce::String& valueText, const juce::String& label) { shown = valueText; name = label; repaint(); }
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    std::function<void (int)> onChange;     // neuer Rohwert (Eingabe)
    std::function<void()> onTouch;          // Berührung (LCD)

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

    static constexpr int width = 44, height = 150;

private:
    void commit (int v);
    juce::Rectangle<float> trackArea() const;
    int lo = 0, hi = 127, initValue = 0, value = 0;
    int dragStartValue = 0;
    float dragStartY = 0;
    juce::String shown, name;
    juce::Colour accent = theme::ctrl;
};

/** Kleiner Pfeilknopf (▲/▼) — gezeichnet statt Text, damit er auch 20 px schmal lesbar bleibt. */
class StepButton : public juce::Button
{
public:
    explicit StepButton (bool upwards) : juce::Button (upwards ? "up" : "down"), isUp (upwards) {}
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? theme::edge : theme::sunk);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (over ? juce::Colour (0xff525862) : theme::edge);
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
        auto c = r.getCentre();
        juce::Path tri;
        float w = 4.5f, h = 4.0f, dir = isUp ? -1.0f : 1.0f;
        tri.addTriangle (c.x - w, c.y - dir * h * 0.5f, c.x + w, c.y - dir * h * 0.5f, c.x, c.y + dir * h * 0.5f);
        g.setColour (isEnabled() ? theme::silk : theme::silkDim);
        g.fillPath (tri);
    }
private:
    bool isUp;
};

/** Schloss im Gruppenkopf: Gruppe für Variation/Morph sperren. */
class LockButton : public juce::Button, private juce::ChangeListener
{
public:
    LockButton (Model& m, const juce::String& g) : juce::Button ("lock"), model (m), group (g)
    {
        setTooltip (juce::String::fromUTF8 ("Gruppe sperren: Variation und Morph lassen sie unverändert"));
        onClick = [this] { model.toggleLock (group); };
        model.addChangeListener (this);
    }
    ~LockButton() override { model.removeChangeListener (this); }
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        bool on = model.isLocked (group);
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (on ? theme::toneOn : theme::sunk.withAlpha (over ? 0.9f : 0.55f));
        g.fillRoundedRectangle (r, 2.0f);
        // Vorhängeschloss: Bügel + Körper (offen, wenn nicht gesperrt)
        auto c = r.getCentre();
        juce::Colour ink = on ? theme::ink : (over ? theme::silk : theme::silkDim);
        g.setColour (ink);
        g.fillRoundedRectangle (c.x - 4.5f, c.y - 0.5f, 9.0f, 6.5f, 1.2f);
        juce::Path shackle;
        float sx = on ? c.x : c.x + 2.5f;
        shackle.addCentredArc (sx, c.y - 1.5f, 3.0f, 3.2f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.strokePath (shackle, juce::PathStrokeType (1.4f));
    }
private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    Model& model;
    juce::String group;
};

class ParamControl : public juce::Component, private juce::ChangeListener
{
public:
    /** `fixedTone`: −2 = aktueller Tone des Modells (Tone-Parameter), −1 = Patch-Parameter,
        0–3 = fester Tone (z. B. Key-Range-Zeile). */
    ParamControl (Model&, const juce::String& id, int fixedTone = -2, bool compact = false);
    ~ParamControl() override;
    int preferredWidth() const;
    int preferredHeight() const;
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
    bool valid() const { return param != nullptr; }
    const Model::Param* getParam() const { return param; }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { update(); }
    int toneNow() const;
    Model& model;
    const Model::Param* param = nullptr;
    int fixed;
    bool compact;
    bool isChoice = false;
    bool isMask = false;                   // Tone-Maske (Layer/Active): vier Schalter A–D statt Liste
    bool isWave = false;                   // Wellenform: ▲▼ zum schnellen Durchblättern
    Fader fader;
    juce::ComboBox box;
    StepButton up { true }, down { false };
    juce::TextButton toneSw[4];
    void step (int delta);
    juce::String caption;
};
