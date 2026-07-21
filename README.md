# claude-chats-browser

**Your chats. Your machine. Actually readable.**

A cross-platform desktop app (Qt 6 / C++20) for browsing, searching, and reading
[claude.ai](https://claude.ai) data exports offline. The official export is a single
giant JSON file that no human can read; this app turns years of conversations into
a fast, pleasant, fully searchable chat browser.

---

## The Problem

claude.ai lets you export your data, but what you get is barely usable:

| What you get | Why it hurts |
|--------------|--------------|
| One `conversations.json`, potentially 100+ MB | No viewer opens it comfortably, let alone pleasantly |
| Messages as raw content blocks (text, thinking, tool calls) | Unreadable without reconstructing the structure |
| Conversations are trees (edit/retry branches), stored flat | Branches are invisible in the raw JSON |
| Deleted chats appear as nameless, empty skeletons | Noise you can't distinguish from data loss |
| No search across the export | Two years of conversations, zero findability |

## The Solution

claude-chats-browser imports the export **once** into a local SQLite database with an
FTS5 full-text index, then gives you a native desktop UI over it:

```text
┌──────────────────┐      ┌──────────────────┐      ┌──────────────────┐
│  claude.ai       │      │  Importer        │      │  Qt Quick UI     │
│  export zip      │─────►│                  │─────►│                  │
│                  │      │  SQLite + FTS5   │      │  browse, search, │
│  (huge JSON)     │      │  (local file)    │      │  read, export    │
└──────────────────┘      └──────────────────┘      └──────────────────┘
```

- **Instant startup** — parse the 100+ MB JSON once, never again
- **Full-text search** across every conversation you ever had
- **Merge-by-UUID imports** — feed it multiple exports over time; it keeps the union, never destroys anything
- **Honest history** — deleted-chat tombstones are shown greyed-out (date + message count), not hidden

## Status

**Pre-scaffold.** The data layer is fully analysed (export schema, content block types,
branching, tombstones — see [.claude/CLAUDE.md](.claude/CLAUDE.md)) and the fetch tooling
works. The application itself is in design; this README describes the agreed plan.

## Planned Features

1. **Conversation browser** — sidebar with search-as-you-type (FTS5), date filters, project grouping
2. **Reader** — markdown rendering, collapsible thinking blocks, tool calls as expandable chips, inline attachments
3. **Branch navigation** — conversations are trees; edit/retry branches become navigable
4. **Artifact extraction** — reconstruct artifacts from tool-call history, preview and export to disk
5. **Stats dashboard** — activity over time, model and tool usage

---

## Privacy

This app processes your private conversations, so the rules are strict
(see [SECURITY.md](SECURITY.md) § Privacy Model):

- **Offline-first:** browsing, search, and reading never touch the network
- **No telemetry:** nothing is collected, nothing is transmitted
- **User-initiated network access only:** the optional backfill tool fetches your own
  conversations from claude.ai with your own session cookie — entered interactively,
  never written to disk
- **Your data stays yours:** exports and the SQLite database live in the gitignored
  `data/` directory and never leave your machine

---

## Platforms

| Platform | Status |
|----------|--------|
| Windows  | Planned (primary development platform) |
| Linux    | Planned |
| macOS    | Planned |

All three are first-class targets.

## Requirements

- **C++20** compiler (MSVC 19.30+, GCC 12+, or Clang 15+)
- **CMake** 3.24+
- **Ninja** build system
- **Qt 6.x** (Quick and Sql modules)
- **Python** 3.10+ (for `tools/` scripts only)

## Building

Build presets will be defined when the application scaffold lands; this section will
then become the canonical source for build commands.

## Getting Your Export

1. On [claude.ai](https://claude.ai): **Settings → Privacy → Export data**
2. You'll receive a download link by email
3. Extract the zip into `data/` (gitignored), e.g. `data/export-2026-07-20/`

## Tools

### `tools/backfill_conversations.py`

Fetches your conversations directly from the claude.ai backend API using your own
browser session — useful for grabbing conversations newer than your last export.
Requires `curl_cffi` (`pip install curl_cffi`; plain HTTP clients are blocked by
Cloudflare TLS fingerprinting).

```bash
python tools/backfill_conversations.py list  --export data/export-2026-07-20   # count content-less conversations
python tools/backfill_conversations.py test  --export data/export-2026-07-20   # check your session key works
python tools/backfill_conversations.py fetch --export data/export-2026-07-20   # download (resumable, rate-limited)
python tools/backfill_conversations.py merge --export data/export-2026-07-20   # write conversations-merged.json
```

The session key is read from a hidden interactive prompt (or `CLAUDE_SESSION_KEY`) and
is never stored.

---

## Design Principles

1. **Import once, browse forever** — no repeated parsing of giant JSON
2. **Never destroy data** — imports merge by UUID; the copy with content always wins
3. **Schema-tolerant** — the export format evolved over years; every field is optional, unknown block types are preserved raw
4. **Offline-first** — the network is opt-in, per action, never ambient
5. **Honest history** — gaps (deleted chats) are shown, not papered over
6. **Don't over-engineer** — no abstractions until there's a concrete second use case

## Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Storage | SQLite + FTS5 | Instant startup, real full-text search, single-file database |
| SQLite driver | Qt's bundled QSQLITE | FTS5 compiled in, no extra dependency |
| UI | Qt Quick (QML) | Fluid chat UI, cross-platform |
| Data models | C++ `QAbstractListModel` | Logic in C++, QML stays declarative |
| Markdown | Qt's native CommonMark support | No extra dependency |
| Import strategy | Merge-by-UUID snapshots | Multiple exports form a union over time |
| Licence | GPL v3 | Consistent with my other projects |

## Documentation

| Document | Purpose |
|----------|---------|
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contributor guidelines |
| [STYLE.md](STYLE.md) | Code style conventions (C++, QML, SQL, Python) |
| [SECURITY.md](SECURITY.md) | Privacy model, security policy, vulnerability reporting |
| [CHANGELOG.md](CHANGELOG.md) | Change history (reserved until first stable release) |
| [.claude/CLAUDE.md](.claude/CLAUDE.md) | Export-format findings and project plan |

---

## Contributing

Contributions welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

- Follow the style guide in [STYLE.md](STYLE.md)
- Security issues: see [SECURITY.md](SECURITY.md)

---

## Licence

Copyright (C) 2026 Matej Gomboc <https://github.com/MatejGomboc/claude-chats-browser>.

GNU General Public License v3.0 — see [LICENCE](LICENCE).

---

## Links

- [Qt 6 Documentation](https://doc.qt.io/qt-6/)
- [SQLite FTS5](https://www.sqlite.org/fts5.html)
- [Keep a Changelog](https://keepachangelog.com/)
- [Report an Issue](https://github.com/MatejGomboc/claude-chats-browser/issues)
