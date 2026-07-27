#!/usr/bin/env python3
"""Build the Windows installer with the Qt Installer Framework.

Stages the QtIFW input tree from installer/ (substituting the application
version into the config templates), copies the deployed application into the
package's data directory, and runs binarycreator. The same script runs in the
release workflow and locally, so an installer can always be reproduced and
tested on a development machine before tagging a release.

Usage (from the repository root, after a Release build + deploy):
  cmake --preset windows-msvc-release
  cmake --build --preset windows-msvc-release
  cmake --install build/windows-msvc-release
  python tools/build_installer.py \
      --deploy-dir build/windows-msvc-release-deploy \
      --output claude-chats-browser-setup.exe

The application version is read from CMakeLists.txt (the single source of
truth, already enforced against release tags by the release workflow).
binarycreator is found in the conventional QtIFW install locations —
C:/Qt/Tools/QtInstallerFramework/*/bin (the official installer's default)
and ~/Qt/Tools/QtInstallerFramework/*/bin (aqtinstall's install-tool, used
by the release workflow). Deliberately not configurable: the tool only ever
executes a binarycreator found at these fixed locations.

All directory and output arguments must resolve to paths inside the
repository (the working tree this script lives in); the tool refuses
anything else, so arguments can never write outside the checkout.
"""

import argparse
import datetime
import glob
import os
import re
import shutil
import subprocess
import sys

REPO_ROOT = os.path.realpath(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PACKAGE_ID = "io.github.matejgomboc.claudechatsbrowser"

# Only version lines of exactly this shape are accepted; the pattern is linear
# (no nested/ambiguous quantifiers), so hostile file content cannot trigger
# catastrophic backtracking.
VERSION_RE = re.compile(r"^\s*project\s*\(", re.MULTILINE)
VERSION_NUMBER_RE = re.compile(r"VERSION\s+(\d+\.\d+\.\d+)")


def contained_path(value, what):
    """Resolve value and require it to live inside the repository.

    Every path argument crosses a trust boundary (argparse input), and each is
    used to create, delete or write files. Normalising and then prefix-checking
    against the repository root guarantees the tool cannot touch anything
    outside its own checkout, whatever the arguments say.
    """
    resolved = os.path.normpath(os.path.abspath(value))
    if not resolved.startswith(REPO_ROOT + os.sep):
        raise SystemExit(f"error: {what} must be inside the repository: {value!r}")
    return resolved


def read_project_version():
    """Return the MAJOR.MINOR.PATCH version declared in CMakeLists.txt."""
    cmakelists = os.path.join(REPO_ROOT, "CMakeLists.txt")
    with open(cmakelists, encoding="utf-8") as fh:
        text = fh.read()
    # Find the project() call, then the VERSION number on the rest of that line.
    project = VERSION_RE.search(text)
    if project is not None:
        line_end = text.find("\n", project.end())
        match = VERSION_NUMBER_RE.search(text[project.end():line_end])
        if match is not None:
            return match.group(1)
    raise SystemExit("error: could not find the project VERSION in CMakeLists.txt")


def find_binarycreator():
    """Locate binarycreator in the conventional QtIFW install locations.

    Deliberately takes no input: the only program this tool ever executes is
    a binarycreator(.exe) found under C:/Qt/Tools or ~/Qt/Tools, so no
    argument or environment variable can redirect execution elsewhere.
    """
    exe = "binarycreator.exe" if os.name == "nt" else "binarycreator"
    candidates = []
    candidates.extend(
        sorted(glob.glob(f"C:/Qt/Tools/QtInstallerFramework/*/bin/{exe}"), reverse=True))
    candidates.extend(
        sorted(glob.glob(os.path.expanduser(f"~/Qt/Tools/QtInstallerFramework/*/bin/{exe}")),
               reverse=True))
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    raise SystemExit(
        "error: binarycreator not found; install the Qt Installer Framework under "
        "C:/Qt/Tools or ~/Qt/Tools (aqt install-tool <host> desktop tools_ifw)")


def substitute(template_path, output_path, version):
    """Write template with @VERSION@/@RELEASE_DATE@ filled in.

    Only these two placeholders are replaced: QtIFW's own runtime variables
    (@TargetDir@, @ApplicationsDirX64@, ...) use the same @...@ syntax and
    must survive verbatim.
    """
    with open(template_path, encoding="utf-8") as fh:
        text = fh.read()
    text = text.replace("@VERSION@", version)
    text = text.replace("@RELEASE_DATE@", datetime.date.today().isoformat())
    with open(output_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)


def main():
    """Stage the QtIFW input tree and run binarycreator; return the exit code."""
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--deploy-dir", required=True,
        help="directory with the windeployqt-deployed application")
    parser.add_argument(
        "--output", required=True, help="path of the installer executable to create")
    parser.add_argument(
        "--staging-dir", help="staging directory (default: <deploy-dir>/../installer-staging)")
    args = parser.parse_args()

    # Every path argument is confined to the repository before first use.
    deploy_dir = contained_path(args.deploy_dir, "--deploy-dir")
    output = contained_path(args.output, "--output")
    staging = contained_path(
        args.staging_dir or os.path.join(deploy_dir, os.pardir, "installer-staging"),
        "--staging-dir")

    # The deploy install produces Qt's bin/plugins/translations layout.
    if not os.path.isfile(os.path.join(deploy_dir, "bin", "claude-chats-browser.exe")):
        raise SystemExit(
            f"error: {deploy_dir} does not contain bin/claude-chats-browser.exe; "
            "run the deploy install first")

    version = read_project_version()
    binarycreator = find_binarycreator()

    # Stage a fresh QtIFW input tree: config/ + packages/<id>/{meta,data}.
    if os.path.isdir(staging):
        shutil.rmtree(staging)
    config_dir = os.path.join(staging, "config")
    meta_dir = os.path.join(staging, "packages", PACKAGE_ID, "meta")
    data_dir = os.path.join(staging, "packages", PACKAGE_ID, "data")
    os.makedirs(config_dir)
    os.makedirs(meta_dir)

    installer_src = os.path.join(REPO_ROOT, "installer")
    meta_src = os.path.join(installer_src, "packages", PACKAGE_ID, "meta")
    substitute(
        os.path.join(installer_src, "config", "config.xml.in"),
        os.path.join(config_dir, "config.xml"), version)
    substitute(
        os.path.join(meta_src, "package.xml.in"), os.path.join(meta_dir, "package.xml"), version)
    shutil.copy2(os.path.join(meta_src, "installscript.qs"), meta_dir)
    shutil.copy2(os.path.join(REPO_ROOT, "LICENCE"), meta_dir)  # referenced by package.xml

    shutil.copytree(deploy_dir, data_dir)

    print(f"building installer {output} (version {version}) with {binarycreator}")
    subprocess.run(
        [
            binarycreator,
            "--config",
            os.path.join(config_dir, "config.xml"),
            "--packages",
            os.path.join(staging, "packages"),
            output,
        ],
        check=True,
    )
    print(f"installer written: {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
