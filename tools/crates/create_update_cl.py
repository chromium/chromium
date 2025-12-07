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
import fnmatch
import itertools
import os
import re
import shlex
import shutil
import subprocess
import sys
import textwrap
import toml
from dataclasses import dataclass
from typing import List, Set, Dict

THIS_DIR = os.path.dirname(__file__)
CHROMIUM_DIR = os.path.normpath(os.path.join(THIS_DIR, '..', '..'))
THIRD_PARTY_RUST = os.path.join(CHROMIUM_DIR, "third_party", "rust")
CRATES_DIR = os.path.join(THIRD_PARTY_RUST, "chromium_crates_io")
VENDOR_DIR = os.path.join(CRATES_DIR, "vendor")
INCLUSIVE_LANG_SCRIPT = os.path.join(
    CHROMIUM_DIR, "infra", "update_inclusive_language_presubmit_exempt_dirs.sh")
INCLUSIVE_LANG_CONFIG = os.path.join(
    CHROMIUM_DIR, "infra", "inclusive_language_presubmit_exempt_dirs.txt")
RUN_GNRT = os.path.join(THIS_DIR, "run_gnrt.py")
UPDATE_RUST_SCRIPT = os.path.join(CHROMIUM_DIR, "tools", "rust",
                                  "update_rust.py")

sys.path.append(CRATES_DIR)
import crate_utils

# As in `third_party/rust/chromium_crates_io/crate_utils.py`, the following
# naming conventions are used in this script (illustrated with a crate named
# `syn` and published as version `2.0.50`):
#
# * `crate_name`   : "syn" string
# * `crate_version`: "2.0.50" string
# * `crate_id`     : "syn@2.0.50" string (syntax used by `cargo`)
# * `crate_epoch`  : "v2" string (syntax used in dir names under
#                    //third_party/rust/<crate name>/<crate epoch>)
#
# Note that `crate_name` may not be unique (e.g. if there is both `syn@1.0.109`
# and `syn@2.0.50`).  Also note that f`{crate_name}@{crate_epoch}` doesn't
# change during a minor version update (such as the one that this script
# produces in `auto` and `single` modes).

g_is_verbose = False

timestamp = datetime.datetime.now()
BRANCH_BASENAME = "rust-crates-update"
BRANCH_BASENAME += f"--{timestamp.year}{timestamp.month:02}{timestamp.day:02}"
BRANCH_BASENAME += f"-{timestamp.hour:02}{timestamp.minute:02}"


def RunCommandAndCheckForErrors(args, check_stdout: bool, check_exitcode: bool):
    """Runs a command and returns its output."""
    args = list(args)
    assert args

    if g_is_verbose:
        escaped = [shlex.quote(arg) for arg in args]
        msg = " ".join(escaped)
        print(f"    Running: {msg}")

    # Needs shell=True on Windows due to git.bat in depot_tools.
    is_win = sys.platform.startswith('win32')
    result = subprocess.run(args,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            text=True,
                            shell=is_win)

    success = True
    if check_exitcode:
        success &= (result.returncode == 0)
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
    return RunCommandAndCheckForErrors(['git'] + list(args),
                                       check_stdout=False,
                                       check_exitcode=True)


def GitAddRustFiles():
    Git("add", "-f", f"{VENDOR_DIR}")
    Git("add", f"{THIRD_PARTY_RUST}")


def Gnrt(*args) -> str:
    """Runs a gnrt command."""
    return RunCommandAndCheckForErrors([RUN_GNRT] + list(args),
                                       check_stdout=True,
                                       check_exitcode=True)


def GnrtUpdate(args: List[str], check_stdout: bool,
               check_exitcode: bool) -> str:
    """Runs `gnrt update` command."""
    # See the `[dependencies.cxxbridge-cmd]` section in
    # `third_party/rust/chromium_crates_io/Cargo.toml` for explanation why
    # `-Zbindeps` flag is needed.
    args = ["update", "--"] + args + ["-Zunstable-options", "-Zbindeps"]
    return RunCommandAndCheckForErrors([RUN_GNRT] + list(args),
                                       check_stdout=check_stdout,
                                       check_exitcode=check_exitcode)


