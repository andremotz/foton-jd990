#include "Panels.h"

namespace
{
    juce::String u (const char* s) { return juce::String::fromUTF8 (s); }
    const char* toneLetter[] = { "A", "B", "C", "D" };

    juce::StringArray idsOfGroup (const Model& m, const juce::String& group, bool tone)
    {
        juce::StringArray ids;
        for (auto& p : m.params())
            if (p.group == group && p.isTone() == tone)
                ids.add (p.id);
        return ids;
    }

    /** Gruppen in UI-Reihenfolge (group_order der Spec). */
    juce::StringArray groupsInOrder (const Model& m, bool tone)
    {
        std::vector<std::pair<int, juce::String>> g;
        for (auto& p : m.params())
            if (p.isTone() == tone && std::none_of (g.begin(), g.end(), [&] (auto& x) { return x.second == p.group; }))
                g.emplace_back (p.info.group_rank, p.group);
        std::stable_sort (g.begin(), g.end(), [] (auto& a, auto& b) { return a.first < b.first; });
        juce::StringArray out;
        for (auto& x : g) out.add (x.second);
        return out;
    }

    void header (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour accent, const juce::String& text)
    {
        g.setColour (accent);
        g.fillRect (r);
        g.setColour (theme::ink);
        g.setFont (theme::label (10.5f));
        g.drawText (text.toUpperCase(), r.reduced (8, 0), juce::Justification::centredLeft);
    }
}

// ── GroupBox ─────────────────────────────────────────────────────────────────

GroupBox::GroupBox (Model& m, const juce::String& t, juce::Colour a, const juce::StringArray& ids, int fixedTone, bool compact)
    : baseTitle (t), title (t), accent (a)
{
    if (fixedTone == -2 && ! compact && EnvelopeView::supports (t))
    {
        env = std::make_unique<EnvelopeView> (m, t, a);
        addAndMakeVisible (*env);
        // Standard: nur Grafik + Velo/T-Velo/T-KF; „Werte“ klappt die T-/L-Fader auf (gespeichert je Gruppe)
        valuesBtn.setClickingTogglesState (true);
        valuesBtn.setTooltip (juce::String::fromUTF8 ("Zeiten und Pegel zusätzlich als Fader zeigen"));
        auto* s = Model::settings();
        valuesBtn.setToggleState (s != nullptr && s->getBoolValue ("envValues." + t, false), juce::dontSendNotification);
        valuesBtn.onClick = [this] { showValues (valuesBtn.getToggleState()); };
        addAndMakeVisible (valuesBtn);
    }
    for (auto& id : ids)
    {
        auto* c = new ParamControl (m, id, fixedTone, compact);
        if (! c->valid()) { delete c; continue; }
        controls.add (c);
        addAndMakeVisible (c);
    }
    // Schloss nur bei Gruppen aus genau einer Spec-Gruppe (Tone-Gruppen, Effekte, EQ …)
    if (! compact && ! controls.isEmpty())
    {
        auto g = controls[0]->getParam()->group;
        bool same = std::all_of (controls.begin(), controls.end(), [&] (auto* c) { return c->getParam()->group == g; });
        if (same) { lock = std::make_unique<LockButton> (m, g); addAndMakeVisible (*lock); }
        if (same && FxSketch::supports (g)) { sketch = std::make_unique<FxSketch> (m, g); addAndMakeVisible (*sketch); }
    }
}

void GroupBox::showValues (bool on)
{
    valuesBtn.setToggleState (on, juce::dontSendNotification);
    if (auto* s = Model::settings()) { s->setValue ("envValues." + baseTitle, on); s->saveIfNeeded(); }
    resized();                               // auch wenn die Größe gleich bleibt
    if (onLayoutChange) onLayoutChange();
}

std::vector<std::pair<juce::Component*, juce::Point<int>>> GroupBox::items() const
{
    std::vector<std::pair<juce::Component*, juce::Point<int>>> out;
    for (auto* c : controls) if (shown (c)) out.push_back ({ c, { c->preferredWidth(), c->preferredHeight() } });
    if (sketch != nullptr) out.push_back ({ sketch.get(), { FxSketch::width, Fader::height - 10 } });
    return out;
}

int GroupBox::naturalWidth() const
{
    int w = 16;
    for (auto& it : items()) w += it.second.x + 4;
    return collapsed() ? w + minGraphWidth : w;
}

int GroupBox::preferredHeight (int width) const
{
    if (collapsed()) return 30 + Fader::height + 8;   // Grafik links, Velo/T-Velo/T-KF rechts daneben
    int x = 8, rowH = 0, h = top();
    for (auto& [c, size] : items())
    {
        if (x + size.x > width - 8 && x > 8) { h += rowH + 6; x = 8; rowH = 0; }
        x += size.x + 4;
        rowH = std::max (rowH, size.y);
    }
    return h + rowH + 8;
}

void GroupBox::resized()
{
    if (lock != nullptr) lock->setBounds (getWidth() - 24, 3, 18, 16);
    if (env != nullptr) valuesBtn.setBounds (getWidth() - (lock != nullptr ? 88 : 64), 3, 58, 16);
    for (auto* c : controls) c->setVisible (shown (c));
    if (collapsed())
    {
        int x = getWidth() - 8;
        for (int i = controls.size(); --i >= 0;)
            if (auto* c = controls[i]; shown (c))
            {
                x -= c->preferredWidth();
                c->setBounds (x, 30, c->preferredWidth(), c->preferredHeight());
                x -= 4;
            }
        env->setBounds (6, 28, x - 10, Fader::height + 2);
        return;
    }
    if (env != nullptr) env->setBounds (6, 28, getWidth() - 12, envHeight);
    int x = 8, y = top(), rowH = 0;
    for (auto& [c, size] : items())
    {
        if (x + size.x > getWidth() - 8 && x > 8) { y += rowH + 6; x = 8; rowH = 0; }
        c->setBounds (x, y, size.x, size.y);
        x += size.x + 4;
        rowH = std::max (rowH, size.y);
    }
}

void GroupBox::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    header (g, getLocalBounds().removeFromTop (22), accent, title);
}

// ── LCD ──────────────────────────────────────────────────────────────────────

Lcd::Lcd (Model& m) : model (m) { model.addChangeListener (this); }
Lcd::~Lcd() { model.removeChangeListener (this); }

