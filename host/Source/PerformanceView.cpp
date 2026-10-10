#include "PerformanceView.h"

namespace
{
    juce::String u (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour partColour[7] = { theme::wg, theme::pitch, theme::tvf, theme::tva, theme::lfo, theme::fxB, theme::x990 };
}

// ── PerfControl ──────────────────────────────────────────────────────────────

PerfControl::PerfControl (Model& m, const juce::String& id, int p) : model (m), param (m.findPerf (id)), part (p)
{
    if (param == nullptr) return;
    auto& info = param->info;
    isChoice = info.kind == 1 || (info.kind == 2 && info.max - info.min < 48);
    if (isChoice)
    {
        char buf[64];
        for (int v = info.min; v <= info.max; ++v)
        {
            jd990_perf_param_format (param->index, (uint16_t) v, buf, sizeof buf);
            box.addItem (juce::String::fromUTF8 (buf), v + 1);
        }
        box.onChange = [this] { model.perfSet (param->index, part, box.getSelectedId() - 1); };
        addAndMakeVisible (box);
    }
    else
    {
        fader.setRange (info.min, info.max, jd990_perf_param_init (param->index));
        fader.setAccent (part >= 0 && part < 7 ? partColour[part] : theme::fxB);
        fader.onChange = [this] (int v) { model.perfSet (param->index, part, v); };
        addAndMakeVisible (fader);
    }
    model.addChangeListener (this);
    update();
}

PerfControl::~PerfControl() { model.removeChangeListener (this); }

void PerfControl::resized()
{
    if (isChoice) box.setBounds (getLocalBounds().withTrimmedTop (2).withHeight (22));
    else fader.setBounds (getLocalBounds());
}

void PerfControl::paint (juce::Graphics& g)
{
    if (! isChoice || param == nullptr) return;
    g.setColour (theme::silkDim);
    g.setFont (theme::label (9.5f));
    g.drawFittedText (param->label.toUpperCase(), getLocalBounds().removeFromBottom (14), juce::Justification::centred, 1, 0.8f);
}

void PerfControl::update()
{
    if (param == nullptr) return;
    bool on = model.perfMode();
    setEnabled (on);
    setAlpha (on ? 1.0f : 0.4f);
    int v = model.perfGet (param->index, part);
    if (isChoice) box.setSelectedId (v + 1, juce::dontSendNotification);
    else
    {
        fader.setValue (v);
        fader.setText (on ? model.perfDisplay (param->index, part) : juce::String(), param->id.startsWith ("perf.reserve") ? u ("Stimmen") : param->label);
    }
}

// ── PerformanceView ──────────────────────────────────────────────────────────

PerformanceView::PerformanceView (Model& m) : model (m)
{
    content.owner = this;
    view.setViewedComponent (&content, false);
    view.setScrollBarsShown (true, false);
    addAndMakeVisible (view);

    for (auto* c : { (juce::Component*) &name, (juce::Component*) &voices, (juce::Component*) &readBtn, (juce::Component*) &sendBtn })
        content.addAndMakeVisible (c);
    content.addChildComponent (modeBtn);
    name.setFont (theme::monoBold (16.0f));
    name.setInputRestrictions (16);
    name.setTooltip (u ("Name der Performance (16 Zeichen) — Eingabetaste übernimmt"));
    name.onReturnKey = [this] { model.setPerfName (name.getText()); };
    name.onFocusLost = name.onReturnKey;
    voices.setFont (theme::mono (13.0f));
    voices.setJustificationType (juce::Justification::centredLeft);
    modeBtn.onClick = [this] { model.setMode (1); };
    readBtn.onClick = [this] { model.readSetupFromDevice(); };
    sendBtn.onClick = [this] { model.sendSetupToDevice(); };
    sendBtn.setTooltip (u ("Performance + Part-Patches 1–7 in die Temp-Bereiche (erst Performance, dann Patches)"));
    sync = std::make_unique<PerfControl> (m, "perf.sync_part", -1);
    content.addAndMakeVisible (*sync);

    for (int p = 0; p < 7; ++p)
    {
        auto& s = strips[p];
        s.edit.setTooltip (u ("Patch dieses Parts im Panel bearbeiten"));
        s.edit.onClick = [this, p] { model.selectPart (p); if (onEditPart) onEditPart(); };
        s.rx.setClickingTogglesState (false);
        s.rx.setColour (juce::TextButton::buttonOnColourId, partColour[p]);
        s.rx.setTooltip (u ("Part empfängt MIDI (an/aus)"));
        s.rx.onClick = [this, p] { if (auto* q = model.findPerf ("part.rx_sw")) model.perfSet (q->index, p, 1 - model.perfGet (q->index, p)); };
        for (int ch = 1; ch <= 16; ++ch) s.channel.addItem ("Kanal " + juce::String (ch), ch);
        s.channel.onChange = [this, p] { if (auto* q = model.findPerf ("part.rx_ch")) model.perfSet (q->index, p, strips[p].channel.getSelectedId() - 1); };
        s.patchBtn.setTooltip (u ("Patch aus dem Speicher wählen — das Gerät lädt ihn in den Part (Änderungen am Part-Patch gehen verloren)"));
        s.patchBtn.onClick = [this, p] { showPatchMenu (p); };
        s.hear.setTooltip (u ("Gedrückt halten: C4 auf dem Kanal dieses Parts"));
        s.hear.onStateChange = [this, p] {
            auto& st = strips[p];
            int ch = model.partChannel (p);
            if (st.hear.isDown() && held[p].empty() && model.link.isOpen()) { held[p] = { 60 }; model.link.noteOn (ch, 60, 100); }
            else if (! st.hear.isDown() && ! held[p].empty()) { for (int nte : held[p]) model.link.noteOff (ch, nte); held[p].clear(); }
        };
        for (auto* c : { (juce::Component*) &s.edit, (juce::Component*) &s.rx, (juce::Component*) &s.patchBtn, (juce::Component*) &s.hear, (juce::Component*) &s.channel })
            content.addAndMakeVisible (c);
        for (auto id : { "part.output", "part.fx_mode", "part.level", "part.pan", "", "part.coarse", "part.fine", "part.fx_level" })
        {
            auto* c = juce::String (id).isEmpty() ? new PerfControl (m, "perf.reserve_" + juce::String (p + 1), -1)
                                                 : new PerfControl (m, id, p);
            s.controls.add (c);
            content.addAndMakeVisible (c);
        }
    }
    const char* groups[3] = { "PERF CHORUS", "PERF DELAY", "PERF REVERB" };
    for (int g = 0; g < 3; ++g)
        for (auto& prm : m.perfParams())
            if (prm.group == groups[g])
            {
                auto* c = new PerfControl (m, prm.id, -1);
                fx[g].add (c);
                content.addAndMakeVisible (c);
            }
    model.addChangeListener (this);
    changeListenerCallback (nullptr);
}

PerformanceView::~PerformanceView() { model.removeChangeListener (this); }

void PerformanceView::showPatchMenu (int p)
{
    juce::PopupMenu menu;
    menu.addSectionHeader (u ("Part ") + juce::String (p + 1) + u (" — Patch aus dem Speicher laden"));
    auto bankMenu = [&] (const char* letter, int bank, int base) {
        juce::PopupMenu sub;
        for (int row = 0; row < 8; ++row)
        {
            juce::PopupMenu r;
            for (int col = 0; col < 8; ++col)
            {
                int number = base + row * 8 + col;
                r.addItem (juce::String (letter) + ":" + juce::String (row + 1) + juce::String (col + 1), [this, p, bank, number] { model.choosePartPatch (p, bank, number); });
            }
            sub.addSubMenu (juce::String (letter) + ":" + juce::String (row + 1) + "x", r);
        }
        return sub;
    };
    menu.addSubMenu (u ("Intern I:11–I:88"), bankMenu ("I", 0, 0));
    menu.addSubMenu (u ("Preset A:11–A:88"), bankMenu ("A", 1, 0));
    menu.addSubMenu (u ("Preset B:11–B:88"), bankMenu ("B", 1, 64));
    menu.addSeparator();
    menu.addItem (u ("Änderungen am Part-Patch gehen dabei verloren"), false, false, nullptr);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (strips[p].patchBtn));
}

