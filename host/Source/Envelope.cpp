#include "Envelope.h"

namespace
{
    juce::String u (const char* s) { return juce::String::fromUTF8 (s); }
    constexpr float holdUnits = 45.0f;   // Breite der Halte-Phase (Key gedrückt), in Zeiteinheiten
    constexpr float minUnits = 6.0f;     // Mindestbreite je Segment, damit Zeit 0 sichtbar bleibt
}

// ── Hüllkurven-Form (gemeinsam für EnvelopeView und LCD) ─────────────────────

bool envshape::supports (const juce::String& group)
{
    return group == "PITCH ENV" || group == "TVF ENV" || group == "TVA ENV";
}

envshape::Shape envshape::forGroup (const Model& model, const juce::String& group)
{
    Shape s;
    auto p = [&] (const juce::String& id) { return model.find (id); };
    if (group == "PITCH ENV")
    {
        // L0 → T1/L1 → T2/SUS → (halten) → T3/L3 · Pegel bipolar (−50 … +50)
        s.bipolar = true;
        s.sustainIndex = 2;
        s.points = { { nullptr, p ("tone.pitch_env.level_0"), "", "L0" },
                     { p ("tone.pitch_env.time_1"), p ("tone.pitch_env.level_1"), "T1", "L1" },
                     { p ("tone.pitch_env.time_2"), p ("tone.pitch_env.sustain"), "T2", "SUS" },
                     { p ("tone.pitch_env.time_3"), p ("tone.pitch_env.level_3"), "T3", "L3" } };
    }
    else if (group == "TVF ENV" || group == "TVA ENV")
    {
        // 0 → T1/L1 → T2/L2 → T3/SUS → (halten) → T4/L4 (TVA: → 0)
        bool tvf = group == "TVF ENV";
        juce::String g = tvf ? "tone.tvf_env." : "tone.tva_env.";
        s.sustainIndex = 3;
        s.points = { { nullptr, nullptr, "", "" },
                     { p (g + "time_1"), p (g + "level_1"), "T1", "L1" },
                     { p (g + "time_2"), p (g + "level_2"), "T2", "L2" },
                     { p (g + "time_3"), p (g + "sustain"), "T3", "SUS" },
                     { p (g + "time_4"), tvf ? p (g + "level_4") : nullptr, "T4", tvf ? "L4" : "" } };
    }
    return s;
}

envshape::Layout envshape::layout (const Shape& s, const Model& m, int tone, juce::Rectangle<float> r)
{
    Layout out;
    if (! s.valid()) return out;
    auto val = [&] (const Model::Param* p, int fallback) { return p != nullptr && m.hasPatch() ? m.get (p->index, tone) : fallback; };
    auto levelY = [&] (int raw) { return r.getBottom() - (float) juce::jlimit (0, 100, raw) / 100.0f * r.getHeight(); };

    // Segmentbreite ∝ Zeit; die Halte-Phase hat eine feste Breite
    float units = holdUnits;
    for (size_t i = 1; i < s.points.size(); ++i) units += (float) val (s.points[i].time, 0) + minUnits;
    float pxPerUnit = r.getWidth() / units;
    out.unitsPerPixel = 1.0f / pxPerUnit;
    out.baseY = s.bipolar ? levelY (50) : r.getBottom();

    float x = r.getX();
    for (size_t i = 0; i < s.points.size(); ++i)
    {
        if ((int) i == s.sustainIndex + 1) { x += holdUnits * pxPerUnit; out.holdEndX = x; }
        if (i > 0) x += ((float) val (s.points[i].time, 0) + minUnits) * pxPerUnit;
        out.pos.push_back ({ x, levelY (val (s.points[i].level, s.bipolar ? 50 : 0)) });
    }
    return out;
}

juce::Path envshape::line (const Shape& s, const Layout& l)
{
    juce::Path p;
    if (l.pos.empty()) return p;
    p.startNewSubPath (l.pos[0]);
    for (size_t i = 1; i < l.pos.size(); ++i)
    {
        if ((int) i == s.sustainIndex + 1) p.lineTo (l.holdEndX, l.pos[i - 1].y);
        p.lineTo (l.pos[i]);
    }
    return p;
}

// ── EnvelopeView ─────────────────────────────────────────────────────────────

EnvelopeView::EnvelopeView (Model& m, const juce::String& group, juce::Colour a)
    : model (m), accent (a), shape (envshape::forGroup (m, group))
{
    setTooltip (u ("Punkte ziehen: waagerecht = Zeit, senkrecht = Pegel · ⇧ = fein · ⌥ = nur eine Richtung · Doppelklick = Init-Wert"));
    model.addChangeListener (this);
}

EnvelopeView::~EnvelopeView() { model.removeChangeListener (this); }

