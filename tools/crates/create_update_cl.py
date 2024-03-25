#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script runs `gnrt update`, `gnrt vendor`, and `gnrt gen`, and then uploads
zero, one, or more resulting CLs to Gerrit.  For more details please see
`tools/crates/create_update_cl.md`."""

import argparse
import datetime
import os
import re
import subprocess
import sys
import textwrap
import toml
from typing import List, Set, Dict

# Throughout the script, the following naming conventions are used (illustrated
# with a crate named `syn` and published as version `2.0.50`):
#
# * `crate_name`   : "syn" string
# * `crate_version`: "2.0.50" string
# * `crate_id`     : "syn@2.0.50" string (syntax used by `cargo`)
# * `crate_epoch`  : "syn@v2" string (made up syntax)
#
# The `old_versions` and `new_versions` dictionaries use `crate_epoch` as a key
# and `crate_version` as the value.  Note that `crate_name` may not be unique
# (e.g. if there is both `syn@1.0.109` and `syn@2.0.50`).  Also note that
# `crate_epoch` doesn't change during a minor version update (such as the one
# that this script produces in `auto` and `single` modes).

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
UPDATE_RUST_SCRIPT = os.path.join(CHROMIUM_DIR, "tools", "rust",
                                  "update_rust.py")

timestamp = datetime.datetime.now()
BRANCH_BASENAME = "rust-crates-update"
BRANCH_BASENAME += f"--{timestamp.year}{timestamp.month:02}{timestamp.day:02}"
BRANCH_BASENAME += f"-{timestamp.hour:02}{timestamp.minute:02}"


def RunCommandAndCheckForErrors(args, check_stdout: bool):
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
    if check_stdout:
        success &= re.search(r'\bwarning\b', result.stdout.lower()) is None
        success &= re.search(r'\berror\b', result.stdout.lower()) is None
    if not success:
        print(f"ERROR: Failure when running: {' '.join(args)}")
        print(result.stdout)
        raise RuntimeError(f"Failure when running {args[0]}")
    return result.stdout


def Git(*args) -> str:
    """Runs a git command."""
    return RunCommandAndCheckForErrors(['git'] + list(args), False)


def Gnrt(*args) -> str:
    """Runs a gnrt command."""
    return RunCommandAndCheckForErrors([RUN_GNRT] + list(args), True)


def GetCurrentCrateVersions() -> Dict[str, str]:
    """Parses Cargo.lock and returns a dictionary from crate epochs to versions
    (e.g. "syn@v2" -> "2.0.50")."""
    t = toml.load(open(CARGO_LOCK))
    result = dict()
    for p in t["package"]:
        name = p["name"]
        version = p["version"]
        key = GetCrateEpoch(name, version)
        assert key not in result
        result[key] = version
    return result


def DiffCrateVersions(old_versions: Dict[str, str],
                      new_versions: Dict[str, str]) -> Set[str]:
    """Compares two results of `GetCurrentCrateVersions` and returns a `set` of
    old crate ids (e.g. "syn@2.0.50") that have a different minor version in
    `new_versions`"""
    result = set()
    for crate_epoch, old_version in old_versions.items():
        new_version = new_versions.get(crate_epoch)
        if new_version and old_version != new_version:
            name = ConvertCrateIdToCrateName(crate_epoch)
            result.add(f"{name}@{old_version}")
    return result


def GetEpoch(crate_version: str) -> str:
    v = crate_version.split('.')
    if v[0] == '0':
        return f'v0_{v[1]}'
    return f'v{v[0]}'


def GetCrateEpoch(crate_name: str, crate_version: str) -> str:
    epoch = GetEpoch(crate_version)
    return f'{crate_name}@{epoch}'


def ConvertCrateIdToCrateName(crate_id: str) -> str:
    """ Converts a `crate_id` (`crate_epoch` also ok) into a `crate_name`."""
    return crate_id[:crate_id.find("@")]


def ConvertCrateIdToCrateVersion(crate_id: str) -> str:
    """ Converts a `crate_id` into a `crate_version`."""
    # Unlike ConvertCrateIdToCrateName this func can't work with a `crate_epoch`
    assert '@v' not in crate_id

    crate_version = crate_id[crate_id.find("@") + 1:]
    return crate_version


def FindUpdateableCrates() -> List[str]:
    """Runs `gnrt update` and returns a `list` of old crate ids (e.g.
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


def FindSizeOfCrateUpdate(crate_id: str) -> int:
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


def FormatMarkdownItem(item: str) -> str:
    return textwrap.fill(f"* {item}",
                         width=72,
                         subsequent_indent='  ',
                         break_long_words=False,
                         break_on_hyphens=False).strip()


def SortedMarkdownList(input_list: List[str]) -> str:
    input_list = [FormatMarkdownItem(item) for item in sorted(input_list)]
    return "\n".join(input_list)


def CreateVetPolicyDescription(crate_ids: List[str]) -> str:
    """Returns a textual description of the required `cargo vet`'s
    certifications.

    Args:
        crate_ids: List of crate ids - e.g. `["clap@1.2.3","clap_derive@1.2.3"]`

    Returns:
        String suitable for including in the CL description.
    """
    vet_config = toml.load(open(VET_CONFIG))
    crate_id_to_criteria = dict()
    for crate_id in crate_ids:
        crate_name = ConvertCrateIdToCrateName(crate_id)
        crate_version = ConvertCrateIdToCrateVersion(crate_id)
        policy = vet_config["policy"][f"{crate_name}:{crate_version}"][
            "criteria"]
        policy.sort()
        crate_id_to_criteria[crate_id] = policy

    criteria_to_crate_ids = dict()
    for crate_id, criteria in crate_id_to_criteria.items():
        criteria = ', '.join(criteria)
        if criteria not in criteria_to_crate_ids:
            criteria_to_crate_ids[criteria] = []
        criteria_to_crate_ids[criteria].append(crate_id)

    items = []
    for criteria, crate_ids in criteria_to_crate_ids.items():
        crate_ids.sort()
        crate_ids = ', '.join(crate_ids)
        if not criteria:
            criteria = "No audit criteria found. Crates with no audit " \
                       "criteria can be submitted without an update to " \
                       "audits.toml."
        items.append(f"{crate_ids}: {criteria}")

    description = "The following `cargo vet` audit criteria are required:\n\n"
    description += SortedMarkdownList(items)

    return description


def CreateCommitDescription(crate_id: str, old_versions: Dict[str, str],
                            new_versions: Dict[str, str],
                            include_vet_criteria: bool) -> str:
    crate_name = ConvertCrateIdToCrateName(crate_id)
    old_version = ConvertCrateIdToCrateVersion(crate_id)
    crate_epoch = GetCrateEpoch(crate_name, old_version)
    new_version = new_versions[crate_epoch]
    assert crate_epoch == GetCrateEpoch(crate_name, new_version)
    roll_summary = f"{crate_name}: {old_version} => {new_version}"
    description = f"""Roll {roll_summary} in //third_party/rust.

This CL has been created semi-automatically.  The expected review
process and other details can be found at
//tools/crates/create_update_cl.md
"""

    new_or_updated_crate_ids = []
    update_descriptions = []
    new_crate_descriptions = []
    for new_epoch, new_version in new_versions.items():
        name = ConvertCrateIdToCrateName(new_epoch)
        if new_epoch in old_versions:
            old_version = old_versions[new_epoch]
            if old_version != new_version:
                new_or_updated_crate_ids.append(f'{name}@{new_version}')
                update_descriptions.append(
                    f"{name}: {old_version} => {new_version}")
        else:
            new_or_updated_crate_ids.append(f'{name}@{new_version}')
            new_crate_descriptions.append(f"{name} {new_version}")
    removed_crate_descriptions = []
    for old_epoch, old_version in old_versions.items():
        if old_epoch not in new_versions:
            name = ConvertCrateIdToCrateName(old_epoch)
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

    if include_vet_criteria:
        vet_policies = CreateVetPolicyDescription(new_or_updated_crate_ids)
        description += f"\n{vet_policies}"

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


def UpdateCrate(args, crate_id: str, upstream_branch: str):
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
    description = CreateCommitDescription(crate_id, old_versions, new_versions,
                                          False)

    # Checkout a new git branch + `git cl upload`
    new_branch = f"{BRANCH_BASENAME}--{crate_id.replace('@', '-')}"
    Git("checkout", upstream_branch, "-b", new_branch)
    Git("branch", "--set-upstream-to", upstream_branch)
    Git("add", "-f", "third_party/rust")
    Git("commit", "-m", description)
    if args.upload:
        print(f"  Running `git cl upload ...` ...")
        Git("cl", "upload", "--bypass-hooks", "--force",
            "--hashtag=cratesio-autoupdate",
            "--cc=chrome-rust-experiments+autoupdate@google.com")
        issue = Git("cl", "issue")

    # git mv <vendor/old version> <vendor/new version>
    print(f"  Running `git mv <vendor/old version> <vendor/new version>`...")
    for new_epoch, new_version in new_versions.items():
        if new_epoch in old_versions:
            old_version = old_versions[new_epoch]
            if old_version != new_version:
                crate_name = ConvertCrateIdToCrateName(new_epoch)
                old_dir = os.path.join(VENDOR_DIR,
                                       f"{crate_name}-{old_version}")
                new_dir = os.path.join(VENDOR_DIR,
                                       f"{crate_name}-{new_version}")
                Git("mv", "--", old_dir, new_dir)
    Git("add", "-f", "third_party/rust")
    Git("commit", "-m", "git mv <old dir> <new dir> (for better diff)")
    if args.upload:
        print(f"  Running `git cl upload ...` ...")
        Git("cl", "upload", "--bypass-hooks", "--force", "-m",
            "git mv <old dir> <new dir> (for better diff)")

    # gnrt vendor
    print(f"  Running `gnrt vendor`...")
    Git("reset", "--hard", "HEAD^")  # Undoing `git mv ...`
    Gnrt("vendor")
    Git("add", "-f", "third_party/rust")
    # `INCLUSIVE_LANG_SCRIPT` below uses `git grep` and therefore depends on the
    # earlier `Git("add"...)` above.  Please don't reorder/coalesce the `add`.
    new_content = RunCommandAndCheckForErrors([INCLUSIVE_LANG_SCRIPT], False)
    with open(INCLUSIVE_LANG_CONFIG, "w") as f:
        f.write(new_content)
    Git("add", INCLUSIVE_LANG_CONFIG)
    Git("commit", "-m", "gnrt vendor")
    if args.upload:
        print(f"  Running `git cl upload ...` ...")
        Git("cl", "upload", "--bypass-hooks", "--force", "-m", "gnrt vendor")
        print(f"  Running `git cl description ...` ...")
        description = CreateCommitDescription(crate_id, old_versions,
                                              new_versions, True)
        Git("cl", "description", f"--new-description={description}")

    # gnrt gen
    print(f"  Running `gnrt gen`...")
    Gnrt("gen")
    # Some crates (e.g. ones in the `remove_crates` list of `gnrt_config.toml`)
    # may result in no changes - this is why we have an `if` below...
    if Git("status", "--porcelain"):
        Git("add", "-f", "third_party/rust")
        Git("commit", "-m", "gnrt gen")
        if args.upload:
            print(f"  Running `git cl upload ...` ...")
            Git("cl", "upload", "--bypass-hooks", "--force", "-m", "gnrt gen")

    if args.upload:
        print(f"  {issue}")

    return new_branch


def CheckoutInitialBranch(branch):
    print(f"Checking out the `{branch}` branch...")
    if Git("status", "--porcelain"):
        raise RuntimeError("Dirty `git status` - save you local changes "\
                           "before rerunning the script")
    Git("checkout", branch)
    if Git("status", "--porcelain"):
        raise RuntimeError("Dirty `git status` - save you local changes "\
                           "before rerunning the script")

    # Ensure the //third_party/rust-toolchain version matches the branch.
    print("Running //tools/rust/update_rust.py (hopefully a no-op)...")
    RunCommandAndCheckForErrors([UPDATE_RUST_SCRIPT], False)


def ResolveCrateNameToCrateId(crate_name):
    """Parses `Cargo.toml` to resolve `crate_name` into "crate-name@1.2.3".
    Throws if `crate_name` can't be resolved.

    Parameters:
      crate_name: Either "crate-name" or already "crate-name@1.2.3"
    """
    t = toml.load(open(CARGO_LOCK))
    if '@' in crate_name:
        resolved_crate_version = ConvertCrateIdToCrateVersion(crate_name)
        resolved_crate_name = ConvertCrateIdToCrateName(crate_name)
    else:
        same_name = [p for p in t["package"] if p["name"] == crate_name]
        if len(same_name) == 0:
            raise RuntimeError(
                f"`Cargo.toml` has no crates matching `{crate_name}`")
        elif len(same_name) > 1:
            ver1 = same_name[0]["version"]
            ver2 = same_name[1]["version"]
            raise RuntimeError(
                f"Ambiguous argument - specify which old version to update, "\
                f"e.g. `{crate_name}@{ver1}` or `{crate_name}@{ver2}")
        resolved_crate_name = crate_name
        resolved_crate_version = same_name[0]["version"]

    crate_id = f"{resolved_crate_name}@{resolved_crate_version}"
    return crate_id


def AutoUpdate(args):
    upstream_branch = args.upstream_branch
    CheckoutInitialBranch(upstream_branch)

    crate_ids = FindUpdateableCrates()
    if not crate_ids:
        print("There were no updates - exiting early...")
        return 0

    update_sizes = dict()
    for crate_id in crate_ids:
        update_sizes[crate_id] = FindSizeOfCrateUpdate(crate_id)

    crate_ids = sorted(crate_ids, key=lambda crate_id: update_sizes[crate_id])
    print(f"** Updating {len(crate_ids)} crates! "
          f"Expect this to take about {len(crate_ids) * 2} minutes.")
    for crate_id in crate_ids:
        upstream_branch = UpdateCrate(args, crate_id, upstream_branch)


def SingleCrate(args):
    upstream_branch = args.upstream_branch
    CheckoutInitialBranch(upstream_branch)

    crate_id = ResolveCrateNameToCrateId(args.crate[0])
    UpdateCrate(args, crate_id, upstream_branch)


def main():
    parser = argparse.ArgumentParser(description="Update Rust crates")
    parser.add_argument("--no-upload",
                        dest='upload',
                        default=True,
                        action='store_false',
                        help="Avoids uploading CLs to Gerrit")
    subparsers = parser.add_subparsers(required=True)

    parser_auto = subparsers.add_parser(
        "auto", description="Automatically update minor version of all crates")
    parser_auto.set_defaults(func=AutoUpdate)
    parser_auto.add_argument(
        "--upstream-branch",
        default="origin/main",
        help="The upstream branch on which to base the series of CLs.")

    parser_single = subparsers.add_parser(
        "single",
        description="Automatically update minor version of a single crate")
    parser_single.set_defaults(func=SingleCrate)
    parser_single.add_argument("crate",
                               nargs=1,
                               help="The name of the crate to update.")
    parser_single.add_argument(
        "--upstream-branch",
        default="origin/main",
        help="The upstream branch on which to base the update CL.")

    args = parser.parse_args()
    args.func(args)

    return 0


if __name__ == '__main__':
    sys.exit(main())
