# python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The gen module implements the gen action.

The gen action will generate the dependency graph of from crates rooted in
`third_party/rust/third_party.toml` including transitive dependencies which are
all found in `third_party/rust/`, and then write the BUILD.gn files required to
build each such crate."""

from __future__ import annotations

from typing import Iterator, Optional
from contextlib import contextmanager
from datetime import datetime
from pprint import pprint
import argparse
import tempfile
import os
import re
import subprocess
import sys

from lib.build_rule import BuildRule
from lib import common
from lib import compiler
from lib import consts
from lib import cargo


class BuildData:
    """The root of the crate dependency tree.

    This holds information collected from the dependency tree that is used to
    generate BUILD.gn rules. This data needs to be collected and merged from
    across many crates, as each crate `X` depending on `Z` will contribute to
    data about what is needed from `Z`."""

    def __init__(self):
        self.for_normal = DataForUsage()
        self.for_buildrs = DataForUsage()
        self.for_tests = DataForUsage()

    @contextmanager
    def for_usage(self, usage: cargo.CrateUsage) -> Iterator[DataForUsage]:
        """A ContextManager for accessing the nested `DataForUsage` structure.

        The `DataForUsage` contains data which is different depending how
        crates are being used (for building a library/executable, for building
        a build script, or for building tests).
        """
        try:
            if usage == cargo.CrateUsage.FOR_NORMAL:
                yield self.for_normal
            elif usage == cargo.CrateUsage.FOR_BUILDRS:
                yield self.for_buildrs
            elif usage == cargo.CrateUsage.FOR_TESTS:
                yield self.for_tests
            else:
                assert False  # Unhandled CrateUsage
        finally:
            pass

    def has_crate_for_usage(self, usage: cargo.CrateUsage,
                            crate: cargo.CrateKey) -> bool:
        """Whether a crate has been seen yet as a dependency."""
        with self.for_usage(usage) as for_usage:
            return for_usage.has_crate(crate)

    @contextmanager
    def per_crate_for_usage(self, usage: cargo.CrateUsage,
                            crate: cargo.CrateKey) -> Iterator[PerCrateData]:
        """A shortcut to PerCrateData without going through `DataForUsage`.

        This is equivalent to using `for_usage(usage)` and then
        `per_crate(crate)`."""
        assert crate
        with self.for_usage(usage) as for_usage:
            if not crate in for_usage._map:
                for_usage._map[crate] = PerCrateData()
            try:
                yield for_usage._map[crate]
            finally:
                pass

    def get_per_crate_data_copy(self, usage: cargo.CrateUsage,
                                crate: cargo.CrateKey) -> PerCrateData:
        with self.per_crate_for_usage(usage, crate) as crate_data:
            return crate_data


class DataForUsage:
    """Data that differs depending on how crates are being consumed.

    Crates can be built up to 3 times, for use in building different types of
    targets.
    """

    def __init__(self):
        self._map: dict[cargo.CrateKey, PerCrateData] = {}

    @contextmanager
    def per_crate(self, crate: cargo.CrateKey) -> Iterator[PerCrateData]:
        """A ContextManager to access the PerCrateData structure.

        This contains data about how the crate should be build, how other crates
        depend on it and what it depends on.
        """
        assert crate
        if not crate in self._map:
            self._map[crate] = PerCrateData()
        try:
            yield self._map[crate]
        finally:
            pass

    def has_crate(self, crate: cargo.CrateKey) -> bool:
        """Whether a crate has been seen as a dependency yet.

        Note this includes crates depended on by our fictional top-level
        third_party.toml crate.
        """
        return crate in self._map

    def all_crates(self) -> set[cargo.CrateKey]:
        """All crates that appear as a dependency in the dependency tree.

        Note this includes crates depended on by our fictional top-level
        third_party.toml crate.
        """
        return set(self._map.keys())

    def all_crates_data(self) -> list[tuple[cargo.CrateKey, PerCrateData]]:
        """All crates along with their PerCrateData in the dependency tree.

        Note this includes crates depended on by our fictional top-level
        third_party.toml crate.
        """
        return list(self._map.items())


class PerCrateData:
    """The set of data about a Crate for a particular usage of that crate.

    This defines how other crates depend on this crate, as well as what it
    depends on."""

    def __init__(self):
        # Whether the crate is allowed to be used from first-party code. This is
        # the set of crates depended on by our fictional third_party.toml
        # crate.
        self.for_first_party = False
        # The set of features requested by other crates, and the architectures
        # that they requested them on.
        self.features = FeatureSet()
        # Dependencies of this crate, with different sets for the normal
        # (library or executable) output, the build script, or tests.
        self.deps = {o: DepSet() for o in cargo.CrateBuildOutput}
        # On what architectures this crate is depended on by another crate
        # (including by our fictional top-level third_party.toml). If this is
        # empty, the crate is not used at all (at least under a particular usage
        # aka DataForUsage).
        self.used_on_archs = compiler.ArchSet.EMPTY()
        # Whether the crate is considered "architecture-specific". When true,
        # the crate may have different dependencies or feature-requirements on
        # different architectures, and we must check each architecture for them.
        # As a result, any dependencies of this crate would also be considered
        # as "architecture-specific" and would get this flag set eventually as
        # well.
        self.arch_specific = False
        # The set of files generated from the crate's build.rs build script.
        self.build_script_outputs: set[str] = set()
        # The map of full version numbers that Cargo has requested for the crate
        # + epoch to the path where Cargo said it has found them. The path will
        # be missing if there's no local crate that matches.
        self.cargo_full_versions: dict[str, str] = {}


class DepSet:
    """The set of dependencies, and what architectures they are needed on."""

    class Data:
        def __init__(self):
            # The architectures where the dependency is needed.
            self.archset = compiler.ArchSet.EMPTY()

    def __init__(self):
        self._map: dict[cargo.CrateKey, DepSet.Data] = {}

    @contextmanager
    def dep_data(self, dep: cargo.CrateKey) -> Iterator[DepSet.Data]:
        """A ContextManager to access the dependency data of a crate."""
        assert dep
        if not dep in self._map:
            self._map[dep] = DepSet.Data()
        try:
            yield self._map[dep]
        finally:
            pass

    def all_deps(self) -> set[cargo.CrateKey]:
        """The set of all crates which are dependencies, on any architecture."""
        return {k for k in self._map.keys()}

    def all_deps_data(self) -> list[tuple[cargo.CrateKey, DepSet.Data]]:
        """The set of all crates and their dependency data."""
        return [p for p in self._map.items()]


class FeatureSet:
    """The set of features requested of a crate.

    This contains the set of requested features, and the archictures they are
    requested for.
    """

    def __init__(self):
        self._map: dict[str, compiler.ArchSet] = {}

    def has_feature(self, feature: str) -> bool:
        """Whether the named feature is requested for any architecture."""
        return feature in self._map

    @contextmanager
    def archset_for_feature(self, feature: str) -> Iterator[compiler.ArchSet]:
        """A ContextManager to access the ArchSet for anamed feature.

        This is the set of architectures where the feature is requested."""
        assert feature
        if not feature in self._map:
            self._map[feature] = compiler.ArchSet.EMPTY()
        try:
            yield self._map[feature]
        finally:
            pass

    def all_archsets(self) -> list[tuple[str, compiler.ArchSet]]:
        """A set of all features and their feature data."""
        return [p for p in self._map.items()]


def run(args: argparse.Namespace):
    """Entry point for the 'gen' action."""

    if not args.force:
        print("Build file generation is deprecated. Use gnrt instead.")
        print("To run anyway, pass --force.")
        return

    # This step constructs a BuildData which has all the information we need to
    # construct build rules in GN. This is slow as it has to query `cargo tree`
    # a lot.
    build_data_set = _construct_build_data_from_3p_crates(args)

    NAME_PRINT_LEN = 30
    num_done = 0
    num_total = len(build_data_set.for_normal.all_crates())
    last_printed = 0
    for crate_key in build_data_set.for_normal.all_crates():
        last_printed = common.print_same_line(
            "Generating BUILD.gn for crates {}/{} {} v{}".format(
                1 + num_done, num_total, crate_key.name, crate_key.epoch),
            last_printed)
        # This uses the entire BuildData structure to construct a BuildRule for
        # each crate, one at at time. The BuildRule is a typed representation of
        # what will end up in the crate's BUILD.gn file.
        build_rule = _gen_build_rule(args, build_data_set, crate_key)
        assert build_rule.normal_usage.used_on_archs or \
            build_rule.buildrs_usage.used_on_archs or \
            build_rule.test_usage.used_on_archs

        copyright_year = _get_copyright_year(
            common.os_crate_version_dir(crate_key.name,
                                        crate_key.epoch,
                                        rel_path=["BUILD.gn"]))
        with open(
                common.os_crate_version_dir(crate_key.name,
                                            crate_key.epoch,
                                            rel_path=["BUILD.gn"]),
                "w") as build_file:
            build_file.write(
                _run_gn_format(build_rule.generate_gn(args, copyright_year)))
        num_done += 1
    common.print_same_line("Generating BUILD.gn for crates {}/{} ".format(
        num_done, num_total),
                           last_printed,
                           done=True)

    if not args.skip_patch:
        patch_path = common.os_third_party_dir(consts.BUILD_PATCH_FILE)
        try:
            with open(patch_path, "r") as patch_file:
                _apply_build_patch(patch_file.read())
        except FileNotFoundError:
            # If the file does not exist simply print a warning and continue.
            print(f"Warning: file {patch_path} did not exist.")


def _get_copyright_year(path: str) -> str:
    try:
        with open(path, "r") as file:
            top_line = file.readline()
            m = consts.GN_HEADER_YEAR_REGEX.search(top_line)
            if m:
                return m.group("year")
    except FileNotFoundError:
        pass
    return str(datetime.now().year)


def _construct_build_data_from_3p_crates(args: argparse.Namespace) -> BuildData:
    """Read the `third_party.toml` data and populate a BuildData from it.

    Returns a dictionary containing information about all third-party crate
    dependencies, including transitive dependencies, build dependencies and
    dev (test-only) dependencies. They are all generated from the set of
    first-party dependencies (and their features) declared in
    `${THIRD_PARTY}/third_party.toml`.
    """
    toml_3p = common.load_toml(
        common.os_third_party_dir(rel_path=["third_party.toml"]))
    assert "dependencies" in toml_3p, \
        "third_party.toml must specify 'dependencies'"

    # Validate the dependencies listed in third_party.toml.
    TOML_DEPS_KEYS = ["dependencies", "build-dependencies", "dev-dependencies"]
    for toml_key in TOML_DEPS_KEYS:
        if not toml_key in toml_3p:
            continue
        for (dep, details) in toml_3p[toml_key].items():
            version = details["version"] if isinstance(details,
                                                       dict) else details
            # Verify that the version is specified as an epoch.
            epoch = common.version_epoch_dots(version)
            assert version == epoch, \
                "third_party.toml must specify version as an epoch for " \
                " '{}', " "found '{}', did you mean '{}'?".format(
                    dep, version, epoch)

            # Verify that a crate is only listed once.
            for other_toml_key in [k for k in TOML_DEPS_KEYS if k != toml_key]:
                if other_toml_key not in toml_3p:
                    continue
                assert not dep in toml_3p[other_toml_key], \
                    "A crate may only appear in one section of " \
                    "third_party.toml, but '{}' appears more than " \
                    "once.".format(dep)

    if args.crates is not None:
        crate_set: set[str] = set(args.crates)
        # This option is for testing. Include only regular dependencies. Filter
        # out any that are not listed.
        toml_3p.pop("dev-dependencies", None)
        toml_3p.pop("build-dependencies", None)
        dep_keys = set(toml_3p["dependencies"].keys())
        for dep_key in dep_keys:
            if dep_key not in crate_set:
                del toml_3p["dependencies"][dep_key]

    # For every crate in third_party, we will generate a patch to redirect
    # crates.io to that directory, so that if we have local changes to the
    # Cargo.toml files, running `cargo tree` will see them. To do this we
    # save a list of all 3p crates.
    list_of_3p_cargo_toml = cargo.gen_list_of_3p_cargo_toml()

    # We can't use these to see the deps tree, as it will put everything as a
    # dep of the root target, but it's helpful for failing faster on missing
    # crates.
    transitive_normal_build_data_set = BuildData()

    # The set of info needed to make a GN target for the crate, in each
    # CrateUsage.
    build_data_set = BuildData()

    last_printed = 0

    # We will take the third-party dependencies, specified in a toml format,
    # and build a Cargo.toml file for running `cargo tree` against.
    with tempfile.TemporaryDirectory() as workdir:
        # First we make a copy of the third-party deps input as-is, but with
        # additional required fields added. We can use that with `cargo tree` to
        # get a full set of all dependencies, including transitive ones.
        cargo_toml_path = cargo.write_cargo_toml_in_tempdir(
            workdir,
            list_of_3p_cargo_toml,
            orig_toml_parsed=cargo.add_required_cargo_fields(toml_3p),
            verbose=args.verbose)

        # This collects the direct dependencies from first-party code. For
        # normal dependencies, cargo can output them all here, which we collect
        # up front to try print out all the missing ones at once. This would
        # confuse our actual dependency calculations though, since we would
        # have all the deps on a single build target (the third-party root). So
        # we will have to throw them away.
        last_printed = common.print_same_line(
            "Collecting all transitive normal dependencies.", last_printed)
        _collect_deps_for_crate(args, cargo_toml_path,
                                transitive_normal_build_data_set,
                                cargo.CrateUsage.FOR_NORMAL, True)
        # We don't look for blocked stuff here since we won't have the data to
        # say what's depending on them yet anyways.
        _look_for_missing_or_blocked_deps(transitive_normal_build_data_set,
                                          find_blocked=False)

        last_printed = common.print_same_line(
            "Collecting top-level normal dependencies.", last_printed)
        _collect_deps_for_crate(args,
                                cargo_toml_path,
                                build_data_set,
                                cargo.CrateUsage.FOR_NORMAL,
                                True,
                                depth=1)
        last_printed = common.print_same_line(
            "Collecting top-level build-dependencies.", last_printed)
        _collect_deps_for_crate(args,
                                cargo_toml_path,
                                build_data_set,
                                cargo.CrateUsage.FOR_BUILDRS,
                                True,
                                depth=1)

        last_printed = common.print_same_line(
            "Collecting top-level dev-dependencies.", last_printed)
        _collect_deps_for_crate(args,
                                cargo_toml_path,
                                build_data_set,
                                cargo.CrateUsage.FOR_TESTS,
                                True,
                                depth=1)

    # Now we have enough to build our first-party code, from prebuilts of the
    # dependencies. But we don't have enough to build each dependency. We have
    # to query `cargo tree` for each one to learn what's needed to build it.
    #
    # So here we walk through each crate and query its Cargo.toml for its build
    # and dev dependencies.
    #
    # This step is slow...
    crate_keys_left = build_data_set.for_normal.all_crates()
    num_done = 0
    # TODO: Cache this work somewhere in `consts.THIRD_PARTY`.
    while crate_keys_left:
        crate_key = crate_keys_left.pop()
        crate_name = crate_key.name
        crate_epoch = crate_key.epoch

        orig_cargo_toml_path = common.os_crate_cargo_dir(
            crate_name, crate_epoch, rel_path=["Cargo.toml"])

        # Skip missing dependencies. We will print them all at the end.
        if not os.path.exists(orig_cargo_toml_path):
            num_done += 1
            continue

        with tempfile.TemporaryDirectory() as workdir:
            tmp_cargo_toml_path = cargo.write_cargo_toml_in_tempdir(
                workdir,
                list_of_3p_cargo_toml,
                orig_toml_path=orig_cargo_toml_path,
                verbose=args.verbose)

            last_printed = common.print_same_line(
                "Collecting normal dependencies: {}/{} {} v{}".format(
                    1 + num_done, 1 + num_done + len(crate_keys_left),
                    crate_name, crate_epoch), last_printed)
            new_keys = _collect_deps_for_crate(args,
                                               tmp_cargo_toml_path,
                                               build_data_set,
                                               cargo.CrateUsage.FOR_NORMAL,
                                               False,
                                               crate_key=crate_key,
                                               depth=1)
            crate_keys_left.update(new_keys)

            last_printed = common.print_same_line(
                "Collecting build dependencies: {}/{} {} v{}".format(
                    1 + num_done, 1 + num_done + len(crate_keys_left),
                    crate_name, crate_epoch), last_printed)
            new_keys = _collect_deps_for_crate(args,
                                               tmp_cargo_toml_path,
                                               build_data_set,
                                               cargo.CrateUsage.FOR_BUILDRS,
                                               False,
                                               crate_key=crate_key,
                                               depth=1)
            crate_keys_left.update(new_keys)
            find_test_deps = args.with_tests
            with build_data_set.for_tests.per_crate(crate_key) as per_crate:
                if per_crate.for_first_party:
                    find_test_deps = True
            if find_test_deps:
                last_printed = common.print_same_line(
                    "Collecting test dependencies: {}/{} {} v{}".format(
                        1 + num_done, 1 + num_done + len(crate_keys_left),
                        crate_name, crate_epoch), last_printed)
                new_keys = _collect_deps_for_crate(args,
                                                   tmp_cargo_toml_path,
                                                   build_data_set,
                                                   cargo.CrateUsage.FOR_TESTS,
                                                   False,
                                                   crate_key=crate_key,
                                                   depth=1)
                crate_keys_left.update(new_keys)

        num_done += 1
    common.print_same_line("Collecting dependencies: {}/{} ".format(
        num_done, num_done),
                           last_printed,
                           done=True)

    if args.verbose:
        pprint(build_data_set)
        print()

    # If anything was missing, we will abort here and print them out all at
    # once.
    _look_for_missing_or_blocked_deps(build_data_set)

    return build_data_set


def _run_gn_format(build_gn: str) -> str:
    """Formats the input GN file contents, and returns the formatted output."""
    import subprocess
    try:
        return subprocess.check_output(["gn", "format", "--stdin"],
                                       stderr=subprocess.PIPE,
                                       text=True,
                                       input=build_gn)
    except subprocess.CalledProcessError as e:
        print()
        print("stdout")
        print(e.stdout)
        print()
        print("stderr")
        print(e.stderr)
        raise e


def _look_for_missing_or_blocked_deps(build_data_set: BuildData,
                                      find_blocked: bool = True):
    """Find crate dependencies for which we have no local copy downloaded.

    If `find_blocked` is true, this also looks for dependencies listed in
    Cargo.toml files to crates that are considered blocked, and thus should not
    be used.

    If a problem is found, this function will terminate the program with
    `exit(1)`.
    """
    missing: list[tuple[str, str, str]] = []
    missing_version: list[tuple[str, str, str]] = []
    blocked: list[tuple[str, str]] = []
    # Every crate appears in all 3 CrateUsage entries, so we can just look at
    # one.
    for crate_key in build_data_set.for_normal.all_crates():
        name = crate_key.name
        epoch = crate_key.epoch
        if name in consts.BLOCKED_CRATES:
            if find_blocked:
                reason = consts.BLOCKED_CRATES[name]
                blocked += [(name, reason)]
            continue

        with build_data_set.for_normal.per_crate(crate_key) as data:
            for (ver, dir) in data.cargo_full_versions.items():
                if dir:
                    continue  # If cargo gave a path then it was found locally.
                epoch_path = common.os_crate_cargo_dir(name,
                                                       epoch,
                                                       rel_path=["Cargo.toml"])
                if not os.path.exists(epoch_path):
                    missing += [(name, ver, epoch)]
                else:
                    missing_version += [(name, ver, epoch)]

    if missing or missing_version or blocked:
        print(file=sys.stderr)
        print(file=sys.stderr)

    if missing_version:
        print("Missing correct version for {} dependencies:".format(
            len(missing_version)),
              file=sys.stderr)
        for (name, ver, epoch) in missing_version:
            print("Cargo wants version \"{}\" for crate \"{}\". But a different"
                  " version is downloaded for epoch v{}.".format(
                      ver, name, epoch),
                  file=sys.stderr)
        print(
            "To resolve these errors, remove the epoch and download the exact"
            " version by specifying it in third_party.toml. If multiple exact"
            " versions are required, we will need some sort of extension to"
            " third_party.toml to support this.",
            file=sys.stderr)
        print(file=sys.stderr)
    if missing:
        print("Missing {} dependencies. To download:".format(len(missing)),
              file=sys.stderr)
        for (name, ver, epoch) in missing:
            print("{} download {} {} --security-critical=yes_or_no".format(
                sys.argv[0], name, ver),
                  file=sys.stderr)
        print(file=sys.stderr)
    if blocked:
        for (name, reason) in blocked:
            found_from = []
            for usage in cargo.CrateUsage:
                with build_data_set.for_usage(usage) as for_usage:
                    for (from_key,
                         per_crate_data) in for_usage.all_crates_data():
                        for (usage, depset) in per_crate_data.deps.items():
                            for dep_key in depset.all_deps():
                                if (dep_key.name in consts.BLOCKED_CRATES
                                        and not from_key in found_from):
                                    found_from += [from_key]
            print("Found dependency on blocked crate \"{}\" from:".format(name),
                  file=sys.stderr)
            for crate_key in found_from:
                print("    {} v{}".format(
                    crate_key.name,
                    common.version_epoch_normalized(crate_key.epoch)),
                      file=sys.stderr)
            print("  Reason: {}".format(reason), file=sys.stderr)
        print("Remove blocked crates from any Cargo.toml files "
              "in {} before proceeding.".format(
                  os.path.join(*consts.THIRD_PARTY)),
              file=sys.stderr)
        print(
            "Note: You will also need to remove usage of the blocked "
            "crate from the dependent crate(s) as well.",
            file=sys.stderr)
        print(file=sys.stderr)

    if missing or missing_version or blocked:
        exit(1)


class ArchSpecific:
    def __init__(self, archset: compiler.ArchSet):
        self.archset = archset
        # Whether the crate has arch-specific dependencies. That is to say,
        # outgoing edges are arch-dependent.
        self.arch_specific_deps: list[cargo.CrateKey] = []
        # Whether the crate is an arch-specific dep from some other crate. That
        # is to say, incoming edges are arch-dependent.
        self.is_used_as_arch_specific_dep = False

    def archs_to_test(self) -> set[str]:
        """The ArchSet to test against as a list of target strings.

        These are strings we can pass to Cargo or rustc, and can build other
        ArchSets from.
        """
        return self.archset.as_strings()

    def is_single_arch(self) -> bool:
        """If we can consider only one architecture for accurate results."""
        return (not self.arch_specific_deps
                and not self.is_used_as_arch_specific_dep)

    def apply_results_to(self, target_arch: str) -> compiler.ArchSet:
        """Return an ArchSet to use for any new outgoing dependency edges."""
        if self.is_single_arch():
            # If we're applying results to other architectures, we expect that
            # we are only testing one.
            assert (len(self.archs_to_test()) == 1)
            # We're only checking one arch but if a dep is found, it should
            # be enabled for all archs. We're only checking one arch because
            # we have no arch-specific rules.
            return compiler.ArchSet.ALL()
        assert (target_arch in self.archs_to_test())
        # We need to check each arch, so only add deps to the arch they
        # are found on.
        return compiler.ArchSet(initial={target_arch})


def _get_archs_of_interest(cargo_toml: dict,
                           crate_usage_data: Optional[PerCrateData],
                           user_target_arch: Optional[str]) -> ArchSpecific:
    """Determine which archs to check dependencies for a crate.

    Args:
        cargo_toml: The Cargo.toml file of the crate as a dict.
        crate_usage_data: Data about how the crate is used by other crates. It
            can be None, in which case, it's assumed to not be depended on by
            any other crates.
        target: The user-specific target (one of the keys from
            compiler.RUSTC_ARCH_TO_BUILD_CONDITION) to generate build files
            for. If specified, only that target will be considered. Otherwise,
            all possible targets would be considered.
    """
    if user_target_arch:
        return ArchSpecific(compiler.ArchSet(initial={user_target_arch}))

    # Whether the crate has arch-specific dependencies. That is to say,
    # outgoing edges are arch-dependent.
    has_arch_specific_deps = False
    if "target" in cargo_toml:
        for target_data in cargo_toml["target"].values():
            if "dependencies" in target_data:
                has_arch_specific_deps = True
                break

    # Whether the crate is an arch-specific dep from some other crate. That
    # is to say, incoming edges are arch-dependent.
    is_used_as_arch_specific_dep = crate_usage_data.arch_specific \
        if crate_usage_data else False

    # When there's no arch-specific incoming or outgoing edges, we can just
    # choose any target architecture and collect its dependencies there,
    # applying them too all architectures.
    if not is_used_as_arch_specific_dep and not has_arch_specific_deps:
        # We can just test one arch even though it'll apply to all.
        return ArchSpecific(compiler.ArchSet.ONE())

    # This crate has architecture-specific dependencies, or is part of
    # an architecture-specific dependency chain.

    if crate_usage_data:
        a = ArchSpecific(crate_usage_data.used_on_archs)
    else:
        a = ArchSpecific(compiler.ArchSet.ALL())
    if has_arch_specific_deps:
        # A list of (crate name, dependency data) for each dependency that is
        # found under a "target" rule in the Cargo.toml.
        targetted_deps = []
        for (target_name, target_data) in cargo_toml["target"].items():
            targetted_deps += list(
                target_data.get("dependencies", dict()).items())
        # Convert the list of (crate name, dependency data) into a more useful
        # list of `cargo.CrateKey`s.

        def version_from_maybe_dict(maybe_dict: dict | str) -> str:
            if isinstance(maybe_dict, dict):
                return maybe_dict["version"]
            else:
                return maybe_dict

        a.arch_specific_deps = [
            cargo.CrateKey(crate_name, version_from_maybe_dict(dep_data))
            for (crate_name, dep_data) in targetted_deps
        ]
    a.is_used_as_arch_specific_dep = is_used_as_arch_specific_dep
    return a


def _crate_features_on_target_arch(crate_usage_data: PerCrateData,
                                   target_arch: str) -> list[str]:
    """Returns the subset of features requested of the crate for the target.

    A crate has a set of features that are enabled by the union of all other
    crates that depend upon it. The incoming dependency edges specify which
    features are requested.

    But some of those edges may be arch-specific. We return the set of features
    that are requested for the specific architecture named in `target_arch`.
    """
    all_features = crate_usage_data.features.all_archsets()
    return [f for (f, archset) in all_features if archset.has_arch(target_arch)]


class CargoTreeDependency:
    def __init__(self,
                 key: cargo.CrateKey,
                 full_version: Optional[str] = None,
                 crate_path: Optional[str] = None,
                 features: list[str] = [],
                 is_for_first_party_code: bool = False,
                 build_script_outputs: set[str] = set()):
        self.key: cargo.CrateKey = key
        self.full_version: Optional[str] = full_version
        self.crate_path: Optional[str] = crate_path
        self.features: list[str] = features
        self.is_for_first_party_code: bool = is_for_first_party_code
        self.build_script_outputs: set[str] = build_script_outputs

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, CargoTreeDependency):
            return NotImplemented
        return (self.key == other.key
                and self.full_version == other.full_version
                and self.crate_path == other.crate_path
                and self.features == other.features and
                self.is_for_first_party_code == other.is_for_first_party_code
                and self.build_script_outputs == other.build_script_outputs)

    def __repr__(self) -> str:
        return "CargoTreeDependency(key={}, full_version={}, crate_path={}, " \
            "features={}, " \
            "is_for_first_party_code={}, " \
            "build_script_outputs={})".format(
                self.key, self.full_version, self.crate_path, self.features,
                self.is_for_first_party_code,
                self.build_script_outputs)


def _parse_cargo_tree_dependency_line(args: argparse.Namespace,
                                      cargo_toml: dict,
                                      is_third_party_toml: bool, line: str
                                      ) -> Optional[CargoTreeDependency]:
    m = re.search(consts.CARGO_DEPS_REGEX, line)
    if not m:
        return None
    if args.verbose:
        print(m.group(0))

    dep_name = m.group("name")
    dep_version = m.group("version")
    dep_path = m.group("path") if "path" in m.groupdict() else None
    dep_key = cargo.CrateKey(dep_name, dep_version)

    dep_features = m.group("features").split(",") if m.group("features") else []
    dep_isprocmacro = bool(m.group("isprocmacro"))

    parse_ext_key = None
    if is_third_party_toml:
        parse_ext_key = "dependencies" if dep_name in cargo_toml.get(
            "dependencies",
            {}) else "dev-dependencies" if dep_name in cargo_toml.get(
                "dev-dependencies", {}) else None

    build_script_outputs: set[str] = set()
    if not parse_ext_key:
        # Extensions from third_party.toml that aren't in normal Cargo.toml,
        # these are the defaults for stuff outside of third_party.toml.
        for_first_party_code = False
    else:
        for_first_party_code = True
        # Usually the dependency value is just a version number, but if it
        # is a dict, then it can declare values for extensions.
        extensions = cargo_toml[parse_ext_key][dep_name]
        if type(extensions) is dict:
            build_script_outputs = set(
                extensions.get("build-script-outputs", build_script_outputs))
            for_first_party_code = extensions.get("allow-first-party-usage",
                                                  for_first_party_code)
    return CargoTreeDependency(dep_key,
                               full_version=dep_version,
                               crate_path=dep_path,
                               features=dep_features,
                               is_for_first_party_code=for_first_party_code,
                               build_script_outputs=build_script_outputs)


def _add_edges_for_dep_on_target_arch(
        build_data_set: BuildData, parent_crate_key: Optional[cargo.CrateKey],
        parent_arch_specific: ArchSpecific, parent_usage: cargo.CrateUsage,
        parent_output: cargo.CrateBuildOutput, dep: CargoTreeDependency,
        target_arch: str, new_keys: set[cargo.CrateKey]):
    """Adds edges between a parent crate and a dependency crate.

    Args:
        build_data_set: The data structure holding all the dependency edges.
        parent_crate_key: The parent crate. Can be None if the parent is our
            third_party.toml which is not actually a crate at all.
        parent_arch_specific: Data about the parent crate's relationship to
            target architectures, which can apply transitively to dependencies
            as well.
        parent_usage: The parent crate may be building a library for use by
            other crates. Those (ancestor) crates build different outputs such
            as normal binaries, build scripts, and tests. The `parent_usage` is
            which mode of the parent crate we are adding edges to dependencies
            from. The usage controls what features are enabled in the parent
            crate, which can influence its dependencies or configuration.
        parent_output: The parent crate itself also has these various build
            outputs and we're currently adding edges for one of those outputs.
            The `parent_output` specifies which build artifact from the parent
            we're adding dependency edges for.
        dep: Information about the dependency crate.
        target_arch: The target architecture we're currently evaluating. We may
            apply the edges more generally to multiple architectures
        new_keys: An out-param of crates that will need to be (re)visited to
            examine for dependencies.

    """
    archset_for_new_edges = parent_arch_specific.apply_results_to(target_arch)

    # First ensure this dependency crate exists in the `build_data_set` under
    # all CrateUsages.
    for u in cargo.CrateUsage:
        with build_data_set.per_crate_for_usage(u, dep.key) as crate_data:
            crate_data.for_first_party |= dep.is_for_first_party_code
            crate_data.build_script_outputs |= dep.build_script_outputs
            if dep.full_version is not None and dep.crate_path is not None:
                crate_data.cargo_full_versions.update(
                    {dep.full_version: dep.crate_path})

    # The parent crate may use a dependency in various ways: to produce normal
    # output, a build script, or tests. We examine just one in this function.
    # Based on that usage from the parent, from the perspective of the
    # dependency, this is how it is being used.
    dep_usage = parent_output.as_dep_usage()

    # Determine if the dependency crate should be considered arch-specific.
    if parent_arch_specific.is_used_as_arch_specific_dep:
        # Firstly, arch-specific-ness is transitive. So if this crate is
        # arch-specific (is used as an arch-specific dependency), then its
        # dependencies are also. But if not, then we look for arch-specific
        # dependency edges come out of this crate to the dependency.
        dep_is_arch_specific = True
    else:
        # For this dependency of the crate, we need to see if a "target" rule
        # affects it, in which case it is considered arch-specific and we will
        # need to compute its deps/features for all configs.
        dep_is_arch_specific = \
            dep.key in parent_arch_specific.arch_specific_deps

    # Update the PreCrateData of the dependency crate. Any changes in here mean
    # we will need to (re)visit the dependency crate to determine its transitive
    # dependencies. This data is about how to build the dependency crate based
    # on its incoming edges from the parent.
    changed = False
    with build_data_set.per_crate_for_usage(dep_usage,
                                            dep.key) as dep_crate_data:
        # This marks the dependency crate as being used on the given target
        # architectures.
        changed |= dep_crate_data.used_on_archs.add_archset(
            archset_for_new_edges)

        if dep_is_arch_specific and not dep_crate_data.arch_specific:
            dep_crate_data.arch_specific = True
            changed = True

        for f in dep.features:
            with dep_crate_data.features.archset_for_feature(f) as archset:
                changed |= archset.add_archset(archset_for_new_edges)

    if changed:
        new_keys.add(dep.key)

    # Add outgoing edges from the parent crate to the dependency crate. If the
    # parent is the virtual third_party.toml crate, then the crate key will not
    # exist, and we don't need to set up any edges.
    if not parent_crate_key:
        return

    with build_data_set.for_usage(parent_usage) as for_usage:
        assert parent_crate_key in for_usage.all_crates()
        with for_usage.per_crate(parent_crate_key) as parent_data:
            with parent_data.deps[parent_output].dep_data(dep.key) as dep_data:
                # Adds the edge from parent to dependency for all given target
                # architectures.
                dep_data.archset.add_archset(archset_for_new_edges)


def _collect_deps_for_crate(args: argparse.Namespace,
                            cargo_toml_path: str,
                            build_data_set: BuildData,
                            usage: cargo.CrateUsage,
                            is_third_party_toml: bool,
                            crate_key: Optional[cargo.CrateKey] = None,
                            depth: Optional[int] = None) -> set[cargo.CrateKey]:
    """Runs `cargo tree` and collects all dependency data for a specific crate.

    Args:
        args: The command line args that were given to the crates.py tool.
        cargo_toml_path: The Cargo.toml file for the crate to collect from.
        build_data_set: The BuildData structure to collect data into.
        usage: Dependencies are different depending how the crate is being used,
          this specifies to look for dependencies for a particular usage.
        is_third_party_toml: True if the crate being collected from is the
          synthetic top level crate in `third_party.toml`.
        crate_key: The CrateKey of the crate at cargo_toml_path. It will be None
          if `is_third_party_toml` is true, since there's no real crate there.
        depth: How deep to look for dependencies with `cargo tree`. Defaults to
          None, which will look through the whole tree.

    Returns:
        A set of crates for which we will need to run this function on. Either
        they have never been seen before, or the data we collected for the
        crate in the past has been diritied.
    """
    assert is_third_party_toml != bool(crate_key)

    # New dependencies, or ones with new features, that need to be visited
    # by this function.
    new_keys: set[cargo.CrateKey] = set()

    cargo_toml = common.load_toml(cargo_toml_path)

    arch_specific = _get_archs_of_interest(
        cargo_toml,
        build_data_set.get_per_crate_data_copy(usage, crate_key)
        if crate_key else None, args.target)

    # The crate can have a few different types of output: normal output like a
    # library, its build script, and its tests. We walk over and construct
    # the dependency graph for each.
    first = True
    for output_type in cargo.CrateBuildOutput:
        if output_type == cargo.CrateBuildOutput.TESTS:
            if not args.with_tests and not is_third_party_toml:
                continue
        for target_arch in arch_specific.archs_to_test():
            if is_third_party_toml:
                # For the root third_party.toml crate, nothing can depend on it,
                # so there's no features (and no crate_key).
                crate_derived_features = []
            else:
                assert crate_key is not None
                crate_derived_features = _crate_features_on_target_arch(
                    build_data_set.get_per_crate_data_copy(usage, crate_key),
                    target_arch)

            if args.verbose:
                if first:
                    first = False
                    print()
                print("output: {} target: {}".format(output_type, target_arch))
            cargo_tree_output = cargo.run_cargo_tree(cargo_toml_path,
                                                     output_type, target_arch,
                                                     depth,
                                                     crate_derived_features)
            for line in cargo_tree_output:
                tree_dependency = _parse_cargo_tree_dependency_line(
                    args, cargo_toml, is_third_party_toml, line)
                if not tree_dependency:
                    # Some lines of `cargo tree` output may not point at a
                    # dependency.
                    continue
                _add_edges_for_dep_on_target_arch(build_data_set, crate_key,
                                                  arch_specific, usage,
                                                  output_type, tree_dependency,
                                                  target_arch, new_keys)
    return new_keys


def _gen_build_rule(args: argparse.Namespace, build_data_set: BuildData,
                    crate_key: cargo.CrateKey) -> BuildRule:
    """Generate a BuildRule for a single crate from the BuildData structure.

    Args:
        args: The command-line arguments.
        build_data_set: The fully-generated dependency tree data structure.
        crate_key: The crate to generate a BuildRule for, which will be used to
          generate its BUILD file.

    Returns:
        A BuildRule for the crate, which can be used to generate its BUILD file.
    """
    crate_name = crate_key.name
    crate_epoch = crate_key.epoch

    # We need the contents of the Cargo.toml to get a few pieces of data that
    # aren't part of the BuildData structure.
    cargo_toml = common.load_toml(
        common.os_crate_cargo_dir(crate_name,
                                  crate_epoch,
                                  rel_path=["Cargo.toml"]))

    build_rule = BuildRule(crate_name, crate_epoch)

    cargo_toml_has_lib_key = "lib" in cargo_toml
    cargo_toml_has_bin_key = "bin" in cargo_toml

    # Cargo defaults to 2015 for backward compatibility if it's not specified.
    build_rule.edition = cargo_toml["package"].get("edition", "2015")

    # Gather some cargo metadata which we will want to pass to rustc using
    # environment variables in case the crates use 'crate_authors!' or similar.
    build_rule.cargo_pkg_authors = cargo_toml["package"].get("authors")
    build_rule.cargo_pkg_version = cargo_toml["package"].get("version")
    build_rule.cargo_pkg_name = cargo_toml["package"].get("name")
    build_rule.cargo_pkg_description = cargo_toml["package"].get("description")

    # Prefix from the BUILD.gn file to the crate's source files.
    path_prefix = ("/".join(consts.CRATE_INNER_DIR) +
                   "/") if consts.CRATE_INNER_DIR else ""

    # We compute the lib type, but don't store it in BuildRule until we know
    # there is a lib target in the crate.
    lib_type = "rlib"
    if cargo_toml_has_lib_key:
        lib_type = "proc-macro" if cargo_toml["lib"].get("proc-macro",
                                                         False) else "rlib"

    default_lib_root_os_path = common.os_crate_cargo_dir(
        crate_name, crate_epoch, rel_path=["src", "lib.rs"])
    if cargo_toml_has_lib_key and "path" in cargo_toml["lib"]:
        build_rule.lib_root = path_prefix + cargo_toml["lib"]["path"]
    elif os.path.exists(default_lib_root_os_path):
        build_rule.lib_root = path_prefix + "src/lib.rs"

    if build_rule.lib_root:
        build_rule.lib_type = lib_type
        # TODO(danakj): Find all the sources.
        build_rule.lib_sources = [build_rule.lib_root]

    if cargo_toml_has_bin_key:
        for toml_bin in cargo_toml["bin"]:
            name = toml_bin["name"]

            default_bin_root_os_path = common.os_crate_cargo_dir(
                crate_name, crate_epoch, rel_path=["src", "main.rs"])
            named_bin_root_os_path = common.os_crate_cargo_dir(
                crate_name, crate_epoch, rel_path=["src", "bin", name + ".rs"])

            if "path" in toml_bin:
                path = path_prefix + toml_bin["path"]
            elif os.path.exists(named_bin_root_os_path):
                path = path_prefix + "src/bin/{}.rs".format(name)
            else:
                assert os.path.exists(default_bin_root_os_path)
                path = path_prefix + "src/main.rs"

            build_rule.bins += [{
                "name": name,
                "root": path,
                # TODO(danakj): Find all the sources.
                "sources": [path],
            }]

    build_script_root: Optional[str] = None
    if "build" in cargo_toml["package"]:
        if cargo_toml["package"]["build"]:  # May be `false`
            build_script_root = path_prefix + cargo_toml["package"]["build"]
    elif os.path.exists(
            common.os_crate_cargo_dir(crate_name,
                                      crate_epoch,
                                      rel_path=["build.rs"])):
        build_script_root = path_prefix + "build.rs"

    if build_script_root:
        build_rule.build_root = build_script_root
        # TODO(danakj): Find all the sources.
        build_rule.build_sources = [build_rule.build_root]
        # The third_party.toml specifies this, as it has to come from a human.
        with build_data_set.for_normal.per_crate(crate_key) as crate_data:
            with build_data_set.for_buildrs.per_crate(
                    crate_key) as for_build_data:
                with build_data_set.for_tests.per_crate(
                        crate_key) as for_tests_data:
                    # We assume that the build.rs has the same outputs no matter
                    # what features are going to be enabled, but if that's not
                    # true, then we need a way to specify in the
                    # third_party.toml which outputs to expect depending on how
                    # the crate's being used. And then these asserts could fail.
                    # Then we would need to store these on the BuildRuleUsage
                    # instead of on the BuildRule.
                    assert crate_data.build_script_outputs == \
                        for_build_data.build_script_outputs
                    assert crate_data.build_script_outputs == \
                        for_tests_data.build_script_outputs
            build_rule.build_script_outputs = \
                list(crate_data.build_script_outputs)

    # Each usage can define its own set of features/deps and will turn into a
    # separate GN target.
    for usage in cargo.CrateUsage:
        with build_data_set.per_crate_for_usage(usage, crate_key) as crate_data:
            if not crate_data.used_on_archs:
                continue

            normal_deps: DepSet = crate_data.deps[cargo.CrateBuildOutput.NORMAL]
            build_deps: DepSet = crate_data.deps[cargo.CrateBuildOutput.BUILDRS]
            if usage == cargo.CrateUsage.FOR_NORMAL:
                dev_deps: DepSet = crate_data.deps[cargo.CrateBuildOutput.TESTS]
            else:
                # We only care to run tests for the featureset used at runtime.
                dev_deps = DepSet()

            build_rule_usage = build_rule.get_usage(usage)
            build_rule_usage.for_first_party = crate_data.for_first_party
            build_rule_usage.used_on_archs = compiler.BuildConditionSet(
                crate_data.used_on_archs)
            build_rule_usage.features = [
                (name, compiler.BuildConditionSet(archset))
                for (name, archset) in crate_data.features.all_archsets()
            ]

            build_rule_usage.deps = [{
                "deppath":
                common.gn_crate_path(k.name, k.epoch) + ":" +
                cargo.CrateBuildOutput.NORMAL.gn_target_name_for_dep(),
                "compile_modes":
                compiler.BuildConditionSet(depdata.archset),
            } for (k, depdata) in normal_deps.all_deps_data()]

            build_rule_usage.build_deps = [{
                "deppath":
                common.gn_crate_path(k.name, k.epoch) + ":" +
                cargo.CrateBuildOutput.BUILDRS.gn_target_name_for_dep(),
                "compile_modes":
                compiler.BuildConditionSet(depdata.archset),
            } for (k, depdata) in build_deps.all_deps_data()]

            build_rule_usage.dev_deps = [{
                "deppath":
                common.gn_crate_path(k.name, k.epoch) + ":" +
                cargo.CrateBuildOutput.TESTS.gn_target_name_for_dep(),
                "compile_modes":
                compiler.BuildConditionSet(depdata.archset),
            } for (k, depdata) in dev_deps.all_deps_data()]

    build_rule.sort_internals()
    if args.verbose:
        print()
        print("build_rule for {}", crate_key)
        pprint(vars(build_rule))
        print("for {normal}: ")
        pprint(vars(build_rule.normal_usage))
        print("for buildrs: ")
        pprint(vars(build_rule.buildrs_usage))
        print("for test: ")
        pprint(vars(build_rule.test_usage))
    return build_rule


def _apply_build_patch(patch_contents: str):
    """Apply a git patch after generating BUILD.gn files. If the patch does not
    apply, throws an error.

    Args:
        patch_contents: The patch contents (not the filename)"""

    subprocess.run(["git", "apply", "-"],
                   input=patch_contents.encode(),
                   check=True)
