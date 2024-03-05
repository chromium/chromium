#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script runs `gnrt update`, `gnrt vendor`, and `gnrt gen`, and then uploads
zero, one, or more resulting CLs to Gerrit.  For more details please see
`tools/crates/create_update_cl.md`."""

import datetime
import os
import re
import subprocess
import sys
import textwrap
import toml

THIS_DIR = os.path.dirname(__file__)
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..'))
CRATES_DIR = os.path.join(CHROMIUM_DIR, "third_party", "rust",
                          "chromium_crates_io")
VENDOR_DIR = os.path.join(CRATES_DIR, "vendor")
CARGO_LOCK = os.path.join(CRATES_DIR, "Cargo.lock")
VET_CONFIG = os.path.join(CRATES_DIR, "supply-chain", "config.toml")
INCLUSIVE_LANG_SCRIPT = os.path.join(
    CHROMIUM_DIR, "infra", "update_inclusive_language_presubmit_exempt_dirs.sh")
INCLUSIVE_LANG_CONFIG = os.path.join(
    CHROMIUM_DIR, "infra", "inclusive_language_presubmit_exempt_dirs.txt")
RUN_GNRT = os.path.join(THIS_DIR, "run_gnrt.py")

timestamp = datetime.datetime.now()
BRANCH_BASENAME = "rust-crates-update"
BRANCH_BASENAME += f"--{timestamp.year}{timestamp.month:02}{timestamp.day:02}"
BRANCH_BASENAME += f"-{timestamp.hour:02}{timestamp.minute:02}"


def RunCommandAndCheckForErrors(args):
    """Runs a command and returns its output."""
    args = list(args)
    assert args

    # Needs shell=True on Windows due to git.bat in depot_tools.
    is_win = sys.platform.startswith('win32')
    result = subprocess.run(args,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            text=True,
                            shell=is_win)

    success = result.returncode == 0
    success &= re.search(r'\bwarning\b', result.stdout.lower()) is None
    success &= re.search(r'\berror\b', result.stdout.lower()) is None
    if not success:
        print(f"ERROR: Failure when running: {' '.join(args)}")
        print(result.stdout)
        raise RuntimeError(f"Failure when running {args[0]}")
    return result.stdout


def Git(*args):
    """Runs a git command."""
    return RunCommandAndCheckForErrors(['git'] + list(args))


def Gnrt(*args):
    """Runs a gnrt command."""
    return RunCommandAndCheckForErrors([RUN_GNRT] + list(args))


def GetCurrentCrateVersions():
    """Parses Cargo.lock and returns a dictionary from crate name to version."""
    t = toml.load(open(CARGO_LOCK))
    result = dict()
    for p in t["package"]:
        name = p["name"]
        version = p["version"]
        result[name] = version
    return result


def DiffCrateVersions(old_versions, new_versions):
    """Compares two results of `GetCurrentCrateVersions` and returns a `set` of
    old package identifiers (e.g. "syn@2.0.50") that have a different version in
    `new_versions`"""
    result = set()
    for name, old_version in old_versions.items():
        new_version = new_versions.get(name)
        if new_version and old_version != new_version:
            result.add(f"{name}@{old_version}")
    return result


def ConvertCrateIdToCrateName(crate_id):
    return crate_id[:crate_id.find("@")]


def FindUpdateableCrates():
    """Runs `gnrt update` and returns a `list` of old package identifiers (e.g.
    "syn@2.0.50") that can be updated to a new version.  (Idempotent -
    afterwards it runs `git reset --hard` to undo any changes.)"""
    print("Checking which crates can be updated...")
    assert not Git("status", "--porcelain")  # No local changes expected here.
    old_versions = GetCurrentCrateVersions()
    Gnrt("update")
    crate_ids = DiffCrateVersions(old_versions, GetCurrentCrateVersions())
    Git("reset", "--hard")
    if crate_ids:
        names = sorted([ConvertCrateIdToCrateName(id) for id in crate_ids])
        text = f"Found updates for {len(crate_ids)} crates: {', '.join(names)}"
        print(textwrap.shorten(text, 80))
    return sorted(crate_ids)


def FindSizeOfCrateUpdate(crate_id):
    """Runs `gnrt update <crate_id>` and returns how many crates this would
    update.  (`crate_id` typically looks like "syn@2.0.50".  This function is
    idempotent - at the end it runs `git reset --hard` to undo any changes.)"""
    print(f"Measuring the delta of updating {crate_id} to a newer version...")
    assert not Git("status", "--porcelain")  # No local changes expected here.
    old_versions = GetCurrentCrateVersions()
    Gnrt("update", crate_id)
    update_size = len(DiffCrateVersions(old_versions,
                                        GetCurrentCrateVersions()))
    Git("reset", "--hard")
    return update_size


def SortedMarkdownList(input_list):
    input_list = [f"* {item}" for item in sorted(input_list)]
    return "\n".join(input_list)


def CreateCommitDescription(crate_id, old_versions, new_versions):
    description = f"""Updating //third_party/rust libraries: {crate_id}.

