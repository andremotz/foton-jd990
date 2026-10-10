#include "Controls.h"

// ── Fader ────────────────────────────────────────────────────────────────────

Fader::Fader()
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

juce::Rectangle<float> Fader::trackArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (16).withTrimmedBottom (28).reduced (0, 4);
}

void Fader::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    // Wert
    g.setColour (theme::silk);
    g.setFont (theme::mono (10.5f));
    g.drawFittedText (shown, getLocalBounds().removeFromTop (14), juce::Justification::centred, 1, 0.7f);

    // Schiene
    auto t = trackArea();
    auto rail = juce::Rectangle<float> (t.getCentreX() - 3.5f, t.getY(), 7.0f, t.getHeight());
    g.setColour (theme::sunk);
    g.fillRoundedRectangle (rail, 1.0f);
    g.setColour (juce::Colour (0xff0c0e11));
    g.drawRoundedRectangle (rail, 1.0f, 1.0f);

    float frac = hi > lo ? (float) (value - lo) / (float) (hi - lo) : 0.0f;
    frac = juce::jlimit (0.0f, 1.0f, frac);
    float capY = t.getBottom() - frac * t.getHeight();

    // Füllung in Sektionsfarbe
    g.setColour (accent.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float> (t.getCentreX() - 1.5f, capY, 3.0f, t.getBottom() - capY));

    // Kappe
    auto capR = juce::Rectangle<float> (t.getCentreX() - 13.0f, capY - 5.5f, 26.0f, 11.0f);
    g.setColour (theme::cap);
    g.fillRoundedRectangle (capR.withHeight (5.5f), 2.0f);
    g.setColour (theme::capEdge);
    g.fillRoundedRectangle (capR.withTrimmedTop (5.0f), 2.0f);
    g.setColour (juce::Colour (0xff6e6a63));
    g.drawRoundedRectangle (capR, 2.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawHorizontalLine ((int) capY, capR.getX() + 3, capR.getRight() - 3);

    if (hasKeyboardFocus (false))
    {
        g.setColour (theme::focus);
        g.drawRoundedRectangle (b.reduced (1), 3.0f, 1.5f);
    }

    // Beschriftung
    g.setColour (theme::silkDim);
    g.setFont (theme::label (9.5f));
    g.drawFittedText (name.toUpperCase(), getLocalBounds().removeFromBottom (26), juce::Justification::centredTop, 2, 0.8f);
}

void Fader::commit (int v)
{
    v = juce::jlimit (lo, hi, v);
    if (v != value && onChange) onChange (v);
}

void Fader::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (onTouch) onTouch();
    dragStartValue = value;
    dragStartY = (float) e.getMouseDownY();
}

void Fader::mouseDrag (const juce::MouseEvent& e)
{
    float range = trackArea().getHeight();
    float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
    float dv = (dragStartY - (float) e.y) / range * (float) (hi - lo) * fine;
    commit (dragStartValue + juce::roundToInt (dv));
}

void Fader::mouseDoubleClick (const juce::MouseEvent&) { commit (initValue); }

void Fader::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (onTouch) onTouch();
    if (std::abs (w.deltaY) > 0.0f) commit (value + (w.deltaY > 0 ? 1 : -1));
}

bool Fader::keyPressed (const juce::KeyPress& k)
{
    int step = k.getModifiers().isShiftDown() ? 10 : 1;
    int c = k.getKeyCode();
    if (c == juce::KeyPress::upKey)   { commit (value + step); return true; }
    if (c == juce::KeyPress::downKey) { commit (value - step); return true; }
    if (c == juce::KeyPress::homeKey) { commit (lo); return true; }
    if (c == juce::KeyPress::endKey)  { commit (hi); return true; }
    return false;
}

// ── ParamControl ─────────────────────────────────────────────────────────────