bool EnvelopeView::isShapeParam (const Model::Param* p) const
{
    for (auto& pt : shape.points)
        if (p != nullptr && (pt.time == p || pt.level == p)) return true;
    return false;
}

int EnvelopeView::valueOf (const Model::Param* p) const
{
    return p != nullptr && model.hasPatch() ? model.get (p->index, model.tone) : (shape.bipolar ? 50 : 0);
}

juce::Rectangle<float> EnvelopeView::plot() const
{
    return getLocalBounds().toFloat().reduced (18.0f, 0.0f).withTrimmedTop (18.0f).withTrimmedBottom (20.0f);
}

void EnvelopeView::paint (juce::Graphics& g)
{
    auto r = plot();
    g.setColour (theme::sunk);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 3.0f);
    if (! model.hasPatch() || ! shape.valid()) return;

    auto l = currentLayout();
    auto& pos = l.pos;
    auto s = pos[(size_t) shape.sustainIndex];

    // Nulllinie (Pitch: keine Verstimmung) bzw. Grundlinie
    g.setColour (theme::edge);
    g.drawHorizontalLine ((int) l.baseY, r.getX(), r.getRight());

    // Fläche + Linie
    auto line = envshape::line (shape, l);
    juce::Path area (line);
    area.lineTo (pos.back().x, l.baseY);
    area.lineTo (pos.front().x, l.baseY);
    area.closeSubPath();
    g.setColour (accent.withAlpha (0.18f));
    g.fillPath (area);
    g.setColour (accent);
    g.strokePath (line, juce::PathStrokeType (2.0f));

    // Halten (Taste gedrückt) · Key-Off-Marke
    float dashes[] = { 4.0f, 4.0f };
    g.setColour (theme::silkDim);
    g.drawDashedLine (juce::Line<float> (l.holdEndX, r.getY() - 4, l.holdEndX, r.getBottom()), dashes, 2, 1.0f);
    g.setFont (theme::label (9.0f));
    g.drawText ("KEY OFF", juce::Rectangle<float> (l.holdEndX + 4, r.getY() - 16, 60, 12), juce::Justification::centredLeft);
    g.drawText (u ("HALTEN"), juce::Rectangle<float> (s.x, s.y - 15, l.holdEndX - s.x, 12), juce::Justification::centred);

    // Punkte, Beschriftung; zuletzt bewegter Parameter / Maus hervorgehoben
    const Model::Param* last = model.lastIndex >= 0 ? &model.params()[(size_t) model.lastIndex] : nullptr;
    for (size_t i = 0; i < pos.size(); ++i)
    {
        auto& p = shape.points[i];
        bool touched = last != nullptr && (last == p.time || last == p.level);
        bool active = touched || (int) i == hover || (int) i == dragging;
        if (p.time != nullptr || p.level != nullptr)
        {
            g.setColour (active ? theme::toneOn : accent);
            float rad = active ? 6.0f : 4.5f;
            g.fillEllipse (pos[i].x - rad, pos[i].y - rad, 2 * rad, 2 * rad);
        }
        g.setFont (theme::label (10.0f));
        if (p.levelLabel.isNotEmpty())
        {
            g.setColour (touched && last == p.level ? theme::toneOn : theme::silk);
            float ly = pos[i].y < r.getY() + 14 ? pos[i].y + 7 : pos[i].y - 19;
            g.drawText (p.levelLabel, juce::Rectangle<float> (pos[i].x - 22, ly, 44, 12), juce::Justification::centred);
        }
        if (i > 0 && p.timeLabel.isNotEmpty())
        {
            float x0 = (int) i == shape.sustainIndex + 1 ? l.holdEndX : pos[i - 1].x;
            g.setColour (touched && last == p.time ? theme::toneOn : theme::silkDim);
            g.drawText (p.timeLabel, juce::Rectangle<float> (x0, r.getBottom() + 4, juce::jmax (18.0f, pos[i].x - x0), 13), juce::Justification::centred);
        }
    }

    // Zahlen am Punkt unter der Maus bzw. beim Ziehen: „T1 34 · L1 +25“
    int shown = dragging >= 0 ? dragging : hover;
    if (shown >= 0)
    {
        auto& p = shape.points[(size_t) shown];
        juce::StringArray parts;
        if (p.time != nullptr)  parts.add (p.timeLabel + " " + model.display (p.time->index, model.tone));
        if (p.level != nullptr) parts.add (p.levelLabel + " " + model.display (p.level->index, model.tone));
        if (dragging >= 0 && axisLock != 0) parts.add (axisLock == 1 ? u ("nur Zeit") : u ("nur Pegel"));
        auto text = parts.joinIntoString (u (" · "));
        g.setFont (theme::mono (11.0f));
        float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 14.0f, h = 18.0f;
        auto pt = pos[(size_t) shown];
        float bx = juce::jlimit (4.0f, (float) getWidth() - w - 4.0f, pt.x + 10.0f);
        float by = pt.y - h - 10.0f < 2.0f ? pt.y + 10.0f : pt.y - h - 10.0f;
        juce::Rectangle<float> box (bx, by, w, h);
        g.setColour (theme::ink.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (theme::toneOn);
        g.drawRoundedRectangle (box, 3.0f, 1.0f);
        g.drawText (text, box, juce::Justification::centred);
    }
}