def GnrtUpdateCrate(old_crate_id: str, new_crate_id: str, check_stdout: bool,
                    check_exitcode: bool):
    old_crate_version = crate_utils.ConvertCrateIdToCrateVersion(old_crate_id)
    new_crate_version = crate_utils.ConvertCrateIdToCrateVersion(new_crate_id)
    old_epoch = crate_utils.ConvertCrateIdToCrateEpoch(old_crate_id)
    new_epoch = crate_utils.ConvertCrateIdToCrateEpoch(new_crate_id)
    is_major_update = (old_epoch != new_epoch)

    cargo_update_args = [old_crate_id, "--precise", f"{new_crate_version}"]
    if is_major_update:
        cargo_update_args.append("--breaking")

    return GnrtUpdate(cargo_update_args,
                      check_stdout=check_stdout,
                      check_exitcode=check_exitcode)


@dataclass(eq=True, order=True)
class UpdatedCrate:
    old_crate_id: str
    new_crate_id: str

    def __str__(self):
        name = crate_utils.ConvertCrateIdToCrateName(self.old_crate_id)
        assert name == crate_utils.ConvertCrateIdToCrateName(self.new_crate_id)

        old_version = crate_utils.ConvertCrateIdToCrateVersion(
            self.old_crate_id)
        new_version = crate_utils.ConvertCrateIdToCrateVersion(
            self.new_crate_id)

        return f"{name}: {old_version} => {new_version}"


@dataclass
class CratesDiff:
    updates: List[UpdatedCrate]
    removed_crate_ids: List[str]
    added_crate_ids: List[str]

    def size(self):
        return len(self.updates) + len(self.added_crate_ids) + len(
            self.removed_crate_ids)


def DiffCrateIds(old_crate_ids: Set[str], new_crate_ids: Set[str],
                 only_minor_updates: bool) -> CratesDiff:
    """Compares two results of `GetCurrentCrateIds` and returns what changed.
    When `only_minor_updates` is True, then `foo@1.0` => `foo@2.0` will be
    treated as a removal of `foo@1.0` and an addition of `foo@2.0`.  Otherwise,
    it will be treated as an update.
    """

    def CrateIdsToDict(crate_ids: Set[str],
                       only_minor_updates) -> Dict[str, str]:
        """Transforms `crate_ids` into a dictionary that maps either 1) a crate
        name (when `only_minor_updates=False`) or 2) a crate epoch (when
        `only_minor_updates=True`) into 3) a crate version.  The caller should
        treat the key format as an opaque implementation detail and just assume
        that it will be stable for tracking a crate version across updates."""
        result = dict()
        for crate_id in crate_ids:
            name = crate_utils.ConvertCrateIdToCrateName(crate_id)
            version = crate_utils.ConvertCrateIdToCrateVersion(crate_id)
            if only_minor_updates:
                epoch = crate_utils.ConvertCrateIdToCrateEpoch(crate_id)
                key = f'{name}@{epoch}'
            else:
                key = name
            if key in result:
                # No conflicts expected in `auto` or `single` mode.
                assert not only_minor_updates
                old_crate_id = result[key]
                new_crate_id = crate_id
                raise RuntimeError(f"Error calculating a `Cargo.lock` diff:" + \
                                   f" conflict between {old_crate_id} and " + \
                                   f"{new_crate_id}")
            result[key] = crate_id
        return result

    # Ignoring `unchanged_ids` limits the situations when the key of `syn@1.x.x`
    # may conflict with the key of `syn@2.x.x`.
    unchanged_ids = new_crate_ids & old_crate_ids
    old_dict = CrateIdsToDict(old_crate_ids - unchanged_ids, only_minor_updates)
    new_dict = CrateIdsToDict(new_crate_ids - unchanged_ids, only_minor_updates)

    updates = list()
    removed_crate_ids = list()
    for key, old_crate_id in old_dict.items():
        new_crate_id = new_dict.get(key)
        if new_crate_id:
            assert old_crate_id != new_crate_id
            updates.append(UpdatedCrate(old_crate_id, new_crate_id))
        else:
            removed_crate_ids.append(old_crate_id)

    added_crate_ids = list()
    for key, new_crate_id in new_dict.items():
        if key not in old_dict:
            added_crate_ids.append(new_crate_id)

    return CratesDiff(sorted(updates), sorted(removed_crate_ids),
                      sorted(added_crate_ids))


