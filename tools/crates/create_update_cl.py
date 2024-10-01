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
import shlex
import subprocess
import sys
import textwrap
import toml
from dataclasses import dataclass
from typing import List, Set, Dict

# Throughout the script, the following naming conventions are used (illustrated
# with a crate named `syn` and published as version `2.0.50`):
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

THIS_DIR = os.path.dirname(__file__)
CHROMIUM_DIR = os.path.normpath(os.path.join(THIS_DIR, '..', '..'))
THIRD_PARTY_RUST = os.path.join(CHROMIUM_DIR, "third_party", "rust")
CRATES_DIR = os.path.join(THIRD_PARTY_RUST, "chromium_crates_io")
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


def GetCurrentCrateIds() -> Set[str]:
    """Parses Cargo.lock and returns a set of crate ids
    (e.g. "serde@1.0.197", "syn@2.0.50", ...)."""
    t = toml.load(open(CARGO_LOCK))
    result = set()
    for p in t["package"]:
        name = p["name"]
        version = p["version"]
        crate_id = f"{name}@{version}"
        assert crate_id not in result
        result.add(crate_id)
    return result


def GetVetExemptedCrateIds() -> Set[str]:
    """Parses supply-chain/config.toml and returns a set of crate ids
    that have `exemptions` entries."""
    vet_config = toml.load(open(VET_CONFIG))
    result = set()
    if "exemptions" not in vet_config:
        return result
    for crate_name, exemptions in vet_config["exemptions"].items():
        for exemption in exemptions:
            # Ignoring which criteria are covered by the exemption
            # (it is not needed if we just want to emit a warning).
            crate_version = exemption["version"]
            crate_id = f"{crate_name}@{crate_version}"
            result.add(crate_id)
    return result


@dataclass(eq=True, order=True)
class UpdatedCrate:
    old_crate_id: str
    new_crate_id: str

    def __str__(self):
        name = ConvertCrateIdToCrateName(self.old_crate_id)
        assert name == ConvertCrateIdToCrateName(self.new_crate_id)

        old_version = ConvertCrateIdToCrateVersion(self.old_crate_id)
        new_version = ConvertCrateIdToCrateVersion(self.new_crate_id)

        return f"{name}: {old_version} => {new_version}"


@dataclass
class CratesDiff:
    updates: List[UpdatedCrate]
    removed_crate_ids: List[str]
    added_crate_ids: List[str]

    def size(self):
        return len(self.updates) + len(self.added_crate_ids) + len(
            self.removed_crate_ids)


def DiffCrateIds(old_crate_ids: Set[str],
                 new_crate_ids: Set[str],
                 only_minor_updates=True) -> CratesDiff:
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
            name = ConvertCrateIdToCrateName(crate_id)
            version = ConvertCrateIdToCrateVersion(crate_id)
            if only_minor_updates:
                epoch = GetEpoch(version)
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


def GetEpoch(crate_version: str) -> str:
    v = crate_version.split('.')
    if v[0] == '0':
        return f'v0_{v[1]}'
    return f'v{v[0]}'


def ConvertCrateIdToCrateName(crate_id: str) -> str:
    """ Converts a `crate_id` into a `crate_name`."""
    return crate_id[:crate_id.find("@")]


def ConvertCrateIdToCrateVersion(crate_id: str) -> str:
    """ Converts a `crate_id` into a `crate_version`."""
    crate_version = crate_id[crate_id.find("@") + 1:]
    return crate_version


def _ConvertCrateIdToEpochDirRelativeToChromiumRoot(crate_id: str) -> str:
    """ Converts a `crate_id` (e.g. "foo@1.2.3") into an epoch dir
    (e.g. "third_party/rust/foo/v_1").  The returned dir is relative
    to Chromium root. """
    crate_name = ConvertCrateIdToCrateName(crate_id)
    crate_name = crate_name.replace("-", "_")
    epoch = GetEpoch(ConvertCrateIdToCrateVersion(crate_id))
    target = os.path.join("third_party", "rust", crate_name, epoch)
    return target


