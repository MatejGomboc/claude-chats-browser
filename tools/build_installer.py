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
binarycreator is located via --qtifw-bin, the QTIFW_BIN environment variable,
or by scanning C:/Qt/Tools/QtInstallerFramework/*/bin.
"""

import argparse
import datetime
import glob
import os
import re
import shutil
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PACKAGE_ID = "io.github.matejgomboc.claudechatsbrowser"


def read_project_version():
    """Return the MAJOR.MINOR.PATCH version declared in CMakeLists.txt."""
    cmakelists = os.path.join(REPO_ROOT, "CMakeLists.txt")
    with open(cmakelists, encoding="utf-8") as fh:
        match = re.search(r"project\s*\([^)]*VERSION\s+(\d+\.\d+\.\d+)", fh.read())
    if match is None:
        raise SystemExit("error: could not find the project VERSION in CMakeLists.txt")
    return match.group(1)


def find_binarycreator(explicit):
    """Locate binarycreator: --qtifw-bin, then $QTIFW_BIN, then C:/Qt/Tools."""
    exe = "binarycreator.exe" if os.name == "nt" else "binarycreator"
    candidates = []
    if explicit:
        candidates.append(os.path.join(explicit, exe))
    if os.environ.get("QTIFW_BIN"):
        candidates.append(os.path.join(os.environ["QTIFW_BIN"], exe))
    candidates.extend(
        sorted(glob.glob(f"C:/Qt/Tools/QtInstallerFramework/*/bin/{exe}"), reverse=True))
    candidates.extend(
        sorted(glob.glob(os.path.expanduser(f"~/Qt/Tools/QtInstallerFramework/*/bin/{exe}")),
               reverse=True))
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    raise SystemExit(
        "error: binarycreator not found; pass --qtifw-bin or set QTIFW_BIN to the "
        "QtIFW bin directory")


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
    parser.add_argument("--qtifw-bin", help="QtIFW bin directory containing binarycreator")
    parser.add_argument(
        "--staging-dir", help="staging directory (default: <deploy-dir>/../installer-staging)")
    args = parser.parse_args()

    deploy_dir = os.path.abspath(args.deploy_dir)
    # The deploy install produces Qt's bin/plugins/translations layout.
    if not os.path.isfile(os.path.join(deploy_dir, "bin", "claude-chats-browser.exe")):
        raise SystemExit(
            f"error: {deploy_dir} does not contain bin/claude-chats-browser.exe; "
            "run the deploy install first")

    version = read_project_version()
    binarycreator = find_binarycreator(args.qtifw_bin)
    staging = os.path.abspath(
        args.staging_dir or os.path.join(deploy_dir, os.pardir, "installer-staging"))

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

    output = os.path.abspath(args.output)
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