def DoArgsAskForBreakingChanges(cargo_update_args) -> bool:
    # Hardcoding implementation details of `cargo update` is a bit icky, but it
    # helps to ensure that `DiffCrateIds` won't see dictionary key conflicts.
    return ("-b" in cargo_update_args) or ("--breaking" in cargo_update_args)


def FindUpdateableCrates(args) -> List[str]:
    """Runs `gnrt update` and returns a `list` of (old, new) crate ids (e.g.
    `("syn@2.0.50", "syn@2.0.51")`) that represent possible updates.
    (Idempotent - afterwards it runs `git reset --hard` to undo any changes.)"""
    print("Checking which crates can be updated...")
    assert not IsGitDirty()  # No local changes expected here.
    old_crate_ids = crate_utils.GetCurrentCrateIds()
    GnrtUpdate(args.remaining_args, check_stdout=False, check_exitcode=False)
    new_crate_ids = crate_utils.GetCurrentCrateIds()
    Git("reset", "--hard")
    only_minor_updates = not DoArgsAskForBreakingChanges(args.remaining_args)
    diff = DiffCrateIds(old_crate_ids, new_crate_ids, only_minor_updates)
    crate_updates = [(update.old_crate_id, update.new_crate_id)
                     for update in diff.updates]
    if crate_updates:
        names = sorted([
            crate_utils.ConvertCrateIdToCrateName(id)
            for (id, _) in crate_updates
        ])
        names = f"{', '.join(names)}"
        text = f"Found updates for {len(crate_updates)} crates: {names}"
        print("\n".join(textwrap.wrap(text, 80)))
    return sorted(crate_updates)


def FindSizeOfCrateUpdate(old_crate_id: str, new_crate_id: str,
                          only_minor_updates: bool) -> int:
    """Runs `gnrt update <crate_id>` and returns how many crates this would
    update.  (`crate_id` typically looks like "syn@2.0.50".  This function is
    idempotent - at the end it runs `git reset --hard` to undo any changes.)"""

    print(
        f"Measuring the delta of updating {old_crate_id} => {new_crate_id}...")
    assert not IsGitDirty()  # No local changes expected here.
    old_crate_ids = crate_utils.GetCurrentCrateIds()
    GnrtUpdateCrate(old_crate_id,
                    new_crate_id,
                    check_stdout=False,
                    check_exitcode=False)
    new_crate_ids = crate_utils.GetCurrentCrateIds()
    Git("reset", "--hard")
    diff = DiffCrateIds(old_crate_ids, new_crate_ids, only_minor_updates)
    return diff.size()


def FormatMarkdownItem(item: str) -> str:
    return textwrap.fill(f"* {item}",
                         width=72,
                         subsequent_indent='  ',
                         break_long_words=False,
                         break_on_hyphens=False).strip()


def SortedMarkdownList(input_list: List[str]) -> str:
    input_list = [FormatMarkdownItem(item) for item in sorted(input_list)]
    return "\n".join(input_list)


def CreateCommitTitle(old_crate_id: str, new_crate_id: str) -> str:
    crate_name = crate_utils.ConvertCrateIdToCrateName(old_crate_id)
    old_version = crate_utils.ConvertCrateIdToCrateVersion(old_crate_id)
    new_version = crate_utils.ConvertCrateIdToCrateVersion(new_crate_id)
    roll_summary = f"{crate_name}: " + \
        f"{old_version} => {new_version}"
    title = f"Roll {roll_summary} in //third_party/rust."
    return title