void PerformanceView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    bool on = model.perfMode();
    modeBtn.setVisible (! on);
    for (auto* c : { (juce::Component*) &name, (juce::Component*) &readBtn, (juce::Component*) &sendBtn })
        c->setEnabled (on);
    if (! name.hasKeyboardFocus (true)) name.setText (on ? model.perfName() : juce::String(), false);
    int v = model.voicesReserved();
    voices.setText (on ? u ("Stimmen reserviert ") + juce::String (v) + " / 24" : juce::String(), juce::dontSendNotification);
    voices.setColour (juce::Label::textColourId, v >= 24 ? theme::toneOn : theme::silk);
    readBtn.setEnabled (on && model.link.isOpen());
    sendBtn.setEnabled (on && model.link.isOpen() && model.deviceMode == 1);
    auto* rx = model.findPerf ("part.rx_sw");
    for (int p = 0; p < 7; ++p)
    {
        auto& s = strips[p];
        for (auto* c : { (juce::Component*) &s.edit, (juce::Component*) &s.rx, (juce::Component*) &s.patchBtn, (juce::Component*) &s.channel })
            c->setEnabled (on);
        s.hear.setEnabled (on && model.link.isOpen());
        bool rxOn = on && rx != nullptr && model.perfGet (rx->index, p) == 1;
        s.rx.setToggleState (rxOn, juce::dontSendNotification);
        s.rx.setButtonText (rxOn ? "Rx an" : "Rx aus");
        s.channel.setSelectedId (on ? model.partChannel (p) : 0, juce::dontSendNotification);
        s.patchBtn.setButtonText (on ? model.partSlot (p) + u (" ▾") : u ("–"));
    }
    repaint();
}