def ConvertCrateIdToEpochDir(crate_id: str) -> str:
    """ Converts a `crate_id` (e.g. "foo@1.2.3") into a path to an epoch dir
    (e.g. on Windows: "<path to chromium root>\\third_party\\rust\\foo\\v_1").
    """
    return os.path.join(
        CHROMIUM_DIR, _ConvertCrateIdToEpochDirRelativeToChromiumRoot(crate_id))


def ConvertCrateIdToVendorDir(crate_id: str) -> str:
    """ Converts a `crate_id` (e.g. "foo@1.2.3") into a path to a target dir
    (e.g. on Windows: "<path to chromium root>\\third_party\\rust\\foo\\v_1").
    """
    crate_name = ConvertCrateIdToCrateName(crate_id)
    crate_version = ConvertCrateIdToCrateVersion(crate_id)
    crate_vendor_dir = os.path.join(VENDOR_DIR, f"{crate_name}-{crate_version}")
    return crate_vendor_dir


def ConvertCrateIdToGnLabel(crate_id: str) -> str:
    """ Converts a `crate_id` (e.g. "foo@1.2.3") into
    [a GN label](https://gn.googlesource.com/gn/+/main/docs/reference.md#labels)
    (e.g. "//third_party/rust/foo/v_1:lib").  """
    dir_name = _ConvertCrateIdToEpochDirRelativeToChromiumRoot(crate_id)
    dir_name = dir_name.replace(os.sep, "/")  # GN uses `/` as a path separator.
    return f"//{dir_name}:lib"


def FindUpdateableCrates() -> List[str]:
    """Runs `gnrt update` and returns a `list` of old crate ids (e.g.
    "syn@2.0.50") that can be updated to a new version.  (Idempotent -
    afterwards it runs `git reset --hard` to undo any changes.)"""
    print("Checking which crates can be updated...")
    assert not Git("status", "--porcelain")  # No local changes expected here.
    old_crate_ids = GetCurrentCrateIds()
    Gnrt("update")
    new_crate_ids = GetCurrentCrateIds()
    Git("reset", "--hard")
    diff = DiffCrateIds(old_crate_ids, new_crate_ids)
    crate_ids = [update.old_crate_id for update in diff.updates]
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
    old_crate_ids = GetCurrentCrateIds()
    Gnrt("update", crate_id)
    new_crate_ids = GetCurrentCrateIds()
    Git("reset", "--hard")
    diff = DiffCrateIds(old_crate_ids, new_crate_ids)
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

    description = \
"""Chromium `supply-chain/config.toml` policy requires that the following
audit criteria are met (note that these are the *minimum* required
criteria and `supply-chain/audits.toml` can and should record a stricter
certification if possible;  see also //docs/rust-unsafe.md):

"""
    description += SortedMarkdownList(items)

    return description


def CreateCommitTitle(old_crate_id: str, diff: CratesDiff) -> str:
    crate_name = ConvertCrateIdToCrateName(old_crate_id)
    old_version = ConvertCrateIdToCrateVersion(old_crate_id)
    update = [u for u in diff.updates if u.old_crate_id == old_crate_id][0]
    update = next(filter(lambda u: u.old_crate_id == old_crate_id,
                         diff.updates))
    new_version = ConvertCrateIdToCrateVersion(update.new_crate_id)
    roll_summary = f"{crate_name}: " + \
        f"{old_version} => {new_version}"
    title = f"Roll {roll_summary} in //third_party/rust."
    return title