int EnvelopeView::nearest (juce::Point<float> m) const
{
    auto l = currentLayout();
    int best = -1;
    float bestD = 16.0f;
    for (size_t i = 0; i < l.pos.size(); ++i)
    {
        if (shape.points[i].time == nullptr && shape.points[i].level == nullptr) continue;
        float d = l.pos[i].getDistanceFrom (m);
        if (d < bestD) { bestD = d; best = (int) i; }
    }
    return best;
}

void EnvelopeView::mouseMove (const juce::MouseEvent& e)
{
    int n = model.hasPatch() ? nearest (e.position) : -1;
    if (n != hover) { hover = n; repaint(); }
    setMouseCursor (n >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void EnvelopeView::mouseDown (const juce::MouseEvent& e)
{
    dragging = model.hasPatch() ? nearest (e.position) : -1;
    if (dragging < 0) return;
    auto& p = shape.points[(size_t) dragging];
    startTime = valueOf (p.time);
    startLevel = valueOf (p.level);
    dragUnitsPerPixel = currentLayout().unitsPerPixel;   // Maßstab beim Anfassen festhalten
    axisLock = 0;
    model.beginGesture();
    model.lastIndex = (int) (p.level != nullptr ? p.level : p.time)->index;
    model.lastTone = model.tone;
    model.sendChangeMessage();
}

void EnvelopeView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    auto& p = shape.points[(size_t) dragging];
    float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
    // ⌥: Richtung nach den ersten Pixeln festlegen — dann ändert sich nur Zeit oder nur Pegel
    int dx = e.getDistanceFromDragStartX(), dy = e.getDistanceFromDragStartY();
    if (! e.mods.isAltDown()) axisLock = 0;
    else if (axisLock == 0 && std::abs (dx) + std::abs (dy) >= 4) axisLock = std::abs (dx) >= std::abs (dy) ? 1 : 2;
    if (axisLock == 0 && e.mods.isAltDown()) return;
    if (p.time != nullptr && axisLock != 2)
    {
        int t = juce::jlimit ((int) p.time->info.min, (int) p.time->info.max,
                              startTime + juce::roundToInt ((float) e.getDistanceFromDragStartX() * dragUnitsPerPixel * fine));
        if (t != valueOf (p.time)) model.set (p.time->index, model.tone, t);
    }
    if (p.level != nullptr && axisLock != 1)
    {
        int l = juce::jlimit ((int) p.level->info.min, (int) p.level->info.max,
                              startLevel - juce::roundToInt ((float) e.getDistanceFromDragStartY() / plot().getHeight() * 100.0f * fine));
        if (l != valueOf (p.level)) model.set (p.level->index, model.tone, l);
    }
}

void EnvelopeView::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0) model.endGesture();
    dragging = -1;
    repaint();
}

void EnvelopeView::mouseDoubleClick (const juce::MouseEvent& e)
{
    int n = model.hasPatch() ? nearest (e.position) : -1;
    if (n < 0) return;
    auto& p = shape.points[(size_t) n];
    model.beginGesture();
    for (auto* q : { p.time, p.level })
        if (q != nullptr) model.set (q->index, model.tone, jd990_param_init (q->index));
    model.endGesture();
}

// ── LCD-Skizzen ──────────────────────────────────────────────────────────────

namespace
{
    float hash01 (int n)   // deterministisches „Zufalls“-Muster für S&H/RND/CHS
    {
        uint32_t x = (uint32_t) n * 747796405u + 2891336453u;
        x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
        return (float) ((x >> 22u) ^ x) / 4294967295.0f;
    }

