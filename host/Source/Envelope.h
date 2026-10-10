#pragma once
// Grafiken zu den Parametern:
//  * EnvelopeView — Hüllkurve (Pitch/TVF/TVA Env) über den Fadern, live aus dem
//    aktuellen Tone, beschriftet (T1, L1 …), zuletzt bewegter Parameter hervorgehoben.
//    Punkte lassen sich ziehen: waagerecht = Zeit, senkrecht = Pegel (⇧ = fein).
//    Ein Zug ist ein Undo-Schritt; Doppelklick auf einen Punkt = Init-Werte.
//  * drawLcdGraph — kleine Skizze im grünen Display zum gerade berührten Parameter
//    (Hüllkurve, LFO-Verlauf, Filterkurve). Konzeptbild, keine Messung des Klangs.

#include "Model.h"
#include "Theme.h"

namespace envshape
{
    struct Point
    {
        const Model::Param* time = nullptr;    // Segment, das an diesem Punkt endet
        const Model::Param* level = nullptr;   // Pegel des Punkts (nullptr = fest 0)
        juce::String timeLabel, levelLabel;
    };
    struct Shape
    {
        bool bipolar = false;                  // Pitch: Mitte = keine Verstimmung
        int sustainIndex = 0;                  // nach diesem Punkt: Halten bis Key-Off
        std::vector<Point> points;
        bool valid() const { return ! points.empty(); }
    };
    struct Layout
    {
        std::vector<juce::Point<float>> pos;   // Bildschirmposition je Punkt
        float holdEndX = 0, unitsPerPixel = 1;
        float baseY = 0;                       // Nulllinie
    };
    bool supports (const juce::String& group);
    Shape forGroup (const Model&, const juce::String& group);
    Layout layout (const Shape&, const Model&, int tone, juce::Rectangle<float> area);
    juce::Path line (const Shape&, const Layout&);
}

class EnvelopeView : public juce::Component, public juce::SettableTooltipClient, private juce::ChangeListener
{
public:
    /** `group`: "PITCH ENV", "TVF ENV" oder "TVA ENV". */
    EnvelopeView (Model&, const juce::String& group, juce::Colour accent);
    ~EnvelopeView() override;
    static bool supports (const juce::String& group) { return envshape::supports (group); }
    /** Zeit oder Pegel eines Kurvenpunkts (per Grafik bedienbar) — nicht Velo/T-Velo/T-KF. */
    bool isShapeParam (const Model::Param*) const;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    juce::Rectangle<float> plot() const;
    envshape::Layout currentLayout() const { return envshape::layout (shape, model, model.tone, plot()); }
    int nearest (juce::Point<float>) const;
    int valueOf (const Model::Param*) const;

    Model& model;
    juce::Colour accent;
    envshape::Shape shape;
    int hover = -1, dragging = -1;
    int startTime = 0, startLevel = 0;
    float dragUnitsPerPixel = 1.0f;
    int axisLock = 0;                  // ⌥-Ziehen: 0 offen, 1 nur Zeit, 2 nur Pegel
};

/** Skizze im LCD zum Parameter `p` (Tone `tone`, −1 bei Patch-Parametern). false = für diese Gruppe keine Grafik. */
bool drawLcdGraph (juce::Graphics&, juce::Rectangle<float> area, const Model&, const Model::Param* p, int tone);

/** Kleines grünes Display mit der Skizze einer Effekt-Gruppe (EQ, Distortion … Reverb). */
class FxSketch : public juce::Component, private juce::ChangeListener
{
public:
    FxSketch (Model&, const juce::String& group);
    ~FxSketch() override;
    static bool supports (const juce::String& group);
    void paint (juce::Graphics&) override;
    static constexpr int width = 250;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    Model& model;
    const Model::Param* first = nullptr;   // irgendein Parameter der Gruppe (für drawLcdGraph)
};