def CreateCommitTitleForBreakingUpdate(diff: CratesDiff) -> str:
    update_descriptions = [str(update) for update in diff.updates]
    roll_summary = ", ".join(update_descriptions)
    title = f"Roll {roll_summary}"
    return textwrap.shorten(title, width=72, placeholder="...")


def FormatCrateUpdateForClDescriptionBody(update: UpdatedCrate) -> str:
    name = crate_utils.ConvertCrateIdToCrateName(update.new_crate_id)
    new_version = crate_utils.ConvertCrateIdToCrateVersion(update.new_crate_id)
    return (f"{update}; https://docs.rs/crate/{name}/{new_version}")


def FormatCrateIdForClDescriptionBody(crate_id: str) -> str:
    name = crate_utils.ConvertCrateIdToCrateName(crate_id)
    version = crate_utils.ConvertCrateIdToCrateVersion(crate_id)
    return f"{crate_id}; https://docs.rs/crate/{name}/{version}"


def CreateCommitDescription(title: str, diff: CratesDiff) -> str:
    description = f"""{title}

This CL has been created semi-automatically.  The expected review
process and other details can be found at
//tools/crates/create_update_cl.md
"""

    update_descriptions = SortedMarkdownList(
        map(FormatCrateUpdateForClDescriptionBody, diff.updates))
    new_crate_descriptions = SortedMarkdownList(
        map(FormatCrateIdForClDescriptionBody, diff.added_crate_ids))
    removed_crate_descriptions = SortedMarkdownList(
        map(FormatCrateIdForClDescriptionBody, diff.removed_crate_ids))
    assert (update_descriptions)
    description += f"\nUpdated crates:\n\n{update_descriptions}\n"
    if new_crate_descriptions:
        description += f"\nNew crates:\n\n{new_crate_descriptions}\n"
    if removed_crate_descriptions:
        description += f"\nRemoved crates:\n\n{removed_crate_descriptions}\n"

    new_or_updated_crate_ids = diff.added_crate_ids + \
        [update.new_crate_id for update in diff.updates]

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


def UpdateCrate(args, old_crate_id: str, new_crate_id: str,
                upstream_branch: str, branch_number: int):
    """Runs `gnrt update <crate_id>` and other follow-up commands to actually
    update the crate."""

    only_minor_updates = not DoArgsAskForBreakingChanges(args.remaining_args)

    print(f"Updating {old_crate_id} to {new_crate_id}...")
    assert not IsGitDirty()  # No local changes expected here.
    Git("checkout", upstream_branch)
    assert not IsGitDirty()  # No local changes expected here.

    # gnrt update
    old_crate_ids = crate_utils.GetCurrentCrateIds()
    print(f"  Running `gnrt update` for {old_crate_id} => {new_crate_id} ...")
    GnrtUpdateCrate(old_crate_id,
                    new_crate_id,
                    check_stdout=True,
                    check_exitcode=True)
    new_crate_ids = crate_utils.GetCurrentCrateIds()
    if old_crate_ids == new_crate_ids:
        print("  `gnrt update` resulted in no changes - "\
              "maybe other steps will handle this crate...")
        return upstream_branch
    diff = DiffCrateIds(old_crate_ids, new_crate_ids, only_minor_updates)
    title = CreateCommitTitle(old_crate_id, new_crate_id)
    description = CreateCommitDescription(title, diff)

    # Checkout a new git branch + `git cl upload`
    branch_suffix = f"{branch_number:02}-{old_crate_id.replace('@', '-')}"
    new_branch = f"{BRANCH_BASENAME}--{branch_suffix}"
    Git("checkout", upstream_branch, "-b", new_branch)
    Git("branch", "--set-upstream-to", upstream_branch)
    GitAddRustFiles()
    Git("commit", "-m", description)
    if args.upload:
        print(f"  Running `git cl upload ...` ...")
        GitClUpload("--hashtag=cratesio-autoupdate",
                    "--cc=chrome-rust-experiments+autoupdate@google.com")

    FinishUpdatingCrate(args, title, diff)
    return new_branch


