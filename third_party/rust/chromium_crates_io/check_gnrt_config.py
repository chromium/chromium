#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script checks for issues in:
# * `//third_party/rust/chromium_crates_io/Cargo.lock` and
# * `//third_party/rust/chromium_crates_io/gnrt_config.toml`.
#
# We don't surface these issues earlier (by reporting a fatal error from `gnrt
# vendor`, `gnrt gen`, `gn gen`, or failing the builds), because we want to
# avoid friction when new teams experiment with using Rust.  Some of these
# issues may also happen during the crate update rotation and in this case we
# want to allow the `tools/crate/create_update_cl.py` to continue creating CLs
# (that other script uses `git cl upload ... --bypass-hooks`).
#
# This script is typically not invoked directly, but instead is invoked as part
# of `//third_party/rust/PRESUBMIT.py`

import os
import sys
import toml

import crate_utils

CRATES_DIR = os.path.normpath(os.path.dirname(__file__))
GNRT_CONFIG_PATH = os.path.join(CRATES_DIR, 'gnrt_config.toml')


def _GetCrateConfigForCrateName(crate_name, gnrt_config):
    if gnrt_config and isinstance(gnrt_config, dict):
        crates = gnrt_config.get("crate")
        if crates and isinstance(crates, dict):
            crate_cfg = crates.get(crate_name)
            if crate_cfg and isinstance(crate_cfg, dict):
                return crate_cfg
    return dict()


def _GetExtraKvForCrateName(crate_name, gnrt_config):
    crate_cfg = _GetCrateConfigForCrateName(crate_name, gnrt_config)
    extra_kv = crate_cfg.get("extra_kv")
    if extra_kv and isinstance(extra_kv, dict):
        return extra_kv
    return dict()


def CheckExplicitAllowUnsafeForAllCrates(crate_ids, gnrt_config):
    """Checks that `gnrt_config.toml` has `allow_unsafe = ...` for each crate.

       Returns an error message if a problem is detected.
       Returns an empty string if there are no problems.
    """
    result = []
    for crate_id in sorted(crate_ids):
        crate_name = crate_utils.ConvertCrateIdToCrateName(crate_id)

        # Ignore the root package and placeholder crates.
        if crate_name == "chromium": continue
        if crate_utils.IsPlaceholderCrate(crate_id): continue

        # Ignore crates that specify `allow_unsafe`.
        extra_kv = _GetExtraKvForCrateName(crate_name, gnrt_config)
        if "allow_unsafe" in extra_kv:
            continue

        # Report a problem for all other crates.
        if not result:  # Is is the **first** problematic `crate_name`?
            result.append("ERROR: Please ensure that `gnrt_config.toml` "
                          "explicitly specifies `allow_unsafe = ...` for all "
                          "crates that `chromium_crates_io` depends on.  "
                          "This helps `//third_party/rust/OWNERS` to check at "
                          "a glance if a given crate contains `unsafe` Rust "
                          "code.")
            result.append("")
        result += [
            f"    [crate.{crate_name}.extra_kv]",
            f"    allow_unsafe = false (or true if needed)",
        ]

    return "\n".join(result)

def CheckMultiversionCrates(crate_ids, gnrt_config):
    """Checks that a bug tracks each crate with multiple versions.

       This check has been discussed in https://crbug.com/404867240.  Having 2
       or more different versions of a crate in Chromium's dependency tree is
       undesirable in general.  So we want to detect when a 2nd version is
       imported, and require opening a bug + recording the bug in
       `gnrt_config.toml` for the given crate.

       Returns an error message if a problem is detected.
       Returns an empty string if there are no problems.
    """

    # Group `crate_id`s by their `crate_name`.
    crate_name_to_list_of_crate_ids = dict()
    for crate_id in crate_ids:
        crate_name = crate_utils.ConvertCrateIdToCrateName(crate_id)
        if crate_name not in crate_name_to_list_of_crate_ids:
            crate_name_to_list_of_crate_ids[crate_name] = []
        crate_name_to_list_of_crate_ids[crate_name] += [crate_id]

    result = []
    for (crate_name, crate_ids) in crate_name_to_list_of_crate_ids.items():
        # Ignore crates where we depend only on a single version.
        if len(crate_ids) == 1:
            continue

        # Ignore crates that already have a bug to track cleaning up a
        # multiversion situation.
        extra_kv = _GetExtraKvForCrateName(crate_name, gnrt_config)
        if "multiversion_cleanup_bug" in extra_kv:
            continue

        # Report a problem for other multiversion crates.
        if not result:  # Is is the **first** problematic `crate_name`?
            result.append("ERROR: Transitive dependency graph includes "
                          "multiple versions of the same crate.  Please "
                          "open a bug to track removing one of the "
                          "versions and put a link to the bug into "
                          "`gnrt_config.toml` like this:")
            result.append("")

        result += [
            f"    # TODO: Remove multiple versions of the `{crate_name}` crate:",
            f"    # {', '.join(sorted(crate_ids))}",
            f"    [crate.{crate_name}.extra_kv]",
            f'    multiversion_cleanup_bug = "https://crbug.com/<bug number>"\n',
        ]

    return "\n".join(result)


def main():
    crate_ids = crate_utils.GetCurrentCrateIds()
    gnrt_config = toml.load(open(GNRT_CONFIG_PATH))

    success = True

    def RunChecks(check_impl):
        nonlocal success
        nonlocal crate_ids
        nonlocal gnrt_config
        result = check_impl(crate_ids, gnrt_config)
        if result:
            if not success:
                # Add a separator if this is a 2nd, 3rd, or later problem.
                print()
                print("-" * 72)
                print()
            success = False
            print(result)

    RunChecks(CheckMultiversionCrates)
    RunChecks(CheckExplicitAllowUnsafeForAllCrates)

    # TODO(https://crbug.com/399935219): RunChecks(CheckNonapplicableGnrtConfigEntries).
    # Move the useless-entry-in-gnrt_config check from `gnrt` into this script.
    # Consider also adding CheckNonapplicablePatches.

    if success:
        return 0
    else:
        return -1


if __name__ == '__main__':
    sys.exit(main())