void Lcd::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0d0f0c));
    g.fillRoundedRectangle (b, 3.0f);
    auto s = b.reduced (7.0f);
    juce::ColourGradient grad (theme::lcdLit, s.getX(), s.getY(), theme::lcdLit2, s.getRight(), s.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (s);
    // Pixelraster
    g.setColour (juce::Colours::black.withAlpha (0.03f));
    for (float y = s.getY(); y < s.getBottom(); y += 5.0f) g.drawHorizontalLine ((int) y, s.getX(), s.getRight());

    auto r = s.reduced (11.0f, 8.0f).toNearestInt();
    g.setColour (theme::lcdInk);
    g.setFont (theme::monoBold (15.0f));
    auto l1 = r.removeFromTop (20);
    g.drawText (model.patchName().toUpperCase(), l1, juce::Justification::centredLeft);
    g.setColour (theme::lcdInkDim);
    g.setFont (theme::mono (10.5f));
    g.drawText (model.perfMode() ? "PART " + juce::String (model.part() + 1) : juce::String ("TEMP"), l1, juce::Justification::centredRight);

    const Model::Param* p = model.lastIndex >= 0 ? &model.params()[(size_t) model.lastIndex] : nullptr;
    int t = p != nullptr && p->isTone() ? (model.lastTone >= 0 ? model.lastTone : model.tone) : -1;

    // Skizze zum berührten Parameter (Hüllkurve, LFO, Filter) rechts — Text rückt zusammen
    auto graph = r.withTrimmedLeft (r.getWidth() * 50 / 100).withTrimmedTop (4).toFloat();
    juce::Graphics::ScopedSaveState keep (g);
    if (drawLcdGraph (g, graph, model, p, t))
        r = r.withWidth (r.getWidth() * 50 / 100 - 10);
    juce::String ctx = p == nullptr ? u ("BEREIT") : (t >= 0 ? juce::String ("TONE ") + toneLetter[t] + u (" · ") : juce::String()) + p->group;
    g.drawText (ctx.toUpperCase(), r.removeFromTop (16), juce::Justification::centredLeft);

    auto l3 = r.removeFromTop (26);
    g.setColour (theme::lcdInk);
    if (p != nullptr && model.hasPatch())
    {
        g.setFont (theme::mono (13.0f));
        g.drawText (p->label.toUpperCase(), l3, juce::Justification::centredLeft);
        g.setFont (theme::monoBold (21.0f));
        g.drawText (model.display (p->index, t), l3, juce::Justification::centredRight);
    }

    // Balken
    auto bar = r.removeFromTop (12).withTrimmedTop (4);
    float frac = 0.0f;
    if (p != nullptr && model.hasPatch() && p->info.max > p->info.min)
        frac = (float) (model.get (p->index, t) - p->info.min) / (float) (p->info.max - p->info.min);
    const int n = 32;
    float w = (float) bar.getWidth() / n;
    for (int i = 0; i < n; ++i)
    {
        g.setColour (i < juce::roundToInt (frac * n) ? theme::lcdInk : theme::lcdInk.withAlpha (0.16f));
        g.fillRect (juce::Rectangle<float> ((float) bar.getX() + (float) i * w, (float) bar.getY(), w - 2.0f, (float) bar.getHeight()));
    }
}

// ── Inspector ────────────────────────────────────────────────────────────────

Inspector::Inspector (Model& m) : model (m) { model.addChangeListener (this); }
Inspector::~Inspector() { model.removeChangeListener (this); }

void Inspector::paint (juce::Graphics& g)
{
    g.fillAll (theme::raised);
    g.setColour (theme::edge);
    g.drawHorizontalLine (0, 0, (float) getWidth());
    g.setColour (theme::silkDim);
    g.setFont (theme::label (10.0f));
    g.drawText (u ("ZULETZT GESENDET · DT1"), 14, 6, 300, 14, juce::Justification::centredLeft);

    auto& m = model.lastSent;
    int x = 14, y = 24;
    g.setFont (theme::mono (12.0f));
    for (size_t i = 0; i < m.size() && x < getWidth() - 40; ++i)
    {
        bool isAddr = i >= 5 && i < 9, isSum = m.size() > 2 && i == m.size() - 2, isData = i >= 9 && ! isSum && i < m.size() - 1;
        juce::Colour c = isAddr ? juce::Colour (0xff7fc6ea) : isData ? juce::Colour (0xffe8c05a) : isSum ? juce::Colour (0xff8fd48f) : theme::silkDim;
        juce::Rectangle<int> box (x, y, 26, 20);
        g.setColour (theme::sunk);
        g.fillRoundedRectangle (box.toFloat(), 2.0f);
        g.setColour (c.withAlpha (0.35f));
        g.drawRoundedRectangle (box.toFloat(), 2.0f, 1.0f);
        g.setColour (c);
        g.drawText (juce::String::toHexString (m[i]).toUpperCase().paddedLeft ('0', 2), box, juce::Justification::centred);
        x += 30;
    }
    if (m.empty())
    {
        g.setColour (theme::silkDim);
        g.drawText (u ("— noch nichts gesendet —"), 14, y, 400, 20, juce::Justification::centredLeft);
    }
}

// ── CompareView ──────────────────────────────────────────────────────────────

CompareView::CompareView (Model& m) : model (m)
{
    groups = groupsInOrder (m, true);
    for (int i = 0; i < groups.size(); ++i) groupBox.addItem (groups[i], i + 1);
    groupBox.setSelectedId (1, juce::dontSendNotification);
    groupBox.onChange = [this] { repaint(); };
    addAndMakeVisible (groupBox);
    model.addChangeListener (this);
}

CompareView::~CompareView() { model.removeChangeListener (this); }

void CompareView::resized() { groupBox.setBounds (80, 10, 180, 24); }

void CompareView::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::silkDim);
    g.setFont (theme::label (10.5f));
    g.drawText ("GRUPPE", 14, 10, 60, 24, juce::Justification::centredLeft);
    g.drawText (u ("ABWEICHENDE WERTE HERVORGEHOBEN"), 280, 10, 320, 24, juce::Justification::centredLeft);

    auto group = groups[groupBox.getSelectedId() - 1];
    int y = 46, colW = std::max (110, (getWidth() - 200) / 4);
    g.setFont (theme::label (10.5f));
    for (int t = 0; t < 4; ++t)
        g.drawText (juce::String ("TONE ") + toneLetter[t], 180 + t * colW, y, colW - 10, 18, juce::Justification::centredRight);
    y += 22;
    for (auto& p : model.params())
    {
        if (! p.isTone() || p.group != group) continue;
        juce::String v[4];
        bool differ = false;
        for (int t = 0; t < 4; ++t)
        {
            v[t] = model.hasPatch() ? model.display (p.index, t) : juce::String();
            differ |= v[t] != v[0];
        }
        g.setColour (theme::edge);
        g.drawHorizontalLine (y + 21, 14.0f, (float) getWidth() - 14);
        g.setColour (theme::silkDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (p.label, 14, y, 160, 20, juce::Justification::centredLeft);
        g.setFont (theme::mono (12.0f));
        for (int t = 0; t < 4; ++t)
        {
            g.setColour (differ ? theme::toneOn : theme::silk);
            g.drawText (v[t], 180 + t * colW, y, colW - 10, 20, juce::Justification::centredRight);
        }
        y += 22;
    }
}

