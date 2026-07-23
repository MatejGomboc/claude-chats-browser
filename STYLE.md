# Style Guide

Code style conventions for claude-chats-browser.

---

## General Rules

| Rule | Setting |
|------|---------|
| Indentation | 4 spaces (no tabs) |
| Max line length | 170 characters |
| Charset | UTF-8 |
| Final newline | Always |
| Trailing whitespace | Trim (except Markdown) |

These rules are enforced by `.editorconfig`. Install the EditorConfig plugin for your editor:

- **VS Code:** [EditorConfig for VS Code](https://marketplace.visualstudio.com/items?itemName=EditorConfig.EditorConfig)

---

## Single Source of Truth

Avoid duplicating information across files. Each piece of information should have one canonical location.

| Information | Canonical Source |
|-------------|------------------|
| Build commands | `README.md` § Building (once the scaffold lands) |
| Toolchain prerequisites | `README.md` § Requirements |
| Planned features (and build order) | `README.md` § Planned Features |
| Backfill tool usage | `README.md` § Tools |
| Coding standards & naming | `STYLE.md` (this file) |
| Commit conventions | `CONTRIBUTING.md` § Commit Messages |
| British spelling 🇬🇧 | `CONTRIBUTING.md` § British Spelling |
| PR requirements | `CONTRIBUTING.md` § Pull Requests |
| Contributor privacy rules | `CONTRIBUTING.md` § Privacy Considerations |
| Privacy model & security policy | `SECURITY.md` § Privacy Model |
| Formatting rules | `.editorconfig`, `.clang-format` |
| Export-format findings & architecture plan | `.claude/CLAUDE.md` |

**Guidelines:**

- Reference the canonical source instead of duplicating content
- If information must appear in multiple places (e.g., PR template checklists), keep it minimal
- When updating information, update the canonical source first
- Cross-reference using `filename` § Section Name format

**Deliberate exception:** the *"`data/` must never be committed"* rule is repeated in
`.gitignore`, `.claude/CLAUDE.md`, `CONTRIBUTING.md`, and the PR template. This is
intentional defence in depth for a safety-critical rule, not an SSOT violation — keep
every copy.

---

## C++

### Formatting

Use `.clang-format` (LLVM-based). All code should be formatted before committing.
Run `clang-format -i` on `.cpp` and `.hpp` files only — never on `.ui` files (Designer-generated XML).

Key settings:

| Setting | Value |
|---------|-------|
| Brace style | Allman for functions/namespaces, attached for classes/structs/enums |
| Indent | 4 spaces |
| Column limit | 170 |
| Pointer alignment | Left (`int* ptr`) |
| Braces on bodies | Always — even single-line `if`/`else`/`for`/`while` (`InsertBraces: true`) |

### Naming Conventions

| Item | Convention | Example |
|------|------------|---------|
| Namespaces | PascalCase | `ChatsBrowser` |
| Types / Classes / Structs | PascalCase | `ConversationModel` |
| Functions / Methods / Slots / Signals | camelCase | `loadExport`, `messageCountChanged` |
| Constants | SCREAMING_SNAKE_CASE | `MAX_SEARCH_RESULTS` |
| Variables | snake_case | `conversation_uuid` |
| Member variables | m_snake_case | `m_database_path` |
| Macros | SCREAMING_SNAKE_CASE | `APP_VERSION_STRING` |

### Language Standard

C++20 (and **NOT** beyond it!). Do not throw exceptions in project code. Catch exceptions from third-party libraries
at API boundaries only. For unrecoverable errors in project code, log via `qFatal()` (which aborts).

### Type Explicitness

Do not use `auto` — write the explicit type so the reader never has to guess. The only exception
is where the type is impossible to spell (lambdas).

```cpp
// Correct
QSqlQuery query = QSqlQuery(m_database);
uint32_t count = static_cast<uint32_t>(messages.size());

// Wrong
auto query = QSqlQuery(m_database);
auto count = static_cast<uint32_t>(messages.size());

// Exception — lambdas have unspellable types
auto on_finished = [this](int row) { ... };
```

### Attributes

Use `[[nodiscard]]` on all functions that return a value the caller must not silently discard —
getters, factory functions, query functions.

```cpp
[[nodiscard]] const QString& databasePath() const;
[[nodiscard]] int messageCount() const;
```

### Operator Precedence

Use explicit parentheses when combining arithmetic, bitwise, or increment/decrement operators
with comparison or logical operators. Do not rely on the reader knowing precedence rules:

```cpp
// Correct — each sub-expression is explicit
if ((row < 0) || (row >= m_conversations.size())) { ... }
if ((flags & IS_TOMBSTONE) != 0) { ... }

// Wrong — relies on implicit precedence
if (row < 0 || row >= m_conversations.size()) { ... }
if (flags & IS_TOMBSTONE != 0) { ... }
```

Single boolean variables do not need extra parentheses — the intent is already obvious:

```cpp
// Fine — single booleans
if (is_tombstone || is_empty) { ... }
```

### Constants

Use `constexpr` for compile-time constants. Name them `SCREAMING_SNAKE_CASE`. Do not use
plain `const` or magic numbers where `constexpr` applies.

```cpp
constexpr int SEARCH_DEBOUNCE_MS = 150;
constexpr int IMPORT_BATCH_SIZE = 500;
```

### Modern C++20 Idioms

Prefer `std::ranges::` algorithms over `std::` + `.begin()/.end()`:

```cpp
// Correct
std::ranges::find_if(conversations, predicate);

// Wrong
std::find_if(conversations.begin(), conversations.end(), predicate);
```

Prefer `QStringView` for read-only Qt string parameters and `std::string_view` for read-only
`std::string` parameters — avoids unnecessary allocations.

### Include Order

All `#include` directives are flush — no blank lines between groups. Order:

1. Same-module headers (`"conversation_model.hpp"`)
2. Project headers (`"database.hpp"`)
3. Qt headers (`<QSqlQuery>`, `<QAbstractListModel>`)
4. Standard library headers (`<vector>`, `<string>`)

```cpp
#include "conversation_model.hpp"
#include "database.hpp"
#include <QAbstractListModel>
#include <QSqlQuery>
#include <cstdint>
#include <vector>
```

### Comment Alignment

Do not column-align trailing comments. Use a single space before `//` or `//!<`:

```cpp
// Correct
QSqlDatabase m_database; //!< SQLite connection with FTS5 index.
QString m_export_dir; //!< Root of the extracted export.

// Wrong — padded to align
QSqlDatabase m_database;    //!< SQLite connection with FTS5 index.
QString m_export_dir;       //!< Root of the extracted export.
```

The same applies to enum values — no extra spaces between the value and its comment.

### Constructor Initialiser Lists

Always break after the colon. Each initialiser gets its own line with 4-space indentation.
A single initialiser is one line; multiple initialisers are one per line:

```cpp
ConversationModel::ConversationModel(Database& database, QObject* parent) :
    QAbstractListModel(parent),
    m_database(&database)
{
}
```

### Member Initialisation

Use brace initialisation `{}` for all member default values — not `= value` assignment:

```cpp
// Correct
int m_row_count{0};
bool m_loaded{false};
Database* m_database{nullptr};

// Wrong
int m_row_count = 0;
bool m_loaded = false;
```

### Qt Conventions

- Use function-pointer `connect()` syntax — never the string-based `SIGNAL()`/`SLOT()` macros:

```cpp
// Correct
connect(&m_importer, &Importer::progressChanged, this, &MainWindow::onImportProgress);

// Wrong
connect(&m_importer, SIGNAL(progressChanged(int)), this, SLOT(onImportProgress(int)));
```

- Ownership: use Qt parent-child ownership for `QObject`s; `std::unique_ptr` for non-`QObject`
  heap objects. No raw `new` without a parent.
- Expose data to views via `QAbstractItemModel` subclasses — logic lives in models,
  views and widgets stay passive.
- Long-running work (JSON parsing, database queries) never blocks the UI thread.

### Doxygen Comments

All doxygen comments must be proper sentences — capital letter start, period end.

| Style | Use | Example |
|-------|-----|---------|
| `//!` | Single-line brief | `//! Returns the number of imported conversations.` |
| `//!<` | Inline member | `int m_row_count; //!< Cached row count for the model.` |
| `/*! */` | Multi-line block | See below |

Multi-line doxygen blocks use 4-space indented content. Use Qt-style backslash commands
(`\param`, `\return`, `\brief`) — not Javadoc `@` prefix:

```cpp
/*!
    Imports a claude.ai export directory into the SQLite store.

    \param export_dir Directory containing conversations.json.
    \return Number of conversations imported or updated.
*/
```

Licence headers use plain `/* */` (not doxygen) with the full GPL v3 notice:

```cpp
/*
    Copyright (C) 2026 Matej Gomboc https://github.com/MatejGomboc/claude-chats-browser

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
*/
```

---

## Qt Designer Forms (`.ui`)

- Edit `.ui` files through Qt Designer (or Qt Creator's Design mode) — do not hand-edit the XML
  beyond trivial fixes, and never run formatters on them
- `objectName` values are lowerCamelCase (`conversationSidebar`, `menuBar`)
- Keep forms structural: layouts, widgets, actions. All behaviour is wired in C++
- One form per top-level window/dialog class

---

## SQL

- SQLite dialect; schema lives in one canonical location (the importer)
- Keywords UPPERCASE, identifiers snake_case:

```sql
CREATE TABLE conversations (
    uuid TEXT PRIMARY KEY,
    name TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL
);

SELECT uuid, name FROM conversations WHERE is_tombstone = 0 ORDER BY updated_at DESC;
```

- Always use bound parameters — never string-interpolate values into SQL

---

## Python

### Scope

Python is used for helper scripts only (`tools/`).

### Formatting

**4 spaces** indentation. Max line length **170 characters** (project-wide convention).

### Conventions

| Rule | Setting |
|------|---------|
| File encoding | UTF-8 (explicit `encoding="utf-8"` on all `open()` calls) |
| String quotes | Double quotes |
| Imports | Standard library preferred; third-party only when unavoidable (e.g. `curl_cffi` for Cloudflare TLS impersonation) |
| Docstrings | Required for all modules and non-trivial functions |
| Spelling 🇬🇧 | British English in comments and strings |

### Validation

```bash
python -m py_compile tools/backfill_conversations.py  # Syntax check
```

---

## YAML (GitHub Actions)

### Indentation

**4 spaces** for structure levels — aligned with project-wide convention.

```yaml
jobs:
    build:
        name: Build
        runs-on: ubuntu-latest

        steps:
            - name: Checkout
              uses: actions/checkout@v6

            - name: Build
              run: cmake --workflow --preset=linux-gcc
```

### List Item Indentation

List items use **2-space continuation** from the `-` character (standard YAML behaviour):

```yaml
updates:
    - package-ecosystem: "github-actions"
      directory: "/"
      schedule:
        interval: "weekly"
```

### Multi-line Scripts (`run: |`)

Shell script content inside `run: |` blocks uses **4-space indentation** for shell constructs (if/else, loops):

```yaml
            - name: Example step
              shell: bash
              run: |
                if [[ -n "$VAR" ]]; then
                    echo "Variable is set"
                else
                    echo "Variable is not set"
                fi
```

### Structure

- Blank line between top-level keys (`on`, `env`, `jobs`)
- Blank line between jobs
- Blank line before `steps:` in complex jobs
- Comments on their own line, not inline

---

## JSON

### Indentation

**4 spaces**.

```json
{
    "key": "value",
    "nested": {
        "item": 123
    }
}
```

---

## Markdown

### Headings

Use ATX-style headings with blank lines before and after:

```markdown
## Section Title

Content here.
```

### Lists

Use `-` for unordered lists, `1.` for ordered lists.

### Code Blocks

Always specify the language:

````markdown
```cpp
int main()
{
    return 0;
}
```
````

### Trailing Whitespace

Markdown files are exempt from trailing whitespace trimming (needed for line breaks).

---

## British Spelling 🇬🇧

See `CONTRIBUTING.md` § British Spelling for the full reference table.

**Quick rule:** Use British spelling in documentation, comments, and user-facing strings
(colour, behaviour, initialise). Code identifiers may use American spelling where it matches
library/API conventions (e.g. Qt's `color` properties, SQLite keywords).

---

## Code Quality Tooling

### Compiler Warnings

Warnings are errors on all compilers — zero-warning policy:

- **MSVC:** `/W4 /WX`
- **GCC/Clang:** `-Wall -Wextra -Wpedantic -Werror`

### Static Analysis (Clang-Tidy)

Configuration in `.clang-tidy`. Runs via `clangd` in VS Code. Bugprone, analyser, and
concurrency checks are promoted to errors. To suppress a specific check on a line:

```cpp
int x = legacy_function(); // NOLINT(bugprone-unused-return-value)
```

### Tests

Unit tests use the Qt Test framework, live in `tests/`, and run via CTest
(`ctest --test-dir build/<preset>`). Pure logic (e.g. the conversation reply
tree) is factored out of the widgets so it can be tested without a GUI.

### Sanitisers

Planned: CMake sanitiser presets (AddressSanitizer/UndefinedBehaviorSanitizer on Linux).
This section will be expanded when they land.

---

## Commit Messages

See `CONTRIBUTING.md` § Commit Messages for conventions and allowed types.

---

*Last updated: 2026-07-21*