This CL has been created semi-automatically.  The expected review
process and other details can be found at
//tools/crates/create_update_cl.md
"""

    update_descriptions = []
    new_crate_descriptions = []
    for name, new_version in new_versions.items():
        if name in old_versions:
            old_version = old_versions[name]
            if old_version != new_version:
                update_descriptions.append(
                    f"{name}: {old_version} => {new_version}")
        else:
            new_crate_descriptions.append(f"{name} {new_version}")
    removed_crate_descriptions = []
    for name, old_version in old_versions.items():
        if name not in new_versions:
            removed_crate_descriptions.append(f"{name} {old_version}")
    update_descriptions = SortedMarkdownList(update_descriptions)
    new_crate_descriptions = SortedMarkdownList(new_crate_descriptions)
    removed_crate_descriptions = SortedMarkdownList(removed_crate_descriptions)
    assert (update_descriptions)
    description += f"\nUpdated crates:\n\n{update_descriptions}\n"
    if new_crate_descriptions:
        description += f"\nNew crates:\n\n{new_crate_descriptions}\n"
    if removed_crate_descriptions:
        description += f"\nRemoved crates:\n\n{removed_crate_descriptions}\n"

    vet_config = toml.load(open(VET_CONFIG))
    criteria = vet_config["policy"][crate_id.replace("@", ":")]["criteria"]
    if criteria:
        criteria = ", ".join(criteria)
    else:
        criteria = "<no applicable cargo vet audit criteria (unused crate?)>"
    criteria = f"""
The following `cargo vet` audit criteria are required for {crate_id}:
{criteria}
"""
    description += "\n"
    description += textwrap.fill(criteria,
                                 width=72,
                                 break_long_words=False,
                                 break_on_hyphens=False).strip()
    description += "\n"

    description += """