// ── PanelView (Tone-Ebene) ───────────────────────────────────────────────────

PanelView::PanelView (Model& m)
    : model (m), basics (m, "Basics", theme::basics, idsOfGroup (m, "BASICS", false), -1, true),
      lcd (m), inspector (m), compare (m)
{
    addAndMakeVisible (basics);
    addAndMakeVisible (lcd);
    addAndMakeVisible (inspector);
    addChildComponent (compare);
    for (int t = 0; t < 4; ++t)
    {
        toneBtn[t].setButtonText (toneLetter[t]);
        toneBtn[t].setColour (juce::TextButton::buttonOnColourId, theme::toneOn);
        toneBtn[t].onClick = [this, t] {
            if (model.linkMode && juce::ModifierKeys::currentModifiers.isShiftDown()) model.toggleLinked (t);
            else model.selectTone (t);
        };
        toneBtn[t].setTooltip (u ("Tone wählen · mit Link: ⇧-Klick nimmt Tones in den Link auf/heraus"));
        addAndMakeVisible (toneBtn[t]);
    }
    linkBtn.setClickingTogglesState (true);
    linkBtn.setTooltip (u ("Fader bewegen alle gelinkten Tones relativ (⇧-Klick auf A–D wählt die Tones)"));
    linkBtn.onClick = [this] {
        model.linkMode = linkBtn.getToggleState();
        if (model.linkMode) model.linked = { 0, 1, 2, 3 };
        else model.linked = { model.tone };
        model.sendChangeMessage();
    };
    compareBtn.setClickingTogglesState (true);
    compareBtn.onClick = [this] { compare.setVisible (compareBtn.getToggleState()); wallView.setVisible (! compareBtn.getToggleState()); };
    addAndMakeVisible (linkBtn);
    addAndMakeVisible (compareBtn);
    addAndMakeVisible (toneMenuBtn);
    addAndMakeVisible (varBtn);
    addAndMakeVisible (varAmount);
    for (int a : { 5, 10, 20, 35, 50 }) varAmount.addItem (juce::String (a) + " %", a);
    varAmount.setSelectedId (20, juce::dontSendNotification);
    varBtn.setTooltip (u ("Gewichtete Zufallsvariante des aktuellen Patches (rückgängig mit ⌘Z). Tone-Schalter, Structure, Bereiche und Pegel bleiben."));
    varBtn.onClick = [this] { variation (varAmount.getSelectedId()); };
    toneMenuBtn.setTooltip (u ("Aktuellen Tone ganz oder gruppenweise kopieren, oder Tones tauschen — rückgängig mit ⌘Z"));
    toneMenuBtn.onClick = [this] { showToneMenu(); };

    for (auto& g : groupsInOrder (m, true))
    {
        auto* box = new GroupBox (m, g, theme::accent (g), idsOfGroup (m, g, true));
        box->onLayoutChange = [this] { layoutWall(); };
        groups.add (box);
        wall.addAndMakeVisible (box);
    }
    wallView.setViewedComponent (&wall, false);
    wallView.setScrollBarsShown (true, false);
    addAndMakeVisible (wallView);
    model.addChangeListener (this);
    changeListenerCallback (nullptr);
}

PanelView::~PanelView() { model.removeChangeListener (this); }

void PanelView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (int t = 0; t < 4; ++t)
    {
        toneBtn[t].setToggleState (t == model.tone, juce::dontSendNotification);
        bool inLink = model.linkMode && model.linked.count (t) && t != model.tone;
        toneBtn[t].setColour (juce::TextButton::textColourOffId, inLink ? theme::toneOn : theme::silkDim);
    }
    linkBtn.setToggleState (model.linkMode, juce::dontSendNotification);
    for (auto* g : groups)
        g->setTitle (juce::String ("TONE ") + toneLetter[model.tone] + (model.linkMode && model.linked.size() > 1 ? "+" : "") + u (" · ") + g->baseTitle);
}

void PanelView::variation (int percent)
{
    if (! model.hasPatch()) return;
    varAmount.setSelectedId (percent, juce::dontSendNotification);
    PatchPtr next (jd990_patch_variation (model.patch(), (float) percent / 100.0f, varSeed++));
    model.applyLocks (next.get(), model.patch());          // gesperrte Gruppen bleiben
    model.applyWithUndo (std::move (next), u ("Variation ") + juce::String (percent) + " %"
                         + (model.lockedGroups.empty() ? juce::String() : u (" (") + juce::String ((int) model.lockedGroups.size()) + u (" Gruppe(n) gesperrt)")));
}

void PanelView::showToneMenu()
{
    if (! model.hasPatch()) return;
    int from = model.tone;
    juce::String X = toneLetter[from];
    juce::PopupMenu copy, swap, group;
    for (int t = 0; t < 4; ++t)
    {
        if (t == from) continue;
        copy.addItem (u ("nach Tone ") + toneLetter[t], [this, from, t] { model.copyTone (from, t); });
        swap.addItem (u ("mit Tone ") + toneLetter[t], [this, from, t] { model.swapTones (from, t); });
    }
    copy.addSeparator();
    copy.addItem (u ("nach allen anderen"), [this, from] { for (int t = 0; t < 4; ++t) if (t != from) model.copyTone (from, t); });
    for (auto& gname : groupsInOrder (model, true))
    {
        juce::PopupMenu targets;
        for (int t = 0; t < 4; ++t)
            if (t != from) targets.addItem (u ("nach Tone ") + toneLetter[t], [this, from, t, gname] { model.copyTone (from, t, gname); });
        targets.addItem (u ("nach allen anderen"), [this, from, gname] { for (int t = 0; t < 4; ++t) if (t != from) model.copyTone (from, t, gname); });
        group.addSubMenu (gname, targets);
    }
    juce::PopupMenu m;
    m.addSectionHeader (u ("Tone ") + X);
    m.addSubMenu (u ("Ganzen Tone ") + X + u (" kopieren"), copy);
    m.addSubMenu (u ("Nur eine Gruppe kopieren"), group);
    m.addSubMenu (u ("Tone ") + X + u (" tauschen"), swap);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (toneMenuBtn));
}

