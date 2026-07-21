# claude-chats-browser

Qt 6 / C++ desktop app for browsing, searching and reading claude.ai
data-export dumps offline. The raw export is a single huge JSON file that is
unusable for humans; this app turns it into a fast, pleasant chat browser.

**Status: scaffold stage.** A minimal Qt Widgets app builds (main window only);
no features yet. The plan below is agreed — follow it unless Matej says
otherwise. Obey `STYLE.md` in all code.

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

Feature order (by value): see `README.md` § Planned Features — the canonical
list. Build them in that order.

## Tools

`tools/backfill_conversations.py`: usage and subcommands are documented in
`README.md` § Tools (canonical). Context the README doesn't carry: it was
originally written to backfill tombstones — moot, since they 404 (see
§ Export format above) — and the `check` subcommand exists to distinguish
"conversation deleted" from "endpoint wrong" when the API returns 404.
