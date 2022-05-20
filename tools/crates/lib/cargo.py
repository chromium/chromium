# python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provide abstractions and helpers for the Rust Cargo build tool.

In addition to data types representing Cargo concepts, this module has helpers
for parsing and generating Cargo.toml files, for running the Cargo tool, and for
parsing its output."""

from __future__ import annotations

from enum import Enum
import os
import subprocess
import sys
import toml
import typing
from typing import Any, Optional

from lib import common
from lib import consts


class CrateKey:
    """A unique identifier for any given third-party crate.

    This is a combination of the crate's name and its epoch, since we have at
    most one crate for a given epoch.

    The name and version/epoch are directly from the crate and are not
    normalized.
    """

    def __init__(self, name: str, version: str):
        self.name = name
        self.epoch = common.version_epoch_dots(version)

    def __repr__(self) -> str:
        return "CrateKey({} v{})".format(self.name, self.epoch)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, CrateKey):
            return NotImplemented
        return self.name == other.name and self.epoch == other.epoch

    def __hash__(self) -> int:
        return hash("{}: {}".format(self.name, self.epoch))


class CrateUsage(Enum):
    """The ways that a crate's library can be used from other crates."""
    FOR_NORMAL = 1,  # Used from another crate's lib/binary outputs.
    FOR_BUILDRS = 2,  # Used from another crates's build.rs.
    FOR_TESTS = 3,  # Used from another crate's tests.

    def gn_target_name(self) -> str:
        """The name to use for a gn target.

        This is the name of the target used for generating the target in the
        BUILD.gn file. The name is based on how the target will be used, since
        crates have different features enabled when being built for use in
        tests, or for use from a build.rs build script."""
        if self == CrateUsage.FOR_NORMAL:
            return CrateBuildOutput.NORMAL.gn_target_name_for_dep()
        elif self == CrateUsage.FOR_BUILDRS:
            return CrateBuildOutput.BUILDRS.gn_target_name_for_dep()
        elif self == CrateUsage.FOR_TESTS:
            return CrateBuildOutput.TESTS.gn_target_name_for_dep()
        else:
            return NotImplemented


class CrateBuildOutput(Enum):
    """The various build outputs when building a crate."""
    NORMAL = 1  # Building the crate's normal output.
    BUILDRS = 2  # Building the crate's build.rs.
    TESTS = 3  # Building the crate's tests.

    def as_dep_usage(self) -> CrateUsage:
        if self == CrateBuildOutput.NORMAL:
            return CrateUsage.FOR_NORMAL
        elif self == CrateBuildOutput.BUILDRS:
            return CrateUsage.FOR_BUILDRS
        elif self == CrateBuildOutput.TESTS:
            return CrateUsage.FOR_TESTS
        else:
            assert False  # Unhandled CrateBuildOutput?

    def gn_target_name_for_dep(self):
        """The name to use for gn dependency targets.

        This is the name of the target to use for a dependency in the `deps`,
        `build_deps`, or `dev_deps` section of a BUILD.gn target. The name
        depends on what kind of dependency it is, since crates have different
        features enabled when being built for use in tests, or for use from a
        build.rs build script."""
        if self == CrateBuildOutput.NORMAL:
            return "lib"
        if self == CrateBuildOutput.BUILDRS:
            return "buildrs_support"
        if self == CrateBuildOutput.TESTS:
            return "test_support"

    def _cargo_tree_edges(self) -> str:
        """Get the argument for `cargo tree --edges`

        Returns what to pass to the --edges argument when running `cargo tree`
        to see the dependencies of a given build output."""
        if self == CrateBuildOutput.NORMAL:
            return "normal"
        elif self == CrateBuildOutput.BUILDRS:
            return "build"
        elif self == CrateBuildOutput.TESTS:
            return "dev"
        else:
            return NotImplemented