Bug: None
Cq-Include-Trybots: chromium/try:android-rust-arm32-rel
Cq-Include-Trybots: chromium/try:android-rust-arm64-dbg
Cq-Include-Trybots: chromium/try:android-rust-arm64-rel
Cq-Include-Trybots: chromium/try:linux-rust-x64-dbg
Cq-Include-Trybots: chromium/try:linux-rust-x64-rel
Cq-Include-Trybots: chromium/try:win-rust-x64-dbg
Cq-Include-Trybots: chromium/try:win-rust-x64-rel
Disable-Rts: True
"""

    return description


def UpdateCrate(crate_id, upstream_branch):
    """Runs `gnrt update <crate_id>` and other follow-up commands to actually
    update the crate."""

    print(f"Updating {crate_id} to a newer version...")
    assert not Git("status", "--porcelain")  # No local changes expected here.
    Git("checkout", upstream_branch)
    assert not Git("status", "--porcelain")  # No local changes expected here.

    # gnrt update
    old_versions = GetCurrentCrateVersions()
    print(f"  Running `gnrt update {crate_id}` ...")
    Gnrt("update", crate_id)
    new_versions = GetCurrentCrateVersions()
    if old_versions == new_versions:
        print("  `gnrt update` resulted in no changes - "\
              "maybe other steps will handle this crate...")
        return upstream_branch
    description = CreateCommitDescription(crate_id, old_versions, new_versions)

    # Checkout a new git branch + `git cl upload`
    new_branch = f"{BRANCH_BASENAME}--{crate_id.replace('@', '-')}"
    Git("checkout", upstream_branch, "-b", new_branch)
    Git("branch", "--set-upstream-to", upstream_branch)
    Git("add", "-f", "third_party/rust")
    Git("commit", "-m", description)
    print(f"  Running `git cl upload ...` ...")
    Git("cl", "upload", "--bypass-hooks", "--force",
        "--hashtag=cratesio-autoupdate",
        "--cc=chrome-rust-experiments+autoupdate@google.com")

    # git mv <vendor/old version> <vendor/new version>
    print(f"  Running `git mv <vendor/old version> <vendor/new version>`...")
    updated_crate_names = []
    for crate_name, new_version in new_versions.items():
        if crate_name in old_versions:
            old_version = old_versions[crate_name]
            if old_version != new_version:
                updated_crate_names.append(crate_name)
    for crate_name in updated_crate_names:
        old_dir = os.path.join(VENDOR_DIR,
                               f"{crate_name}-{old_versions[crate_name]}")
        new_dir = os.path.join(VENDOR_DIR,
                               f"{crate_name}-{new_versions[crate_name]}")
        Git("mv", "--", old_dir, new_dir)
    Git("add", "-f", "third_party/rust")
    Git("commit", "-m", "git mv <old dir> <new dir> (for better diff)")
    print(f"  Running `git cl upload ...` ...")
    Git("cl", "upload", "--bypass-hooks", "--force", "-m",
        "git mv <old dir> <new dir> (for better diff)")

    # gnrt vendor
    print(f"  Running `gnrt vendor`...")
    Git("reset", "--hard", "HEAD^")  # Undoing `git mv ...`
    Gnrt("vendor")
    new_content = RunCommandAndCheckForErrors([INCLUSIVE_LANG_SCRIPT])
    with open(INCLUSIVE_LANG_CONFIG, "w") as f:
        f.write(new_content)
    Git("add", INCLUSIVE_LANG_CONFIG)
    Git("add", "-f", "third_party/rust")
    Git("commit", "-m", "gnrt vendor")
    print(f"  Running `git cl upload ...` ...")
    Git("cl", "upload", "--bypass-hooks", "--force", "-m", "gnrt vendor")

    # gnrt gen
    print(f"  Running `gnrt gen`...")
    Gnrt("gen")
    # Some crates (e.g. ones in the `remove_crates` list of `gnrt_config.toml`)
    # may result in no changes - this is why we have an `if` below...
    if Git("status", "--porcelain"):
        Git("add", "-f", "third_party/rust")
        Git("commit", "-m", "gnrt gen")
        print(f"  Running `git cl upload ...` ...")
        Git("cl", "upload", "--bypass-hooks", "--force", "-m", "gnrt gen")

    return new_branch


def main():
    # Checkout `origin/main` branch.
    print("Checking out the `origin/main` branch...")
    if Git("status", "--porcelain"):
        raise RuntimeError("Dirty `git status` - save you local changes "\
                           "before rerunning the script")
    Git("checkout", "origin/main")
    if Git("status", "--porcelain"):
        raise RuntimeError("Dirty `git status` - save you local changes "\
                           "before rerunning the script")

    crate_ids = FindUpdateableCrates()
    if not crate_ids:
        print("There were no updates - exiting early...")
        return 0

    update_sizes = dict()
    for crate_id in crate_ids:
        update_sizes[crate_id] = FindSizeOfCrateUpdate(crate_id)

    crate_ids = sorted(crate_ids, key=lambda crate_id: update_sizes[crate_id])
    upstream_branch = "origin/main"
    for crate_id in crate_ids:
        upstream_branch = UpdateCrate(crate_id, upstream_branch)

    return 0


if __name__ == '__main__':
    sys.exit(main())