def CreateCommitDescription(title: str, diff: CratesDiff,
                            include_vet_criteria: bool) -> str:
    description = f"""{title}

This CL has been created semi-automatically.  The expected review
process and other details can be found at
//tools/crates/create_update_cl.md
"""

    update_descriptions = SortedMarkdownList(
        [str(update) for update in diff.updates])
    new_crate_descriptions = SortedMarkdownList(diff.added_crate_ids)
    removed_crate_descriptions = SortedMarkdownList(diff.removed_crate_ids)
    assert (update_descriptions)
    description += f"\nUpdated crates:\n\n{update_descriptions}\n"
    if new_crate_descriptions:
        description += f"\nNew crates:\n\n{new_crate_descriptions}\n"
    if removed_crate_descriptions:
        description += f"\nRemoved crates:\n\n{removed_crate_descriptions}\n"

    new_or_updated_crate_ids = diff.added_crate_ids + \
        [update.new_crate_id for update in diff.updates]
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
    old_crate_ids = GetCurrentCrateIds()
    print(f"  Running `gnrt update {crate_id}` ...")
    Gnrt("update", crate_id)
    new_crate_ids = GetCurrentCrateIds()
    if old_crate_ids == new_crate_ids:
        print("  `gnrt update` resulted in no changes - "\
              "maybe other steps will handle this crate...")
        return upstream_branch
    diff = DiffCrateIds(old_crate_ids, new_crate_ids)
    title = CreateCommitTitle(crate_id, diff)
    description = CreateCommitDescription(title, diff, False)

    # Checkout a new git branch + `git cl upload`
    new_branch = f"{BRANCH_BASENAME}--{crate_id.replace('@', '-')}"
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
    vet_exempted_crate_ids = GetVetExemptedCrateIds()
    updated_old_crate_ids = set()

    # git mv <vendor/old version> <vendor/new version>
    print(f"  Running `git mv <old dir> <new dir>` (for better diff)...")
    for update in diff.updates:
        updated_old_crate_ids.add(update.old_crate_id)

        old_dir = ConvertCrateIdToVendorDir(update.old_crate_id)
        new_dir = ConvertCrateIdToVendorDir(update.new_crate_id)
        Git("mv", "--force", f"{old_dir}", f"{new_dir}")

        old_target_dir = ConvertCrateIdToEpochDir(update.old_crate_id)
        new_target_dir = ConvertCrateIdToEpochDir(update.new_crate_id)
        if old_target_dir != new_target_dir:
            Git("mv", "--force", old_target_dir, new_target_dir)
    GitAddRustFiles()
    GitCommit(args, "git mv <old dir> <new dir> (for better diff)")
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
    if args.upload:
        print(f"  Running `git cl upload --commit-description=...` ...")
        description = CreateCommitDescription(title, diff, True)
        GitClUpload(f"--commit-description={description}", "-t",
                    "Edit CL description to include vet policy")

    # gnrt gen
    print(f"  Running `gnrt gen`...")
    Gnrt("gen")
    # Some crates (e.g. ones in the `remove_crates` list of `gnrt_config.toml`)
    # may result in no changes - this is why we have an `if` below...
    if Git("status", "--porcelain"):
        GitAddRustFiles()
        GitCommit(args, "gnrt gen")

    if args.upload:
        issue = Git("cl", "issue")
        print(f"  {issue}")

    for exempted_crate_id in (
            updated_old_crate_ids.intersection(vet_exempted_crate_ids)):
        exempted_crate_name = ConvertCrateIdToCrateName(exempted_crate_id)
        print(f"  WARNING: The `{exempted_crate_name}` crate "\
               "is covered by an exemption rather than an audit. "\
               "Please bump the exemption in `vet_config.toml.hbs` "\
               "and run `tools/crates/run_gnrt.py vendor` again.")


def IsGitDirty():
    if Git("status", "--porcelain"):
        return True
    else:
        return False


def RaiseErrorIfGitIsDirty():
    if IsGitDirty():
        raise RuntimeError("Dirty `git status` - save you local changes "\
                           "before rerunning the script")


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
    # `--bypass-hooks` because the uploaded CL will initially fail
    # `tools/crates/run_cargo_vet.py check`.
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
        *args)