void PerformanceView::resized()
{
    view.setBounds (getLocalBounds());
    layoutContent();
}

void PerformanceView::layoutContent()
{
    int W = std::max (980, view.getWidth() - 14), pad = 12, y = pad;
    // Kopf: Name, Stimmen, Sync-Part, Lesen/Senden
    name.setBounds (pad, y, 220, 30);
    voices.setBounds (pad + 230, y, 220, 30);
    sync->setBounds (pad + 460, y - 2, sync->preferredWidth(), sync->preferredHeight());
    readBtn.setBounds (W - pad - 280, y, 134, 30);
    sendBtn.setBounds (W - pad - 140, y, 140, 30);
    modeBtn.setBounds (pad, y + 40, 300, 30);
    y += 54;

    int gap = 6, w = (W - 2 * pad - 6 * gap) / 7;
    for (int p = 0; p < 7; ++p)
    {
        auto& s = strips[p];
        int x = pad + p * (w + gap), yy = y;
        stripArea[p] = { x, y, w, 0 };
        yy += 22 + 30;                                        // Kopf + Patchname (gemalt)
        s.patchBtn.setBounds (x + 4, yy, w - 8, 24);          yy += 28;
        s.edit.setBounds (x + 4, yy, w - 8 - 34, 24);
        s.hear.setBounds (x + w - 4 - 30, yy, 30, 24);        yy += 28;
        s.rx.setBounds (x + 4, yy, 60, 24);
        s.channel.setBounds (x + 68, yy, w - 72, 24);         yy += 30;
        // Auswahl: Ausgang, FX-Send · Fader: Level, Pan, Stimmen · Coarse, Fine, FX-Level
        auto& c = s.controls;
        int half = (w - 12) / 2;
        c[0]->setBounds (x + 4, yy, half, c[0]->preferredHeight());
        c[1]->setBounds (x + 8 + half, yy, half, c[1]->preferredHeight());   yy += 48;
        int fw = Fader::width, fg = (w - 3 * fw) / 4;
        for (int k = 0; k < 3; ++k) c[2 + k]->setBounds (x + fg + k * (fw + fg), yy, fw, Fader::height);
        yy += Fader::height + 6;
        for (int k = 0; k < 3; ++k) c[5 + k]->setBounds (x + fg + k * (fw + fg), yy, fw, Fader::height);
        yy += Fader::height + 8;
        stripArea[p].setHeight (yy - y);
    }
    y += stripArea[0].getHeight() + pad + 20;                 // Platz für den Effekt-Hinweis

    // Gemeinsame Effekte der Performance
    for (int g = 0; g < 3; ++g)
    {
        int x = pad + 8, h = 30 + Fader::height + 8;
        fxArea[g] = { pad, y, W - 2 * pad, h };
        for (auto* c : fx[g])
        {
            c->setBounds (x, y + 30 + (Fader::height - c->preferredHeight()) / 2, c->preferredWidth(), c->preferredHeight());
            x += c->preferredWidth() + 8;
        }
        y += h + 4;
    }
    content.setSize (W, y + pad);
}