    /** LFO-Wellenform, Phase in Zyklen → −1 … +1. */
    float lfoWave (int wave, float phase)
    {
        float p = phase - std::floor (phase);
        int cycle = (int) std::floor (phase);
        auto tri = [] (float q) { return q < 0.25f ? 4 * q : q < 0.75f ? 2 - 4 * q : 4 * q - 4; };
        switch (wave)
        {
            case 0: return tri (p);                                                     // TRI
            case 1: return std::sin (juce::MathConstants<float>::twoPi * p);            // SIN
            case 2: return 1.0f - 2.0f * p;                                             // SAW
            case 3: return p < 0.5f ? 1.0f : -1.0f;                                     // SQU
            case 4: return juce::jlimit (-1.0f, 1.0f, 2.0f * tri (p));                  // TRP
            case 5: return hash01 (cycle) * 2 - 1;                                      // S&H
            case 6:                                                                     // RND (weich)
            {
                float a = hash01 (cycle) * 2 - 1, b = hash01 (cycle + 1) * 2 - 1;
                return a + (b - a) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * p));
            }
            default:                                                                    // CHS (chaotisch)
            {
                int step = (int) std::floor (phase * (1.0f + 3.0f * hash01 (cycle * 7 + 3)));
                return hash01 (step * 13 + cycle) * 2 - 1;
            }
        }
    }

    void drawLfo (juce::Graphics& g, juce::Rectangle<float> r, const Model& m, const juce::String& prefix, int tone, juce::Colour ink)
    {
        auto val = [&] (const char* id) { auto* p = m.find (prefix + id); return p != nullptr ? m.get (p->index, tone) : 0; };
        int wave = val ("waveform"), rate = val ("rate"), delay = val ("delay"), fade = val ("fade") - 50, offset = val ("offset");
        float midY = r.getCentreY(), amp = r.getHeight() * 0.5f;
        float dashes[] = { 3.0f, 3.0f };
        g.setColour (ink.withAlpha (0.35f));
        g.drawDashedLine (juce::Line<float> (r.getX(), midY, r.getRight(), midY), dashes, 2, 1.0f);

        bool release = delay > 100;                              // Delay „REL“: LFO erst nach dem Loslassen
        float x0 = r.getX() + r.getWidth() * (release ? 0.55f : 0.4f * (float) delay / 100.0f);
        float fadeW = r.getWidth() * 0.6f * (float) std::abs (fade) / 50.0f;
        float cycles = 0.6f + 11.0f * (float) rate / 100.0f;
        juce::Path p;
        p.startNewSubPath (r.getX(), midY);
        for (float x = x0; x <= r.getRight(); x += 1.0f)
        {
            float t = (x - x0) / (r.getRight() - x0);
            float env = fade > 0 ? juce::jmin (1.0f, (x - x0) / juce::jmax (1.0f, fadeW))           // Einblenden
                      : fade < 0 ? juce::jmax (0.0f, 1.0f - (x - x0) / juce::jmax (1.0f, fadeW))    // Ausblenden
                      : 1.0f;
            float w = lfoWave (wave, t * cycles) * env;
            if (offset == 0) w = (w + env) * 0.5f;                               // „+“: nur nach oben
            else if (offset == 2) w = (w - env) * 0.5f;                          // „−“: nur nach unten
            p.lineTo (x, midY - w * amp * 0.9f);
        }
        g.setColour (ink);
        g.strokePath (p, juce::PathStrokeType (1.6f));
        g.setFont (theme::mono (9.5f));
        g.setColour (ink.withAlpha (0.7f));
        if (release) g.drawText ("KEY OFF", juce::Rectangle<float> (x0 + 3, r.getY() - 2, 60, 11), juce::Justification::centredLeft);
        else if (delay > 0) g.drawText ("DELAY", juce::Rectangle<float> (r.getX(), r.getY() - 2, 60, 11), juce::Justification::centredLeft);
    }

    void drawFilter (juce::Graphics& g, juce::Rectangle<float> r, const Model& m, int tone, juce::Colour ink)
    {
        auto val = [&] (const char* id) { auto* p = m.find (juce::String ("tone.tvf.") + id); return p != nullptr ? m.get (p->index, tone) : 0; };
        int mode = val ("filter_mode");                          // 0 HPF · 1 BPF · 2 LPF
        float xc = (float) val ("cutoff") / 100.0f, q = 0.6f + 9.0f * std::pow ((float) val ("resonance") / 100.0f, 1.6f);
        const float dbTop = 18.0f, dbBottom = -42.0f;
        auto yOf = [&] (float db) { return r.getY() + (dbTop - juce::jlimit (dbBottom, dbTop, db)) / (dbTop - dbBottom) * r.getHeight(); };
        float dashes[] = { 3.0f, 3.0f };
        g.setColour (ink.withAlpha (0.35f));
        g.drawDashedLine (juce::Line<float> (r.getX(), yOf (0), r.getRight(), yOf (0)), dashes, 2, 1.0f);
        g.drawDashedLine (juce::Line<float> (r.getX() + xc * r.getWidth(), r.getY(), r.getX() + xc * r.getWidth(), r.getBottom()), dashes, 2, 1.0f);

        juce::Path p;
        bool first = true;
        for (float x = r.getX(); x <= r.getRight(); x += 1.0f)
        {
            float f = std::pow (2.0f, ((x - r.getX()) / r.getWidth() - xc) * 9.0f);   // 9 Oktaven Anzeigebereich
            float den = std::sqrt ((1 - f * f) * (1 - f * f) + (f / q) * (f / q));
            float h = mode == 2 ? 1.0f / den : mode == 0 ? f * f / den : (f / q) / den;
            float db = 20.0f * std::log10 (juce::jmax (1.0e-4f, h));
            if (first) { p.startNewSubPath (x, yOf (db)); first = false; } else p.lineTo (x, yOf (db));
        }
        g.setColour (ink);
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }
}

