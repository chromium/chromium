# python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper functions for general tasks.

Helps for things such as generating versions, paths, and crate names."""

from __future__ import annotations

import os
import sys
from typing import Any, List

from lib import consts


def _find_chromium_root(cwd: str) -> list[str]:
    """Finds and returns the path from the root of the Chromium tree."""
    # This file is at tools/crates/lib/common.py, so 4 * '..' will take us up
    # to the root chromium dir.
    path_components_to_root = [os.path.abspath(__file__)] + [os.pardir] * 4
    abs_root_path = os.path.join(*path_components_to_root)
    path_from_root = os.path.relpath(cwd, abs_root_path)

    def split_path(p: str) -> list[str]:
        if not p: return []
        head, tail = os.path.split(p)
        tail_l = [] if tail == "." else [tail]
        return split_path(head) + tail_l

    return split_path(path_from_root)


# The path from the root of the chromium tree to the current working directory
# as a list of path components. If chromium's src.git is rooted at `/f/b/src``,
# and the this tool is run from `/f/b/src/in/there`, then the value here would
# be `["in", "there"]`. If the tool is run from the root `/f/b/src` then the
# value here is `[]`.
_PATH_FROM_CHROMIUM_ROOT = _find_chromium_root(os.getcwd())


def crate_name_normalized(crate_name: str) -> str:
    """Normalizes a crate name for GN and file paths."""
    return crate_name.replace("-", "_").replace(".", "_")


def version_is_complete(version_str: str) -> bool:
    """Returns whether the `version_str` is fully resolved or not.

    A full version includes MAJOR.MINOR.PATCH components."""
    parts = _version_to_parts(version_str)
    # This supports semmvers with pre-release and build flags.
    # https://semver.org/#backusnaur-form-grammar-for-valid-semver-versions
    return len(parts) >= 3


def _version_to_parts(version_str: str) -> list[str]:
    """Converts a version string into its MAJOR.MINOR.PATCH parts."""
    # TODO(danakj): This does not support pre-release or build versions such as
    # 1.0.0-alpha.1 or 1.0.0+1234 at this time. We only need support it if we
    # want to include such a crate in our tree.
    # https://semver.org/#backusnaur-form-grammar-for-valid-semver-versions
    # TODO(danakj): It would be real nice to introduce a SemmVer type instead of
    # using strings, which sometimes hold partial versions, and sometimes use
    # dots as separators or underscores.
    parts = version_str.split(".")
    assert len(parts) >= 1 and len(parts) <= 3, \
        "The version \"{}\" is an invalid semmver.".format(version_str)
    return parts


def version_epoch_dots(version_str: str) -> str:
    """Returns a version epoch from a given version string.

    Returns a string with `.` as the component separator."""
    parts = _version_to_parts(version_str)
    if parts[0] != "0":
        return ".".join(parts[:1])
    elif parts[1] != "0":
        return ".".join(parts[:2])
    else:
        return ".".join(parts[:3])


def version_epoch_normalized(version_str: str) -> str:
    """Returns a version epoch from a given version string.

    Returns a string with `_` as the component separator."""
    parts = _version_to_parts(version_str)
    if parts[0] != "0":
        return "_".join(parts[:1])
    elif parts[1] != "0":
        return "_".join(parts[:2])
    else:
        return "_".join(parts[:3])


def gn_third_party_path(rel_path: list[str] = []) -> str:
    """Returns the full GN path to the root of all third_party crates."""
    path = _PATH_FROM_CHROMIUM_ROOT + consts.THIRD_PARTY
    return "//" + "/".join(path + rel_path)


def gn_crate_path(crate_name: str, version: str,
                  rel_path: list[str] = []) -> str:
    """Returns the full GN path to a crate that is in third_party. This is the
    path to the crate's BUILD.gn file."""
    name = crate_name_normalized(crate_name)
    epoch = "v" + version_epoch_normalized(version)
    path = _PATH_FROM_CHROMIUM_ROOT + consts.THIRD_PARTY + [name, epoch]
    return "//" + "/".join(path + rel_path)


def os_third_party_dir(rel_path: list[str] = []) -> str:
    """The relative OS disk path to the top of the third party Rust directory
    where all third party crates are found, along with third_party.toml."""
    return os.path.join(*consts.THIRD_PARTY, *rel_path)


def os_crate_name_dir(crate_name: str, rel_path: list[str] = []) -> str:
    """The relative OS disk path to a third party crate's top-most directory
    where all versions of that crate are found."""
    return os_third_party_dir(rel_path=[crate_name_normalized(crate_name)] +
                              rel_path)


def os_crate_version_dir(crate_name: str,
                         version: str,
                         rel_path: list[str] = []) -> str:
    """The relative OS disk path to a third party crate's versioned directory
    where BUILD.gn and README.chromium are found."""
    epoch = "v" + version_epoch_normalized(version)
    return os_crate_name_dir(crate_name, rel_path=[epoch] + rel_path)


def os_crate_cargo_dir(crate_name: str, version: str,
                       rel_path: list[str] = []) -> str:
    """The relative OS disk path to a third party crate's Cargo root.

    This directory is where Cargo.toml and the Rust source files are found. This
    is where the crate is extracted when it is downloaded."""
    return os_crate_version_dir(crate_name,
                                version,
                                rel_path=consts.CRATE_INNER_DIR + rel_path)


def crate_download_url(crate: str, version: str) -> str:
    """Returns the crates.io URL to download the crate."""
    return consts.CRATES_IO_DOWNLOAD.format(crate=crate, version=version)


def crate_view_url(crate: str) -> str:
    """Returns the crates.io URL to see info about the crate."""
    return consts.CRATES_IO_VIEW.format(crate=crate)


def load_toml(path: str) -> dict[str, Any]:
    """Loads a file at the path and parses it as a TOML file.

    This is a helper for times when you don't need the raw text content of the
    TOML file.

    Returns:
        A dictionary of the contents of the TOML file."""
    with open(path, "r") as cargo_file:
        toml_string = cargo_file.read()
        import toml
        return dict(toml.loads(toml_string))


def print_same_line(s: str, fill_num_chars: int, done: bool = False) -> int:
    """A helper to repeatedly print to the same line.

    Args:
        s: The text to be printed.
        fill_num_chars: This should be `0` on the first call to
          print_same_line() for a series of prints to the same output line. Then
          it should be the return value of the previous call to
          print_same_line() repeatedly until `done` is True, at which time the
          cursor will be moved to the next output line.
        done: On the final call to print_same_line() for a given line of output,
          pass `True` to have the cursor move to the next line.

    Returns:
        The number of characters that were written, which should be passed as
        `fill_num_chars` on the next call. At the end of printing over the same
        line, finish by calling with `done` set to true, which will move to the
        next line."""
    s += " " * (fill_num_chars - len(s))
    if not done:
        print("\r" + s, end="")
    else:
        print("\r" + s)
    return len(s)