def run_cargo_tree(path: str, build: CrateBuildOutput,
                   target_arch: Optional[str], depth: Optional[int],
                   features: list) -> list[str]:
    """Runs `cargo tree` on the Cargo.toml file at `path`.

    Note that `cargo tree` actually invokes `rustc` a bunch to collect its
    output, but it does not appear to actually compile anything. Additionally,
    we are running `cargo tree` in a temp directory with placeholder rust files
    present to satisfy `cargo tree`, so no source code from crates.io should
    be compiled, or run, by this tool.

    Args:
        target_arch: one of the ALL_RUSTC_ARCH which are targets understood by
          rustc, and shown by `rustc --print target-list`. Or none, in which
          case the current machine's architecture is used.

    Returns:
        The output of cargo tree, with split by lines into a list.
    """
    tree_cmd = [
        "cargo",
        "tree",
        "--manifest-path",
        path,
        "--edges",
        build._cargo_tree_edges(),
        "--format={p} {f}",
        "-v",
    ]
    if target_arch:
        tree_cmd += ["--target", target_arch]
    if depth is not None:
        tree_cmd += ["--depth", str(depth)]
    if "default" not in features:
        tree_cmd += ["--no-default-features"]
    features = [f for f in features if not f == "default"]
    if features:
        tree_cmd += ["--features", ",".join(features)]
    try:
        r = subprocess.check_output(tree_cmd, text=True, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as e:
        print()
        print(' '.join(tree_cmd))
        print(e.stderr)
        raise e
    return r.splitlines()


def add_required_cargo_fields(toml_3p):
    """Add required fields for a Cargo.toml to be parsed by `cargo tree`."""
    toml_3p["package"] = {
        "name": "chromium",
        "version": "1.0.0",
    }
    return toml_3p


class ListOf3pCargoToml:
    """A typesafe cache of info about local third-party Cargo.toml files."""

    class CargoToml:
        def __init__(self, name: str, epoch: str, path: str):
            self.name = name
            self.epoch = epoch
            self.path = path

    def __init__(self, list_of: list[CargoToml]):
        self._list_of = list_of


def write_cargo_toml_in_tempdir(
        dir: str,
        all_3p_tomls: ListOf3pCargoToml,
        orig_toml_parsed: Optional[dict[str, Any]] = None,
        orig_toml_path: Optional[str] = None,
        verbose: bool = False) -> str:
    """Write a temporary Cargo.toml file that will work with `cargo tree`.

    Creates a copy of a Cargo.toml, specified in `orig_toml_path`, in to the
    temp directory specified by `dir` and sets up the temp dir so that running
    `cargo` will succeed. Also points all crates named in `all_3p_tomls` to
    the downloaded versions.

    Exactly one of `orig_toml_parsed` or `orig_toml_path` must be specified.

    Args:
        dir: An OS path to a temp directory where the Cargo.toml file is to be
          written.
        all_3p_tomls: A cache of local third-party Cargo.toml files, crated by
          gen_list_of_3p_cargo_toml(). The generated Cargo.toml will be patched
          to point `cargo tree` to local Cargo.tomls for dependencies in order
          to see local changes.
        orig_toml_parsed: The Cargo.toml file contents to write, as a
          dictionary.
        orig_toml_path: An OS path to the Cargo.toml file which should be copied
          into the output Cargo.toml.
        verbose: Whether to print verbose output, including the full TOML
          content.

    Returns:
        The OS path to the output Cargo.toml file in `dir`, for convenience.
    """
    assert bool(orig_toml_parsed) ^ bool(orig_toml_path)
    orig_toml_text: Optional[str] = None
    if orig_toml_path:
        with open(orig_toml_path, "r") as f:
            orig_toml_text = f.read()
        orig_toml_parsed = dict(toml.loads(orig_toml_text))

    # This assertion is necessary for type checking. Now mypy deduces
    # orig_toml_parsed's type as dict[str, Any] instead of Optional[...]
    assert orig_toml_parsed is not None

    orig_name = orig_toml_parsed["package"]["name"]
    orig_epoch = common.version_epoch_dots(
        orig_toml_parsed["package"]["version"])

    if all_3p_tomls is None:
        all_3p_tomls = ListOf3pCargoToml([])

    # Since we're putting a Cargo.toml in a temp dir, cargo won't be
    # able to find the src/lib.rs and will bail out, so we make it.
    os.mkdir(os.path.join(dir, "src"))
    with open(os.path.join(dir, "src", "lib.rs"), mode="w") as f:
        f.write("lib.rs")
    # Same thing for build.rs, as some Cargo.toml flags make it go looking
    # for a build script to verify it exists.
    if not "build" in orig_toml_parsed["package"]:
        with open(os.path.join(dir, "build.rs"), mode="w") as f:
            f.write("build.rs")
    # And [[bin]] targets, if they have a name but no path, expect to
    # find a file at src/bin/%name%.rs or at src/main.rs, though when
    # one is preferred is unclear. It seems to always work with the
    # former one though, but not always with the latter.
    if "bin" in orig_toml_parsed:
        os.mkdir(os.path.join(dir, "src", "bin"))
        for bin in orig_toml_parsed["bin"]:
            if "path" not in bin and "name" in bin:
                with open(os.path.join(dir, "src", "bin",
                                       "{}.rs".format(bin["name"])),
                          mode="w") as f:
                    f.write("bin main.rs")
    # Workspaces in a crate's Cargo.toml need to point to other Cargo.toml files
    # on disk, and those Cargo.toml files require a lib or binary source as
    # well. We don't support building workspaces, but cargo will die if it can't
    # find them.
    if "workspace" in orig_toml_parsed:
        for m in orig_toml_parsed["workspace"].get("members", []):
            workspace_dir = os.path.join(dir, *(m.split("/")))
            os.makedirs(workspace_dir)
            with open(os.path.join(workspace_dir, "Cargo.toml"), mode="w") as f:
                f.write(consts.FAKE_EMPTY_CARGO_TOML)
            bin_dir = os.path.join(workspace_dir, "src", "bin")
            os.makedirs(bin_dir)
            with open(os.path.join(bin_dir, "main.rs"), mode="w") as f:
                f.write("workspace {} bin main.rs".format(m))

    # Generate a patch that points the current crate, to the temp dir, and all
    # others to `consts.THIRD_PARTY`. This is to deal with build/dev deps that
    # transitively depend back on the current crate. Otherwise it gets seen in
    # 2 paths.
    patch: dict[str, Any] = {"patch": {"crates-io": {}}}
    cwd = os.getcwd()
    for in_3p in all_3p_tomls._list_of:
        if in_3p.name == orig_name and in_3p.epoch == orig_epoch:
            # If this is the crate we're creating a temp Cargo.toml for, point
            # the patch to the temp dir.
            abspath = dir
        else:
            # Otherwise, point the patch to the downloaded third-party crate's
            # dir.
            abspath = os.path.join(cwd, in_3p.path)
        patch_name = ("{}_v{}".format(
            in_3p.name, common.version_epoch_normalized(in_3p.epoch)))
        patch["patch"]["crates-io"][patch_name] = {
            "version": in_3p.epoch,
            "path": abspath,
            "package": in_3p.name,
        }

    tmp_cargo_toml_path = os.path.join(dir, "Cargo.toml")
    # This is the third-party Cargo.toml file. Note that we do not write
    # the `orig_toml_parsed` as the python parser does not like the contents
    # of some Cargo.toml files that cargo is just fine with. So we write the
    # contents without a round trip through the parser.
    if orig_toml_text:
        cargo_toml_text = orig_toml_text
    else:
        cargo_toml_text = toml.dumps(orig_toml_parsed)
    # We attach our "patch" keys onto it to redirect all crates.io
    # dependencies into `consts.THIRD_PARTY`.
    cargo_toml_text = cargo_toml_text + toml.dumps(patch)
    # Generate our own (temp) copy of a Cargo.toml for the dependency
    # that we will run `cargo tree` against.
    with open(tmp_cargo_toml_path, mode="w") as tmp_cargo_toml:
        tmp_cargo_toml.write(cargo_toml_text)
    if verbose:
        print("Writing to %s:" % tmp_cargo_toml_path)
        print("=======")
        print(cargo_toml_text)
        print("=======")

    return tmp_cargo_toml_path


def gen_list_of_3p_cargo_toml() -> ListOf3pCargoToml:
    """Create a cached view of existing third-party crates.

    Find all the third-party crates present and cache them for generating
    Cargo.toml files in temp dirs that will point to them."""
    list_of: list[ListOf3pCargoToml.CargoToml] = []
    for normalized_crate_name in os.listdir(common.os_third_party_dir()):
        crate_dir = common.os_crate_name_dir(normalized_crate_name)
        if not os.path.isdir(crate_dir):
            continue
        for v_epoch in os.listdir(crate_dir):
            epoch = v_epoch.replace("v", "").replace("_", ".")
            filepath = common.os_crate_cargo_dir(normalized_crate_name,
                                                 epoch,
                                                 rel_path=["Cargo.toml"])
            if os.path.exists(filepath):
                cargo_toml = toml.load(filepath)
                # Note this can't use the directory name because it was
                # normalized, so we read the real name from the Cargo.toml.
                name = cargo_toml["package"]["name"]
                assert common.crate_name_normalized(
                    name) == normalized_crate_name
                # The version epoch comes from the directory name.
                list_of += [
                    ListOf3pCargoToml.CargoToml(
                        name, epoch,
                        common.os_crate_cargo_dir(normalized_crate_name, epoch))
                ]
    return ListOf3pCargoToml(list_of)
