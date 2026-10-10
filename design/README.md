# design/ — Entwurfsprototypen

Schnelle Iteration am Layout, **nicht** die spätere Implementierung.

## panel-prototype.html

Interaktiver Entwurf der Tone-Ebene, gerendert aus `../spec/params.toml`.
Im Browser öffnen, keine Abhängigkeiten.

**Warum HTML und nicht gleich JUCE?** Der Compile-Zyklus von C++ ist der falsche
Takt für Layoutfragen. Hier kostet eine Variante Sekunden. Was portiert wird, ist
nicht dieser Code, sondern die **Designentscheidungen**: Rastermaße, Farbsystem,
Faderverhalten, Gruppierung. Die landen in M4 in JUCE.

**Designelemente mit Herkunft**
- *Seitenteil (BASICS)* — Platz des JD-800-Seitenteils, trägt die Patch-Basics.
  **Die Schräge ist vorerst raus.** Sie war umgesetzt, aber über die volle
  Panelhöhe ergaben 30 px Versatz nur ~3° — unsichtbar. Sichtbar wird sie erst
  bei 60–80 px, und die kosten dann echte Breite. Wiederaufnahme sinnvoll, wenn
  das Seitenteil eine feste Höhe bekommt statt mit der Fader-Wand mitzuwachsen.
- *Grün hinterleuchtetes Display* — Zitat des JD-990-LCD: heller Backlight-Grund,
  dunkle Zeichen, Pixelraster. Zeigt den zuletzt berührten Parameter mit Kontext,
  Wert und Bargraph. Kein Dekor: bei 105 Parametern braucht die Fader-Wand eine
  Stelle, die sagt, woran man gerade dreht.
- *Waveform* — ein Dropdown in der WG-Gruppe, an genau einer Stelle. Der frühere
  Picker mit Suchfeld oben rechts ist raus: er war für 195 Einträge überdimensioniert,
  und zwei Bedienstellen für denselben Wert lesen sich als Redundanz statt als Komfort.
- *Gruppenreihenfolge* — steht als `group_order` in der Spec, getrennt von den
  Byte-Offsets. Die MIDI-Implementation ist nach Speicherlayout sortiert, nicht
  danach, wie man einen Klang baut.
- *Sektionsfarben* — eigene Zuordnung, kein Original. Siehe F3 in
  `../docs/06-risiken.md`: keine Original-Panelgrafik, damit eine spätere
  Veröffentlichung offenbleibt.

**Was der Prototyp beantworten soll**
- Trägt die Sektionsfarbe die Orientierung über 92 Parameter?
- Sind 40 px Faderbreite mit der Maus bedienbar?
- Funktioniert der Layer-Link (relative Änderung über mehrere Tones)?
- Ist der 4-Spalten-Tonevergleich das, was die Hardware nicht kann?

**Was er bewusst nicht tut:** kein MIDI, keine echten Patches. Die Werte sind
generierte Beispieldaten, die Adressen im SysEx-Inspektor eine Annahme.

Regenerieren nach Änderungen an der Spec:
```
python3 -c "import tomllib,json;d=tomllib.load(open('spec/params.toml','rb'));json.dump({'params':d['tone']['param'],'enums':{k:v['values'] for k,v in d['enums'].items()},'bases':d['meta']['tone_bases'],'areas':d['meta']['areas'],'model_id':d['meta']['model_id']},open('/tmp/spec.json','w'),ensure_ascii=False)"
```
Der Prototyp trägt die Spec aktuell einkompiliert — in M1 übernimmt das der Codegen.
