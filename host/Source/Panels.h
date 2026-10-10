#pragma once
// Ansichten (M4): Tone-Panel im JD-800-Geist, Patch & Effekte, Alle Parameter.
// Aufbau und Farben nach design/panel-prototype.html und docs/04-ui-konzept.md.

#include "Controls.h"
#include "Envelope.h"

/** Gruppe mit farbigem Kopf; Bedienelemente fließen zeilenweise. */
class GroupBox : public juce::Component
{
public:
    GroupBox (Model&, const juce::String& title, juce::Colour accent, const juce::StringArray& ids, int fixedTone = -2, bool compact = false);
    int preferredHeight (int width) const;
    int naturalWidth() const;   // alle Elemente in einer Zeile
    void resized() override;
    void paint (juce::Graphics&) override;
    void setTitle (const juce::String& t) { title = t; repaint(); }
    juce::String baseTitle;
    /** Hüllkurven-Gruppe: Werte-Fader auf-/zugeklappt → Größe ändert sich, Elternteil neu anordnen. */
    std::function<void()> onLayoutChange;
    void showValues (bool on);

private:
    juce::String title;
    juce::Colour accent;
    juce::OwnedArray<ParamControl> controls;
    std::unique_ptr<LockButton> lock;      // Gruppe für Variation/Morph sperren
    std::unique_ptr<EnvelopeView> env;     // Hüllkurven-Grafik (Env-Gruppen)
    std::unique_ptr<FxSketch> sketch;      // grünes Mini-Display (EQ, Effekte) am Ende der Reihe
    std::vector<std::pair<juce::Component*, juce::Point<int>>> items() const;   // sichtbare Elemente + Größe
    juce::TextButton valuesBtn { "Werte" };  // T-/L-Fader zusätzlich zur Grafik zeigen
    static constexpr int envHeight = 128, minGraphWidth = 420;
    bool collapsed() const { return env != nullptr && ! valuesBtn.getToggleState(); }
    bool shown (const ParamControl* c) const { return ! collapsed() || ! env->isShapeParam (c->getParam()); }
    int top() const { return env != nullptr ? 30 + envHeight + 6 : 30; }
};

/** Grün hinterleuchtetes Display: zuletzt berührter Parameter mit Kontext, Wert, Balken. */
class Lcd : public juce::Component, private juce::ChangeListener
{
public:
    explicit Lcd (Model&);
    ~Lcd() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    Model& model;
};

/** Zuletzt gesendete DT1-Nachricht, Adresse/Daten/Prüfsumme farbig. */
class Inspector : public juce::Component, private juce::ChangeListener
{
public:
    explicit Inspector (Model&);
    ~Inspector() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    Model& model;
};

/** 4×-Vergleich: eine Gruppe, Tones A–D nebeneinander, Abweichungen hervorgehoben. */
class CompareView : public juce::Component, private juce::ChangeListener
{
public:
    explicit CompareView (Model&);
    ~CompareView() override;
    void selectGroup (const juce::String& g) { if (int i = groups.indexOf (g); i >= 0) groupBox.setSelectedId (i + 1); }
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    Model& model;
    juce::ComboBox groupBox;
    juce::StringArray groups;
};

/** Seite 1 — Tone-Ebene: Seitenteil BASICS, LCD, Tone A–D/Link/Vergleich, Fader-Wand. */
class PanelView : public juce::Component, private juce::ChangeListener
{
public:
    explicit PanelView (Model&);
    ~PanelView() override;
    /** Test-Haken --envvalues: in allen Hüllkurven-Gruppen die Werte-Fader aufklappen. */
    void showEnvValues (bool on) { for (auto* g : groups) if (EnvelopeView::supports (g->baseTitle)) g->showValues (on); }
    void showCompare (bool on, const juce::String& group = {}) { compareBtn.setToggleState (on, juce::sendNotificationSync); if (group.isNotEmpty()) compare.selectGroup (group); }
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void layoutWall();
    Model& model;
    GroupBox basics;
    Lcd lcd;
    Inspector inspector;
    CompareView compare;
    juce::TextButton toneBtn[4], linkBtn { "Link" }, compareBtn { juce::String::fromUTF8 ("4× Vergleich") },
                     toneMenuBtn { juce::String::fromUTF8 ("Tone kopieren / tauschen…") }, varBtn { "Variation" };
    juce::ComboBox varAmount;
    uint64_t varSeed = 1;
    void showToneMenu();
public:
    void variation (int percent);
private:
    juce::Viewport wallView;
    juce::Component wall;
    juce::OwnedArray<GroupBox> groups;
};

/** Seite 2 — Patch & Effekte: Key FX, Structure als Grafik, Key/Velocity je Tone,
    EQ, Effektketten A/B als Blöcke mit den Parametern des gewählten Blocks. */
class PatchView : public juce::Component, private juce::ChangeListener
{
public:
    explicit PatchView (Model&);
    ~PatchView() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void layoutContent();
    Model& model;
    juce::Viewport view;
    struct Content : juce::Component
    {
        PatchView* owner = nullptr;
        void paint (juce::Graphics& g) override { owner->paintContent (g); }
        void mouseDown (const juce::MouseEvent& e) override { owner->dragStart (e); }
        void mouseDrag (const juce::MouseEvent& e) override { owner->dragMove (e); }
        void mouseUp (const juce::MouseEvent& e) override { if (! owner->dragEnd (e)) owner->clickContent (e); }
    } content;
    void paintContent (juce::Graphics&);
    void clickContent (const juce::MouseEvent&);

    // Effektkette umsortieren (Drag & Drop): Gruppe, Startposition, aktuelle x-Position
    int dragGroup = -1, dragFrom = -1;
    float dragX = 0;
    bool dragging = false;
    void dragStart (const juce::MouseEvent&);
    void dragMove (const juce::MouseEvent&);
    bool dragEnd (const juce::MouseEvent&);          // true = war ein Ziehen, kein Klick
    int dropPosition (int grp, float x) const;
    juce::Rectangle<float> blockRect (int grp, int pos) const;
public:
    /** Effekt von Position `from` an Position `to` verschieben (Test-Haken, Drag & Drop). */
    void moveEffect (int grp, int from, int to);
    /** Effektblock wählen (Test-Haken --fxselect): grp 0 = A, 1 = B; idx = Effekt (A: DS PH SP EN · B: CHO DLY REV). */
    void selectEffect (int grp, int idx) { (grp == 0 ? selectedA : selectedB) = idx; layoutContent(); content.repaint(); }
private:

    juce::OwnedArray<GroupBox> boxes;
    GroupBox* keyFx = nullptr;
    GroupBox* eq = nullptr;
    GroupBox* fxCtrl = nullptr;
    GroupBox* rangeRows[4] {};
    juce::OwnedArray<GroupBox> fxParams;    // Parameter je Effektblock (sichtbar: gewählter)
    int selectedA = 0, selectedB = 0;       // gewählter Block (Effekt-Index) in Kette A/B
    juce::Rectangle<int> structArea[2], chainArea[2], keyboardArea;
};

/** Seite 3 — Alle Parameter (M3-Ansicht, „Expertenmodus“): eine Zeile je Parameter. */
class GenericView : public juce::Component, private juce::ChangeListener
{
public:
    explicit GenericView (Model&);
    ~GenericView() override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    struct Row;
    Model& model;
    juce::Viewport view;
    juce::Component content;
    std::vector<std::unique_ptr<Row>> rows;
    std::vector<std::unique_ptr<juce::Label>> headers;
    std::vector<std::pair<juce::Label*, juce::String>> toneHeaders;
};
