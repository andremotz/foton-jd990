#include "DeviceLink.h"
#include "jd990.h"

namespace
{
    constexpr double bytesPerSecond = 3125.0 * 0.6;   // ADR-8: 60 % der MIDI-Bandbreite
    constexpr double commonSettleMs = 150.0;          // Logbuch 2026-09-28
    constexpr double bulkGapMs = 20.0;

    uint32_t addressOf (const std::vector<uint8_t>& m)
    {
        return (uint32_t (m[5]) << 21) | (uint32_t (m[6]) << 14) | (uint32_t (m[7]) << 7) | m[8];
    }

    constexpr double reloadSettleMs = 700.0;          // Moduswechsel / Patchwahl: Gerät lädt aus dem Speicher

    /** Wartezeit nach einer Bulk-Nachricht: Blockanfang 150 ms, System mode und
        Patchnummer eines Parts 700 ms, sonst 20 ms. */
    double settleAfter (const std::vector<uint8_t>& m)
    {
        auto a = addressOf (m);
        if (a == 0) return reloadSettleMs;                                  // System mode
        if (m[5] == 0x01)
        {
            int off = (int) (a & 0x3FFF);
            if (off >= 0x30 && (off - 0x30) % 12 == 3) return reloadSettleMs;   // Part: Patch Number
            if (off == 0) return commonSettleMs;                            // ganzer Performance-Block
            return bulkGapMs;
        }
        return (a & 0x3FFF) == 0 ? commonSettleMs : bulkGapMs;              // Patch-Blockanfang
    }
}

bool DeviceLink::allowedDt1 (const uint8_t* m, size_t len)
{
    if (len < 12 || m[0] != 0xF0 || m[1] != 0x41 || m[3] != 0x57 || m[4] != 0x12 || m[len - 1] != 0xF7) return false;
    size_t data = len - 11;
    int area = m[5], number = m[6], off = (m[7] << 7) | m[8];
    switch (area)
    {
        case 0x03: return number == 0 && off + (int) data <= 486;
        case 0x02: return number <= 6 && off + (int) data <= 486;
        case 0x01: return number == 0 && off + (int) data <= 144;
        case 0x00: return number == 0 && off == 0 && data == 1 && m[9] <= 1;   // nur System mode PATCH/PERFORMANCE
        default:   return false;
    }
}

DeviceLink::DeviceLink() { startTimer (5); }

DeviceLink::~DeviceLink() { stopTimer(); close(); }

juce::Array<juce::MidiDeviceInfo> DeviceLink::outputs() { return juce::MidiOutput::getAvailableDevices(); }

bool DeviceLink::open (const juce::String& name)
{
    close();
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (d.name == name)
            out = juce::MidiOutput::openDevice (d.identifier);
    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (d.name == name)
            in = juce::MidiInput::openDevice (d.identifier, this);
    if (out == nullptr || in == nullptr)
    {
        close();
        return false;
    }
    in->start();
    openName = name;
    return true;
}

void DeviceLink::noteOn (int channel, int note, int velocity)
{
    if (out == nullptr) return;
    out->sendMessageNow (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, velocity)));
    held.insert ({ channel, note });
}

void DeviceLink::noteOff (int channel, int note)
{
    if (out != nullptr) out->sendMessageNow (juce::MidiMessage::noteOff (channel, note));
    held.erase ({ channel, note });
}

void DeviceLink::allNotesOff()
{
    auto h = held;
    for (auto& [c, n] : h) noteOff (c, n);
}

void DeviceLink::close()
{
    allNotesOff();
    {
        const juce::ScopedLock sl (lock);
        requests.clear();
        waiting = false;
        currentDone = nullptr;
    }
    if (in != nullptr) in->stop();
    in.reset();
    out.reset();
    openName = {};
}

bool DeviceLink::busy() const
{
    const juce::ScopedLock sl (lock);
    return waiting || ! requests.empty() || ! pending.empty() || ! bulk.empty()
        || juce::Time::getMillisecondCounterHiRes() - lastSendAt < 1500.0;
}

void DeviceLink::sendRaw (const std::vector<uint8_t>& msg)
{
    lastSendAt = juce::Time::getMillisecondCounterHiRes();
    if (out != nullptr)
    {
        out->sendMessageNow (juce::MidiMessage (msg.data(), (int) msg.size()));
        if (onSent && msg.size() > 4 && msg[4] == 0x12) onSent (msg);   // nur DT1 zeigen
    }
}

void DeviceLink::requestTempPatch() { requestInternalPatch (-1); }

