#pragma once
// Gemeinsamer Zustand aller Ansichten (Panel, Patch & Effekte, Alle Parameter,
// Bibliothek). Ansichten lesen über get()/display() und schreiben über set();
// Änderungen melden sie per ChangeBroadcaster (synchron).
//
// Hier liegen die Regeln, die für jede Ansicht gleich sein müssen:
//  * jede Änderung → Rust-Kern (Bereich, Geräteregeln) → DT1 in die Warteschlange
//  * Undo/Redo gebündelt, sendet nur den Diff
//  * Link: Tone-Parameter ändern sich in allen gelinkten Tones relativ
//  * ehrlicher Sync-Status und optionaler Abgleich (R5)

#include "DeviceLink.h"
#include "jd990.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <set>

struct PatchDeleter { void operator() (Jd990Patch* p) const { jd990_patch_free (p); } };
using PatchPtr = std::unique_ptr<Jd990Patch, PatchDeleter>;
struct PerfDeleter { void operator() (Jd990Performance* p) const { jd990_perf_free (p); } };
using PerfPtr = std::unique_ptr<Jd990Performance, PerfDeleter>;

class Model : public juce::ChangeBroadcaster, private juce::Timer
{
public:
    /** Testmodus (--testmode): feste Uhrzeit/kein Datum in der Anzeige → reproduzierbare Snapshots. */
    static inline bool testMode = false;
    /** Einstellungen der App (Zoom, aufgeklappte Gruppen …); nullptr im Testmodus → nichts wird gespeichert. */
    static juce::PropertiesFile* settings();
    Model();
    ~Model() override;

    struct Param
    {
        uint32_t index;
        Jd990ParamInfo info;
        juce::String id, label, group;
        bool isTone() const { return info.is_tone != 0; }
    };
    const std::vector<Param>& params() const { return all; }
    const Param* find (const juce::String& id) const;

    // ── Patch ────────────────────────────────────────────────────────────────
    Jd990Patch* patch() const { return current.get(); }
    bool hasPatch() const { return loaded; }
    juce::String patchName() const;
    int get (uint32_t index, int tone) const;             // tone −1 bei Patch-Parametern
    juce::String display (uint32_t index, int tone) const;
    /** Wie eine Eingabe: Bereich, Geräteregeln, Undo-Bündelung, Link, Senden. */
    void set (uint32_t index, int tone, int value);
    void setPatch (PatchPtr p, const juce::String& origin);
    bool loadFile (const juce::File&);
    bool saveFile (const juce::File&);
    void loadInit();
    /** Mausgeste (z. B. Hüllkurvenpunkt ziehen): alle set() dazwischen = ein Undo-Schritt. */
    void beginGesture() { gesture = true; gesturePending = true; }
    void endGesture() { gesture = false; gesturePending = false; lastEditKey = -1; }
    void undo();
    void redo();
    void copyTone (int from, int to, const juce::String& group = {})
    {
        if (! loaded) return;
        applyWithUndo (PatchPtr (jd990_patch_tone_copy (current.get(), from, to, group.isEmpty() ? nullptr : group.toRawUTF8())),
                       juce::String::fromUTF8 ("Tone ") + juce::String::charToString ((juce::juce_wchar) ('A' + from)) + (group.isEmpty() ? "" : " " + group)
                           + juce::String::fromUTF8 (" → ") + juce::String::charToString ((juce::juce_wchar) ('A' + to)) + " kopiert");
    }
    void swapTones (int a, int b)
    {
        if (! loaded) return;
        applyWithUndo (PatchPtr (jd990_patch_tone_swap (current.get(), a, b)),
                       juce::String::fromUTF8 ("Tone ") + juce::String::charToString ((juce::juce_wchar) ('A' + a)) + juce::String::fromUTF8 (" ↔ ")
                           + juce::String::charToString ((juce::juce_wchar) ('A' + b)) + " getauscht");
    }
    /** Neuer Patch als rückgängig machbarer Schritt, sendet nur den Unterschied (Variation). */
    void applyWithUndo (PatchPtr p, const juce::String& what) { if (! loaded) return; pushUndo (what); redoStack.clear(); applyPatch (std::move (p), what); }
    /** A/B-Vergleich: zeigt und sendet `p` (nur den Unterschied), ohne Undo-Schritt. */
    void showAB (PatchPtr p, const juce::String& what) { applyPatch (std::move (p), what); }
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    /** Verlauf für die Liste: zuerst die rückgängig machbaren Schritte (neuester zuerst), dann die wiederholbaren. */
    juce::StringArray undoHistory (int max = 40) const;
    juce::StringArray redoHistory (int max = 40) const;
    /** Mehrere Schritte auf einmal (sendet nur den Unterschied zum Ziel). */
    void undoSteps (int n);
    void redoSteps (int n);