def FinishUpdatingCrate(args, title: str, diff: CratesDiff):
    updated_old_crate_ids = set()

    # git mv <vendor/old version> <vendor/new version>
    print(f"  Running `git mv <old dir> <new dir>` " +
          "(for better diff of major version updates)...")
    for update in diff.updates:
        updated_old_crate_ids.add(update.old_crate_id)

        old_dir = crate_utils.ConvertCrateIdToVendorDir(update.old_crate_id)
        new_dir = crate_utils.ConvertCrateIdToVendorDir(update.new_crate_id)
        if old_dir != new_dir:
            Git("mv", "--force", f"{old_dir}", f"{new_dir}")

            old_target_dir = crate_utils.ConvertCrateIdToBuildDir(
                update.old_crate_id)
            new_target_dir = crate_utils.ConvertCrateIdToBuildDir(
                update.new_crate_id)
            if old_target_dir != new_target_dir:
                Git("mv", "--force", old_target_dir, new_target_dir)
    GitAddRustFiles()
    did_commit = GitCommit(args,
                           "git mv <old dir> <new dir> (for better diff)",
                           error_if_no_changes=False)
    if did_commit:
        Git("reset", "--hard", "HEAD^")  # Undoing `git mv ...`

    # gnrt vendor
    print(f"  Running `gnrt vendor`...")
    Gnrt("vendor")
    GitAddRustFiles()
    # `INCLUSIVE_LANG_SCRIPT` below uses `git grep` and therefore depends on the
    # earlier `Git("add"...)` above.  Please don't reorder/coalesce the `add`.
    new_content = RunCommandAndCheckForErrors([INCLUSIVE_LANG_SCRIPT],
                                              check_stdout=False,
                                              check_exitcode=True)
    with open(INCLUSIVE_LANG_CONFIG, "w") as f:
        f.write(new_content)
    Git("add", INCLUSIVE_LANG_CONFIG)
    GitCommit(args, "gnrt vendor")

    # gnrt gen
    print(f"  Running `gnrt gen`...")
    Gnrt("gen")
    # Some crates (e.g. ones in the `remove_crates` list of `gnrt_config.toml`)
    # may result in no changes - this is why we have an `if` below...
    if IsGitDirty():
        GitAddRustFiles()
        GitCommit(args, "gnrt gen")

    # Remove old `//third_party/rust/foo/v<old>` directories
    # (in case this is a major version update)
    print(f"  Removing //third_party/rust/.../<old_epoch> ...")
    for update in diff.updates:
        old_target_dir = crate_utils.ConvertCrateIdToBuildDir(
            update.old_crate_id)
        new_target_dir = crate_utils.ConvertCrateIdToBuildDir(
            update.new_crate_id)
        if old_target_dir == new_target_dir:
            continue  # Skip minor crate updates

        old_files_count = len(
            Git("ls-files", "--", old_target_dir).splitlines())
        new_files_count = len(
            Git("ls-files", "--", new_target_dir).splitlines())
        if old_files_count == new_files_count:
            Git("rm", "-r", "--force", "--", old_target_dir)
        elif old_files_count > new_files_count:
            print(f"WARNING: not deleting {old_target_dir} "\
                   "because it contains extra files")
        else:
            print(
                f"WARNING: {old_target_dir} unexpectedly has less files "\
                f"than {new_target_dir}")
    GitCommit(
        args,
        "Removing //third_party/rust/.../<old_epoch>",
        # Just skip this commit when this is a minor-version update.
        error_if_no_changes=False)

    # Fix up the target names
    # (in case this is a major version update)
    print(f"  Updating the target name in BUILD.gn files...")
    for update in diff.updates:
        old_target = crate_utils.ConvertCrateIdToGnLabel(update.old_crate_id)
        new_target = crate_utils.ConvertCrateIdToGnLabel(update.new_crate_id)
        if old_target == new_target: continue
        # `check_exitcode=False` to gracefully handle no hits.
        grep = RunCommandAndCheckForErrors(
            ["git", "grep", "-l", old_target, "--", "*/BUILD.gn"],
            check_stdout=False,
            check_exitcode=False)
        for path in grep.splitlines():
            if not path: continue
            if "third_party/rust" in path: continue
            with open(path, 'r') as file:
                file_contents = file.read()
            file_contents = file_contents.replace(old_target, new_target)
            with open(path, 'w') as file:
                file.write(file_contents)
            Git("add", "--", path)
    GitCommit(
        args,
        "Updating the target name in BUILD.gn files",
        # Just skip this commit when this is a minor-version update.
        error_if_no_changes=False)

    if args.upload:
        issue = Git("cl", "issue")
        print(f"  {issue}")


