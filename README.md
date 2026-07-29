# claude-chats-browser

[![CI - Main Branch](https://github.com/MatejGomboc/claude-chats-browser/actions/workflows/ci_main.yml/badge.svg)](https://github.com/MatejGomboc/claude-chats-browser/actions/workflows/ci_main.yml)

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
│  claude.ai       │      │  Importer        │      │  Qt Widgets UI   │
│  export zip      │─────►│                  │─────►│                  │
│                  │      │  SQLite + FTS5   │      │  browse, search, │
│  (huge JSON)     │      │  (local file)    │      │  read, export    │
└──────────────────┘      └──────────────────┘      └──────────────────┘
```

- **Instant startup** — parse the 100+ MB JSON once, never again
- **Full-text search** across every conversation you ever had
- **Merge-by-UUID imports** — feed it multiple exports over time; it keeps the union, never destroys anything
- **Honest history** — deleted-chat tombstones are shown greyed-out (date + message count), not hidden

## Installing

Grab the latest build for your platform from
[Releases](https://github.com/MatejGomboc/claude-chats-browser/releases):

| Platform | Artifact | First-run note |
|----------|----------|----------------|
| Windows | `…-setup.exe` installer (or portable `.zip`) | SmartScreen shows "Windows protected your PC" for unsigned apps — click **More info → Run anyway** |
| Linux | `.AppImage` | `chmod +x` the file, then run it — no installation needed |
| macOS | `.dmg` (universal: Intel + Apple Silicon) | Drag to Applications; first launch needs **right-click → Open** (unnotarised app) |

All artifacts are checksummed and carry signed build-provenance attestations —
see [Verifying Downloads](#verifying-downloads).

## Features

- **Conversation browser** — sidebar with search-as-you-type full-text search across
  every conversation; deleted-conversation tombstones shown greyed-out, not hidden
- **Reader** — markdown rendering, syntax-highlighted code blocks, collapsible
  thinking sections, tool calls and results as expandable sections, inline pasted-text
  attachments, per-message timestamps, copy buttons for messages and code
- **Find in conversation** — Ctrl+F with match highlighting and previous/next
  navigation (Ctrl+Shift+F for the across-conversations search)
- **Branch navigation** — conversations are trees; edited prompts and retried replies
  become navigable sibling branches
- **Artifacts** — every artifact reconstructed from its tool-call history, with
  rendered previews (markdown, HTML, SVG), highlighted source, copy, and export to disk
- **Statistics** — headline archive numbers, a messages-per-month activity chart,
  and ranked tool usage
- **Export** — save any conversation as shareable Markdown (exactly the branch
  path you are reading) or as lossless, re-importable JSON

---

## Privacy

This app processes your private conversations. In one sentence: **nothing ever leaves
your machine unless you explicitly ask it to.** The full privacy model — offline-first,
no telemetry, user-initiated network access only, all data confined to the gitignored
`data/` directory — is defined in [SECURITY.md](SECURITY.md) § Privacy Model, the
canonical source.

---

## Platforms

| Platform | Status |
|----------|--------|
| Windows  | Supported — CI-built and tested; installer + zip (primary development platform) |
| Linux    | Supported — CI-built and tested; AppImage |
| macOS    | Supported — CI-built and tested; universal (Intel + Apple Silicon) DMG |

All three are first-class targets, built and tested on every pull request.

## Requirements

- **C++20** compiler (MSVC 19.30+, GCC 12+, or Clang 15+)
- **CMake** 3.25+ (the presets file requires it)
- **Ninja** build system
- **Qt 6.5+** (Widgets and Sql modules)
- **Python** 3.10+ (for `tools/` scripts only)

## Building

Set the `QTDIR` environment variable to your Qt kit directory
(e.g. `C:\Qt\6.11.1\msvc2022_64` or `~/Qt/6.11.1/gcc_64`), then:

```bash
# Windows (from a VS developer prompt, or via VS Code CMake Tools)
cmake --preset windows-msvc-debug
cmake --build build/windows-msvc-debug

# Linux
cmake --preset linux-gcc-debug
cmake --build build/linux-gcc-debug

# macOS
cmake --preset macos-clang-debug
cmake --build build/macos-clang-debug
```

Each preset has a `-release` twin. `cmake --list-presets` shows what's available on your platform.

### Tests

Unit tests (Qt Test) run via CTest:

```bash
ctest --test-dir build/windows-msvc-debug --output-on-failure
```

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

## Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Storage | SQLite + FTS5 | Instant startup, real full-text search, single-file database |
| SQLite driver | Qt's bundled QSQLITE | FTS5 compiled in, no extra dependency |
| UI | Qt Widgets | Native desktop feel, mature model/view classes, straightforward debugging |
| Data models | C++ `QAbstractItemModel` subclasses | Logic in models, views stay passive |
| Markdown | Qt's native CommonMark support (`QTextDocument`) | No extra dependency |
| Import strategy | Merge-by-UUID snapshots | Multiple exports form a union over time; nothing is ever destroyed |
| Schema handling | Tolerant parsing | The export format evolved over years — every field optional, unknown block types preserved raw |
| Abstractions | None until a concrete second use case | Don't over-engineer |
| Licence | GPL v3 | Consistent with my other projects |

## Documentation

| Document | Purpose |
|----------|---------|
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contributor guidelines |
| [STYLE.md](STYLE.md) | Code style conventions (C++, SQL, Python) |
| [SECURITY.md](SECURITY.md) | Privacy model, security policy, vulnerability reporting |
| [CHANGELOG.md](CHANGELOG.md) | Change history (Keep a Changelog format; feeds the release notes) |
| [.claude/CLAUDE.md](.claude/CLAUDE.md) | Export-format findings and project plan |

---

## Contributing

Contributions welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

- Follow the style guide in [STYLE.md](STYLE.md)
- Security issues: see [SECURITY.md](SECURITY.md)

---

## Verifying Downloads

Every release artifact ships with a SHA-256 checksum and a signed
[build provenance attestation](https://docs.github.com/en/actions/security-for-github-actions/using-artifact-attestations)
proving it was built by this repository's release workflow from the tagged commit —
not modified or built elsewhere.

```bash
# Integrity: compare against SHA256SUMS.txt from the release
sha256sum --check --ignore-missing SHA256SUMS.txt

# Authenticity: verify the signed provenance (requires the GitHub CLI)
gh attestation verify claude-chats-browser-windows-x86_64-setup.exe --owner MatejGomboc
```

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
