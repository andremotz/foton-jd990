#pragma once
// Tab „Performance“ (ADR-22): 7 Kanalzüge (Patch, Empfang, Kanal, Level, Pan, Tune,
// Ausgang, FX-Send, Stimmen), Name/Sync-Part/Stimmen-Summe, gemeinsame Effekte.
// Die Patchwahl eines Parts ist eine bewusste Menü-Aktion: das Gerät lädt dabei den
// Speicher-Patch in den Part (P0) — Änderungen am Part-Patch gehen verloren.

#include "Controls.h"

/** Fader oder Auswahlliste für einen Performance-/Part-Parameter. */
class PerfControl : public juce::Component, private juce::ChangeListener
{
public:
    PerfControl (Model&, const juce::String& id, int part);   // part −1 bei Common-Parametern
    ~PerfControl() override;
    bool valid() const { return param != nullptr; }
    int preferredWidth() const { return isChoice ? 86 : Fader::width; }
    int preferredHeight() const { return isChoice ? 44 : Fader::height; }
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { update(); }
    void update();
    Model& model;
    const Model::PerfParam* param = nullptr;
    int part;
    bool isChoice = false;
    Fader fader;
    juce::ComboBox box;
};

class PerformanceView : public juce::Component, private juce::ChangeListener
{
public:
    explicit PerformanceView (Model&);
    ~PerformanceView() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    /** „Bearbeiten“ eines Parts: Panel-Tab zeigen (setzt MainComponent). */
    std::function<void()> onEditPart;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void layoutContent();
    void showPatchMenu (int part);
    Model& model;

    struct Strip
    {
        juce::TextButton edit { "Bearbeiten" }, rx { "Rx" }, patchBtn, hear { juce::String::fromUTF8 ("▶") };
        juce::ComboBox channel;
        juce::OwnedArray<PerfControl> controls;
    };
    juce::Viewport view;
    struct Content : juce::Component
    {
        PerformanceView* owner = nullptr;
        void paint (juce::Graphics& g) override { owner->paintContent (g); }
    } content;
    void paintContent (juce::Graphics&);
    juce::TextEditor name;
    juce::Label voices;
    juce::TextButton modeBtn { juce::String::fromUTF8 ("In den Performance-Modus wechseln") },
                     readBtn { juce::String::fromUTF8 ("Vom Gerät lesen") }, sendBtn { juce::String::fromUTF8 ("Setup senden") };
    std::unique_ptr<PerfControl> sync;
    Strip strips[7];
    juce::OwnedArray<PerfControl> fx[3];         // Chorus, Delay, Reverb der Performance
    juce::Rectangle<int> stripArea[7], fxArea[3];
    std::vector<int> held[7];                    // gehaltene Noten je Part (Vorhören)
};
