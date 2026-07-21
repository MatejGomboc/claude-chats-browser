# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.x.x   | Yes (development)  |

Once we reach v1.0, we will maintain security updates for the current major version and one previous major version.

## Privacy Model

claude-chats-browser processes **private conversation data** (claude.ai data exports). Its privacy guarantees:

1. **Offline-first:** All browsing, searching, and reading happens locally. The app never sends conversation data anywhere.
2. **No telemetry:** The app collects and transmits nothing.
3. **User-initiated network access only:** The only network operation is the optional fetch of the user's own
   conversations from the claude.ai backend API (`tools/backfill_conversations.py`), authenticated with the user's own
   session cookie, entered interactively and never written to disk.
4. **Local data stays out of the repository:** `data/` is gitignored; exports and the generated SQLite database never leave the user's machine.

Given this model, we consider these especially critical:

| Severity | Examples |
|----------|----------|
| **Critical** | Anything that transmits conversation data off the user's machine |
| **Critical** | Session key leakage into logs, files, or error messages |
| **High** | Code execution or memory corruption when parsing a malicious/corrupted export |
| **Medium** | Denial of service (e.g., crafted export causing hangs or crashes) |
| **Low** | Issues requiring local access or unlikely scenarios |

## Reporting a Vulnerability

**Please do NOT report security vulnerabilities through public GitHub issues.**

### How to Report

1. **Preferred:** Use [GitHub Security Advisories](https://github.com/MatejGomboc/claude-chats-browser/security/advisories/new) to report vulnerabilities privately.

2. **Alternative:** Email the repository owner directly at <matejg03@gmail.com>.

### What to Include

When reporting a vulnerability, please include:

- A clear description of the vulnerability
- Steps to reproduce the issue
- Potential impact assessment
- Any suggested fixes (optional but appreciated)

### Response Timeline

| Action | Timeframe |
|--------|-----------|
| Initial acknowledgement | Within 48 hours |
| Preliminary assessment | Within 1 week |
| Fix development | Depends on severity and complexity |
| Security advisory publication | After fix is available |

### What to Expect

1. **Acknowledgement:** We will acknowledge receipt of your report within 48 hours.

2. **Communication:** We will keep you informed of our progress and may ask for additional information.

3. **Credit:** Unless you prefer to remain anonymous, we will credit you in our security advisory and release notes.

4. **Disclosure:** We follow responsible disclosure practices. We ask that you give us reasonable time to address the issue before any public disclosure.

---

*This security policy was last updated on 2026-07-21.*
