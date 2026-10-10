#include "LibraryView.h"

namespace
{
    juce::String u (const char* s) { return juce::String::fromUTF8 (s); }

    /** FFI-Funktion mit (buf, cap) → String (zweimal aufrufen: Länge, dann Inhalt). */
    template <typename F>
    juce::String fetch (F&& f)
    {
        int n = f (nullptr, (size_t) 0);
        if (n <= 0) return {};
        juce::HeapBlock<char> buf ((size_t) n);
        f (buf.get(), (size_t) n);
        return juce::String::fromUTF8 (buf.get());
    }
}

LibraryView::LibraryView (Model& m, const juce::String& dbPath) : model (m)
{
    lib = jd990_lib_open (dbPath.isNotEmpty() ? dbPath.toRawUTF8() : nullptr);

    searchBox.setTextToShowWhenEmpty (u ("Suchen — Name, Quelle, Tags, Notizen"), theme::silkDim);
    searchBox.onTextChange = [this] { similarBase.reset(); reload(); };
    favOnly.onClick = [this] { reload(); };
    list.setRowHeight (26);
    list.setMultipleSelectionEnabled (true);
    list.setColour (juce::ListBox::backgroundColourId, theme::sunk);
    count.setColour (juce::Label::textColourId, theme::silkDim);
    title.setFont (theme::monoBold (18.0f));
    meta.setColour (juce::Label::textColourId, theme::silkDim);
    meta.setFont (theme::mono (11.0f));
    tags.setTextToShowWhenEmpty ("Tags, kommagetrennt (z. B. bass, dunkel)", theme::silkDim);
    notes.setMultiLine (true);
    notes.setReturnKeyStartsNewLine (true);
    notes.setTextToShowWhenEmpty ("Notizen", theme::silkDim);
    diffView.setMultiLine (true);
    diffView.setReadOnly (true);
    diffView.setFont (theme::mono (11.0f));
    diffView.setColour (juce::TextEditor::backgroundColourId, theme::panel);

    tags.onFocusLost = [this] { if (auto* e = current()) { jd990_lib_set_tags (lib, e->id, tags.getText().toRawUTF8()); reload(); } };
    tags.onReturnKey = tags.onFocusLost;
    notes.onFocusLost = [this] { if (auto* e = current()) jd990_lib_set_notes (lib, e->id, notes.getText().toRawUTF8()); };

    importBtn.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Patches importieren", juce::File(), "*.syx;*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc) {
                                  int total = 0;
                                  for (auto& f : fc.getResults()) total += import (f);
                                  juce::ignoreUnused (total);
                              });
    };
    storeBtn.onClick = [this] {
        if (! model.hasPatch()) return;
        auto id = jd990_lib_add (lib, model.patch(), "Editor");
        if (model.onStatus)
            model.onStatus (id > 0 ? u ("In der Bibliothek gespeichert: „") + model.patchName() + u ("“")
                                   : id < 0 ? u ("Schon in der Bibliothek (gleicher Inhalt, Eintrag ") + juce::String (-id) + ")"
                                            : u ("Speichern fehlgeschlagen"));
        reload();
    };
    exportBtn.onClick = [this] {
        auto ids = selectedIds();
        if (ids.empty()) return;
        chooser = std::make_unique<juce::FileChooser> (ids.size() == 1 ? "Patch exportieren" : "Bank exportieren", juce::File(), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, ids] (const juce::FileChooser& fc) {
                                  if (fc.getResult() == juce::File()) return;
                                  bool ok = jd990_lib_export (lib, ids.data(), ids.size(), fc.getResult().getFullPathName().toRawUTF8()) == 0;
                                  if (model.onStatus)
                                      model.onStatus (ok ? juce::String ((int) ids.size()) + u (" Patch(es) exportiert: ") + fc.getResult().getFileName()
                                                            + (ids.size() > 1 ? u (" (Bank, Plätze I:11 …)") : u (" (Temp-Patch)"))
                                                         : u ("Export fehlgeschlagen"));
                              });
    };
    deviceBtn.setTooltip (juce::String::fromUTF8 ("Alle 64 internen Patches lesen (nur lesen, nichts wird geschrieben) und in die Bibliothek übernehmen"));
    deviceBtn.onClick = [this] {
        auto added = std::make_shared<int> (0);
        deviceBtn.setEnabled (false);
        model.readBank ([this, added] (int n, PatchPtr p) {
                            auto src = u ("Gerät · I:") + juce::String (n / 8 + 1) + juce::String (n % 8 + 1) + u (" · ") + juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
                            if (jd990_lib_add (lib, p.get(), src.toRawUTF8()) > 0) ++*added;
                        },
                        [this, added] (int read) {
                            deviceBtn.setEnabled (true);
                            if (model.onStatus) model.onStatus (juce::String (read) + u (" Patches vom Gerät gelesen: ") + juce::String (*added) + u (" neu, ")
                                                                + juce::String (read - *added) + u (" schon in der Bibliothek"));
                            reload();
                        });
    };
    loadBtn.onClick = [this] { loadSelected (false); };
    sendBtn.onClick = [this] { loadSelected (true); };
    favBtn.onClick = [this] { if (auto* e = current()) { jd990_lib_set_favorite (lib, e->id, e->favorite ? 0 : 1); reload(); } };
    removeBtn.onClick = [this] {
        auto* e = current();
        if (e == nullptr) return;
        auto id = e->id;
        auto name = e->name;
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, u ("Aus der Bibliothek entfernen?"),
            u ("„") + name + u ("“ wird aus der Bibliothek gelöscht. Das Gerät und Dateien bleiben unberührt."),
            "Entfernen", "Abbrechen", nullptr,
            juce::ModalCallbackFunction::create ([this, id] (int r) { if (r == 1) { jd990_lib_remove (lib, id); reload(); } }));
    };
    morph.setSliderStyle (juce::Slider::LinearHorizontal);
    morph.setRange (0.0, 100.0, 1.0);
    morph.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    morph.setTextValueSuffix (" %");
    morph.setColour (juce::Slider::trackColourId, theme::toneOn.withAlpha (0.6f));
    morph.setColour (juce::Slider::thumbColourId, theme::cap);
    morph.setColour (juce::Slider::backgroundColourId, theme::sunk);
    morph.setTooltip (u ("Morph vom Editor-Patch (0 %) zu diesem Eintrag (100 %) — live am Gerät; auf 0 % zurück = Editor-Patch"));
    morph.onValueChange = [this] { morphTo ((float) morph.getValue() / 100.0f); };
    morphLabel.setText (u ("MORPH A → B"), juce::dontSendNotification);
    morphLabel.setFont (theme::label (10.0f));
    morphLabel.setColour (juce::Label::textColourId, theme::silkDim);
    addAndMakeVisible (morph);
    addAndMakeVisible (morphLabel);
    abBtn.setClickingTogglesState (false);
    abBtn.setTooltip (u ("Zwischen Editor-Patch (A) und diesem Bibliothekspatch (B) umschalten — sendet nur den Unterschied"));
    abBtn.onClick = [this] { toggleAB(); };
    similarBtn.setTooltip (u ("Ähnlich klingende Patches: nach Wellenform, Hüllkurven, Filter … (klingende Tones zählen) — ohne Auswahl zum Editor-Patch"));
    similarBtn.onClick = [this] { if (similarBase) { similarBase.reset(); reload(); } else findSimilar(); };

    for (auto* c : { (juce::Component*) &searchBox, (juce::Component*) &favOnly, (juce::Component*) &importBtn, (juce::Component*) &storeBtn,
                     (juce::Component*) &exportBtn, (juce::Component*) &deviceBtn, (juce::Component*) &list, (juce::Component*) &count, (juce::Component*) &title,
                     (juce::Component*) &meta, (juce::Component*) &tags, (juce::Component*) &notes, (juce::Component*) &diffView,
                     (juce::Component*) &loadBtn, (juce::Component*) &sendBtn, (juce::Component*) &favBtn, (juce::Component*) &removeBtn,
                     (juce::Component*) &abBtn, (juce::Component*) &similarBtn })
        addAndMakeVisible (c);
    model.addChangeListener (this);
    reload();
}