def IsGitDirty():
    # Make sure there are no uncommitted changes in //third_party/rust,
    # including untracked files, because any untracked files might conflict
    # with new files that might need to be added.
    #
    # Since the roll script won't add new files outside //third_party/rust
    # though, ignore untracked changes there.
    if Git("status", "--porcelain", "third_party/rust") or Git(
            "status", "--porcelain", "--untracked-files=no"):
        return True
    else:
        return False


def RaiseErrorIfGitIsDirty():
    if IsGitDirty():
        raise RuntimeError("Dirty `git status` - save your local changes "\
                           "before rerunning the script")


def RaiseErrorIfCantUploadToGerrit():
    if shutil.which('gcertstatus'):
        RunCommandAndCheckForErrors(["gcertstatus", "--check_remaining=45m"],
                                    check_stdout=False,
                                    check_exitcode=True)


def CheckoutInitialBranch(branch):
    print(f"Checking out the `{branch}` branch...")
    RaiseErrorIfGitIsDirty()
    Git("checkout", branch)
    RaiseErrorIfGitIsDirty()

    # Ensure the //third_party/rust-toolchain version matches the branch.
    print("Running //tools/rust/update_rust.py (hopefully a no-op)...")
    RunCommandAndCheckForErrors([UPDATE_RUST_SCRIPT],
                                check_stdout=False,
                                check_exitcode=True)


def GitClUpload(*args):
    # `--bypass-hooks` to avoid disrupting the process of creating update CLs by
    # `//third_party/rust/PRESUBMIT.py` (e.g. it's okay to add
    # `multiversion_cleanup_bug` to `gnrt_config.toml` later, before landing the
    # CLs).
    #
    # `-o banned-words-skip` is used, because the CL is auto-generated and only
    # modifies third-party libraries (where any banned words would be purely
    # accidental; see also https://crbug.com/346174899).
    #
    # I am not 100% sure exactly why `--force` is needed, but without it
    # `git cl upload` hangs sometimes.  I am guessing that `--force` is needed
    # to suppress a prompt, although I am not sure what prompt + why that prompt
    # appears.
    Git("cl", "upload", "--bypass-hooks", "--force", "-o", "banned-words~skip",
        "--squash", *args)


def GitCommit(args, title, error_if_no_changes=True):
    if IsGitDirty():
        Git("commit", "-m", title)
        if args.upload:
            print(f"  Running `git cl upload ...` ...")
            GitClUpload("-m", title)
        return True
    else:
        if error_if_no_changes:
            raise RuntimeError(
                f"The '{title}' commit unexpectedly has no changes")
        else:
            print("    Nothing to commit")
            return False


def GetMissingCrates(args):
    missing_crates = []
    current_crate_ids = crate_utils.GetCurrentCrateIds()
    for needed_crate_id in args.remaining_args:
        if needed_crate_id.startswith("-"):
            # Assume this is a flag
            continue
        found = False
        for crate_id in current_crate_ids:
            if crate_id.startswith(needed_crate_id + "@"):
                found = True
                break
        if not found:
            missing_crates.append(needed_crate_id)
    return missing_crates


