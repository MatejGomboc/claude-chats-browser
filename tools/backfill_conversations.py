#!/usr/bin/env python3
"""Backfill content-less conversations in a claude.ai data export.

The claude.ai export feature often dumps a large fraction of conversations as
nameless skeletons (empty text/content on every message). This script finds
those, re-fetches each one from the claude.ai backend API using your own
logged-in browser session, and can merge the results back into a single
conversations-merged.json in the export schema.

Usage:
  1. Log in to https://claude.ai in your browser.
  2. F12 -> Application -> Cookies -> https://claude.ai -> copy the value of
     `sessionKey` (starts with sk-ant-sid01-...).
  3. python backfill_conversations.py --export <extracted-export-dir> fetch
     (paste the session key when prompted, or set CLAUDE_SESSION_KEY)
  4. python backfill_conversations.py --export <extracted-export-dir> merge

Fetch is resumable: conversations already saved under <out>/ are skipped.
Commands:
  list   just print how many conversations are missing content
  fetch  download missing conversations to <out>/<uuid>.json
  merge  write conversations-merged.json (export skeletons replaced by fetches)
"""

import argparse
import getpass
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request

try:  # browser TLS impersonation — needed to get past Cloudflare
    from curl_cffi import requests as cf_requests
except ImportError:
    cf_requests = None

BASE = "https://claude.ai/api"
UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36")

UUID_RE = re.compile(
    r"\A[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}"
    r"-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\Z")


def require_uuid(value, what="uuid"):
    """Return value iff it is a bare UUID, else raise ValueError.

    Every id used to build a request URL or an on-disk filename comes from the
    export file or the remote API, so it crosses a trust boundary. A UUID
    contains only hex digits and hyphens, so a value that passes this check
    cannot smuggle a path separator ("/", "..") or a URL metacharacter — this
    is the sanitiser for both the partial-SSRF and the path-injection paths.
    """
    if not isinstance(value, str) or UUID_RE.match(value) is None:
        raise ValueError(f"refusing to use a non-UUID {what}: {value!r}")
    return value


def conversation_output_path(out_dir, uuid):
    """Absolute path to <out_dir>/<uuid>.json, verified to stay within out_dir."""
    require_uuid(uuid, "conversation uuid")
    base = os.path.realpath(out_dir)
    path = os.path.realpath(os.path.join(base, uuid + ".json"))
    if os.path.commonpath((base, path)) != base:
        raise ValueError(f"refusing path outside {base!r}: {uuid!r}")
    return path


def conversation_api_path(org_uuid, conversation_uuid):
    """Backend API path for one conversation, built from validated UUIDs only."""
    require_uuid(org_uuid, "organisation uuid")
    require_uuid(conversation_uuid, "conversation uuid")
    return (f"/organizations/{org_uuid}/chat_conversations/{conversation_uuid}"
            "?tree=True&rendering_mode=messages&render_all_tools=true")


def load_export(export_dir):
    path = os.path.join(export_dir, "conversations.json")
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def is_full(conv):
    return any(m.get("text") or m.get("content")
               for m in conv.get("chat_messages", []))


def missing_uuids(convs):
    return [c["uuid"] for c in convs if not is_full(c)]


HEADERS = {
    "Accept": "application/json",
    "Accept-Language": "en-GB,en;q=0.9",
    "Referer": "https://claude.ai/chats",
    "Origin": "https://claude.ai",
    "anthropic-client-platform": "web_claude_ai",
}


def api_get(path, session_key, debug=False):
    url = BASE + path
    if cf_requests is not None:
        r = cf_requests.get(url, impersonate="chrome",
                            cookies={"sessionKey": session_key},
                            headers=HEADERS, timeout=60)
        if debug:
            print(f"HTTP {r.status_code}  content-type: "
                  f"{r.headers.get('content-type')}")
            print("body starts with:", r.text[:400].replace("\n", " "))
        if r.status_code >= 400:
            raise urllib.error.HTTPError(url, r.status_code, r.reason,
                                         None, None)
        return r.json()
    req = urllib.request.Request(url, headers={
        "User-Agent": UA,
        "Accept": "application/json",
        "Cookie": f"sessionKey={session_key}",
    })
    with urllib.request.urlopen(req, timeout=60) as resp:
        return json.loads(resp.read().decode("utf-8"))


def pick_org(session_key):
    orgs = api_get("/organizations", session_key)
    chat_orgs = [o for o in orgs if "chat" in (o.get("capabilities") or [])]
    org = (chat_orgs or orgs)[0]
    print(f"Using organization: {org.get('name', '?')} ({org['uuid']})")
    return org["uuid"]