ParamControl::ParamControl (Model& m, const juce::String& id, int fixedTone, bool compactMode)
    : model (m), param (m.find (id)), fixed (fixedTone), compact (compactMode)
{
    if (param == nullptr) return;
    caption = param->label;
    auto& info = param->info;
    isMask = param->id == "patch.layer_tone" || param->id == "patch.active_tone";
    isWave = info.kind == 5;
    isChoice = ! isMask && (info.kind == 1 || info.kind == 5 || (info.kind == 2 && info.max - info.min < 48));
    if (isMask)
    {
        // Bitmaske A=1 B=2 C=4 D=8 (Enum „OFF, A, B, AB …“) — wie die Tasten am Gerät
        bool layer = param->id == "patch.layer_tone";
        caption = layer ? juce::String::fromUTF8 ("Tone an (Layer)") : juce::String::fromUTF8 ("Edit-Auswahl (Active)");
        for (int t = 0; t < 4; ++t)
        {
            auto& b = toneSw[t];
            b.setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + t)));
            b.setClickingTogglesState (false);
            b.setColour (juce::TextButton::buttonOnColourId, layer ? theme::toneOn : theme::basics);
            b.setTooltip (layer ? juce::String::fromUTF8 ("Tone ein/aus — am Gerät: TONE SWITCH")
                                : juce::String::fromUTF8 ("Tone für die Bearbeitung am Gerät gewählt — am Gerät: TONE SELECT (für den Klang egal)"));
            b.onClick = [this, t] {
                model.lastIndex = (int) param->index;
                model.set (param->index, -1, model.get (param->index, -1) ^ (1 << t));
            };
            addAndMakeVisible (b);
        }
    }
    else if (isChoice)
    {
        char buf[64];
        for (int v = info.min; v <= info.max; ++v)
        {
            jd990_param_format (param->index, (uint16_t) v, buf, sizeof buf);
            auto text = juce::String::fromUTF8 (buf);
            box.addItem (info.kind == 5 ? juce::String (v + 1).paddedLeft ('0', 3) + "  " + text.fromFirstOccurrenceOf (" ", false, false) : text, v + 1);
            if (jd990_param_locked (param->index, (uint16_t) v) == 1)
                box.setItemEnabled (v + 1, false);   // out of scope — sichtbar, nicht wählbar (R7)
        }
        box.onChange = [this] {
            model.lastIndex = (int) param->index;
            model.set (param->index, toneNow(), box.getSelectedId() - 1);
        };
        addAndMakeVisible (box);
        if (isWave)
        {
            box.setScrollWheelEnabled (true);
            box.setTooltip (juce::String::fromUTF8 ("Wellenform · Mausrad oder ↑↓ blättern"));
            up.setTooltip (juce::String::fromUTF8 ("Nächste Wellenform (+1) · ⇧ = +10"));
            down.setTooltip (juce::String::fromUTF8 ("Vorige Wellenform (−1) · ⇧ = −10"));
            up.onClick = [this] { step (juce::ModifierKeys::currentModifiers.isShiftDown() ? 10 : 1); };
            down.onClick = [this] { step (juce::ModifierKeys::currentModifiers.isShiftDown() ? -10 : -1); };
            up.setRepeatSpeed (400, 70);        // gedrückt halten = weiterblättern
            down.setRepeatSpeed (400, 70);
            addAndMakeVisible (up);
            addAndMakeVisible (down);
        }
    }
    else
    {
        fader.setRange (info.min, info.max, jd990_param_init (param->index));
        fader.setAccent (theme::accent (param->group));
        fader.onChange = [this] (int v) { model.set (param->index, toneNow(), v); };
        fader.onTouch = [this] { model.lastIndex = (int) param->index; model.lastTone = toneNow(); model.sendChangeMessage(); };
        addAndMakeVisible (fader);
    }
    model.addChangeListener (this);
    update();
}

ParamControl::~ParamControl() { model.removeChangeListener (this); }

int ParamControl::toneNow() const
{
    if (param == nullptr || ! param->isTone()) return -1;
    return fixed >= 0 ? fixed : model.tone;
}

void ParamControl::step (int delta)
{
    auto& info = param->info;
    int v = model.get (param->index, toneNow()), dir = delta > 0 ? 1 : -1;
    int target = juce::jlimit ((int) info.min, (int) info.max, v + delta);
    while (target != v && jd990_param_locked (param->index, (uint16_t) target) == 1)   // gesperrte überspringen
        target -= dir;
    if (target == v) return;
    model.lastIndex = (int) param->index;
    model.set (param->index, toneNow(), target);
}

int ParamControl::preferredWidth() const
{
    if (isMask) return 186;
    if (! isChoice) return Fader::width;
    if (isWave) return 176 + 44;
    return compact ? 92 : 96;
}

int ParamControl::preferredHeight() const { return isChoice || isMask ? 48 : Fader::height; }

void ParamControl::resized()
{
    if (isMask)
    {
        auto row = getLocalBounds().withTrimmedTop (12).withHeight (24);
        int w = (row.getWidth() - 3 * 4) / 4;
        for (auto& b : toneSw) { b.setBounds (row.removeFromLeft (w)); row.removeFromLeft (4); }
    }
    else if (isChoice)
    {
        auto row = getLocalBounds().withTrimmedTop (14).withHeight (22).reduced (1, 0);
        if (isWave)
        {
            down.setBounds (row.removeFromRight (20));
            row.removeFromRight (2);
            up.setBounds (row.removeFromRight (20));
            row.removeFromRight (2);
        }
        box.setBounds (row);
    }
    else
        fader.setBounds (getLocalBounds());
}

void ParamControl::paint (juce::Graphics& g)
{
    if (isMask)
    {
        g.setColour (theme::silkDim);
        g.setFont (theme::label (9.5f));
        g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (11), juce::Justification::centredLeft);
        return;
    }
    if (! isChoice) return;
    g.setColour (theme::silkDim);
    g.setFont (theme::label (9.5f));
    g.drawFittedText (caption.toUpperCase(), getLocalBounds().removeFromBottom (12), juce::Justification::centred, 1, 0.8f);
}

void ParamControl::update()
{
    if (param == nullptr) return;
    int t = toneNow();
    int v = model.get (param->index, t);
    setEnabled (model.hasPatch());
    setAlpha (model.hasPatch() ? 1.0f : 0.4f);
    if (isMask)
        for (int b = 0; b < 4; ++b) toneSw[b].setToggleState (model.hasPatch() && (v >> b) & 1, juce::dontSendNotification);
    else if (isChoice)
        box.setSelectedId (v + 1, juce::dontSendNotification);
    else
    {
        fader.setValue (v);
        fader.setText (model.hasPatch() ? model.display (param->index, t) : juce::String(), caption);
    }
}
