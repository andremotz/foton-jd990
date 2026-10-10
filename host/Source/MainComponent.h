#pragma once
// Hauptfenster: Verbindungs-/Dateileiste, Tabs (Panel · Patch & Effekte ·
// Alle Parameter · Bibliothek), Statuszeile. Der Zustand liegt im Model.

#include "Panels.h"
#include "LibraryView.h"
#include "PerformanceView.h"

class MainComponent : public juce::Component, private juce::ChangeListener
{
public:
    /** `autoConnect`: beim Start Port öffnen und Temp Patch lesen. */
    explicit MainComponent (bool autoConnect = true, const juce::String& libraryPath = {});
    ~MainComponent() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // Test-Haken (Main.cpp) — dieselben Wege wie die Bedienung
    Model& getModel() { return model; }
    bool loadFile (const juce::File& f) { return model.loadFile (f); }
    void selectTone (int t) { model.selectTone (t); }
    bool setParam (const juce::String& id, int tone, int value);
    void sendToDevice() { model.sendToDevice(); }
    void setPolling (int ms) { model.setPolling (ms); }
    void undo() { model.undo(); }
    void loadInit() { model.loadInit(); }
    void showTab (int i) { tabs.setCurrentTabIndex (i); }
    void setMode (int m) { model.setMode (m); }
    void selectPart (int p) { model.selectPart (p); }
    void variation (int percent) { if (auto* p = dynamic_cast<PanelView*> (tabs.getTabContentComponent (0))) p->variation (percent); }
    PatchView* patchView() { return dynamic_cast<PatchView*> (tabs.getTabContentComponent (1)); }
    LibraryView* library() { return dynamic_cast<LibraryView*> (tabs.getTabContentComponent (3)); }
    void showEnvValues (bool on) { if (auto* p = dynamic_cast<PanelView*> (tabs.getTabContentComponent (0))) p->showEnvValues (on); }
    void showCompare (const juce::String& group = {}) { if (auto* p = dynamic_cast<PanelView*> (tabs.getTabContentComponent (0))) p->showCompare (true, group); }

    // Zoom (ZoomHost): Auswahl in der Leiste, ⌘+ / ⌘− / ⌘0
    std::function<void (int percent)> onZoom;
    void showZoom (int percent);
    static const std::vector<int>& zoomSteps();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void tryConnect (bool quiet);
    void refreshPorts();

    theme::LookAndFeel lnf;
    Model model;
    bool autoConnect;

    juce::ComboBox portBox;
    juce::TextButton connectBtn { "Verbinden" }, readBtn { juce::String::fromUTF8 ("Vom Gerät lesen") },
                     sendBtn { juce::String::fromUTF8 ("An Gerät senden") }, loadBtn { juce::String::fromUTF8 ("Laden…") },
                     saveBtn { juce::String::fromUTF8 ("Speichern…") }, initBtn { "Init" },
                     undoBtn { juce::String::fromUTF8 ("↶") }, redoBtn { juce::String::fromUTF8 ("↷") },
                     historyBtn { juce::String::fromUTF8 ("Verlauf") };
    void showHistory();
    juce::ToggleButton pollBtn { "Abgleich" };
    juce::TextButton hearBtn { juce::String::fromUTF8 ("▶ Hören") };
    juce::ComboBox hearWhat, hearChannel, zoomBox;
    juce::TextButton patchModeBtn { "Patch" }, perfModeBtn { "Performance" }, partBtn[7];
    juce::ComboBox unitBox;            // Unit # des JD-990 (SysEx-Device-ID)
    int topHeight = 46;
    juce::Rectangle<int> stripArea;    // eigene Zeile für Modus/Parts/Zoom, wenn neben den Tabs kein Platz ist
    bool laidOutForPerf = false;
    std::vector<int> sounding;          // aktuell gehaltene Noten
    void hear (bool down);

    juce::Label syncState, status;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::unique_ptr<juce::FileChooser> chooser;

    struct Reconnect : juce::Timer
    {
        std::function<void()> tick;
        void timerCallback() override { tick(); }
    } reconnect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

/** Skaliert die ganze Oberfläche wie ein Browser-Zoom: das Fenster bleibt gleich groß,
    Schrift und Bedienelemente werden größer, der Inhalt ordnet sich neu an.
    Gespeichert in den Einstellungen („uiScale“, Standard 130 %); --testmode nutzt 100 %
    (oder --scale=<n>) und speichert nichts. */
class ZoomHost : public juce::Component
{
public:
    ZoomHost (std::unique_ptr<MainComponent>, int forcedPercent);
    MainComponent& main() { return *content; }
    void setZoom (int percent);
    int zoom() const { return percent; }
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    std::unique_ptr<MainComponent> content;
    int percent = 100;
};