def BreakingUpdate(args):
    only_minor_updates = False

    # gnrt update
    old_crate_ids = crate_utils.GetCurrentCrateIds()
    print(f"Creating a major version update CL...")
    joined_remaining_args = ' '.join(args.remaining_args)
    print(f"  Running `gnrt update -- {joined_remaining_args}` ...")
    GnrtUpdate(args.remaining_args, check_stdout=True, check_exitcode=True)
    new_crate_ids = crate_utils.GetCurrentCrateIds()
    if old_crate_ids == new_crate_ids:
        print("  `gnrt update` resulted in no changes...")
        return
    diff = DiffCrateIds(old_crate_ids, new_crate_ids, only_minor_updates)
    title = CreateCommitTitleForBreakingUpdate(diff)
    description = CreateCommitDescription(title, diff)

    # Checkout a new git branch + `git cl upload`
    new_branch = f"{BRANCH_BASENAME}--major-version-update"
    Git("checkout", args.upstream_branch, "-b", new_branch)
    Git("branch", "--set-upstream-to", args.upstream_branch)
    GitAddRustFiles()
    Git("commit", "-m", description)
    if args.upload:
        print(f"  Running `git cl upload ...` ...")
        GitClUpload("--hashtag=cratesio-autoupdate",
                    "--cc=chrome-rust-experiments+autoupdate@google.com")

    FinishUpdatingCrate(args, title, diff)


def AutoUpdate(args):
    upstream_branch = args.upstream_branch
    CheckoutInitialBranch(upstream_branch)

    # Consider removing the check if upstream cargo detects this
    # problem. See https://github.com/rust-lang/cargo/issues/16258
    missing_crates = GetMissingCrates(args)
    if len(missing_crates) == 1:
        print("Missing crate:", missing_crates[0])
        return
    elif len(missing_crates) > 1:
        print("Missing crates:", ", ".join(missing_crates))
        return

    only_minor_updates = not DoArgsAskForBreakingChanges(args.remaining_args)
    if not only_minor_updates:
        # Major version updates shouldn't be split into smaller CLs - see
        # https://crbug.com/375012699#comment3.
        BreakingUpdate(args)
        return

    todo_crate_updates = FindUpdateableCrates(args)
    if args.skip:
        # Flatten a list of lists into `skip_patterns`:
        skip_patterns = list(itertools.chain.from_iterable(args.skip))
        skipped_crate_names = []
        todo_crate_updates_without_skips = []
        for old_crate_id, new_crate_id in todo_crate_updates:
            crate_name = crate_utils.ConvertCrateIdToCrateName(old_crate_id)
            if any(fnmatch.fnmatch(crate_name, p) for p in skip_patterns):
                skipped_crate_names.append(crate_name)
            else:
                todo_crate_updates_without_skips.append(
                    (old_crate_id, new_crate_id))
        todo_crate_updates = todo_crate_updates_without_skips
        print(f"Skipping the following crates because of `--skip`: " +
              f"{', '.join(skipped_crate_names)}")

    if not todo_crate_updates:
        print("There were no updates - exiting early...")
        return 0

    update_sizes = dict()
    for (old_crate_id, new_crate_id) in todo_crate_updates:
        update_sizes[old_crate_id] = FindSizeOfCrateUpdate(
            old_crate_id, new_crate_id, only_minor_updates)

    # Filter out crates that are not updateable on their own
    # (they need to be updated together with another crate).
    todo_crate_updates = [
        update for update in todo_crate_updates if update_sizes[update[0]] != 0
    ]

    # Start with small updates in an attempt to keep CLs small.
    todo_crate_updates = sorted(
        todo_crate_updates,
        key=lambda crate_update: update_sizes[crate_update[0]])

    print(f"** Updating {len(todo_crate_updates)} crates! "
          f"Expect this to take about {len(todo_crate_updates) * 2} minutes.")

    branch_number = 1
    while todo_crate_updates:
        old_crate_ids = crate_utils.GetCurrentCrateIds()
        for (old_crate_id, new_crate_id) in todo_crate_updates:
            upstream_branch = UpdateCrate(args, old_crate_id, new_crate_id,
                                          upstream_branch, branch_number)
            branch_number += 1

        new_crate_ids = crate_utils.GetCurrentCrateIds()
        diff = DiffCrateIds(old_crate_ids, new_crate_ids, only_minor_updates)
        actually_updated_crate_ids = set([u.old_crate_id for u in diff.updates])
        missed_crate_updates = [
            (old_crate_id, new_crate_id)
            for (old_crate_id, new_crate_id) in todo_crate_updates
            if old_crate_id not in actually_updated_crate_ids
        ]
        if missed_crate_updates:
            if len(missed_crate_updates) == len(todo_crate_updates):
                print("ERROR: Failed to make progress with these crates:")
                for (old_crate_id, new_crate_id) in todo_crate_updates:
                    print(f"  {old_crate_id} => {new_crate_id}")
                raise RuntimeError("Failed to make progress")
            else:
                print(f"** Retrying {len(missed_crate_updates)} crates.")
        todo_crate_updates = missed_crate_updates