void PerformanceView::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
}

namespace
{
    void header (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour c, const juce::String& t, bool strong)
    {
        g.setColour (strong ? c : c.withAlpha (0.45f));
        g.fillRect (r);
        g.setColour (theme::ink);
        g.setFont (theme::label (10.5f));
        g.drawText (t.toUpperCase(), r.reduced (8, 0), juce::Justification::centredLeft);
    }
}

void PerformanceView::paintContent (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    bool on = model.perfMode();
    auto* rx = model.findPerf ("part.rx_sw");
    for (int p = 0; p < 7; ++p)
    {
        auto r = stripArea[p];
        bool selected = on && model.part() == p;
        bool rxOn = on && rx != nullptr && model.perfGet (rx->index, p) == 1;
        g.setColour (selected ? theme::raised.brighter (0.06f) : theme::raised);
        g.fillRect (r);
        if (selected) { g.setColour (partColour[p]); g.drawRect (r, 2); }
        header (g, r.withHeight (22), partColour[p], "Part " + juce::String (p + 1) + (selected ? u (" · bearbeitet") : juce::String()), rxOn || selected);
        g.setColour (rxOn ? theme::silk : theme::silkDim);
        g.setFont (theme::monoBold (13.0f));
        g.drawFittedText (on ? model.partPatchName (p) : u ("—"), r.withTrimmedTop (24).withHeight (26).reduced (6, 0), juce::Justification::centredLeft, 1, 0.8f);
    }
    const char* titles[3] = { "Performance · Chorus", "Performance · Delay", "Performance · Reverb" };
    for (int k = 0; k < 3; ++k)
        header (g, fxArea[k].withHeight (22), theme::fxB, u (titles[k]), true);
    g.setColour (theme::silkDim);
    g.setFont (juce::FontOptions (12.0f));
    if (fxArea[0].getY() > 0)
        g.drawText (u ("Im Performance-Modus gelten die Patch-Effekte nur bei Part 1 (Gruppe A). Chorus, Delay und Reverb sind für alle Parts gemeinsam — Anteil je Part über FX Send."),
                    fxArea[0].getX(), fxArea[0].getY() - 18, fxArea[0].getWidth(), 16, juce::Justification::centredLeft);
    if (! on)
    {
        g.setColour (theme::silk);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (u ("Der Editor ist im Patch-Modus. Performance-Modus: oben „Performance“ oder hier umschalten — "
                       "bei Verbindung schaltet der Editor auch das Gerät um und liest, was es geladen hat."),
                    330, 52, content.getWidth() - 340, 30, juce::Justification::centredLeft);
    }
}