namespace
{
    int raw (const Model& m, const char* id) { auto* p = m.find (id); return p != nullptr ? m.get (p->index, -1) : 0; }
    /** Anzeigewert als Zahl („315 Hz“ → 315, „1.25 kHz“ → 1250, „300 ms“ → 300). */
    float number (const Model& m, const char* id)
    {
        auto* p = m.find (id);
        if (p == nullptr) return 0.0f;
        auto t = m.display (p->index, -1).trim();
        float v = t.retainCharacters ("0123456789.-+").getFloatValue();
        if (t.containsIgnoreCase ("kHz")) v *= 1000.0f;
        if (t.endsWithIgnoreCase (" s") && ! t.containsIgnoreCase ("ms")) v *= 1000.0f;
        return v;
    }
    void dashes (juce::Graphics& g, juce::Line<float> l, juce::Colour ink)
    {
        float d[] = { 3.0f, 3.0f };
        g.setColour (ink.withAlpha (0.35f));
        g.drawDashedLine (l, d, 2, 1.0f);
    }
    void caption (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour ink, bool right = false)
    {
        g.setFont (theme::mono (9.5f));
        g.setColour (ink.withAlpha (0.75f));
        g.drawText (text, r.withHeight (11), right ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }
    juce::Path curve (juce::Rectangle<float> r, std::function<float (float)> yOfT)   // t 0…1 über die Breite → y 0 (unten) … 1 (oben)
    {
        juce::Path p;
        bool first = true;
        for (float x = r.getX(); x <= r.getRight(); x += 1.0f)
        {
            float y = r.getBottom() - juce::jlimit (0.0f, 1.0f, yOfT ((x - r.getX()) / r.getWidth())) * r.getHeight();
            if (first) { p.startNewSubPath (x, y); first = false; } else p.lineTo (x, y);
        }
        return p;
    }

    // Frequenzachse 20 Hz … 20 kHz, logarithmisch
    float tOfHz (float hz) { return std::log10 (juce::jlimit (20.0f, 20000.0f, hz) / 20.0f) / 3.0f; }
    float hzOfT (float t) { return 20.0f * std::pow (10.0f, 3.0f * t); }
    float bell (float hz, float f0, float q, float gainDb)       // Glocke in dB (Oktavabstand, Breite ~ 1/Q)
    {
        float oct = std::log2 (hz / f0);
        return gainDb * std::exp (-oct * oct * q * q * 1.4f);
    }
    float shelf (float hz, float f0, float gainDb, bool low)     // weicher Kuhschwanz in dB
    {
        float s = 1.0f / (1.0f + std::pow (hz / f0, low ? 2.0f : -2.0f));
        return gainDb * s;
    }

    void drawEq (juce::Graphics& g, juce::Rectangle<float> r, const Model& m, juce::Colour ink)
    {
        float lowF = number (m, "patch.eq.low_freq"), midF = number (m, "patch.eq.mid_freq"), highF = number (m, "patch.eq.high_freq");
        float lowG = (float) raw (m, "patch.eq.low_gain") - 15, midG = (float) raw (m, "patch.eq.mid_gain") - 15, highG = (float) raw (m, "patch.eq.high_gain") - 15;
        float q = number (m, "patch.eq.mid_q");
        dashes (g, { r.getX(), r.getCentreY(), r.getRight(), r.getCentreY() }, ink);
        for (float f : { lowF, midF, highF }) dashes (g, { r.getX() + tOfHz (f) * r.getWidth(), r.getY(), r.getX() + tOfHz (f) * r.getWidth(), r.getBottom() }, ink);
        g.setColour (ink);
        g.strokePath (curve (r, [&] (float t) {
            float hz = hzOfT (t);
            float db = shelf (hz, lowF, lowG, true) + bell (hz, midF, juce::jmax (0.3f, q), midG) + shelf (hz, highF, highG, false);
            return 0.5f + db / 40.0f;                                                   // ±20 dB
        }), juce::PathStrokeType (1.6f));
        caption (g, r.withY (r.getBottom() - 10), "20 Hz", ink);
        caption (g, r.withY (r.getBottom() - 10), "20 kHz", ink, true);
    }

    void drawDistortion (juce::Graphics& g, juce::Rectangle<float> r, const Model& m, juce::Colour ink)
    {
        int type = raw (m, "patch.fx.ds_type");
        float drive = 1.0f + (float) raw (m, "patch.fx.ds_drive") / 100.0f * (type >= 3 ? 30.0f : 12.0f);
        float level = 0.3f + 0.7f * (float) raw (m, "patch.fx.ds_level") / 100.0f;
        // Eingang: Sinus (gestrichelt) · Ausgang: verzerrt (je Typ weicher/härter)
        auto in = [] (float t) { return std::sin (juce::MathConstants<float>::twoPi * 2.0f * t); };
        auto shape = [&] (float x) {
            float y = std::tanh (drive * x);
            if (type == 6) y = juce::jlimit (-1.0f, 1.0f, drive * x * 2.0f);                // Fuzz: hart
            else if (type == 2) y = std::tanh (drive * x) * (1.0f + 0.25f * std::sin (6.0f * x)); // Cry: färbt
            return y * level;
        };
        dashes (g, { r.getX(), r.getCentreY(), r.getRight(), r.getCentreY() }, ink);
        g.setColour (ink.withAlpha (0.4f));
        g.strokePath (curve (r, [&] (float t) { return 0.5f + 0.45f * in (t); }), juce::PathStrokeType (1.0f));
        g.setColour (ink);
        g.strokePath (curve (r, [&] (float t) { return 0.5f + 0.45f * shape (in (t)); }), juce::PathStrokeType (1.6f));
    }

    void drawPhaser (juce::Graphics& g, juce::Rectangle<float> r, const Model& m, juce::Colour ink)
    {
        float manual = number (m, "patch.fx.ph_manual");
        if (manual <= 0.0f) manual = 1000.0f;
        float depth = (float) raw (m, "patch.fx.ph_depth") / 100.0f, reso = (float) raw (m, "patch.fx.ph_reso") / 100.0f;
        float mix = (float) raw (m, "patch.fx.ph_mix") / 100.0f;
        // Kerben (Notches) um „Manual“; Depth = wie weit das LFO sie verschiebt (Band), Resonanz = Spitzen
        float lo = manual / (1.0f + 3.0f * depth), hi = manual * (1.0f + 3.0f * depth);
        auto band = juce::Rectangle<float> (r.getX() + tOfHz (lo) * r.getWidth(), r.getY(), (tOfHz (hi) - tOfHz (lo)) * r.getWidth(), r.getHeight());
        g.setColour (ink.withAlpha (0.12f));
        g.fillRect (band);
        g.setColour (ink);
        g.strokePath (curve (r, [&] (float t) {
            float hz = hzOfT (t), db = 0.0f;
            for (float k : { 1.0f, 3.0f, 6.0f, 10.0f })
                db += bell (hz, manual * k, 3.0f, -24.0f * mix) + bell (hz, manual * k * 1.6f, 4.0f, 10.0f * reso * mix);
            return 0.72f + db / 45.0f;
        }), juce::PathStrokeType (1.6f));
        caption (g, band.withY (r.getBottom() - 10), "LFO", ink);
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> r, const Model& m, juce::Colour ink)
    {
        const float bands[] = { 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f };   // Lage schematisch
        float q = 1.8f - 0.3f * (float) raw (m, "patch.fx.sp_width");                    // Width 1 … 5
        float gain[6];
        for (int i = 0; i < 6; ++i) gain[i] = (float) raw (m, (juce::String ("patch.fx.sp_band_") + juce::String (i + 1)).toRawUTF8()) - 15.0f;
        dashes (g, { r.getX(), r.getCentreY(), r.getRight(), r.getCentreY() }, ink);
        g.setColour (ink.withAlpha (0.25f));
        for (int i = 0; i < 6; ++i)
        {
            float x = r.getX() + tOfHz (bands[i]) * r.getWidth(), y = r.getCentreY() - gain[i] / 40.0f * r.getHeight();
            g.fillRect (juce::Rectangle<float> (x - 3, juce::jmin (y, r.getCentreY()), 6, std::abs (y - r.getCentreY())));
        }
        g.setColour (ink);
        g.strokePath (curve (r, [&] (float t) {
            float hz = hzOfT (t), db = 0.0f;
            for (int i = 0; i < 6; ++i) db += bell (hz, bands[i], juce::jmax (0.4f, q), gain[i]);
            return 0.5f + db / 40.0f;
        }), juce::PathStrokeType (1.6f));
    }

    void drawEnhancer (juce::Graphics& g, juce::Rectangle<float> r, const Model& m, juce::Colour ink)
    {
        float sens = (float) raw (m, "patch.fx.en_sens") / 100.0f, mix = (float) raw (m, "patch.fx.en_mix") / 100.0f;
        dashes (g, { r.getX(), r.getBottom() - r.getHeight() * 0.35f, r.getRight(), r.getBottom() - r.getHeight() * 0.35f }, ink);
        g.setColour (ink);
        g.strokePath (curve (r, [&] (float t) {
            float hz = hzOfT (t);
            return 0.35f + mix * 0.55f / (1.0f + std::pow (3000.0f / hz, 2.0f)) * (0.4f + 0.6f * sens);   // Höhen angehoben
        }), juce::PathStrokeType (1.6f));
        caption (g, r, u ("Höhen ↑"), ink, true);
    }

    void drawChorus (juce::Graphics& g, juce::Rectangle<float> r, const Model& m, juce::Colour ink)
    {
        float rate = 0.5f + 7.0f * (float) raw (m, "patch.fx.ch_rate") / 99.0f;
        float depth = (float) raw (m, "patch.fx.ch_depth") / 100.0f;
        float delay = (float) raw (m, "patch.fx.ch_delay") / 99.0f;
        float fb = std::abs ((float) raw (m, "patch.fx.ch_feedback") - 49.0f) / 49.0f;
        // Verzögerungszeit über der Zeit: Grundverzögerung ± Tiefe, im Takt der Rate
        float base = 0.25f + 0.5f * delay;
        dashes (g, { r.getX(), r.getBottom() - base * r.getHeight(), r.getRight(), r.getBottom() - base * r.getHeight() }, ink);
        g.setColour (ink);
        g.strokePath (curve (r, [&] (float t) { return base + 0.22f * depth * std::sin (juce::MathConstants<float>::twoPi * rate * t); }),
                      juce::PathStrokeType (1.6f + 1.5f * fb));
        caption (g, r, "DELAY-ZEIT", ink);
    }

    void drawDelay (juce::Graphics& g, juce::Rectangle<float> r, const Model& m, juce::Colour ink)
    {
        struct Tap { const char* time; const char* level; const char* name; };
        const Tap taps[] = { { "patch.fx.dl_center", "patch.fx.dl_center_lv", "C" }, { "patch.fx.dl_left", "patch.fx.dl_left_lv", "L" },
                             { "patch.fx.dl_right", "patch.fx.dl_right_lv", "R" } };
        float fb = ((float) raw (m, "patch.fx.dl_feedback") - 49.0f) / 49.0f;
        float ms[3], lv[3], span = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            ms[i] = number (m, taps[i].time);
            if (ms[i] <= 0.0f) ms[i] = 250.0f;       // Notenwert (Tempo) — schematisch
            lv[i] = (float) raw (m, taps[i].level) / 100.0f;
        }
        span = juce::jmax (ms[0] * (std::abs (fb) > 0.05f ? 5.0f : 1.3f), ms[1] * 1.3f, ms[2] * 1.3f, 50.0f);
        auto xOf = [&] (float t) { return r.getX() + 4 + t / span * (r.getWidth() - 8); };
        g.setColour (ink);
        g.fillRect (juce::Rectangle<float> (r.getX() + 2, r.getY(), 3, r.getHeight()));   // trockenes Signal
        for (int i = 0; i < 3; ++i)
        {
            float x = xOf (ms[i]), h = lv[i] * r.getHeight() * 0.9f;
            g.setColour (ink);
            g.fillRect (juce::Rectangle<float> (x - 1.5f, r.getBottom() - h, 3, h));
            caption (g, juce::Rectangle<float> (x - 6, r.getBottom() - h - 12, 20, 11), taps[i].name, ink);
        }
        // Rückkopplung: Wiederholungen des Center-Taps
        float a = lv[0];
        for (int k = 2; k < 12 && std::abs (fb) > 0.05f; ++k)
        {
            a *= std::abs (fb);
            float x = xOf (ms[0] * (float) k);
            if (x > r.getRight() || a < 0.03f) break;
            g.setColour (ink.withAlpha (0.55f));
            g.fillRect (juce::Rectangle<float> (x - 1.0f, r.getBottom() - a * r.getHeight() * 0.9f, 2, a * r.getHeight() * 0.9f));
        }
    }

