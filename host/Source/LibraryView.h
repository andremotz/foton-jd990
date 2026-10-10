#pragma once
// M5 — Bibliothek: Suche, Liste, Tags/Notizen/Favoriten, Import/Export,
// Laden in den Editor, Vergleich mit dem Editor-Patch, A/B-Umschalten.
// Die Daten liegen im Rust-Kern (jd990-library, SQLite); `.syx` bleibt die Wahrheit.

#include "Model.h"
#include "Theme.h"

class LibraryView : public juce::Component, public juce::FileDragAndDropTarget, private juce::ListBoxModel, private juce::ChangeListener
{
public:
    LibraryView (Model&, const juce::String& dbPath = {});
    ~LibraryView() override;
    void resized() override;
    void paint (juce::Graphics&) override;

    // Dateien hineinziehen = importieren
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files) if (f.endsWithIgnoreCase (".syx") || f.endsWithIgnoreCase (".mid")) return true;
        return false;
    }
    void filesDropped (const juce::StringArray& files, int, int) override { for (auto& f : files) import (juce::File (f)); }

    // Test-Haken
    void search (const juce::String& q) { searchBox.setText (q, false); similarBase.reset(); reload(); }
    void similar() { findSimilar(); }   // setText meldet asynchron — direkt neu laden
    int import (const juce::File&);
    void selectRow (int r) { list.selectRow (r); }
    void addRow (int r) { list.selectRow (r, false, false); }
    void abToggle() { toggleAB(); }
    void morphPercent (int p) { morph.setValue (p, juce::sendNotificationSync); }

private:
    struct Entry { int64_t id; juce::String name, source, added, notes; bool favorite; juce::StringArray tags; float score = -1.0f; };

    int getNumRows() override { return (int) entries.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int) override { showDetail(); }
    void findSimilar();          // Liste = ähnlichste Einträge zum gewählten Eintrag (oder Editor-Patch)
    PatchPtr similarBase;        // Bezug der Ähnlichkeitsliste (nullptr = normale Liste)
    juce::String similarName;
    juce::String diffTitle { juce::String::fromUTF8 ("VERGLEICH MIT DEM EDITOR") };
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { loadSelected (false); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshDiff(); updateAB(); }
    void refreshDiff();          // Vergleich mit dem Editor-Patch (bei jeder Editor-Änderung)

    void reload();
    void showDetail();
    void loadSelected (bool send);
    void toggleAB();
    void updateAB();
    std::vector<int64_t> selectedIds() const;
    const Entry* current() const;

    Model& model;
    Jd990Library* lib = nullptr;
    std::vector<Entry> entries;

    juce::TextEditor searchBox;
    juce::ToggleButton favOnly { juce::String::fromUTF8 ("nur ★") };
    juce::TextButton importBtn { juce::String::fromUTF8 ("Importieren…") }, storeBtn { juce::String::fromUTF8 ("Editor-Patch speichern") },
                     exportBtn { juce::String::fromUTF8 ("Exportieren…") }, deviceBtn { juce::String::fromUTF8 ("Vom Gerät (I:11–88)") };
    juce::ListBox list { "Bibliothek", this };
    juce::Label count;

    // Details
    juce::Label title, meta;
    juce::TextEditor tags, notes, diffView;
    juce::Slider morph;          // Morph A (Editor) → B (Eintrag), live
    juce::Label morphLabel;
    void morphTo (float t);
    juce::TextButton loadBtn { "In Editor laden" }, sendBtn { juce::String::fromUTF8 ("Laden & senden") },
                     favBtn { juce::String::fromUTF8 ("★ Favorit") }, removeBtn { juce::String::fromUTF8 ("Entfernen…") },
                     abBtn { "A/B" }, similarBtn { juce::String::fromUTF8 ("Ähnliche finden") };
    PatchPtr abStash;            // bei aktivem A/B: der Editor-Patch (A)
    int64_t abId = 0;
    std::unique_ptr<juce::FileChooser> chooser;
};
