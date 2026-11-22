# ADR-26 — New documentation in English

| | |
|---|---|
| **Status** | proposed |
| **Date** | 2026-11-22 |
| **Origin** | Repo review 2026-11-22; André: "let's write ADRs, specs and further work items as markdown, in English" |
| **Amends** | ADR-24 point 4 (artifacts in German), ADR-23 point 2 (docs stay German for now), `openspec/config.yaml` (`Language: German`) |

## Context

The project is public on GitHub (ADR-23). The README is English, but the design
documents, the 22 published ADRs and both OpenSpec changes are German. Contributors and viewers
of the planned video who don't read German can't follow the decisions, which is where
most of the reasoning lives.

## Decision

1. **New** ADRs, OpenSpec changes and backlog items are written in **English** from
   ADR-26 onwards.
2. Existing German documents stay valid as they are. They are **not** translated in one
   go. A document is translated when it is substantially revised anyway (tracked in
   `docs/backlog.md`).
3. `openspec/config.yaml` switches its `context` to `Language: English` once this ADR is
   accepted. The two open changes (`add-release-builds`, `add-plugin-mode`) may stay
   German until they are archived.
4. The user interface language is a separate question (ADR-23 "English UI as an
   option") and is not decided here.

Numbering note: ADR-25 is reserved by `add-plugin-mode` task 1.5 (port ownership in
plugin mode), so the review ADRs start at 26.

## Consequences

* For a while, docs are mixed-language. The ADR index marks each entry's language
  implicitly through its title.
* The README line "22 architecture decision records (in German)" and the language note
  need updating (backlog).
* When accepted: add a dated addendum ("Nachtrag") to ADR-24 and flip the OpenSpec config.
