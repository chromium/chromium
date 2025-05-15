#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This module contains utilities for working with information from
# `//third_party/rust/chromium_crates_io/Cargo.lock`.
#
# Throughout the module, the following naming conventions are used (illustrated
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

import os
import sys
import toml
from typing import Set

THIS_DIR = os.path.dirname(__file__)
CHROMIUM_DIR = os.path.normpath(os.path.join(THIS_DIR, '..', '..', '..'))
THIRD_PARTY_RUST_DIR = os.path.join(CHROMIUM_DIR, "third_party", "rust")
CRATES_DIR = os.path.join(THIRD_PARTY_RUST_DIR, "chromium_crates_io")
CARGO_LOCK_FILEPATH = os.path.join(CRATES_DIR, 'Cargo.lock')
VENDOR_DIR = os.path.join(CRATES_DIR, "vendor")


def GetCurrentCrateIds() -> Set[str]:
    """Parses Cargo.lock and returns a set of crate ids.

    Example return value: `set(["serde@1.0.197", "syn@2.0.50", ...])`.
    """
    with open(CARGO_LOCK_FILEPATH) as f:
        t = toml.load(f)
        result = set()
        for p in t["package"]:
            name = p["name"]
            version = p["version"]
            crate_id = f"{name}@{version}"
            assert crate_id not in result
            result.add(crate_id)
        return result


def ConvertCrateIdToCrateEpoch(crate_id: str) -> str:
    crate_version = ConvertCrateIdToCrateVersion(crate_id)
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


def ConvertCrateIdToBuildDir(crate_id: str) -> str:
    """ Converts a `crate_id` (e.g. "foo@1.2.3") into a path to an epoch dir.

    Example return value:
    `"<path to chromium root>\\third_party\\rust\\foo\\v1"`
    """
    return os.path.join(
        CHROMIUM_DIR, _ConvertCrateIdToBuildDirRelativeToChromiumRoot(crate_id))


def ConvertCrateIdToVendorDir(crate_id: str) -> str:
    """ Converts a `crate_id` (e.g. "foo@1.2.3") into a path to a target dir.

    Example return value:
    `"<path to chromium root>\\third_party\\rust\\chromium_crates_io\\vendor\\foo-v1"`
    """
    crate_name = ConvertCrateIdToCrateName(crate_id)
    crate_epoch = ConvertCrateIdToCrateEpoch(crate_id)
    crate_vendor_dir = os.path.join(VENDOR_DIR, f"{crate_name}-{crate_epoch}")
    return crate_vendor_dir


def ConvertCrateIdToGnLabel(crate_id: str) -> str:
    """ Converts a `crate_id` (e.g. "foo@1.2.3") into a GN label.

    See also
    https://gn.googlesource.com/gn/+/main/docs/reference.md#labels

    Example return value: `"//third_party/rust/foo/v1:lib"`
    """
    dir_name = _ConvertCrateIdToBuildDirRelativeToChromiumRoot(crate_id)
    dir_name = dir_name.replace(os.sep, "/")  # GN uses `/` as a path separator.
    return f"//{dir_name}:lib"


def _ConvertCrateIdToBuildDirRelativeToChromiumRoot(crate_id: str) -> str:
    """ Converts a `crate_id` (e.g. "foo@1.2.3") into an epoch dir.

    The returned dir is relative to Chromium root.
    Example return value: `"//third_party/rust/foo/v1"`
    """
    crate_name = ConvertCrateIdToCrateName(crate_id)
    crate_name = crate_name.replace("-", "_")
    epoch = ConvertCrateIdToCrateEpoch(crate_id)
    target = os.path.join("third_party", "rust", crate_name, epoch)
    return target


assert __name__ != '__main__'
