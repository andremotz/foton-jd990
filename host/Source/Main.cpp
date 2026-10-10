#include "MainComponent.h"

class FotonJD990Application : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    // Normal eine Instanz; Testläufe (--snapshot/--quit) dürfen neben einer laufenden App starten.
    bool moreThanOneInstanceAllowed() override
    {
        auto args = getCommandLineParameters();
        return args.contains ("--snapshot") || args.contains ("--quit");
    }

    // Kommandozeile (für Tests ohne Mausbedienung):
    //   --offline                 keinen MIDI-Port öffnen
    //   --testmode                feste Uhrzeit, kein Datum (reproduzierbare Snapshots)
    //   --load=<datei>            Patch (.syx/.mid) laden (nach --delay)
    //   --tone=<A-D>              Tone vorwählen
    //   --tab=<n>                 Tab 0 Panel · 1 Patch & Effekte · 2 Alle Parameter · 3 Bibliothek
    //   --size=<B>x<H>            Fenstergröße (für Snapshots)
    //   --scale=<prozent>         Zoom der Oberfläche (Default: gespeichert, 130; im --testmode 100)
    //   --library=<datei>         Bibliothek statt Standardort (Tests)
    //   --libimport=<datei>       Datei in die Bibliothek importieren (mehrfach möglich)
    //   --libsearch=<text>        Suche · --libselect=<zeile>[,<zeile>] Eintrag/zwei Einträge wählen · --ab=<n> n-mal A/B umschalten
    //   --libsimilar              ähnliche Einträge zum gewählten Eintrag zeigen
    //   --morph=<prozent>         Morph Editor → gewählter Eintrag · --variation=<prozent> Variation erzeugen
    //   --copytone=A:C[:GRUPPE]   Tone (oder nur eine Gruppe) kopieren · --swaptone=A:B Tones tauschen
    //   --movefx=A:0:3            Effekt in Kette A von Position 0 an Position 3 verschieben
    //   --fxselect=A:2[,B:1]      Effektblock wählen (A: 0 DS 1 PH 2 SP 3 EN · B: 0 CHO 1 DLY 2 REV)
    //   --link                    Link-Modus mit allen vier Tones
    //   --envvalues               Hüllkurven-Gruppen mit aufgeklappten T-/L-Fadern
    //   --compare[=<Gruppe>]      Panel im 4×-Vergleich zeigen (z. B. --compare=TVF)
    //   --set=<id>:<A-D|->:<wert> nach --delay wie eine UI-Eingabe setzen (mehrfach möglich)
    //   --delay=<ms>              Wartezeit vor --set (Default 800; Gerät muss geantwortet haben)
    //   --poll=<ms>               Abgleich-Intervall (Default 5000 wenn verbunden, 0 = aus)
    //   --run=<ms>                Laufzeit nach --delay bis Snapshot/Quit (Default 1500)
    //   --init                    Init-Patch laden (nach --delay, vor --set)
    //   --undo=<n>                nach --set n-mal rückgängig machen
    //   --lock=<Gruppe>           Gruppe für Variation/Morph sperren (mehrfach möglich, vor --variation)
    //   --history                 Undo-Verlauf am Ende auf stdout ausgeben
    //   --unit=<17-32>            Unit # / SysEx-Device-ID (wird gespeichert, außer im --testmode)
    //   --perf                    Performance-Modus (offline: Init-Setup) · --part=<1-7> Part wählen
    //   --perfset=<id>:<1-8|->:<wert>  Performance-/Part-Parameter setzen (nach --perf)
    //   --send                    nach --set den ganzen Patch senden („An Gerät senden“)
    //   --snapshot=<png>          danach Fenster als PNG rendern (--snapshotscale=2 für Retina-Auflösung)
    //   --quit                    danach beenden (implizit bei --snapshot)
    void initialise (const juce::String&) override
    {
        auto args = getCommandLineParameterArray();
        Model::testMode = args.contains ("--testmode");
        auto value = [&] (const juce::String& key) {
            for (auto& a : args) if (a.startsWith (key + "=")) return a.fromFirstOccurrenceOf ("=", false, false);
            return juce::String();
        };
        window = std::make_unique<Window> (getApplicationName(), ! args.contains ("--offline"), value ("--library"),
                                           value ("--scale").getIntValue());
        auto* zoom = dynamic_cast<ZoomHost*> (window->getContentComponent());
        if (zoom == nullptr) return;
        auto* mc = &zoom->main();
        // --load erst nach --delay: sonst überschreibt die Antwort des Geräts auf das
        // RQ1 beim Start die gerade geladene Datei.
        auto load = value ("--load");
        if (auto t = value ("--tone"); t.length() == 1)
            mc->selectTone (t.toUpperCase()[0] - 'A');
        if (auto* lib = mc->library())
        {
            for (auto& a : args)
                if (a.startsWith ("--libimport="))
                    lib->import (juce::File::getCurrentWorkingDirectory().getChildFile (a.fromFirstOccurrenceOf ("=", false, false)));
            if (auto q = value ("--libsearch"); q.isNotEmpty()) lib->search (q);
            if (auto r = value ("--libselect"); r.isNotEmpty())   // --libselect=0 oder 0,2 (zwei Einträge vergleichen)
            {
                auto rows = juce::StringArray::fromTokens (r, ",", "");
                lib->selectRow (rows[0].getIntValue());
                for (int i = 1; i < rows.size(); ++i) lib->addRow (rows[i].getIntValue());
            }
            if (args.contains ("--libsimilar")) lib->similar();
        }
        if (auto tab = value ("--tab"); tab.isNotEmpty())
            mc->showTab (tab.getIntValue());
        if (args.contains ("--link")) { mc->getModel().linkMode = true; mc->getModel().linked = { 0, 1, 2, 3 }; mc->getModel().sendChangeMessage(); }
        if (args.contains ("--envvalues")) mc->showEnvValues (true);
        if (args.contains ("--compare") || value ("--compare").isNotEmpty()) mc->showCompare (value ("--compare"));
        if (auto size = value ("--size"); size.containsChar ('x'))
            window->setSize (size.upToFirstOccurrenceOf ("x", false, false).getIntValue(), size.fromFirstOccurrenceOf ("x", false, false).getIntValue());

        for (auto& a : args)
            if (a.startsWith ("--lock=")) mc->getModel().lockedGroups.insert (a.fromFirstOccurrenceOf ("=", false, false));
        juce::StringArray sets;
        for (auto& a : args) if (a.startsWith ("--set=")) sets.add (a.fromFirstOccurrenceOf ("=", false, false));
        auto snap = value ("--snapshot");
        bool quitAfter = snap.isNotEmpty() || args.contains ("--quit");
        int delay = value ("--delay").getIntValue() > 0 ? value ("--delay").getIntValue() : 800;
        int run = value ("--run").getIntValue() > 0 ? value ("--run").getIntValue() : 1500;
        if (auto poll = value ("--poll"); poll.isNotEmpty())
            mc->setPolling (poll.getIntValue());

        bool send = args.contains ("--send"), init = args.contains ("--init");
        int abToggles = value ("--ab").getIntValue();
        int morphPct = value ("--morph").getIntValue(), varPct = value ("--variation").getIntValue();
        auto copyTone = value ("--copytone"), swapTone = value ("--swaptone"), moveFx = value ("--movefx"), fxSelect = value ("--fxselect");
        int undos = value ("--undo").getIntValue();
        bool perfHook = args.contains ("--perf");
        juce::Timer::callAfterDelay (delay, [mc, sets, send, load, init, undos, abToggles, morphPct, varPct, copyTone, swapTone, moveFx, fxSelect, perfHook] {
            if (load.isNotEmpty())
                mc->loadFile (juce::File::getCurrentWorkingDirectory().getChildFile (load));
            if (init) mc->loadInit();
            for (auto& s : (perfHook && mc->getModel().link.isOpen()) ? juce::StringArray() : sets)   // mit --perf am Gerät: erst nach dem Umschalten (Part)
            {
                auto parts = juce::StringArray::fromTokens (s, ":", "");
                if (parts.size() != 3) continue;
                int tone = parts[1] == "-" ? -1 : parts[1].toUpperCase()[0] - 'A';
                if (! mc->setParam (parts[0], tone, parts[2].getIntValue()))
                    std::fprintf (stderr, "--set: unbekannt %s\n", parts[0].toRawUTF8());
            }
            // Reihenfolge: Eingaben, Variation, A/B, Morph — Undo zuletzt (macht das Vorige rückgängig)
            if (varPct > 0) mc->variation (varPct);
            if (copyTone.isNotEmpty())   // --copytone=A:C[:GRUPPE]
            {
                auto p = juce::StringArray::fromTokens (copyTone, ":", "");
                mc->getModel().copyTone (p[0].toUpperCase()[0] - 'A', p[1].toUpperCase()[0] - 'A', p.size() > 2 ? p[2] : juce::String());
            }
            if (moveFx.isNotEmpty())     // --movefx=A:0:3 (Gruppe, von Position, nach Position)
                if (auto* pv = mc->patchView())
                {
                    auto p = juce::StringArray::fromTokens (moveFx, ":", "");
                    pv->moveEffect (p[0].toUpperCase() == "B" ? 1 : 0, p[1].getIntValue(), p[2].getIntValue());
                }
            if (fxSelect.isNotEmpty())   // --fxselect=A:2[,B:1] (Gruppe, Effekt)
                if (auto* pv = mc->patchView())
                    for (auto& f : juce::StringArray::fromTokens (fxSelect, ",", ""))
                        pv->selectEffect (f.toUpperCase()[0] == 'B' ? 1 : 0, f.fromFirstOccurrenceOf (":", false, false).getIntValue());
            if (swapTone.isNotEmpty())   // --swaptone=A:B
                mc->getModel().swapTones (swapTone.toUpperCase()[0] - 'A', swapTone.toUpperCase()[2] - 'A');
            for (int i = 0; i < abToggles; ++i) if (auto* lib = mc->library()) lib->abToggle();
            if (morphPct > 0) if (auto* lib = mc->library()) lib->morphPercent (morphPct);
            for (int i = 0; i < undos; ++i) mc->undo();
            if (send) mc->sendToDevice();
        });
        if (auto unit = value ("--unit").getIntValue(); unit >= 17 && unit <= 32) mc->getModel().setUnitNumber (unit);
        bool history = args.contains ("--history");
        juce::StringArray perfSets;
        for (auto& a : args) if (a.startsWith ("--perfset=")) perfSets.add (a.fromFirstOccurrenceOf ("=", false, false));
        if (args.contains ("--perf"))
            juce::Timer::callAfterDelay (delay + 50, [mc, perfSets, part = value ("--part").getIntValue(), patchSets = sets] {
                mc->setMode (1);
                // Verbunden: das Gerät schaltet um und wird gelesen (einige Sekunden) — erst danach Part/Werte.
                // Gewartet wird, bis der Editor wirklich im Performance-Modus ist (höchstens 15 s).
                bool online = mc->getModel().link.isOpen();
                auto whenReady = std::make_shared<std::function<void (int)>>();
                *whenReady = [mc, whenReady, perfSets, part, patchSets, online] (int tries) {
                if (! mc->getModel().perfMode() && tries > 0) { juce::Timer::callAfterDelay (250, [whenReady, tries] { (*whenReady) (tries - 1); }); return; }
                if (part >= 1 && part <= 7) mc->selectPart (part - 1);
                if (online)                          // --set wirkt dann auf den gewählten Part
                    for (auto& s : patchSets)
                    {
                        auto t = juce::StringArray::fromTokens (s, ":", "");
                        if (t.size() == 3) mc->setParam (t[0], t[1] == "-" ? -1 : t[1].toUpperCase()[0] - 'A', t[2].getIntValue());
                    }
                for (auto& s : perfSets)
                {
                    auto t = juce::StringArray::fromTokens (s, ":", "");
                    auto* prm = t.size() == 3 ? mc->getModel().findPerf (t[0]) : nullptr;
                    if (prm == nullptr) { std::fprintf (stderr, "--perfset: unbekannt %s\n", s.toRawUTF8()); continue; }
                    mc->getModel().perfSet (prm->index, t[1] == "-" ? -1 : t[1].getIntValue() - 1, t[2].getIntValue());
                }
                };
                (*whenReady) (online ? 60 : 0);
            });
        if (quitAfter)
            juce::Timer::callAfterDelay (delay + run, [this, snap, history, mc, shotScale = juce::jlimit (1.0f, 3.0f, value ("--snapshotscale").getFloatValue())] {
                if (history)
                {
                    for (auto& h : mc->getModel().redoHistory()) std::printf ("redo  %s\n", h.toRawUTF8());
                    for (auto& h : mc->getModel().undoHistory()) std::printf ("undo  %s\n", h.toRawUTF8());
                    std::fflush (stdout);
                }
                if (snap.isNotEmpty())
                {
                    auto img = window->getContentComponent()->createComponentSnapshot (window->getContentComponent()->getLocalBounds(), true, shotScale);
                    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (snap);
                    out.deleteFile();
                    juce::FileOutputStream os (out);
                    juce::PNGImageFormat().writeImageToStream (img, os);
                    os.flush();
                }
                quit();
            });
    }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class Window : public juce::DocumentWindow
    {
    public:
        Window (const juce::String& name, bool autoConnect, const juce::String& libraryPath, int scalePercent)
            : DocumentWindow (name, juce::Colour (0xff1e1f22), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new ZoomHost (std::make_unique<MainComponent> (autoConnect, libraryPath), scalePercent), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (FotonJD990Application)