LibraryView::~LibraryView()
{
    model.removeChangeListener (this);
    jd990_lib_close (lib);
}

int LibraryView::import (const juce::File& f)
{
    int added = 0, dupes = 0;
    int r = jd990_lib_import_file (lib, f.getFullPathName().toRawUTF8(), &added, &dupes);
    if (model.onStatus)
        model.onStatus (r == 0 ? f.getFileName() + ": " + juce::String (added) + u (" neu, ") + juce::String (dupes) + u (" schon vorhanden")
                               : f.getFileName() + u (": keine Patch-Daten"));
    reload();
    return added;
}

void LibraryView::findSimilar()
{
    auto* e = current();
    if (e != nullptr) { similarBase.reset (jd990_lib_get (lib, e->id)); similarName = e->name; }
    else if (model.hasPatch()) { similarBase.reset (jd990_patch_clone (model.patch())); similarName = u ("Editor: ") + model.patchName(); }
    else return;
    list.deselectAllRows();
    reload();
    if (! entries.empty()) list.selectRow (0);
}

void LibraryView::reload()
{
    auto selected = selectedIds();
    entries.clear();
    auto json = fetch ([this] (char* b, size_t c) { return jd990_lib_list_json (lib, similarBase ? "" : searchBox.getText().toRawUTF8(), favOnly.getToggleState() ? 1 : 0, b, c); });
    auto parsed = juce::JSON::parse (json);
    if (auto* arr = parsed.getArray())
        for (auto& v : *arr)
        {
            Entry e { (int64_t) v["id"], v["name"].toString(), v["source"].toString(), v["added"].toString(), v["notes"].toString(), (bool) v["favorite"], {} };
            if (auto* t = v["tags"].getArray()) for (auto& x : *t) e.tags.add (x.toString());
            entries.push_back (e);
        }
    if (similarBase)   // nach Ähnlichkeit sortieren; der Bezugspatch selbst (100 %, gleicher Inhalt) fällt raus
    {
        auto sim = juce::JSON::parse (fetch ([this] (char* b, size_t c) { return jd990_lib_similar_json (lib, similarBase.get(), 30, b, c); }));
        std::vector<Entry> ordered;
        if (auto* arr = sim.getArray())
            for (auto& v : *arr)
            {
                auto id = (int64_t) v[0];
                float score = (float) (double) v[1];
                if (score >= 0.99999f) continue;
                for (auto& x : entries) if (x.id == id) { ordered.push_back (x); ordered.back().score = score; }
            }
        entries = std::move (ordered);
    }
    list.updateContent();
    list.deselectAllRows();
    for (int i = 0; i < (int) entries.size(); ++i)
        if (std::find (selected.begin(), selected.end(), entries[(size_t) i].id) != selected.end())
            list.selectRow (i, false, false);
    count.setText (similarBase ? u ("Ähnlich wie „") + similarName + u ("“ — ") + juce::String ((int) entries.size()) + u (" Einträge, ähnlichste zuerst")
                               : juce::String ((int) entries.size()) + (searchBox.isEmpty() && ! favOnly.getToggleState() ? u (" Patches") : u (" Treffer")),
                   juce::dontSendNotification);
    similarBtn.setButtonText (similarBase ? u ("Alle zeigen") : u ("Ähnliche finden"));
    list.repaint();
    showDetail();
}

