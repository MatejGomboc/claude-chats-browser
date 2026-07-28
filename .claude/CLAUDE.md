# claude-chats-browser

Qt 6 / C++ desktop app for browsing, searching and reading claude.ai
data-export dumps offline. The raw export is a single huge JSON file that is
unusable for humans; this app turns it into a fast, pleasant chat browser.

**Status: usable.** Import (SQLite + FTS5), searchable sidebar, tabbed tree-aware
reader (branches, markdown, syntax-highlighted code, thinking/tool sections,
attachments, timestamps, copy buttons), in-conversation find (Ctrl+F), artifact
reconstruction + viewer, and cross-platform CI. The plan below is agreed — follow
it unless Matej says otherwise. Obey `STYLE.md` in all code.

## Off limits

- **`CODE_OF_CONDUCT.md`** — do not modify (adopted verbatim, Contributor Covenant 3.0).
- **`LICENCE`** — do not modify (legal document).

## Hard rules

- `data/` must NEVER be committed. It contains private conversations and this
  repo is public. Do not weaken the `data/` rule in `.gitignore` for any
  reason. Local exports live in `data/export-YYYY-MM-DD/`.
- On Matej's machines, git talks to GitHub via the SSH host alias
  `github-matejgomboc` (plain `github.com` deliberately has no key).

## Export format — what we learned (reference export: 2026-07)

- `conversations.json` is one JSON array of conversations (can be >100 MB).
  Each conversation: `uuid`, `name`, `summary`, `created_at`, `updated_at`,
  `account`, `chat_messages[]`.
- Messages link via `parent_message_uuid`, so a conversation is a **tree**,
  not a list — ~46% of real conversations have actual branches (edits/retries).
- Message content lives in typed `content[]` blocks: `text` (markdown, with
  `citations`), `thinking` (with `summaries`, truncation flags, `signature`),
  `tool_use`/`tool_result` (including `artifacts` calls — artifacts can be
  reconstructed by replaying create/update/`str_replace` inputs), plus rare
  types (`token_budget`, `flag`). Old messages (2024) use only the top-level
  `text` field with empty `content` — the schema evolved over time.
- **Importer must be schema-tolerant**: every field optional, unknown block
  types preserved raw, never crash on new shapes.
- **Tombstones**: deleted conversations appear in exports as nameless
  skeletons — `name: ""`, every message with empty `text`/`content`, but real
  timestamps and message counts. Verified against the live API: these 404,
  i.e. content is gone server-side, not an export bug. In the reference
  export 1,283 of 1,769 conversations are tombstones; 486 are real (~14.5k
  messages). The app should render tombstones greyed-out (date + message
  count), not hide them.
- Attachments carry `extracted_content` inline; `files` only reference UUIDs
  (binaries are not in the export). Side files: `projects/*.json`,
  `memories.json`, `users.json`, `reflections/*.json` — small, worth showing.
- The live API returns extra per-conversation fields the offline export
  lacks: `model`, `is_starred`, `is_temporary`, `platform`, `settings`.

## Architecture plan

1. **Import, don't parse-on-launch**: first run streams `conversations.json`
   into SQLite with an FTS5 full-text index (Qt's bundled QSQLITE driver has
   FTS5 compiled in). Instant startup afterwards.
2. **Merge-by-UUID incremental imports**: every export (and any API fetch) is
   a snapshot; importing several merges by conversation UUID, preferring the
   copy that has content. No import is ever destructive.
3. **UI**: Qt Widgets (decided 2026-07-21, supersedes the earlier QML idea) —
   `QMainWindow` shell with Designer `.ui` forms, `QAbstractItemModel`
   subclasses over the SQLite store feeding passive views. Markdown via
   `QTextDocument::setMarkdown`; code blocks get a simple highlighter
   (KSyntaxHighlighting as an optional later upgrade).
4. **Build**: CMake + Ninja + presets (`CMakePresets.json`), Qt 6.5+
   (dev machine: 6.11.1 msvc2022_64; set `QTDIR` to the kit directory).
5. **Cross-platform**: Windows + Linux + macOS are all first-class targets.
   Keep code and CMake portable — no platform-specific APIs without a
   portable fallback, no hardcoded path separators, and assume
   case-sensitive filesystems. Primary dev machine is Windows; CI should
   eventually build all three.

All five originally-planned features are shipped — `README.md` § Features is
the canonical delivered list. New feature ideas go through issues/discussions.

## CI/CD Notes

- Workflows follow the family convention shared with Matej's other repos
  (git-proxy-mcp, altium-designer-mcp, tron_grid): `ci_pr` (PRs: caches are
  restore-only), `ci_main` (pushes to main: caches save + stale-cache
  cleanup), `release` (version tags), `cleanup_caches` (manual, dry-run
  option). All third-party actions are pinned to full commit SHAs.
- CI drives builds through **CMake workflow presets**
  (`cmake --workflow --preset <name>`), so CI and local runs are identical.
- Qt is installed by the custom composite action
  `.github/actions/setup-qt/action.yml` (aqtinstall). **Dependabot** only
  updates action references in `.github/workflows/*.yml`; the composite
  action also uses `actions/cache` but is **not** covered. When a Dependabot
  PR bumps `actions/cache`, manually update the hash and version comment in
  the composite action to match.
- CI Qt matches the dev machine (6.11.x); CMakeLists requires only 6.5+.
  Do not downgrade CI below 6.9: older Qt CMake exports reference the
  AGL framework, which modern macOS SDKs no longer ship (link failure).
- Release artifacts: Windows zip + QtIFW setup.exe (bundled MSVC runtime,
  installed silently), Linux AppImage (built on ubuntu-22.04 for an older
  glibc baseline; linuxdeploy pinned by artifact SHA256 — the upstream only
  has a "continuous" tag), macOS universal (x86_64+arm64) ad-hoc-signed
  .app in a DMG. `release.yml` also has a workflow_dispatch **dry-run
  mode**: builds and uploads all artifacts, publishes nothing.
- GUI tests run under `xvfb-run` on Linux with `fonts-dejavu-core` installed
  (the offscreen QPA ships no fonts, which makes text layout pathologically
  slow — see the comment in CMakeLists.txt).

## Tools

`tools/backfill_conversations.py`: usage and subcommands are documented in
`README.md` § Tools (canonical). Context the README doesn't carry: it was
originally written to backfill tombstones — moot, since they 404 (see
§ Export format above) — and the `check` subcommand exists to distinguish
"conversation deleted" from "endpoint wrong" when the API returns 404.