void DeviceLink::requestInternalPatch (int number)
{
    uint8_t rq[32];
    auto n = number < 0 ? jd990_rq1_temp_patch (rq, sizeof rq) : jd990_rq1_patch (6, (uint8_t) number, rq, sizeof rq);
    if (n <= 0) return;
    request (std::vector<uint8_t> (rq, rq + n), 486, [this] (const std::vector<uint8_t>& b) { if (! b.empty() && onPatch) onPatch (b); });
}

void DeviceLink::request (std::vector<uint8_t> rq1, int size, std::function<void (const std::vector<uint8_t>&)> done)
{
    const juce::ScopedLock sl (lock);
    requests.push_back ({ std::move (rq1), size, std::move (done) });
    if (! waiting) startNextRequest();
}

void DeviceLink::startNextRequest()
{
    if (requests.empty() || out == nullptr) return;
    auto r = std::move (requests.front());
    requests.erase (requests.begin());
    incoming.clear();
    incomingData = 0;
    expected = r.size;
    currentDone = std::move (r.done);
    waiting = true;
    waitingSince = juce::Time::getMillisecondCounterHiRes();
    out->sendMessageNow (juce::MidiMessage (r.rq.data(), (int) r.rq.size()));   // Leseanfrage: zählt nicht als „gerade gesendet“
}

void DeviceLink::queueParam (const uint8_t* msg, size_t len)
{
    if (! allowedDt1 (msg, len)) { jassertfalse; return; }   // nur freigegebene Temp-Bereiche
    const juce::ScopedLock sl (lock);
    std::vector<uint8_t> m (msg, msg + len);
    pending[addressOf (m)] = std::move (m);
}

void DeviceLink::sendPatch (const std::vector<uint8_t>& syx)
{
    const juce::ScopedLock sl (lock);
    bulk.clear();
    size_t start = 0;
    for (size_t i = 0; i < syx.size(); ++i)
    {
        if (syx[i] == 0xF0) start = i;
        if (syx[i] == 0xF7)
        {
            if (! allowedDt1 (syx.data() + start, i + 1 - start)) { jassertfalse; bulk.clear(); return; }
            bulk.emplace_back (syx.begin() + (long) start, syx.begin() + (long) i + 1);
        }
    }
    nextBulkAt = 0;
}

void DeviceLink::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (! m.isSysEx()) return;
    auto* raw = m.getRawData();
    auto len = (size_t) m.getRawDataSize();
    if (len < 12 || raw[1] != 0x41 || raw[3] != 0x57 || raw[4] != 0x12) return;

    std::vector<uint8_t> complete;
    std::function<void (const std::vector<uint8_t>&)> done;
    {
        const juce::ScopedLock sl (lock);
        if (! waiting) return;
        incoming.insert (incoming.end(), raw, raw + len);
        incomingData += (int) len - 11;
        if (incomingData >= expected)
        {
            waiting = false;
            complete = incoming;
            done = std::move (currentDone);
            startNextRequest();
        }
    }
    if (! complete.empty() && done)
        juce::MessageManager::callAsync ([done, complete] { done (complete); });
}

void DeviceLink::timerCallback()
{
    auto now = juce::Time::getMillisecondCounterHiRes();
    if (lastTick > 0)
        tokens = std::min (300.0, tokens + (now - lastTick) / 1000.0 * bytesPerSecond);
    lastTick = now;

    std::vector<std::vector<uint8_t>> toSend;
    {
        const juce::ScopedLock sl (lock);
        if (waiting && now - waitingSince > 2000.0)   // 486 Byte brauchen ~0,2 s — 2 s heißt: keine Antwort
        {
            waiting = false;
            incoming.clear();
            incomingData = 0;
            auto done = std::move (currentDone);
            requests.clear();                          // Folgeanfragen hängen am selben Gerät — verwerfen
            juce::MessageManager::callAsync ([this, done] {
                if (done) done ({});
                if (onStatus) onStatus (juce::String::fromUTF8 ("Keine Antwort vom JD-990 (Port, Kabel, Device ID, Rx Exclusive?)"));
            });
        }
        // Bulk zuerst, wenn er läuft — ein Parameter darf ihn nicht überholen
        if (! bulk.empty())
        {
            if (now >= nextBulkAt && tokens >= (double) bulk.front().size())
            {
                auto m = bulk.front();
                bulk.erase (bulk.begin());
                tokens -= (double) m.size();
                nextBulkAt = now + settleAfter (m);
                toSend.push_back (std::move (m));
                if (bulk.empty() && onStatus)
                    juce::MessageManager::callAsync ([this] { onStatus ("Patch gesendet"); });
            }
        }
        else
        {
            while (! pending.empty() && tokens >= (double) pending.begin()->second.size())
            {
                tokens -= (double) pending.begin()->second.size();
                toSend.push_back (std::move (pending.begin()->second));
                pending.erase (pending.begin());
            }
        }
    }
    for (auto& m : toSend)
        sendRaw (m);
}