void PanelView::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::rail);
    g.fillRect (0, 0, 206, getHeight() - 58);
    g.setColour (theme::raised);
    g.fillRect (206, 0, getWidth() - 206, 132);
    g.setColour (theme::silkDim);
    g.setFont (theme::label (10.5f));
    g.drawText ("TONE", toneBtn[0].getX() - 42, toneBtn[0].getY(), 38, toneBtn[0].getHeight(), juce::Justification::centredRight);
}

void PanelView::resized()
{
    auto a = getLocalBounds();
    inspector.setBounds (a.removeFromBottom (58));
    auto left = a.removeFromLeft (206);
    basics.setBounds (left.withHeight (basics.preferredHeight (left.getWidth())));

    auto head = a.removeFromTop (132).reduced (14, 12);
    lcd.setBounds (head.removeFromLeft (std::min (440, head.getWidth() / 2)));
    head.removeFromLeft (60);
    auto row = head.removeFromTop (32);
    for (auto& b : toneBtn) { b.setBounds (row.removeFromLeft (40)); row.removeFromLeft (5); }
    row.removeFromLeft (10);
    linkBtn.setBounds (row.removeFromLeft (70));
    head.removeFromTop (10);
    auto row2 = head.removeFromTop (30);
    compareBtn.setBounds (row2.removeFromLeft (140));
    row2.removeFromLeft (8);
    toneMenuBtn.setBounds (row2.removeFromLeft (210));
    row2.removeFromLeft (8);
    varBtn.setBounds (row2.removeFromLeft (84));
    row2.removeFromLeft (4);
    varAmount.setBounds (row2.removeFromLeft (72));

    wallView.setBounds (a);
    compare.setBounds (a);
    layoutWall();
}

void PanelView::layoutWall()
{
    int W = wallView.getWidth() - 12, x = 0, y = 0, rowH = 0;
    std::vector<GroupBox*> row;
    auto flush = [&] {
        // Restbreite gleichmäßig auf die Gruppen der Zeile verteilen — keine Lücke rechts
        int used = 0;
        for (auto* b : row) used += b->getWidth() + 1;
        int extra = row.empty() ? 0 : (W - used + 1) / (int) row.size(), bx = 0;
        for (auto* b : row)
        {
            b->setBounds (bx, b->getY(), b->getWidth() + extra, rowH);
            bx += b->getWidth() + 1;
        }
        y += rowH + 1;
        x = 0; rowH = 0; row.clear();
    };
    for (auto* b : groups)
    {
        int w = juce::jlimit (180, W, b->naturalWidth());
        if (x + w > W && x > 0) flush();
        b->setBounds (x, y, w, b->preferredHeight (w));
        rowH = std::max (rowH, b->getHeight());
        row.push_back (b);
        x += w + 1;
    }
    flush();
    wall.setSize (W, y);
}

// ── PatchView (Patch & Effekte) ──────────────────────────────────────────────

namespace
{
    // Effektblöcke: Kürzel im Sequenz-Enum → Gruppe der Parameter
    struct Fx { const char* code; const char* name; const char* group; };
    const Fx fxA[] = { { "DS", "Distortion", "DISTORTION" }, { "PH", "Phaser", "PHASER" },
                       { "SP", "Spectrum", "SPECTRUM" },   { "EN", "Enhancer", "ENHANCER" } };
    const Fx fxB[] = { { "CHO", "Chorus", "CHORUS" }, { "DLY", "Delay", "DELAY" }, { "REV", "Reverb", "REVERB" } };

    /** Structure-Diagramm (Handbuch S. 71–73, vereinfacht). X = erster, Y = zweiter Tone des Paars. */
    void drawStructure (juce::Graphics& g, juce::Rectangle<float> r, int type, char x, char y, bool on)
    {
        g.setColour (on ? theme::sunk.brighter (0.15f) : theme::sunk);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? theme::x990 : theme::edge);
        g.drawRoundedRectangle (r, 3.0f, on ? 2.0f : 1.0f);
        auto ink = on ? theme::silk : theme::silkDim;
        g.setFont (theme::label (9.0f));
        auto box = [&] (float bx, float by, const juce::String& t) {
            juce::Rectangle<float> b (r.getX() + bx, r.getY() + by, 24, 13);
            g.setColour (ink.withAlpha (0.25f));
            g.fillRect (b);
            g.setColour (ink);
            g.drawText (t, b, juce::Justification::centred);
        };
        auto line = [&] (float x1, float y1, float x2, float y2) {
            g.setColour (ink.withAlpha (0.7f));
            g.drawLine (r.getX() + x1, r.getY() + y1, r.getX() + x2, r.getY() + y2, 1.0f);
        };
        juce::String X (juce::String::charToString ((juce::juce_wchar) x)), Y (juce::String::charToString ((juce::juce_wchar) y));
        float top = 18, bot = 38;
        g.setColour (ink);
        g.drawText ("TYPE " + juce::String (type + 1), r.withHeight (14).translated (0, 2), juce::Justification::centred);
        bool ring = type >= 2;
        switch (type)
        {
            case 0: // unabhängig
                box (4, top, X); box (32, top, "TVF"); box (60, top, "TVA"); line (28, top + 6, 32, top + 6); line (56, top + 6, 60, top + 6);
                box (4, bot, Y); box (32, bot, "TVF"); box (60, bot, "TVA"); line (28, bot + 6, 32, bot + 6); line (56, bot + 6, 60, bot + 6);
                break;
            case 1: // Filter gekoppelt, TVA des zweiten
                box (4, top, X); box (32, top, "TVF"); line (28, top + 6, 32, top + 6); line (56, top + 6, 64, bot);
                box (4, bot, Y); box (32, bot, "TVF"); box (60, bot, "TVA"); line (28, bot + 6, 32, bot + 6); line (56, bot + 6, 60, bot + 6);
                break;
            default: // 2–5: Ringmodulator vor (2, 3) bzw. nach (4, 5) den Filtern
            {
                bool rmFirst = type == 2 || type == 3;
                box (4, top, X); box (4, bot, Y);
                box (32, (top + bot) / 2, "RM");
                line (28, top + 6, 32, (top + bot) / 2 + 4); line (28, bot + 6, 32, (top + bot) / 2 + 8);
                box (60, (top + bot) / 2, rmFirst ? "TVF" : "TVA");
                line (56, (top + bot) / 2 + 6, 60, (top + bot) / 2 + 6);
                if (type == 3 || type == 5) { g.setColour (ink); g.drawText ("+" + Y, juce::Rectangle<float> (r.getX() + 60, r.getY() + bot + 4, 26, 12), juce::Justification::centred); }
                juce::ignoreUnused (ring);
                break;
            }
        }
    }
}