std::vector<int64_t> LibraryView::selectedIds() const
{
    std::vector<int64_t> ids;
    auto rows = list.getSelectedRows();
    for (int i = 0; i < rows.size(); ++i)
        if (rows[i] < (int) entries.size()) ids.push_back (entries[(size_t) rows[i]].id);
    return ids;
}

const LibraryView::Entry* LibraryView::current() const
{
    int r = list.getSelectedRow();
    return r >= 0 && r < (int) entries.size() ? &entries[(size_t) r] : nullptr;
}

void LibraryView::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row >= (int) entries.size()) return;
    auto& e = entries[(size_t) row];
    g.fillAll (selected ? theme::edge : (row % 2 ? theme::sunk : theme::sunk.brighter (0.03f)));
    g.setColour (e.favorite ? theme::toneOn : theme::edge);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (u ("★"), 6, 0, 18, h, juce::Justification::centred);
    g.setColour (theme::silk);
    g.setFont (theme::mono (12.0f));
    g.drawText (e.name, 28, 0, 160, h, juce::Justification::centredLeft);
    int x = 196;
    if (e.score >= 0.0f)   // Ähnlichkeit als Zahl + Balken
    {
        g.setColour (theme::toneOn);
        g.setFont (theme::mono (11.5f));
        g.drawText (juce::String (juce::roundToInt (e.score * 100)) + " %", x, 0, 44, h, juce::Justification::centredRight);
        g.setColour (theme::edge);
        g.fillRect (x + 50, h / 2 - 2, 60, 4);
        g.setColour (theme::toneOn.withAlpha (0.7f));
        g.fillRect (x + 50, h / 2 - 2, juce::roundToInt (60 * e.score), 4);
        x += 120;
    }
    g.setColour (theme::silkDim);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (e.source, x, 0, w - x - 170, h, juce::Justification::centredLeft, true);
    g.setColour (theme::toneOn.withAlpha (0.8f));
    g.drawText (e.tags.joinIntoString (", "), w - 166, 0, 160, h, juce::Justification::centredRight, true);
}