    void drawReverb (juce::Graphics& g, juce::Rectangle<float> r, const Model& m, juce::Colour ink)
    {
        int type = raw (m, "patch.fx.rv_type");
        float pre = number (m, "patch.fx.rv_predelay"), time = juce::jmax (100.0f, number (m, "patch.fx.rv_time"));
        float early = (float) raw (m, "patch.fx.rv_early") / 100.0f, level = (float) raw (m, "patch.fx.rv_level") / 100.0f;
        float span = pre + time * 1.1f + 20.0f;
        auto xOf = [&] (float t) { return r.getX() + 4 + t / span * (r.getWidth() - 8); };
        g.setColour (ink);
        g.fillRect (juce::Rectangle<float> (r.getX() + 2, r.getY(), 3, r.getHeight()));        // Direktschall
        // frühe Reflexionen
        for (int k = 0; k < 6; ++k)
        {
            float t = pre + 4.0f + (float) k * 9.0f, h = early * (0.8f - 0.1f * (float) k) * r.getHeight();
            g.fillRect (juce::Rectangle<float> (xOf (t) - 1.0f, r.getBottom() - h, 2, h));
        }
        // Nachhall-Fahne (GATE: abgeschnitten, REVERSE: anschwellend)
        juce::Path tail;
        float x0 = xOf (pre + 10.0f);
        tail.startNewSubPath (x0, r.getBottom());
        for (float x = x0; x <= xOf (pre + time); x += 1.0f)
        {
            float t = (x - x0) / juce::jmax (1.0f, xOf (pre + time) - x0);
            float e = type == 6 ? (t < 0.8f ? 1.0f : 0.0f) : type == 7 ? t * t : std::exp (-4.0f * t);
            tail.lineTo (x, r.getBottom() - e * level * r.getHeight() * 0.8f);
        }
        tail.lineTo (xOf (pre + time), r.getBottom());
        tail.closeSubPath();
        g.setColour (ink.withAlpha (0.3f));
        g.fillPath (tail);
        g.setColour (ink);
        g.strokePath (tail, juce::PathStrokeType (1.2f));
        caption (g, r, m.display (m.find ("patch.fx.rv_type")->index, -1), ink, true);
    }
}