def GitCommit(args, title, error_if_no_changes=True):
    if IsGitDirty():
        Git("commit", "-m", title)
        if args.upload:
            print(f"  Running `git cl upload ...` ...")
            GitClUpload("-m", title)
    else:
        if error_if_no_changes:
            raise RuntimeError(
                f"The '%title' commit unexpectedly has no changes")


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

    todo_crate_ids = FindUpdateableCrates()

    if args.skip:
        todo_crate_ids = list([
            crate_id for crate_id in todo_crate_ids
            if not crate_id.split("@")[0] in args.skip
        ])

    if not todo_crate_ids:
        print("There were no updates - exiting early...")
        return 0

    update_sizes = dict()
    for crate_id in todo_crate_ids:
        update_sizes[crate_id] = FindSizeOfCrateUpdate(crate_id)

    todo_crate_ids = sorted(todo_crate_ids,
                            key=lambda crate_id: update_sizes[crate_id])
    print(f"** Updating {len(todo_crate_ids)} crates! "
          f"Expect this to take about {len(todo_crate_ids) * 2} minutes.")
    while todo_crate_ids:
        old_crate_ids = GetCurrentCrateIds()
        for crate_id in todo_crate_ids:
            upstream_branch = UpdateCrate(args, crate_id, upstream_branch)

        new_crate_ids = GetCurrentCrateIds()
        diff = DiffCrateIds(old_crate_ids, new_crate_ids)
        actually_updated_crate_ids = set([u.old_crate_id for u in diff.updates])
        missed_crate_ids = [
            crate_id for crate_id in todo_crate_ids
            if crate_id not in actually_updated_crate_ids
        ]
        if missed_crate_ids:
            if len(missed_crate_ids) == len(todo_crate_ids):
                print("ERROR: Failed to make progress with these crates:")
                print(f"{', '.join(todo_crate_ids)}")
                raise RuntimeError("Failed to make progress")
            else:
                print(f"** Retrying {len(missed_crate_ids)} crates.")
        todo_crate_ids = missed_crate_ids


def SingleCrate(args):
    upstream_branch = args.upstream_branch
    CheckoutInitialBranch(upstream_branch)

    crate_id = ResolveCrateNameToCrateId(args.crate[0])
    UpdateCrate(args, crate_id, upstream_branch)


def ManualUpdate(args):
    title = args.title

    RaiseErrorIfGitIsDirty()
    print(f"Post-processing a manual edit of `Cargo.toml`...")

    print(f"  Running `gnrt vendor` to detect `Cargo.lock` changes...")
    old_crate_ids = GetCurrentCrateIds()
    Gnrt("vendor")
    new_crate_ids = GetCurrentCrateIds()
    Git("reset", "--hard")
    Git("clean", "-d", "--force", "--", f"{THIRD_PARTY_RUST}")
    diff = DiffCrateIds(old_crate_ids, new_crate_ids, only_minor_updates=False)
    if diff.size() == 0:
        raise RuntimeError(
            "No changes in `Cargo.lock` after running `gnrt vendor`")

    # This covers most update steps: git mv, gnrt vendor, gnrt gen
    FinishUpdatingCrate(args, title, diff)

    # Remove old `//third_party/rust/foo/v<old>` directories
    print(f"  Removing //third_party/rust/.../<old_epoch> ...")
    for update in diff.updates:
        old_target_dir = ConvertCrateIdToEpochDir(update.old_crate_id)
        new_target_dir = ConvertCrateIdToEpochDir(update.new_crate_id)
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
        # Just skip this commit when `manual` mode is used in
        # a scenario that *only* performs minor version updates.
        error_if_no_changes=False)

    # Fix up the target names
    print(f"  Updating the target name in BUILD.gn files...")
    for update in diff.updates:
        old_target = ConvertCrateIdToGnLabel(update.old_crate_id)
        new_target = ConvertCrateIdToGnLabel(update.new_crate_id)
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
        # Just skip this commit when `manual` mode is used in
        # a scenario that *only* performs minor version updates.
        error_if_no_changes=False)


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
                             nargs="+",
                             help="Skip updating this crate name.")

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

    global g_is_verbose
    g_is_verbose = args.verbose

    args.func(args)

    return 0


if __name__ == '__main__':
    sys.exit(main())
