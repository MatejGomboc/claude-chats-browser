Perform a thorough C++ code quality and bug-hunting audit on specified files or all source files in `src/`.

## What to Check

### Memory & Resource Safety

- Uninitialised variables
- Use-after-free or dangling pointers
- Missing `nullptr` checks before dereference
- Resource leaks (file handles, database connections, memory allocations)
- Missing cleanup in error paths
- Raw `new` without a Qt parent or smart pointer

### Qt-Specific

- `QObject` ownership: every heap `QObject` has a parent or a documented owner
- Function-pointer `connect()` only — flag string-based `SIGNAL()`/`SLOT()` macros
- Signals connected across threads without explicit connection type where it matters
- Blocking work (JSON parsing, database queries) on the UI thread — long operations belong in worker threads
- `QString`/encoding pitfalls: `toStdString()`/`fromStdString()` round trips, implicit `QByteArray` conversions
- Dangling `QStringView`/`std::string_view` referring to temporaries
- Missing `Q_OBJECT` macro on classes with signals/slots

### Database-Specific

- SQL built by string interpolation — must use bound parameters (`QSqlQuery::addBindValue`)
- Unchecked `QSqlQuery::exec()` / `QSqlDatabase::open()` return values
- Transactions: bulk imports must be wrapped in a transaction
- Schema-tolerance: importer code must never assume a JSON field exists

### Logic Errors

- Off-by-one errors in loops and array indexing
- Integer overflow/underflow
- Incorrect operator precedence
- Unreachable code or dead branches
- Missing `break` in switch cases
- Implicit narrowing conversions

### C++20 Best Practices

- Raw `new`/`delete` instead of smart pointers or RAII
- C-style casts instead of `static_cast`/`reinterpret_cast`
- Missing `const` where appropriate
- Missing `[[nodiscard]]` on getters/factories/query functions
- Using `NULL` instead of `nullptr`

### Cross-Platform (Windows + Linux + macOS)

- Hardcoded path separators — use Qt path APIs
- Case-sensitivity assumptions about the filesystem
- Platform-specific APIs without a portable fallback
- Assumptions about OS-specific behaviour (line endings, home directory layout)

### Privacy

- Conversation content or session keys reaching logs, error messages, or files outside `data/`
- Any network access that is not user-initiated

## Output Format

For each issue found, report:

1. **File and line number**
2. **Severity** (critical / warning / suggestion)
3. **Description** of the issue
4. **Suggested fix**

If no files are specified via $ARGUMENTS, audit all `.cpp` and `.hpp` files in `src/`. Summarise with a count of issues by severity at the end.
