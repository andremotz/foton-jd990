#include "MainComponent.h"

namespace
{
    const juce::String defaultPort = "Unitor8/AMT8 Port 2";
    juce::String u (const char* s) { return juce::String::fromUTF8 (s); }
}

MainComponent::MainComponent (bool autoConnectAtStart, const juce::String& libraryPath) : autoConnect (autoConnectAtStart)
{
    setLookAndFeel (&lnf);
    refreshPorts();
    portBox.setText (defaultPort, juce::dontSendNotification);
    for (auto* c : { (juce::Component*) &portBox, (juce::Component*) &connectBtn, (juce::Component*) &readBtn, (juce::Component*) &sendBtn,
                     (juce::Component*) &loadBtn, (juce::Component*) &saveBtn, (juce::Component*) &initBtn, (juce::Component*) &undoBtn,
                     (juce::Component*) &redoBtn, (juce::Component*) &historyBtn, (juce::Component*) &pollBtn, (juce::Component*) &syncState, (juce::Component*) &status,
                     (juce::Component*) &tabs })
        addAndMakeVisible (c);

    initBtn.setTooltip (u ("Init-Patch aus der Spec laden — bei Verbindung sofort in den Temp-Patch des Geräts"));
    undoBtn.setTooltip (u ("Rückgängig (⌘Z)"));
    redoBtn.setTooltip (u ("Wiederholen (⇧⌘Z)"));
    pollBtn.setTooltip (u ("Alle 5 s leise vom Gerät lesen und vergleichen — der JD-990 meldet Frontplatten-Änderungen nicht"));
    syncState.setJustificationType (juce::Justification::centredRight);
    syncState.setFont (theme::mono (11.0f));
    status.setFont (juce::FontOptions (12.0f));
    status.setColour (juce::Label::textColourId, theme::silkDim);

    // Vorhören: gedrückt halten = Note an, loslassen = aus (auf dem Empfangskanal des JD-990)
    for (auto* c : { (juce::Component*) &hearBtn, (juce::Component*) &hearWhat, (juce::Component*) &hearChannel, (juce::Component*) &zoomBox }) addAndMakeVisible (c);
    for (int z : zoomSteps()) zoomBox.addItem (juce::String (z) + " %", z);
    zoomBox.setSelectedId (100, juce::dontSendNotification);
    zoomBox.setTooltip (u ("Größe der Oberfläche · ⌘+ größer, ⌘− kleiner, ⌘0 = 100 %"));
    zoomBox.onChange = [this] { if (onZoom) onZoom (zoomBox.getSelectedId()); };
    hearWhat.addItem ("C2", 36); hearWhat.addItem ("C3", 48); hearWhat.addItem ("C4", 60); hearWhat.addItem ("C5", 72); hearWhat.addItem ("C6", 84);
    hearWhat.addItem (u ("Akkord C-E-G"), 1000); hearWhat.addItem (u ("Akkord tief"), 1001);
    hearWhat.setSelectedId (60, juce::dontSendNotification);
    for (int c = 1; c <= 16; ++c) hearChannel.addItem ("Kanal " + juce::String (c), c);
    hearChannel.setSelectedId (12, juce::dontSendNotification);     // Rx-Ch des JD-990 (Logbuch 2026-09-27)
    hearBtn.setTooltip (u ("Gedrückt halten zum Anhören — Note(n) auf dem gewählten Kanal"));
    hearBtn.onStateChange = [this] { hear (hearBtn.isDown()); };
    connectBtn.onClick = [this] { tryConnect (false); };
    readBtn.onClick = [this] { model.readFromDevice(); };
    sendBtn.onClick = [this] { if (model.perfMode()) model.sendSetupToDevice(); else model.sendToDevice(); };
    initBtn.onClick = [this] { model.loadInit(); };
    undoBtn.onClick = [this] { model.undo(); };
    redoBtn.onClick = [this] { model.redo(); };
    historyBtn.setTooltip (u ("Alle Änderungen dieser Sitzung — Klick springt zu einem früheren Stand"));
    historyBtn.onClick = [this] { showHistory(); };
    pollBtn.onClick = [this] { model.setPolling (pollBtn.getToggleState() ? 5000 : 0); };
    loadBtn.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Patch laden", juce::File(), "*.syx;*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) model.loadFile (fc.getResult()); });
    };
    saveBtn.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Patch speichern", juce::File(), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc) { model.saveFile (fc.getResult()); });
    };

    auto bg = theme::panel;
    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    tabs.addTab ("Panel", bg, new PanelView (model), true);
    tabs.addTab ("Patch & Effekte", bg, new PatchView (model), true);
    tabs.addTab ("Alle Parameter", bg, new GenericView (model), true);
    tabs.addTab ("Bibliothek", bg, new LibraryView (model, libraryPath), true);
    auto* pv = new PerformanceView (model);
    pv->onEditPart = [this] { tabs.setCurrentTabIndex (0); };
    tabs.addTab ("Performance", bg, pv, true);

    // Modus (Patch | Performance) und Part-Auswahl rechts in der Tab-Leiste (ADR-22)
    for (auto* b : { &patchModeBtn, &perfModeBtn })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, theme::toneOn);
        addAndMakeVisible (b);
    }
    for (int unit = 17; unit <= 32; ++unit) unitBox.addItem ("Unit " + juce::String (unit), unit);
    unitBox.setSelectedId (model.unitNumber(), juce::dontSendNotification);
    unitBox.setTooltip (u ("SysEx-Device-ID — muss zum JD-990 passen: SYSTEM › MIDI › Unit # (Standard 17)"));
    unitBox.onChange = [this] { model.setUnitNumber (unitBox.getSelectedId()); };
    addAndMakeVisible (unitBox);
    patchModeBtn.setTooltip (u ("Patch-Modus — bei Verbindung wird auch das Gerät umgeschaltet"));
    perfModeBtn.setTooltip (u ("Performance-Modus: 7 Parts, je eigener Patch und MIDI-Kanal — bei Verbindung wird auch das Gerät umgeschaltet und gelesen"));
    patchModeBtn.onClick = [this] { if (model.perfMode()) model.setMode (0); };
    perfModeBtn.onClick = [this] { if (! model.perfMode()) model.setMode (1); };
    for (int p = 0; p < 7; ++p)
    {
        partBtn[p].setButtonText (juce::String (p + 1));
        partBtn[p].setColour (juce::TextButton::buttonOnColourId, theme::toneOn);
        partBtn[p].onClick = [this, p] { model.selectPart (p); };
        addChildComponent (partBtn[p]);
    }

    model.onStatus = [this] (const juce::String& s) { status.setText (s, juce::dontSendNotification); };
    model.addChangeListener (this);
    setWantsKeyboardFocus (true);

    reconnect.tick = [this] { if (! model.link.isOpen()) tryConnect (true); };
    if (! autoConnect) status.setText ("Offline (--offline)", juce::dontSendNotification);
    else tryConnect (false);
    changeListenerCallback (nullptr);
    setSize (1320, 900);
}

