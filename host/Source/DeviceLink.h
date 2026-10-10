#pragma once
// Verbindung zum JD-990 über JUCE-MIDI. Kennt nur Bytes; Bedeutung liefert der
// Rust-Kern (jd990.h). Regeln aus M0/M2 (internes Logbuch):
//  * Parameteränderungen werden je Adresse zusammengefasst (last value wins)
//    und mit Byte-Budget gesendet (ADR-8).
//  * Ganzer Patch: nach dem Common-Block 150 ms Pause (Key-Range-Falle).
//  * Geschrieben wird nur in die freigegebenen Temp-Bereiche — hier geprüft (ADR-7/22):
//    03 Temp-Patch, 01 Temp-Performance, 02 00–06 Part-Patches, System mode 00 00 00 00.
//  * Wartezeiten (P0, Logbuch 2026-10-03): nach Block-Anfang 150 ms, nach Modus-
//    wechsel und Patchwahl eines Parts 700 ms (das Gerät lädt dann aus dem Speicher).

#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>
#include <map>
#include <set>

class DeviceLink : private juce::MidiInputCallback, private juce::Timer
{
public:
    DeviceLink();
    ~DeviceLink() override;

    static juce::Array<juce::MidiDeviceInfo> outputs();
    bool open (const juce::String& portName);
    void close();
    bool isOpen() const { return out != nullptr; }
    juce::String portName() const { return openName; }

    /** RQ1 für den ganzen Temp Patch. `onPatch` bekommt die empfangenen
        SysEx-Bytes (F0…F7, aneinandergehängt), sobald 486 Datenbytes da sind. */
    void requestTempPatch();
    /** RQ1 für Internal Patch n (0–63) — nur lesen. Antwort wie bei requestTempPatch über onPatch. */
    void requestInternalPatch (int n);
    /** Allgemeine Leseanfrage (RQ1-Nachricht, erwartete Datenbytes). Anfragen laufen
        nacheinander; `done` bekommt die SysEx-Bytes, bei Zeitüberschreitung leer. */
    void request (std::vector<uint8_t> rq1, int size, std::function<void (const std::vector<uint8_t>&)> done);
    /** Darf diese DT1-Nachricht an das Gerät? (Freigabeliste, ADR-7/22) */
    static bool allowedDt1 (const uint8_t* m, size_t len);
    /** true, solange gesendet wird oder vor < 1,5 s gesendet wurde, oder eine
        Leseanfrage läuft — dann keinen Abgleich starten. */
    bool busy() const;
    std::function<void (const std::vector<uint8_t>&)> onPatch;
    std::function<void (const juce::String&)> onStatus;
    /** Jede gesendete Nachricht (Nachrichten-Thread) — für den SysEx-Inspektor. */
    std::function<void (const std::vector<uint8_t>&)> onSent;

    /** Eine DT1-Nachricht (F0…F7) — wird je Adresse zusammengefasst. */
    void queueParam (const uint8_t* msg, size_t len);

    /** Vorhören: Note an/aus auf `channel` (1–16). Gehaltene Noten werden beim
        Trennen/Beenden sicher ausgeschaltet. */
    void noteOn (int channel, int note, int velocity);
    void noteOff (int channel, int note);
    void allNotesOff();

    /** Ganzer Patch als .syx (mehrere DT1), mit Pause nach dem Common-Block. */
    void sendPatch (const std::vector<uint8_t>& syx);

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void timerCallback() override;
    void sendRaw (const std::vector<uint8_t>& msg);

    std::unique_ptr<juce::MidiOutput> out;
    std::unique_ptr<juce::MidiInput> in;
    juce::String openName;

    juce::CriticalSection lock;
    std::vector<uint8_t> incoming;        // gesammelte DT1-Nachrichten
    int incomingData = 0, expected = 486;
    bool waiting = false;
    double waitingSince = 0;              // Zeitüberschreitung: keine Antwort → nicht ewig „busy“
    struct Request { std::vector<uint8_t> rq; int size; std::function<void (const std::vector<uint8_t>&)> done; };
    std::vector<Request> requests;        // wartende Leseanfragen (nacheinander)
    std::function<void (const std::vector<uint8_t>&)> currentDone;
    void startNextRequest();              // unter `lock` aufrufen

    std::map<uint32_t, std::vector<uint8_t>> pending;    // Adresse → letzte Nachricht
    std::vector<std::vector<uint8_t>> bulk;              // Patch-Nachrichten in Reihenfolge
    double nextBulkAt = 0;
    double tokens = 300, lastTick = 0;
    double lastSendAt = 0;
    std::set<std::pair<int, int>> held;   // (Kanal, Note) — für sicheres Ausschalten
};
