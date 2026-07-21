Review all changed or specified files for STYLE.md compliance.

Check against these rules (from `STYLE.md` and `.clang-format`):

## C++ Formatting

- Allman braces for functions and namespaces, attached for classes/structs/enums
- 4-space indentation (no tabs)
- 170-column limit
- Left-aligned pointers (`int* ptr`, not `int *ptr`)
- No short forms on single lines (no single-line if/for/while/functions)
- No `auto` except for lambdas
- Explicit parentheses in compound conditions
- Brace member initialisation (`int m_count{0};`)
- Constructor initialiser lists break after the colon, one initialiser per line
- GPL v3 licence header on every `.cpp`/`.hpp` file

## C++ Naming

- Namespaces: `PascalCase` (e.g. `ChatsBrowser`)
- Types/Classes: `PascalCase`
- Functions/Methods/Slots/Signals: `camelCase`
- Constants: `SCREAMING_SNAKE_CASE` (`constexpr`)
- Variables: `snake_case`
- Member variables: `m_snake_case`
- Macros: `SCREAMING_SNAKE_CASE`
- Files: `snake_case.hpp` / `snake_case.cpp`

## YAML (GitHub Actions)

- 4-space structure indentation
- 2-space continuation from list items
- Blank lines between top-level keys and between jobs
- No inline comments

## JSON

- 4-space indentation

## Markdown

- ATX-style headings
- Dash lists (`-`), numeric ordered lists (`1.`)
- Fenced code blocks with language specified

## Scope Notes

- Never run clang-format on `.ui` files (Designer-generated XML)
- `CODE_OF_CONDUCT.md` and `LICENCE` are off limits

Report every violation with file path, line number, and what's wrong. If no files are specified via $ARGUMENTS, check all files modified since the last commit.