    // ── Sperren (Variation/Morph lassen gesperrte Gruppen unverändert) ───────
    std::set<juce::String> lockedGroups;
    bool isLocked (const juce::String& group) const { return lockedGroups.count (group) > 0; }
    void toggleLock (const juce::String& group);
    /** Setzt alle gesperrten Parameter in `target` auf die Werte aus `source` (alle Tones). */
    void applyLocks (Jd990Patch* target, const Jd990Patch* source) const;

    // ── Auswahl / Panel-Zustand ──────────────────────────────────────────────
    int tone = 0;                          // aktueller Tone 0–3
    bool linkMode = false;                 // Link: Änderungen wirken relativ auf `linked`
    std::set<int> linked { 0 };
    int lastIndex = -1, lastTone = -1;     // zuletzt berührter Parameter (LCD)
    void selectTone (int t);
    void toggleLinked (int t);

    // ── Performance-Modus (ADR-22) ───────────────────────────────────────────
    // Im Performance-Modus bearbeiten alle Patch-Ansichten den Patch des gewählten
    // Parts (02 0n): `current` ist dann dieser Part, die anderen liegen in `parts`.
    // Jede Patch-Nachricht wird auf den Part umadressiert (jd990_dt1_retarget).
    struct PerfParam
    {
        uint32_t index;
        Jd990ParamInfo info;
        juce::String id, label, group;
        bool isPart() const { return info.is_tone != 0; }
    };
    const std::vector<PerfParam>& perfParams() const { return perfAll; }
    const PerfParam* findPerf (const juce::String& id) const;
    bool perfMode() const { return editPart >= 0; }
    int part() const { return editPart; }                   // 0–6 im Performance-Modus
    int deviceMode = -1;                                    // −1 unbekannt, 0 PATCH, 1 PERFORMANCE
    Jd990Performance* performance() const { return perf.get(); }
    int perfGet (uint32_t index, int part) const;           // part −1 bei Common-Parametern
    juce::String perfDisplay (uint32_t index, int part) const;
    /** Wie eine Eingabe: Bereich/Stimmenregel im Kern, Undo, Senden an 01. Patchwahl
        (Bank/Nummer) lädt am Gerät den Speicher-Patch in den Part — der wird danach gelesen. */
    void perfSet (uint32_t index, int part, int value);
    void setPerfName (const juce::String& name);
    /** Patch eines Parts aus dem Speicher wählen (Bank 0 INT/Card, 1 Preset A/B; Nummer 0–127).
        Das Gerät lädt ihn in den Part; der Editor liest ihn danach. Kein Undo-Schritt. */
    void choosePartPatch (int part, int bank, int number);
    juce::String perfName() const;
    juce::String partPatchName (int part) const;
    juce::String partSlot (int part) const;
    int partChannel (int part) const;                       // 1–16
    int voicesReserved() const { return perf ? jd990_perf_voices (perf.get()) : 0; }
    void selectPart (int p);
    /** Modus wechseln: 0 PATCH, 1 PERFORMANCE. Verbunden: das Gerät umschalten
        (System mode) und dann lesen, was es geladen hat. Offline: nur der Editor. */
    void setMode (int mode);
    void readSetupFromDevice();
    void sendSetupToDevice();