def ManualUpdate(args):
    title = args.title

    RaiseErrorIfGitIsDirty()
    print(f"Post-processing a manual edit of `Cargo.toml`...")

    print(f"  Running `gnrt vendor` to detect `Cargo.lock` changes...")
    old_crate_ids = crate_utils.GetCurrentCrateIds()
    Gnrt("vendor")
    new_crate_ids = crate_utils.GetCurrentCrateIds()
    Git("reset", "--hard")
    Git("clean", "-d", "--force", "--", f"{THIRD_PARTY_RUST}")
    diff = DiffCrateIds(old_crate_ids, new_crate_ids, False)
    if diff.size() == 0:
        raise RuntimeError(
            "No changes in `Cargo.lock` after running `gnrt vendor`")

    # This covers most update steps: git mv, gnrt vendor, gnrt gen
    FinishUpdatingCrate(args, title, diff)

    if args.upload:
        print(f"  Running `git cl upload --commit-description=...` ...")
        description = CreateCommitDescription(title, diff)
        GitClUpload(f"--commit-description={description}", "-t",
                    "Edit CL description to include more info")


def main():
    parser = argparse.ArgumentParser(description="Update Rust crates")
    parser.add_argument("--no-upload",
                        dest='upload',
                        default=True,
                        action='store_false',
                        help="Avoids uploading CLs to Gerrit")
    parser.add_argument("--verbose", action='store_true')
    subparsers = parser.add_subparsers(required=False)

    parser_auto = subparsers.add_parser(
        "auto", description="Automatically update minor version of all crates")
    parser_auto.set_defaults(func=AutoUpdate)
    parser_auto.add_argument(
        "--upstream-branch",
        default="origin/main",
        help="The upstream branch on which to base the series of CLs.")
    parser_auto.add_argument("--skip",
                             default=[],
                             action="append",
                             nargs="+",
                             help="Skip updating this crate name.")
    parser_auto.add_argument("remaining_args",
                             nargs='*',
                             metavar='`cargo update` option',
                             help="Args to pass to `cargo update`")

    parser_manual = subparsers.add_parser(
        "manual",
        description="Generate update CL after manual edit of `Cargo.toml`")
    parser_manual.set_defaults(func=ManualUpdate)
    parser_manual.add_argument("--title",
                               required=True,
                               help="The first line of CL description.")

    args = parser.parse_args()
    if "func" not in args:
        msg = "ERROR: No auto/single/manual mode specified"
        print(msg)
        parser.print_help()
        raise RuntimeError(msg)
    if args.upload:
        RaiseErrorIfCantUploadToGerrit()

    global g_is_verbose
    g_is_verbose = args.verbose

    args.func(args)

    return 0


if __name__ == '__main__':
    sys.exit(main())