MainComponent::~MainComponent()
{
    model.link.allNotesOff();
    model.removeChangeListener (this);
    tabs.clearTabs();
    setLookAndFeel (nullptr);
}

const std::vector<int>& MainComponent::zoomSteps()
{
    static const std::vector<int> steps { 80, 90, 100, 115, 130, 150, 175, 200 };
    return steps;
}

void MainComponent::showZoom (int percent) { zoomBox.setSelectedId (percent, juce::dontSendNotification); }

void MainComponent::refreshPorts()
{
    auto current = portBox.getText();
    portBox.clear (juce::dontSendNotification);
    for (auto& d : DeviceLink::outputs())
        portBox.addItem (d.name, portBox.getNumItems() + 1);
    portBox.setText (current.isNotEmpty() ? current : defaultPort, juce::dontSendNotification);
}

void MainComponent::showHistory()
{
    juce::PopupMenu menu;
    auto redo = model.redoHistory (), undo = model.undoHistory();
    if (redo.isEmpty() && undo.isEmpty()) menu.addItem (u ("Noch keine Änderungen"), false, false, nullptr);
    if (! redo.isEmpty())
    {
        menu.addSectionHeader (u ("Wiederholen"));
        for (int i = redo.size(); --i >= 0;)          // der entfernteste Schritt oben
            menu.addItem (u ("↷  ") + redo[i], [this, i] { model.redoSteps (i + 1); });
    }
    menu.addSectionHeader (u ("Aktueller Stand"));
    for (int i = 0; i < undo.size(); ++i)
        menu.addItem (u ("↶  ") + undo[i], [this, i] { model.undoSteps (i + 1); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (historyBtn).withMaximumNumColumns (1));
}

void MainComponent::tryConnect (bool quiet)
{
    refreshPorts();
    if (model.connect (portBox.getText(), quiet)) { reconnect.stopTimer(); return; }
    if (autoConnect && ! reconnect.isTimerRunning()) reconnect.startTimer (2000);   // Interface aus? weiter suchen
}

void MainComponent::hear (bool down)
{
    int ch = model.perfMode() ? model.partChannel (model.part()) : hearChannel.getSelectedId();   // Performance: Kanal des Parts
    if (down && sounding.empty() && model.link.isOpen())
    {
        int id = hearWhat.getSelectedId();
        sounding = id == 1000 ? std::vector<int> { 60, 64, 67 } : id == 1001 ? std::vector<int> { 36, 43, 48 } : std::vector<int> { id };
        for (int n : sounding) model.link.noteOn (ch, n, 100);
    }
    else if (! down && ! sounding.empty())
    {
        for (int n : sounding) model.link.noteOff (ch, n);
        sounding.clear();
    }
}

bool MainComponent::setParam (const juce::String& id, int tone, int value)
{
    auto* p = model.find (id);
    if (p == nullptr) return false;
    if (p->isTone()) model.selectTone (tone);
    model.lastIndex = (int) p->index;
    model.set (p->index, p->isTone() ? tone : -1, value);
    return true;
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    bool has = model.hasPatch();
    sendBtn.setEnabled (has);
    saveBtn.setEnabled (has);
    hearBtn.setEnabled (model.link.isOpen());
    undoBtn.setEnabled (model.canUndo());
    redoBtn.setEnabled (model.canRedo());
    historyBtn.setEnabled (model.canUndo() || model.canRedo());
    pollBtn.setToggleState (model.polling(), juce::dontSendNotification);
    syncState.setText (model.syncText, juce::dontSendNotification);
    bool perf = model.perfMode();
    patchModeBtn.setToggleState (! perf, juce::dontSendNotification);
    perfModeBtn.setToggleState (perf, juce::dontSendNotification);
    auto devMode = model.deviceMode == 1 ? u ("Gerät: Performance-Modus") : model.deviceMode == 0 ? u ("Gerät: Patch-Modus") : u ("Gerätemodus unbekannt");
    perfModeBtn.setTooltip (devMode + u (" · Performance-Modus: 7 Parts, je eigener Patch und MIDI-Kanal"));
    patchModeBtn.setTooltip (devMode + u (" · Patch-Modus"));
    hearChannel.setEnabled (! perf);
    for (int p = 0; p < 7; ++p)
    {
        partBtn[p].setVisible (perf);
        partBtn[p].setToggleState (perf && model.part() == p, juce::dontSendNotification);
        partBtn[p].setTooltip (perf ? u ("Part ") + juce::String (p + 1) + u (": „") + model.partPatchName (p) + u ("“ · Kanal ") + juce::String (model.partChannel (p)) : juce::String());
    }
    sendBtn.setButtonText (perf ? u ("Setup senden") : u ("An Gerät senden"));
    unitBox.setSelectedId (model.unitNumber(), juce::dontSendNotification);
    if (perf != laidOutForPerf) resized();   // Part-Knöpfe brauchen Platz
    sendBtn.setTooltip (perf ? u ("Performance + Part-Patches 1–7 an das Gerät (Gerät muss im Performance-Modus sein)") : juce::String());
    syncState.setColour (juce::Label::textColourId, model.syncColour);
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { model.undo(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { model.redo(); return true; }
    return false;
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::raised);
    g.fillRect (getLocalBounds().removeFromTop (topHeight));
    g.setColour (theme::edge);
    g.drawHorizontalLine (topHeight, 0, (float) getWidth());
    if (! stripArea.isEmpty())
    {
        g.setColour (theme::panel.darker (0.15f));
        g.fillRect (stripArea);
    }
}

void MainComponent::resized()
{
    auto a = getLocalBounds();
    // Leiste: passt nicht alles in eine Zeile (großer Zoom), wandern Hören/Abgleich/Zoom in Zeile 2
    const int firstRow = 20 + 170 + 6 + 86 + 116 + 116 + 86 + 86 + 52 + 32 + 32 + 64 + 8 * 5 + 14 - 5;
    const int secondRow = 76 + 4 + 96 + 4 + 92 + 12 + 92;
    bool wrap = getWidth() < firstRow + 12 + secondRow;
    topHeight = wrap ? 84 : 46;
    auto bar = a.removeFromTop (topHeight);
    auto top = bar.removeFromTop (46).reduced (10, 8);
    portBox.setBounds (top.removeFromLeft (170));
    top.removeFromLeft (6);
    for (auto* b : { &connectBtn, &readBtn, &sendBtn, &loadBtn, &saveBtn, &initBtn, &undoBtn, &redoBtn, &historyBtn })
    {
        int w = b == &readBtn || b == &sendBtn ? 116 : b == &undoBtn || b == &redoBtn ? 32 : b == &initBtn ? 52 : b == &historyBtn ? 64 : 86;
        b->setBounds (top.removeFromLeft (w));
        top.removeFromLeft (b == &initBtn ? 14 : 5);
    }
    if (wrap) top = bar.withTrimmedTop (-8).reduced (10, 8);
    else top.removeFromLeft (12);
    hearBtn.setBounds (top.removeFromLeft (76));
    top.removeFromLeft (4);
    hearWhat.setBounds (top.removeFromLeft (96));
    top.removeFromLeft (4);
    hearChannel.setBounds (top.removeFromLeft (92));
    top.removeFromLeft (12);
    pollBtn.setBounds (top.removeFromLeft (92));
    // Statuszeile: links Meldungen, rechts der Sync-Status (Gerät = Editor …)
    auto bottom = a.removeFromBottom (24).reduced (10, 0);
    syncState.setBounds (bottom.removeFromRight (bottom.getWidth() / 2));
    status.setBounds (bottom);
    // Unit # · Patch|Performance · Part 1–7 · Zoom: rechts neben den Tabs, wenn Platz ist —
    // sonst (großer Zoom, schmales Fenster) in einer eigenen Zeile über den Tabs
    bool perf = model.perfMode();
    int ctrlW = 92 + 12 + 64 + 2 + 104 + (perf ? 12 + 7 * 30 : 0) + 16 + 90 + 10;
    int tabsW = 0;
    for (int i = 0; i < tabs.getNumTabs(); ++i)
        if (auto* b = tabs.getTabbedButtonBar().getTabButton (i)) tabsW += b->getBestTabLength (30);
    bool ownRow = a.getWidth() - tabsW - 24 < ctrlW;
    stripArea = ownRow ? a.removeFromTop (32) : juce::Rectangle<int>();
    tabs.setBounds (a);
    int y = ownRow ? stripArea.getY() + 4 : a.getY() + 3;
    zoomBox.setBounds (getWidth() - 10 - 90, y, 90, 24);
    int x = zoomBox.getX() - 16;
    for (int p = 6; p >= 0; --p)
    {
        if (perf) x -= 30;
        partBtn[p].setBounds (x, y, 28, 24);
    }
    if (perf) x -= 12;
    perfModeBtn.setBounds (x - 104, y, 104, 24);
    patchModeBtn.setBounds (x - 104 - 64, y, 62, 24);
    unitBox.setBounds (patchModeBtn.getX() - 12 - 92, y, 92, 24);
    for (auto* c : { (juce::Component*) &unitBox, (juce::Component*) &patchModeBtn, (juce::Component*) &perfModeBtn, (juce::Component*) &zoomBox })
        c->toFront (false);
    for (auto& b : partBtn) b.toFront (false);
    laidOutForPerf = perf;
}

// ── ZoomHost ─────────────────────────────────────────────────────────────────

ZoomHost::ZoomHost (std::unique_ptr<MainComponent> mc, int forcedPercent) : content (std::move (mc))
{
    addAndMakeVisible (*content);
    int start = forcedPercent;
    if (auto* settings = Model::settings(); settings != nullptr && start <= 0)
        start = settings->getIntValue ("uiScale", 130);
    content->onZoom = [this] (int p) { setZoom (p); };
    setWantsKeyboardFocus (false);
    setZoom (start > 0 ? start : 100);
    // Fenster so groß, dass bei diesem Zoom dasselbe sichtbar ist wie bei 100 % — soweit der Bildschirm reicht
    auto screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    auto area = screen != nullptr ? screen->userBounds.toNearestInt() : juce::Rectangle<int> (0, 0, 1600, 1000);
    float s = (float) percent / 100.0f;
    setSize (juce::jmin (juce::roundToInt (1320 * s), area.getWidth() - 40), juce::jmin (juce::roundToInt (900 * s), area.getHeight() - 60));
}

void ZoomHost::setZoom (int p)
{
    percent = juce::jlimit (50, 300, p);
    content->showZoom (percent);
    if (auto* settings = Model::settings()) { settings->setValue ("uiScale", percent); settings->saveIfNeeded(); }
    resized();
}

void ZoomHost::resized()
{
    float s = (float) percent / 100.0f;
    content->setTransform (juce::AffineTransform::scale (s));
    content->setBounds (0, 0, juce::roundToInt ((float) getWidth() / s), juce::roundToInt ((float) getHeight() / s));
}

bool ZoomHost::keyPressed (const juce::KeyPress& k)
{
    if (! k.getModifiers().isCommandDown()) return false;
    auto& steps = MainComponent::zoomSteps();
    auto c = k.getTextCharacter();
    int code = k.getKeyCode();
    if (c == '+' || c == '=' || code == '+' || code == '=')
    {
        for (int z : steps) if (z > percent) { setZoom (z); return true; }
        return true;
    }
    if (c == '-' || code == '-')
    {
        for (auto it = steps.rbegin(); it != steps.rend(); ++it) if (*it < percent) { setZoom (*it); return true; }
        return true;
    }
    if (c == '0' || code == '0') { setZoom (100); return true; }
    return false;
}