namespace
{
    juce::StringArray sequenceOf (const Model& m, const char* seqId)
    {
        auto* p = m.find (seqId);
        if (p == nullptr || ! m.hasPatch()) return {};
        return juce::StringArray::fromTokens (m.display (p->index, -1), "-", "");
    }
}

PatchView::PatchView (Model& m) : model (m)
{
    content.owner = this;
    auto add = [&] (const juce::String& title, juce::Colour c, const juce::StringArray& ids, bool compact = false) {
        auto* b = new GroupBox (m, title, c, ids, -1, compact);
        boxes.add (b);
        content.addAndMakeVisible (b);
        return b;
    };
    keyFx = add ("Key FX", theme::basics, idsOfGroup (m, "KEY FX", false), true);
    eq = add ("EQ", theme::accent ("EQ"), idsOfGroup (m, "EQ", false));
    fxCtrl = add ("Effekt-Steuerung", theme::ctrl, idsOfGroup (m, "FX CTRL", false), true);
    for (int t = 0; t < 4; ++t)
    {
        juce::String l = juce::String (toneLetter[t]).toLowerCase();
        rangeRows[t] = add (juce::String ("Tone ") + toneLetter[t] + u (" · Key / Velocity"), theme::x990,
                            { "patch.key_lo_" + l, "patch.key_hi_" + l, "patch.velo_range_" + l, "patch.velo_point_" + l, "patch.velo_fade_" + l }, true);
    }
    for (auto& f : fxA) { auto* b = new GroupBox (m, f.name, theme::fxA, idsOfGroup (m, f.group, false)); fxParams.add (b); content.addChildComponent (b); }
    for (auto& f : fxB) { auto* b = new GroupBox (m, f.name, theme::fxB, idsOfGroup (m, f.group, false)); fxParams.add (b); content.addChildComponent (b); }
    auto* seqA = add ("Gruppe A", theme::fxA, { "patch.fx.a_sequence" }, true);
    auto* seqB = add ("Gruppe B", theme::fxB, { "patch.fx.b_sequence" }, true);
    juce::ignoreUnused (seqA, seqB);

    view.setViewedComponent (&content, false);
    view.setScrollBarsShown (true, false);
    addAndMakeVisible (view);
    model.addChangeListener (this);
}

PatchView::~PatchView() { model.removeChangeListener (this); }

void PatchView::changeListenerCallback (juce::ChangeBroadcaster*) { content.repaint(); }

void PatchView::paint (juce::Graphics& g) { g.fillAll (theme::panel); }

void PatchView::mouseUp (const juce::MouseEvent&) {}

void PatchView::resized()
{
    view.setBounds (getLocalBounds());
    layoutContent();
}

void PatchView::layoutContent()
{
    int W = std::max (900, view.getWidth() - 14), pad = 12, y = pad;
    // Zeile 1: Key FX | Structure
    int kw = std::min (keyFx->naturalWidth(), W / 2);
    keyFx->setBounds (pad, y, kw, keyFx->preferredHeight (kw));
    structArea[0] = { pad + kw + pad, y, W - kw - 3 * pad, keyFx->getHeight() };
    y += keyFx->getHeight() + pad;

    // Zeile 2: Key/Velocity — vier Tones nebeneinander, darunter der Tastaturstreifen
    {
        int w = (W - 2 * pad - 3) / 4, h = 0;
        for (int t = 0; t < 4; ++t)
        {
            rangeRows[t]->setBounds (pad + t * (w + 1), y, w, rangeRows[t]->preferredHeight (w));
            h = std::max (h, rangeRows[t]->getHeight());
        }
        for (auto* r : rangeRows) r->setSize (w, h);
        y += h + 4;
        keyboardArea = { pad, y, W - 2 * pad, 4 * 18 + 26 };
        y += keyboardArea.getHeight() + pad;
    }

    // Zeile 3: EQ | Effekt-Steuerung
    int ew = std::min (eq->naturalWidth(), W - 2 * pad);
    eq->setBounds (pad, y, ew, eq->preferredHeight (ew));
    if (W - ew - 3 * pad >= fxCtrl->naturalWidth())            // passt daneben
    {
        fxCtrl->setBounds (pad + ew + pad, y, fxCtrl->naturalWidth(), fxCtrl->preferredHeight (fxCtrl->naturalWidth()));
        y += std::max (eq->getHeight(), fxCtrl->getHeight()) + pad;
    }
    else                                                        // sonst eigene Zeile statt Umbruch in der Gruppe
    {
        y += eq->getHeight() + pad;
        int cw = std::min (fxCtrl->naturalWidth(), W - 2 * pad);
        fxCtrl->setBounds (pad, y, cw, fxCtrl->preferredHeight (cw));
        y += fxCtrl->getHeight() + pad;
    }

    // Zeile 4/5: Effektketten A und B — Sequenz-Auswahl, Blöcke (gemalt), Parameter des gewählten Blocks
    for (int grp = 0; grp < 2; ++grp)
    {
        auto* seq = boxes[boxes.size() - 2 + grp];
        seq->setBounds (pad, y, 130, seq->preferredHeight (130));
        chainArea[grp] = { pad + 130 + pad, y, W - 130 - 3 * pad, 72 };
        y += std::max (seq->getHeight(), 72) + 6;
        int first = grp == 0 ? 0 : 4, count = grp == 0 ? 4 : 3, sel = grp == 0 ? selectedA : selectedB;
        for (int i = 0; i < count; ++i)
        {
            auto* b = fxParams[first + i];
            b->setVisible (i == sel);
            int w = std::min (b->naturalWidth(), W - 2 * pad);
            b->setBounds (pad, y, w, b->preferredHeight (w));
        }
        y += fxParams[first + sel]->getHeight() + pad;
    }
    content.setSize (W, y);
}