def fetch(export_dir, out_dir, session_key):
    out_dir = os.path.realpath(out_dir)
    convs = load_export(export_dir)
    todo = missing_uuids(convs)
    os.makedirs(out_dir, exist_ok=True)
    done = {f[:-5] for f in os.listdir(out_dir) if f.endswith(".json")}
    todo = [u for u in todo if u not in done]
    print(f"{len(done)} already fetched, {len(todo)} to go")
    org = require_uuid(pick_org(session_key), "organisation uuid")

    errors = 0
    for i, uuid in enumerate(todo, 1):
        try:
            api_path = conversation_api_path(org, uuid)
            out_path = conversation_output_path(out_dir, uuid)
        except ValueError as exc:
            print(f"[{i}/{len(todo)}] skipping — {exc}")
            continue
        delay = 1.2
        for attempt in range(5):
            try:
                data = api_get(api_path, session_key)
                with open(out_path, "w", encoding="utf-8") as f:
                    json.dump(data, f, ensure_ascii=False)
                print(f"[{i}/{len(todo)}] {uuid} ok "
                      f"({len(data.get('chat_messages', []))} msgs)")
                errors = 0
                break
            except urllib.error.HTTPError as e:
                if e.code in (401, 403):
                    sys.exit(f"HTTP {e.code} — session key rejected (or "
                             f"Cloudflare block). Grab a fresh sessionKey "
                             f"cookie and re-run; progress is saved.")
                if e.code == 404:
                    print(f"[{i}/{len(todo)}] {uuid} 404 — skipping")
                    break
                if e.code == 429 or e.code >= 500:
                    wait = delay * (2 ** attempt)
                    print(f"[{i}/{len(todo)}] HTTP {e.code}, retrying "
                          f"in {wait:.0f}s")
                    time.sleep(wait)
                    continue
                raise
            except urllib.error.URLError as e:
                wait = delay * (2 ** attempt)
                print(f"[{i}/{len(todo)}] network error ({e.reason}), "
                      f"retrying in {wait:.0f}s")
                time.sleep(wait)
        else:
            errors += 1
            if errors >= 5:
                sys.exit("5 conversations failed in a row — stopping. "
                         "Re-run later; progress is saved.")
        time.sleep(1.2)
    print("Fetch complete.")


def merge(export_dir, out_dir):
    out_dir = os.path.realpath(out_dir)
    convs = load_export(export_dir)
    replaced = 0
    for idx, c in enumerate(convs):
        if is_full(c):
            continue
        try:
            path = conversation_output_path(out_dir, c.get("uuid", ""))
        except ValueError:
            continue
        if not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8") as f:
            fetched = json.load(f)
        keep = {k: fetched[k] for k in
                ("uuid", "name", "summary", "created_at", "updated_at",
                 "account", "chat_messages") if k in fetched}
        keep.setdefault("uuid", c["uuid"])
        convs[idx] = {**c, **keep}
        replaced += 1
    out = os.path.join(export_dir, "conversations-merged.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(convs, f, ensure_ascii=False)
    still = sum(1 for c in convs if not is_full(c))
    print(f"Replaced {replaced} skeletons; {still} still content-less.")
    print(f"Wrote {out}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("command",
                    choices=["list", "fetch", "merge", "test", "check"])
    ap.add_argument("--export", required=True,
                    help="directory containing conversations.json")
    ap.add_argument("--out", default=None,
                    help="directory for fetched conversations "
                         "(default: <export>/backfill)")
    args = ap.parse_args()
    # Normalise operator-supplied directories up front so downstream joins have
    # no "../" to resolve.
    args.export = os.path.realpath(args.export)
    out_dir = os.path.realpath(args.out or os.path.join(args.export, "backfill"))

    if args.command == "list":
        convs = load_export(args.export)
        miss = missing_uuids(convs)
        print(f"{len(convs)} conversations, {len(miss)} missing content")
        return
    if args.command == "merge":
        merge(args.export, out_dir)
        return

    session_key = (os.environ.get("CLAUDE_SESSION_KEY") or getpass.getpass(
        "Paste sessionKey cookie value (hidden input): ")).strip()

    problems = []
    if not session_key.startswith("sk-ant-sid"):
        problems.append("does not start with sk-ant-sid")
    if len(session_key) < 100:
        problems.append(f"only {len(session_key)} chars — expected ~141")
    if any(ch in session_key for ch in "…. \"'"):
        problems.append("contains dots/ellipsis/quotes/spaces — looks like "
                        "the truncated display text, not the real value")
    if problems:
        print("WARNING — pasted key looks wrong:", "; ".join(problems))
        print("In DevTools, click the cookie row and copy the value from the "
              "editor pane at the bottom, not from the table.")

    if args.command == "check":
        # fetch a conversation the export DOES have content for — if even
        # this 404s, the endpoint/params are wrong, not the conversations
        convs = load_export(args.export)
        good = next(c for c in convs if is_full(c) and c.get("name"))
        print(f"Fetching known-good conversation: {good['name']!r} "
              f"({good['uuid']}, {len(good['chat_messages'])} msgs in export)")
        org = require_uuid(pick_org(session_key), "organisation uuid")
        try:
            data = api_get(conversation_api_path(org, good["uuid"]),
                           session_key, debug=True)
            print(f"SUCCESS — server returned {data.get('name')!r} with "
                  f"{len(data.get('chat_messages', []))} messages")
        except urllib.error.HTTPError as e:
            print(f"FAILED — HTTP {e.code}. The endpoint is wrong for this "
                  f"account, since this conversation definitely exists.")
        return

    if args.command == "test":
        print(f"Key length: {len(session_key)} chars")
        try:
            orgs = api_get("/organizations", session_key, debug=True)
            print("SUCCESS — organizations:",
                  [(o.get("name"), o.get("uuid")) for o in orgs])
        except urllib.error.HTTPError:
            print("Request failed — see status/body above.")
        return

    fetch(args.export, out_dir, session_key)


if __name__ == "__main__":
    main()