void LibraryView::showDetail()
{
    auto* e = current();
    bool has = e != nullptr;
    // Laufender A/B- oder Morph-Vergleich gehört zum bisherigen Eintrag → erst A zurück
    if (abStash && (e == nullptr || e->id != abId))
    {
        model.showAB (std::move (abStash), u ("A: Editor-Patch"));
        abStash.reset();
    }
    for (auto* c : { (juce::Component*) &tags, (juce::Component*) &notes, (juce::Component*) &loadBtn, (juce::Component*) &sendBtn,
                     (juce::Component*) &favBtn, (juce::Component*) &removeBtn, (juce::Component*) &abBtn })
        c->setEnabled (has);
    exportBtn.setEnabled (! selectedIds().empty());
    deviceBtn.setEnabled (model.link.isOpen() && ! model.readingBank());
    sendBtn.setEnabled (has && model.link.isOpen());
    abBtn.setEnabled (has && model.hasPatch());
    morph.setEnabled (has && model.hasPatch());
    morph.setValue (0.0, juce::dontSendNotification);
    if (! has)
    {
        title.setText ({}, juce::dontSendNotification);
        meta.setText (entries.empty() ? u ("Bibliothek leer — „Importieren…“ oder „Editor-Patch speichern“") : u ("Eintrag wählen"), juce::dontSendNotification);
        tags.clear(); notes.clear(); diffView.clear();
        return;
    }
    title.setText (e->name, juce::dontSendNotification);
    meta.setText ("#" + juce::String (e->id) + u (" · ") + e->source + (Model::testMode ? juce::String() : u (" · ") + e->added), juce::dontSendNotification);
    tags.setText (e->tags.joinIntoString (", "), false);
    notes.setText (e->notes, false);
    favBtn.setButtonText (e->favorite ? u ("★ Favorit entfernen") : u ("★ Favorit"));
    refreshDiff();
    updateAB();
}

void LibraryView::refreshDiff()
{
    auto* e = current();
    abBtn.setEnabled (e != nullptr && model.hasPatch());
    if (e == nullptr) { diffView.clear(); return; }
    juce::String text;
    auto ids = selectedIds();
    if (ids.size() == 2)   // zwei Einträge gewählt: die beiden miteinander vergleichen
    {
        PatchPtr a (jd990_lib_get (lib, ids[0])), b (jd990_lib_get (lib, ids[1]));
        auto nameOf = [this] (int64_t id) { for (auto& x : entries) if (x.id == id) return x.name; return juce::String(); };
        auto arr = juce::JSON::parse (fetch ([&] (char* buf, size_t c) { return jd990_patch_diff_json (a.get(), b.get(), buf, c); }));
        int n = arr.getArray() ? arr.getArray()->size() : 0;
        text = n == 0 ? u ("Die beiden Einträge sind identisch.")
                      : juce::String (n) + u (" Unterschiede „") + nameOf (ids[0]) + u ("“ → „") + nameOf (ids[1]) + u ("“:\n\n");
        for (int i = 0; i < n; ++i)
            text << arr[i][0].toString().paddedRight (' ', 28) << arr[i][1].toString().paddedLeft (' ', 14) << u ("  →  ") << arr[i][2].toString() << "\n";
        diffView.setText (text, false);
        diffTitle = u ("VERGLEICH DER BEIDEN GEWÄHLTEN EINTRÄGE");
        repaint();
        return;
    }
    diffTitle = u ("VERGLEICH MIT DEM EDITOR (⌘-Klick: zwei Einträge vergleichen)");
    repaint();
    if (! model.hasPatch()) text = u ("Kein Editor-Patch zum Vergleichen.");
    else
    {
        PatchPtr p (jd990_lib_get (lib, e->id));
        auto json = fetch ([&] (char* b, size_t c) { return jd990_patch_diff_json (model.patch(), p.get(), b, c); });
        auto arr = juce::JSON::parse (json);
        int n = arr.getArray() ? arr.getArray()->size() : 0;
        text = n == 0 ? u ("Identisch mit dem Editor-Patch.")
                      : juce::String (n) + u (" Unterschiede zum Editor-Patch („") + model.patchName() + u ("“ → dieser Eintrag):\n\n");
        for (int i = 0; i < n; ++i)
        {
            auto row = arr[i];
            text << row[0].toString().paddedRight (' ', 28) << row[1].toString().paddedLeft (' ', 14) << u ("  →  ") << row[2].toString() << "\n";
        }
    }
    diffView.setText (text, false);
}