void PatchView::paintContent (juce::Graphics& g)
{
    g.fillAll (theme::panel);

    // Structure A&B und C&D
    auto s = structArea[0];
    header (g, s.removeFromTop (22), theme::x990, "Structure");
    const char* pairs[2][3] = { { "patch.structure_ab", "A", "B" }, { "patch.structure_cd", "C", "D" } };
    for (int pr = 0; pr < 2; ++pr)
    {
        auto row = s.removeFromTop ((s.getHeight()) / (2 - pr)).reduced (6, 4);
        g.setColour (theme::silkDim);
        g.setFont (theme::label (10.0f));
        g.drawText (juce::String (pairs[pr][1]) + " & " + pairs[pr][2], row.removeFromLeft (44), juce::Justification::centredLeft);
        auto* p = model.find (pairs[pr][0]);
        int cur = model.hasPatch() && p != nullptr ? model.get (p->index, -1) : -1;
        float w = std::min (110.0f, (float) row.getWidth() / 6.0f);
        for (int t = 0; t < 6; ++t)
            drawStructure (g, juce::Rectangle<float> ((float) row.getX() + (float) t * w, (float) row.getY(), w - 6, (float) std::min (row.getHeight(), 60)), t,
                           pairs[pr][1][0], pairs[pr][2][0], t == cur);
    }

    // Tastaturstreifen: Key Range aller vier Tones über C-1 … G9
    {
        auto k = keyboardArea;
        g.setColour (theme::silkDim);
        g.setFont (theme::label (9.5f));
        for (int c = 0; c <= 10; ++c)   // Oktavmarken C-1 … C9
        {
            float x = (float) (k.getX() + 24) + (float) (c * 12) / 127.0f * (float) (k.getWidth() - 24);
            g.setColour (theme::edge);
            g.drawVerticalLine ((int) x, (float) k.getY(), (float) k.getBottom() - 16);
            g.setColour (theme::silkDim);
            g.drawText ("C" + juce::String (c - 1), juce::Rectangle<float> (x - 12, (float) k.getBottom() - 14, 24, 12), juce::Justification::centred);
        }
        for (int t = 0; t < 4; ++t)
        {
            juce::Rectangle<int> kb (k.getX() + 24, k.getY() + t * 18, k.getWidth() - 24, 14);
            g.setColour (theme::silkDim);
            g.drawText (toneLetter[t], juce::Rectangle<int> (k.getX(), kb.getY(), 20, 14), juce::Justification::centred);
            g.setColour (theme::sunk);
            g.fillRect (kb);
            juce::String l = juce::String (toneLetter[t]).toLowerCase();
            auto *lo = model.find ("patch.key_lo_" + l), *hi = model.find ("patch.key_hi_" + l);
            if (! model.hasPatch() || lo == nullptr || hi == nullptr) continue;
            float a = (float) model.get (lo->index, -1) / 127.0f, b = (float) model.get (hi->index, -1) / 127.0f;
            g.setColour (theme::x990.withAlpha (0.75f));
            g.fillRect (juce::Rectangle<float> ((float) kb.getX() + a * (float) kb.getWidth(), (float) kb.getY(), (b - a) * (float) kb.getWidth() + 2, (float) kb.getHeight()));
            g.setColour (theme::ink);
            g.setFont (theme::mono (10.0f));
            g.drawText (model.display (lo->index, -1) + u (" … ") + model.display (hi->index, -1), kb.reduced (6, 0), juce::Justification::centredLeft);
        }
    }

    // Effektketten als Blöcke in Signalrichtung (ziehen = umsortieren)
    for (int grp = 0; grp < 2; ++grp)
    {
        g.setColour (theme::silkDim);
        g.setFont (theme::label (9.5f));
        g.drawText (u ("Block ziehen = Reihenfolge ändern · ON/OFF klicken = schalten"),
                    chainArea[grp].withHeight (12).translated (0, -2), juce::Justification::centredLeft);
        if (model.perfMode())   // Performance-Modus: Patch-Effekte gelten nur eingeschränkt (Handbuch Pfom-10)
        {
            bool works = grp == 0 && model.part() == 0;
            g.setColour (works ? theme::silkDim : theme::toneOn);
            g.drawText (works ? u ("Performance-Modus: Gruppe A von Part 1 wirkt") : u ("Performance-Modus: ohne Wirkung — gemeinsame Effekte im Tab „Performance“"),
                        chainArea[grp].withHeight (12).translated (360, -2), juce::Justification::centredLeft);
            g.setColour (theme::silkDim);
        }
        auto area = chainArea[grp];
        auto seq = sequenceOf (model, grp == 0 ? "patch.fx.a_sequence" : "patch.fx.b_sequence");
        int n = grp == 0 ? 4 : 3;
        float w = std::min (150.0f, (float) (area.getWidth() - 30) / (float) n);
        for (int pos = 0; pos < seq.size() && pos < n; ++pos)
        {
            // Effekt an dieser Position
            int fx = -1;
            for (int i = 0; i < n; ++i)
                if (seq[pos] == (grp == 0 ? fxA[i].code : fxB[i].code)) fx = i;
            // Schalter gehört fest zum Effekt (Block 1 = DS/CHO, 2 = PH/DLY …), nicht zur Position —
            // Hörtest André 2026-09-28: Spectrum vor Distortion geschoben → Distortion aus
            auto* sw = fx >= 0 ? model.find (juce::String (grp == 0 ? "patch.fx.a_sw_" : "patch.fx.b_sw_") + juce::String (fx + 1)) : nullptr;
            bool on = sw && model.get (sw->index, -1) == 1;
            bool selected = fx == (grp == 0 ? selectedA : selectedB);
            juce::Rectangle<float> b ((float) area.getX() + (float) pos * (w + 24.0f), (float) area.getY() + 14, w, 44);
            g.setColour (selected ? theme::raised.brighter (0.1f) : theme::sunk);
            g.fillRoundedRectangle (b, 3.0f);
            g.setColour (grp == 0 ? theme::fxA : theme::fxB);
            g.drawRoundedRectangle (b, 3.0f, selected ? 2.0f : 1.0f);
            g.setColour (on ? theme::silk : theme::silkDim);
            g.setFont (theme::label (12.0f));
            g.drawText (fx >= 0 ? (grp == 0 ? fxA[fx].name : fxB[fx].name) : seq[pos], b.withTrimmedBottom (14), juce::Justification::centred);
            // Schalter („LED“) des Effekts
            auto led = juce::Rectangle<float> (b.getCentreX() - 18, b.getBottom() - 15, 36, 11);
            g.setColour (on ? juce::Colour (0xff8fd48f) : theme::edge);
            g.fillRoundedRectangle (led, 2.0f);
            g.setColour (theme::ink);
            g.setFont (theme::label (8.5f));
            g.drawText (on ? "ON" : "OFF", led, juce::Justification::centred);
            if (pos < n - 1)
            {
                g.setColour (theme::silkDim);
                g.drawArrow (juce::Line<float> (b.getRight() + 3, b.getCentreY(), b.getRight() + 21, b.getCentreY()), 1.5f, 7.0f, 7.0f);
            }
        }
        if (dragging && dragGroup == grp)
        {
            auto target = blockRect (grp, dropPosition (grp, dragX));
            g.setColour (theme::toneOn);
            g.drawRoundedRectangle (target.expanded (3), 4.0f, 2.0f);
            auto ghost = blockRect (grp, dragFrom).withCentre ({ dragX, blockRect (grp, dragFrom).getCentreY() });
            g.setColour (theme::toneOn.withAlpha (0.25f));
            g.fillRoundedRectangle (ghost, 3.0f);
        }
    }
}

