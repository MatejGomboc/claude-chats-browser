# Contributing to claude-chats-browser

Thank you for your interest in contributing to claude-chats-browser! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Privacy Considerations](#privacy-considerations)
- [How to Contribute](#how-to-contribute)
    - [Reporting Bugs](#reporting-bugs)
    - [Suggesting Features](#suggesting-features)
    - [Pull Requests](#pull-requests)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Commit Messages](#commit-messages)
- [Documentation](#documentation)

---

## Code of Conduct

This project adheres to the Contributor Covenant Code of Conduct.
By participating, you are expected to uphold this code. Please see [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for details.

---

## Privacy Considerations

**This app processes private conversation data.** claude.ai exports contain personal chats, and the backfill tool
handles live session cookies.

### For All Contributors

- **NEVER** commit anything under `data/` — it is gitignored for a reason; do not weaken that rule
- **NEVER** include real conversation content in issues, PRs, tests, or screenshots — construct synthetic fixtures instead
- **NEVER** include session keys, cookies, or tokens in code, comments, logs, or reports
- **NEVER** paste log output without first redacting any sensitive data
- When in doubt, redact first and ask

### Reporting Security Vulnerabilities

**Do NOT report security vulnerabilities through public GitHub issues.**

Please see [SECURITY.md](SECURITY.md) for the privacy model and instructions on how to report vulnerabilities privately.

---

## How to Contribute

### Reporting Bugs

Before submitting a bug report:

1. Check the [existing issues](https://github.com/MatejGomboc/claude-chats-browser/issues) to avoid duplicates
2. Ensure you're using the latest version
3. Collect relevant information:
    - Operating system and version
    - claude-chats-browser version
    - Qt version
    - Compiler and CMake preset used (if built from source)
    - Steps to reproduce
    - Expected vs actual behaviour

When submitting:

- Use the bug report template
- Provide a clear, descriptive title
- Include minimal reproduction steps
- **Redact any conversation content, session keys, or sensitive data from logs**

### Suggesting Features

We welcome feature suggestions! Before submitting:

1. Check [existing issues](https://github.com/MatejGomboc/claude-chats-browser/issues) and
   [discussions](https://github.com/MatejGomboc/claude-chats-browser/discussions) for similar ideas
2. Consider how the feature fits the project's offline-first privacy model (see `SECURITY.md` § Privacy Model)
3. Think about backwards compatibility with older export formats

When submitting:

- Use the feature request template
- Explain the problem you're trying to solve
- Describe your proposed solution
- Consider alternatives you've thought about

### Pull Requests

#### Before You Start

1. Open an issue first to discuss significant changes
2. Fork the repository
3. Create a feature branch from `main`
4. Make your changes following our [coding standards](#coding-standards)

#### PR Requirements

- [ ] Code compiles without warnings on all relevant presets
- [ ] Code is formatted (`.clang-format`)
- [ ] British spelling in documentation and comments 🇬🇧
- [ ] Documentation is updated if needed
- [ ] Commit messages follow [conventional commits](#commit-messages)
- [ ] **No real conversation data, session keys, or secrets anywhere in the PR**

#### PR Process

1. Submit your PR against the `main` branch
2. Fill out the PR template completely
3. Wait for CI to pass
4. Address any review feedback
5. Once approved, a maintainer will merge

---

## Development Setup

### Prerequisites

- C++20 compiler (MSVC 19.30+, GCC 12+, or Clang 15+)
- CMake 3.24+
- Ninja build system
- Qt 6.x (Quick, Sql modules)
- Python 3.10+ (for `tools/` scripts only)

### Setup

```bash
git clone https://github.com/MatejGomboc/claude-chats-browser.git
cd claude-chats-browser
```

Build presets will be defined when the application scaffold lands; from then on,
`README.md` § Building is the canonical source for build commands.

To try the app you need a claude.ai data export (claude.ai → Settings → Privacy → Export data).
Extract it into `data/` (gitignored).

---

## Coding Standards

### C++ / QML Style

- Follow `.clang-format` formatting (run clang-format before committing)
- Use C++20 features where appropriate
- Allman braces for functions and namespaces, attached for classes/structs/enums
- 4-space indentation, 170-column limit
- See [STYLE.md](STYLE.md) for the full style guide, including QML and SQL conventions

### Naming Conventions

See [STYLE.md](STYLE.md) § Naming Conventions for the full table. Key rules:
camelCase functions, PascalCase types, `m_` member prefix, SCREAMING_SNAKE_CASE constants.

### Documentation

- Add comments for non-obvious logic
- Keep comments up to date with code changes
- Use British spelling in all documentation and comments

### British Spelling 🇬🇧

Use British spelling in all documentation and user-facing text:

| American | British |
|----------|---------|
| color | colour |
| behavior | behaviour |
| organization | organisation |
| center | centre |
| license (noun) | licence |
| analyze | analyse |
| initialize | initialise |
| optimize | optimise |
| synchronize | synchronise |
| customize | customise |

**Note:** Code identifiers may use American spelling where it matches library/API conventions
(e.g., Qt's `color` properties, SQLite keywords). Backtick such identifiers in prose so it's
clear they are API names, not narrative text.

---

## Commit Messages

We use [Conventional Commits](https://www.conventionalcommits.org/). Format:

```text
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
```

### Types

| Type | Description |
|------|-------------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation only |
| `style` | Formatting, no code change |
| `refactor` | Code change that neither fixes a bug nor adds a feature |
| `perf` | Performance improvement |
| `test` | Adding or updating tests |
| `chore` | Maintenance tasks |
| `ci` | CI/CD changes |

### Examples

```text
feat(importer): add merge-by-uuid for incremental exports

fix(reader): handle messages with empty content blocks

docs: update README with build instructions

chore: bump Qt to 6.8
```

### Rules

- Use imperative mood ("Add feature" not "Added feature")
- Don't capitalise the first letter of the description
- No period at the end of the subject line
- Keep the subject line under 72 characters
- Reference issues in the footer: `Fixes #123`

---

## Documentation

### Types of Documentation

| Location | Purpose |
|----------|---------|
| `README.md` | User-facing overview and quick start |
| `CONTRIBUTING.md` | This file — contributor guidelines |
| `CODE_OF_CONDUCT.md` | Contributor Covenant code of conduct |
| `STYLE.md` | Code style conventions (C++, QML, SQL, Python, YAML, Markdown) |
| `SECURITY.md` | Privacy model, security policy, and vulnerability reporting |
| `CHANGELOG.md` | User-facing change history (reserved until first stable release) |
| `.claude/CLAUDE.md` | AI assistant context — export-format findings and project plan |

### Updating Documentation

- Update `README.md` for user-facing changes
- Update code comments when changing public APIs
- Update `.claude/CLAUDE.md` when the architecture or plan changes
- Update `STYLE.md` and bump its `*Last updated:* …` timestamp when changing style conventions
- Keep examples up to date and working
- `CHANGELOG.md` is reserved for the first stable release; skip it for pre-release work

---

## Questions?

- Open a [Discussion](https://github.com/MatejGomboc/claude-chats-browser/discussions) for questions
- Check existing issues and discussions first
- Be patient — maintainers are volunteers

Thank you for contributing!
