# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Export conversation** (File menu): save the current conversation as shareable
  Markdown — the displayed branch path with prose, code and attachment names
  (Ctrl+E) — or as lossless JSON carrying every branch and the original message
  objects, re-importable by the app.

## [0.1.0] - 2026-07-28

First release. Turns the single unusable JSON blob of a claude.ai data export into a
fast, pleasant, fully searchable offline chat browser.

### Added

- **Import**: one-time streaming import of `conversations.json` into SQLite with an
  FTS5 full-text index — instant startup ever after. Imports merge by conversation
  UUID, preferring the copy with content, so repeated exports are incremental and
  never destructive. Schema-tolerant: every field optional, unknown block types
  preserved.
- **Conversation browser**: sidebar with search-as-you-type full-text search
  (runs off the UI thread), conversation icons, and greyed-out entries for deleted
  conversations that appear in exports as content-less tombstones.
- **Reader**: tabbed, tree-aware conversation view — markdown rendering,
  syntax-highlighted code blocks, collapsible thinking sections (markdown streamed
  in chunks so huge blocks never freeze the UI), tool calls and results as
  expandable sections, pasted-text attachments, per-message timestamps, and copy
  buttons for messages and code blocks. Large conversations render in chunks with
  progress reporting.
- **Branch navigation**: conversations are trees — edited prompts and retried
  replies become navigable sibling branches with claude.ai-style controls.
- **Find in conversation** (Ctrl+F): match highlighting with previous/next
  navigation; Ctrl+Shift+F jumps to the across-conversations search.
- **Artifacts**: reconstructs every artifact from its create/update/rewrite
  tool-call history, with rendered previews (markdown, HTML, SVG), highlighted
  source view, copy, and export to disk.
- **Statistics** (View → Statistics): headline archive numbers, a
  messages-per-month activity chart, and ranked tool usage.
- **Packaging**: Windows installer (bundles and installs the MSVC runtime) and
  portable zip; self-contained Linux AppImage; macOS universal (Intel + Apple
  Silicon) DMG. Checksums and build provenance attestations published with every
  release.

### Security

- Fully offline: nothing ever leaves the machine unless explicitly requested.
  See SECURITY.md for the complete privacy model.
- Every artifact below carries a SHA-256 checksum (`SHA256SUMS.txt`) and a signed
  build provenance attestation. Verify authenticity with:
  `gh attestation verify <file> --owner MatejGomboc`
  — see README § Verifying Downloads.

[unreleased]: https://github.com/MatejGomboc/claude-chats-browser/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/MatejGomboc/claude-chats-browser/releases/tag/v0.1.0