juce::Rectangle<float> PatchView::blockRect (int grp, int pos) const
{
    auto area = chainArea[grp];
    int n = grp == 0 ? 4 : 3;
    float w = std::min (150.0f, (float) (area.getWidth() - 30) / (float) n);
    return { (float) area.getX() + (float) pos * (w + 24.0f), (float) area.getY() + 14, w, 44 };
}

int PatchView::dropPosition (int grp, float x) const
{
    int n = grp == 0 ? 4 : 3, best = 0;
    float bestD = 1e9f;
    for (int p = 0; p < n; ++p)
    {
        float d = std::abs (blockRect (grp, p).getCentreX() - x);
        if (d < bestD) { bestD = d; best = p; }
    }
    return best;
}

void PatchView::dragStart (const juce::MouseEvent& e)
{
    dragging = false;
    dragGroup = -1;
    if (! model.hasPatch()) return;
    for (int grp = 0; grp < 2; ++grp)
        for (int pos = 0; pos < (grp == 0 ? 4 : 3); ++pos)
            if (blockRect (grp, pos).contains (e.position)
                && ! juce::Rectangle<float> (blockRect (grp, pos).getCentreX() - 22, blockRect (grp, pos).getBottom() - 19, 44, 19).contains (e.position))
            {
                dragGroup = grp;
                dragFrom = pos;
                dragX = e.position.x;
            }
}

void PatchView::dragMove (const juce::MouseEvent& e)
{
    if (dragGroup < 0) return;
    if (! dragging && e.getDistanceFromDragStart() < 6) return;
    dragging = true;
    dragX = e.position.x;
    content.repaint();
}

bool PatchView::dragEnd (const juce::MouseEvent& e)
{
    bool was = dragging;
    if (dragging)
        moveEffect (dragGroup, dragFrom, dropPosition (dragGroup, e.position.x));
    dragging = false;
    dragGroup = -1;
    content.repaint();
    return was;
}

void PatchView::moveEffect (int grp, int from, int to)
{
    const char* seqId = grp == 0 ? "patch.fx.a_sequence" : "patch.fx.b_sequence";
    auto seq = sequenceOf (model, seqId);
    int n = grp == 0 ? 4 : 3;
    if (seq.size() != n || from == to || from < 0 || to < 0 || from >= n || to >= n) return;
    // neue Reihenfolge
    auto order = seq;
    auto moved = order[from];
    order.remove (from);
    order.insert (to, moved);
    auto wanted = order.joinIntoString ("-");
    auto* seqP = model.find (seqId);
    int index = -1;
    char buf[64];
    for (int v = seqP->info.min; v <= seqP->info.max; ++v)
    {
        jd990_param_format (seqP->index, (uint16_t) v, buf, sizeof buf);
        if (juce::String::fromUTF8 (buf) == wanted) index = v;
    }
    if (index < 0) return;
    // Nur die Sequenz ändert sich — die Schalter gehören fest zum Effekt und bleiben, wo sie sind
    // (vorher wanderten sie mit: falsche Annahme „Schalter je Position“, Logbuch 2026-09-28)
    PatchPtr next (jd990_patch_clone (model.patch()));
    uint8_t msg[32];
    jd990_patch_set (next.get(), seqP->index, -1, (uint16_t) index, msg, sizeof msg);
    model.lastIndex = (int) seqP->index;
    model.applyWithUndo (std::move (next), juce::String (grp == 0 ? "Gruppe A: " : "Gruppe B: ") + wanted);
}

void PatchView::clickContent (const juce::MouseEvent& e)
{
    if (! model.hasPatch()) return;
    auto p = e.getPosition();
    // Structure-Kacheln
    {
        auto s = structArea[0];
        s.removeFromTop (22);
        const char* ids[2] = { "patch.structure_ab", "patch.structure_cd" };
        for (int pr = 0; pr < 2; ++pr)
        {
            auto row = s.removeFromTop ((s.getHeight()) / (2 - pr)).reduced (6, 4);
            row.removeFromLeft (44);
            float w = std::min (110.0f, (float) row.getWidth() / 6.0f);
            for (int t = 0; t < 6; ++t)
                if (juce::Rectangle<float> ((float) row.getX() + (float) t * w, (float) row.getY(), w - 6, (float) std::min (row.getHeight(), 60)).contains (p.toFloat()))
                    if (auto* prm = model.find (ids[pr])) { model.lastIndex = (int) prm->index; model.set (prm->index, -1, t); }
        }
    }
    // Effektblöcke: Klick wählt, Klick auf ON/OFF schaltet
    for (int grp = 0; grp < 2; ++grp)
    {
        auto area = chainArea[grp];
        auto seq = sequenceOf (model, grp == 0 ? "patch.fx.a_sequence" : "patch.fx.b_sequence");
        int n = grp == 0 ? 4 : 3;
        float w = std::min (150.0f, (float) (area.getWidth() - 30) / (float) n);
        for (int pos = 0; pos < seq.size() && pos < n; ++pos)
        {
            juce::Rectangle<float> b ((float) area.getX() + (float) pos * (w + 24.0f), (float) area.getY() + 14, w, 44);
            if (! b.contains (p.toFloat())) continue;
            auto led = juce::Rectangle<float> (b.getCentreX() - 18, b.getBottom() - 15, 36, 11).expanded (4);
            if (led.contains (p.toFloat()))
            {
                int fx = -1;
                for (int i = 0; i < n; ++i)
                    if (seq[pos] == (grp == 0 ? fxA[i].code : fxB[i].code)) fx = i;
                if (fx >= 0)
                    if (auto* sw = model.find (juce::String (grp == 0 ? "patch.fx.a_sw_" : "patch.fx.b_sw_") + juce::String (fx + 1)))
                    {
                        model.lastIndex = (int) sw->index;
                        model.set (sw->index, -1, 1 - model.get (sw->index, -1));
                    }
            }
            else
            {
                for (int i = 0; i < n; ++i)
                    if (seq[pos] == (grp == 0 ? fxA[i].code : fxB[i].code)) (grp == 0 ? selectedA : selectedB) = i;
                layoutContent();
                content.repaint();
            }
        }
    }
}