    // ── Gerät ────────────────────────────────────────────────────────────────
    DeviceLink link;
    /** „Unit #“ des JD-990 (SYSTEM › MIDI), 17–32 ≙ SysEx-Device-ID 0x10–0x1F. Gespeichert. */
    int unitNumber() const { return jd990_device_id() - 0x10 + 17; }
    void setUnitNumber (int unit);
    bool connect (const juce::String& port, bool quiet);
    void readFromDevice();
    /** Liest Internal I:11 … I:88 nacheinander (nur RQ1). `onEach` je Patch, `onDone` am Ende. */
    void readBank (std::function<void (int, PatchPtr)> onEach, std::function<void (int)> onDone);
    bool readingBank() const { return bankIndex >= 0; }
    void sendToDevice();
    void setPolling (int ms);
    bool polling() const { return isTimerRunning(); }
    juce::String syncText;                 // „Gerät = Editor …“ / „⚠ Gerät weicht ab …“
    juce::Colour syncColour { juce::Colours::grey };
    std::vector<uint8_t> lastSent;         // zuletzt gesendete Nachricht (SysEx-Inspektor)

    std::function<void (const juce::String&)> onStatus;

private:
    void timerCallback() override;
    void pushUndo (const juce::String& what = {}, int param = -1, int tone = -1);
    juce::String stepText (size_t i, bool redo) const;
    void applyPatch (PatchPtr target, const juce::String& what);
    void applyPerf (PerfPtr target);
    void enterPerformance (PerfPtr p, std::array<PatchPtr, 7> patches, const juce::String& origin);
    void leavePerformance (PatchPtr patchModePatch, const juce::String& origin);
    void readPartFromDevice (int part);
    void checkDeviceMode();
    std::vector<uint8_t> routed (const uint8_t* msg, size_t len) const;   // Patch-DT1 → Ziel (03 bzw. 02 0n)
    void queueDt1 (const uint8_t* msg, size_t len);
    void status (const juce::String& s) { if (onStatus) onStatus (s); }
    void changed() { sendSynchronousChangeMessage(); }
    std::vector<uint8_t> bytesOf (const Jd990Patch*) const;
    void setSync (const juce::String& text, juce::Colour c) { syncText = text; syncColour = c; }

    std::vector<Param> all;
    PatchPtr current;
    bool loaded = false;
    bool checking = false;
    int bankIndex = -1, bankRead = 0;
    std::function<void (int, PatchPtr)> bankEach;
    std::function<void (int)> bankDone;
    struct Step
    {
        PatchPtr state;                    // Undo: Zustand vor dem Schritt · Redo: Zustand nach dem Schritt
        juce::String what;                 // Beschreibung (leer bei Parameterschritten)
        int param = -1, tone = -1;         // Parameterschritt: Anzeige „alt → neu“
        PerfPtr perfState;                 // Performance-Schritt: Performance vor/nach dem Schritt
        int perfParam = -1, perfPart = -1;
    };
    std::vector<Step> undoStack, redoStack;

    std::vector<PerfParam> perfAll;
    int editPart = -1;
    PerfPtr perf;
    std::array<PatchPtr, 7> parts;         // Part-Patches außer dem gewählten (der liegt in `current`)
    PatchPtr patchModePatch;               // Patch des Patch-Modus, solange Performance bearbeitet wird
    struct Slot { std::vector<Step> undo, redo; };
    std::array<Slot, 8> slots;             // Undo je Ziel: 0–6 Parts, 7 Patch-Modus
    void pushPerfUndo (int param, int part);
    double lastEditMs = 0;
    bool gesture = false, gesturePending = false;
    int64_t lastEditKey = -1;
};