bool FxSketch::supports (const juce::String& group)
{
    return group == "EQ" || group == "DISTORTION" || group == "PHASER" || group == "SPECTRUM" || group == "ENHANCER"
        || group == "CHORUS" || group == "DELAY" || group == "REVERB";
}

FxSketch::FxSketch (Model& m, const juce::String& group) : model (m)
{
    for (auto& p : m.params()) if (p.group == group) { first = &p; break; }
    model.addChangeListener (this);
}

FxSketch::~FxSketch() { model.removeChangeListener (this); }

void FxSketch::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0d0f0c));
    g.fillRoundedRectangle (b, 3.0f);
    auto s = b.reduced (5.0f);
    g.setGradientFill (juce::ColourGradient (theme::lcdLit, s.getX(), s.getY(), theme::lcdLit2, s.getRight(), s.getBottom(), false));
    g.fillRect (s);
    drawLcdGraph (g, s.reduced (8.0f, 8.0f), model, first, -1);
}

bool drawLcdGraph (juce::Graphics& g, juce::Rectangle<float> r, const Model& m, const Model::Param* p, int tone)
{
    if (p == nullptr || ! m.hasPatch()) return false;
    auto ink = theme::lcdInk;
    if (! p->isTone())
    {
        auto grp = p->group;
        if (grp == "EQ")         { drawEq (g, r, m, ink); return true; }
        if (grp == "DISTORTION") { drawDistortion (g, r, m, ink); return true; }
        if (grp == "PHASER")     { drawPhaser (g, r, m, ink); return true; }
        if (grp == "SPECTRUM")   { drawSpectrum (g, r, m, ink); return true; }
        if (grp == "ENHANCER")   { drawEnhancer (g, r, m, ink); return true; }
        if (grp == "CHORUS")     { drawChorus (g, r, m, ink); return true; }
        if (grp == "DELAY")      { drawDelay (g, r, m, ink); return true; }
        if (grp == "REVERB")     { drawReverb (g, r, m, ink); return true; }
        return false;
    }
    if (tone < 0) return false;
    if (envshape::supports (p->group))
    {
        auto shape = envshape::forGroup (m, p->group);
        auto l = envshape::layout (shape, m, tone, r.reduced (4.0f, 2.0f));
        float dashes[] = { 3.0f, 3.0f };
        g.setColour (ink.withAlpha (0.35f));
        g.drawDashedLine (juce::Line<float> (r.getX(), l.baseY, r.getRight(), l.baseY), dashes, 2, 1.0f);
        g.drawDashedLine (juce::Line<float> (l.holdEndX, r.getY(), l.holdEndX, r.getBottom()), dashes, 2, 1.0f);
        g.setColour (ink);
        g.strokePath (envshape::line (shape, l), juce::PathStrokeType (1.6f));
        for (size_t i = 0; i < l.pos.size(); ++i)
            if (shape.points[i].time == p || shape.points[i].level == p)
            {
                g.fillEllipse (l.pos[i].x - 3.5f, l.pos[i].y - 3.5f, 7.0f, 7.0f);
                g.setFont (theme::mono (9.5f));
                auto& pt = shape.points[i];
                g.drawText (pt.level == p ? pt.levelLabel : pt.timeLabel,
                            juce::Rectangle<float> (l.pos[i].x - 20, l.pos[i].y < r.getCentreY() ? l.pos[i].y + 4 : l.pos[i].y - 15, 40, 11),
                            juce::Justification::centred);
            }
        return true;
    }
    if (p->group == "LFO 1" || p->group == "LFO 2")
    {
        drawLfo (g, r, m, p->group == "LFO 1" ? "tone.lfo1." : "tone.lfo2.", tone, ink);
        return true;
    }
    if (p->group == "TVF")
    {
        drawFilter (g, r, m, tone, ink);
        return true;
    }
    return false;
}