void LibraryView::loadSelected (bool send)
{
    auto* e = current();
    if (e == nullptr) return;
    abStash.reset();
    model.setPatch (PatchPtr (jd990_lib_get (lib, e->id)), u ("Bibliothek „") + e->name + u ("“"));
    if (send) model.sendToDevice();
    showDetail();
}

void LibraryView::toggleAB()
{
    auto* e = current();
    if (e == nullptr || ! model.hasPatch()) return;
    if (! abStash)
    {
        abStash.reset (jd990_patch_clone (model.patch()));     // A merken
        abId = e->id;
        model.showAB (PatchPtr (jd990_lib_get (lib, e->id)), u ("B: „") + e->name + u ("“"));
    }
    else
    {
        model.showAB (std::move (abStash), u ("A: Editor-Patch"));
        abStash.reset();
    }
    updateAB();
}

void LibraryView::morphTo (float t)
{
    auto* e = current();
    if (e == nullptr || ! model.hasPatch()) return;
    if (! abStash)                                          // A merken (wie bei A/B)
    {
        if (t <= 0.0f) return;
        abStash.reset (jd990_patch_clone (model.patch()));
        abId = e->id;
    }
    PatchPtr b (jd990_lib_get (lib, abId));
    PatchPtr m (jd990_patch_morph (abStash.get(), b.get(), t));
    model.applyLocks (m.get(), abStash.get());               // gesperrte Gruppen bleiben bei A
    model.showAB (std::move (m), "Morph " + juce::String (juce::roundToInt (t * 100)) + " %");
    if (t <= 0.0f) abStash.reset();                         // zurück bei A
    updateAB();
}

void LibraryView::updateAB()
{
    abBtn.setButtonText (abStash ? u ("A/B — B aktiv") : u ("A/B"));
    abBtn.setToggleState ((bool) abStash, juce::dontSendNotification);
}

void LibraryView::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    auto r = getLocalBounds().reduced (12);
    auto right = r.removeFromRight (r.getWidth() * 5 / 12);
    g.setColour (theme::raised);
    g.fillRect (right.withTrimmedLeft (8));
    g.setColour (theme::silkDim);
    g.setFont (theme::label (10.0f));
    g.drawText ("TAGS", right.getX() + 20, tags.getY() - 16, 100, 14, juce::Justification::centredLeft);
    g.drawText ("NOTIZEN", right.getX() + 20, notes.getY() - 16, 100, 14, juce::Justification::centredLeft);
    g.drawText (diffTitle, right.getX() + 20, diffView.getY() - 16, right.getWidth() - 40, 14, juce::Justification::centredLeft);
}

void LibraryView::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto right = r.removeFromRight (r.getWidth() * 5 / 12).withTrimmedLeft (8).reduced (12);
    auto top = r.removeFromTop (30);
    searchBox.setBounds (top.removeFromLeft (std::max (180, top.getWidth() - 620)));
    top.removeFromLeft (8);
    favOnly.setBounds (top.removeFromLeft (70));
    for (auto* b : { &importBtn, &storeBtn, &exportBtn, &deviceBtn })
    {
        top.removeFromLeft (6);
        b->setBounds (top.removeFromLeft (b == &storeBtn || b == &deviceBtn ? 150 : 104));
    }
    r.removeFromTop (8);
    count.setBounds (r.removeFromBottom (22));
    list.setBounds (r);

    title.setBounds (right.removeFromTop (28));
    meta.setBounds (right.removeFromTop (20));
    right.removeFromTop (10);
    auto buttons = right.removeFromTop (30);
    for (auto* b : { &loadBtn, &sendBtn, &abBtn })
    {
        b->setBounds (buttons.removeFromLeft (b == &abBtn ? 120 : 124));
        buttons.removeFromLeft (6);
    }
    right.removeFromTop (6);
    buttons = right.removeFromTop (30);
    favBtn.setBounds (buttons.removeFromLeft (160));
    buttons.removeFromLeft (6);
    removeBtn.setBounds (buttons.removeFromLeft (110));
    buttons.removeFromLeft (6);
    similarBtn.setBounds (buttons.removeFromLeft (140));
    right.removeFromTop (8);
    auto mrow = right.removeFromTop (26);
    morphLabel.setBounds (mrow.removeFromLeft (96));
    morph.setBounds (mrow);
    right.removeFromTop (22);
    tags.setBounds (right.removeFromTop (26));
    right.removeFromTop (26);
    notes.setBounds (right.removeFromTop (70));
    right.removeFromTop (26);
    diffView.setBounds (right);
}