// ── GenericView (Alle Parameter) ─────────────────────────────────────────────

struct GenericView::Row
{
    const Model::Param* p = nullptr;
    juce::Label name, value;
    KeySlider slider;
    juce::ComboBox choice;
    bool useChoice = false;
};

GenericView::GenericView (Model& m) : model (m)
{
    // Die M3-Ansicht als „Expertenmodus“: jede Zeile ein Parameter, Patch links,
    // Tone rechts, Reihenfolge nach group_order. Vollständig und zur Fehlersuche.
    std::vector<const Model::Param*> order;
    for (auto& p : m.params()) order.push_back (&p);
    std::stable_sort (order.begin(), order.end(), [] (auto* a, auto* b) {
        return std::tie (a->info.is_tone, a->info.group_rank) < std::tie (b->info.is_tone, b->info.group_rank);
    });
    juce::String group;
    bool isTone = false;
    for (auto* p : order)
    {
        if (p->group != group || p->isTone() != isTone || headers.empty())
        {
            group = p->group;
            isTone = p->isTone();
            auto l = std::make_unique<juce::Label>();
            l->setText (group, juce::dontSendNotification);
            l->setFont (theme::label (12.0f));
            l->setColour (juce::Label::textColourId, theme::accent (group));
            content.addAndMakeVisible (*l);
            if (isTone) toneHeaders.emplace_back (l.get(), group);
            headers.push_back (std::move (l));
        }
        auto r = std::make_unique<Row>();
        r->p = p;
        r->name.setText (p->label, juce::dontSendNotification);
        r->name.setTooltip (p->id);
        r->name.setColour (juce::Label::textColourId, theme::silkDim);
        auto& info = p->info;
        r->useChoice = info.kind == 1 || info.kind == 5 || (info.kind == 2 && info.max - info.min < 48);
        auto* row = r.get();
        if (r->useChoice)
        {
            char buf[64];
            for (int v = info.min; v <= info.max; ++v)
            {
                jd990_param_format (p->index, (uint16_t) v, buf, sizeof buf);
                r->choice.addItem (juce::String::fromUTF8 (buf), v + 1);
                if (jd990_param_locked (p->index, (uint16_t) v) == 1) r->choice.setItemEnabled (v + 1, false);
            }
            r->choice.onChange = [this, row] { model.set (row->p->index, row->p->isTone() ? model.tone : -1, row->choice.getSelectedId() - 1); };
            content.addAndMakeVisible (r->choice);
        }
        else
        {
            r->slider.setSliderStyle (juce::Slider::LinearHorizontal);
            r->slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 20);
            r->slider.setRange (info.min, info.max, 1.0);
            r->slider.setDoubleClickReturnValue (true, jd990_param_init (p->index));
            r->slider.setColour (juce::Slider::trackColourId, theme::accent (p->group).withAlpha (0.6f));
            r->slider.setColour (juce::Slider::thumbColourId, theme::cap);
            r->slider.setColour (juce::Slider::backgroundColourId, theme::sunk);
            r->slider.setColour (juce::Slider::textBoxTextColourId, theme::silk);
            r->slider.setColour (juce::Slider::textBoxOutlineColourId, theme::edge);
            r->slider.onValueChange = [this, row] { model.set (row->p->index, row->p->isTone() ? model.tone : -1, (int) row->slider.getValue()); };
            r->value.setFont (theme::mono (11.0f));
            content.addAndMakeVisible (r->slider);
            content.addAndMakeVisible (r->value);
        }
        content.addAndMakeVisible (r->name);
        rows.push_back (std::move (r));
    }
    view.setViewedComponent (&content, false);
    view.setScrollBarsShown (true, false);
    addAndMakeVisible (view);
    model.addChangeListener (this);
    changeListenerCallback (nullptr);
}

GenericView::~GenericView() { model.removeChangeListener (this); }

void GenericView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto& [l, g] : toneHeaders)
        l->setText (juce::String ("TONE ") + toneLetter[model.tone] + u (" · ") + g, juce::dontSendNotification);
    content.setEnabled (model.hasPatch());
    content.setAlpha (model.hasPatch() ? 1.0f : 0.4f);
    for (auto& r : rows)
    {
        int t = r->p->isTone() ? model.tone : -1;
        int v = model.get (r->p->index, t);
        if (r->useChoice) r->choice.setSelectedId (v + 1, juce::dontSendNotification);
        else
        {
            r->slider.setValue (v, juce::dontSendNotification);
            r->value.setText (model.hasPatch() ? model.display (r->p->index, t) : juce::String(), juce::dontSendNotification);
        }
    }
}

void GenericView::resized()
{
    view.setBounds (getLocalBounds());
    const int colW = (view.getWidth() - 36) / 2, rowH = 22, headH = 28;
    int y[2] = { 6, 6 };
    size_t h = 0;
    juce::String prev;
    bool prevTone = false, first = true;
    for (auto& r : rows)
    {
        int c = r->p->isTone() ? 1 : 0;
        if (first || r->p->group != prev || r->p->isTone() != prevTone)
        {
            headers[h++]->setBounds (10 + c * (colW + 12), y[c] + 4, colW, 22);
            y[c] += headH;
        }
        first = false; prev = r->p->group; prevTone = r->p->isTone();
        int x = 10 + c * (colW + 12);
        r->name.setBounds (x, y[c], 110, rowH);
        if (r->useChoice) r->choice.setBounds (x + 112, y[c] + 1, colW - 116, rowH - 2);
        else
        {
            r->slider.setBounds (x + 112, y[c], colW - 112 - 122, rowH);
            r->value.setBounds (x + colW - 118, y[c], 118, rowH);
        }
        y[c] += rowH;
    }
    content.setSize (view.getWidth() - 12, std::max (y[0], y[1]) + 10);
}
