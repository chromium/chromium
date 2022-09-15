# python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from datetime import datetime
import io
import re
import tempfile
import toml
import unittest

from lib import gen
from lib import cargo
from lib import compiler

# An example output from `cargo tree`
# 1. We patch paths into the Cargo.toml files that we feed to `cargo tree` and
#    this demonstrates that for the "quote" crate by including the path there.
# 2. We include features in the output, demonstrated by many deps here. And we
#    also have an example of a crate without any features with the second
#    occurrence of "unicode-xid".
# 3. Some crates get de-duped and that means get a (*) on them, which we can
#    see.
# 4. A proc-macro crate is a different kind of crate and is designated by the
#    (proc-macro) after the version, and we have an example of that with
#    cxxbridge-macro.
# 5. We include direct deps as well as transitive deps, which have more than one
#    piece of ascii art on their line.
CARGO_TREE = """
cxx v1.0.56 (/path/to/chromium/src/third_party/rust/cxx/v1/crate)
├── cxxbridge-macro v1.0.56 (proc-macro)
│   ├── proc-macro2 v1.0.32 default,proc-macro,span-locations
│   │   └── unicode-xid v0.2.2 default
│   ├── quote v1.0.10 (/path/to/chromium/src/third_party/rust/quote/v1/crate) default,proc-macro
│   │   └── proc-macro2 v1.0.32 default,proc-macro,span-locations (*)
│   └── syn v1.0.81 clone-impls,default,derive,full,parsing,printing,proc-macro,quote
│       ├── proc-macro2 v1.0.32 default,proc-macro,span-locations (*)
│       ├── quote v1.0.10 default,proc-macro (*)
│       └── unicode-xid v0.2.2
└── link-cplusplus v1.0.5 default
""".lstrip("\n")


class GenTestCase(unittest.TestCase):
    def assertListSortedEqual(self, a, b, msg=None):
        a.sort()
        b.sort()
        if msg:
            self.assertListEqual(a, b, msg=msg)
        else:
            self.assertListEqual(a, b)

    def matching_archs(self, matching: str) -> set[str]:
        return {
            arch
            for arch in compiler._RUSTC_ARCH_TO_BUILD_CONDITION
            if re.search(matching, arch)
        }

    def all_archs(self):
        return set(compiler._RUSTC_ARCH_TO_BUILD_CONDITION.keys())

    def make_args(self):
        class Args:
            pass

        args = Args
        args.verbose = False
        return args

    def test_parse_cargo_tree_dependency_line(self):
        args = self.make_args()
        lines = CARGO_TREE.split("\n")

        # Here we are simulating `cargo tree` on a third-party Cargo.toml file,
        # not our special third_party.toml file. So we pass False as
        # is_third_party_toml, and we can give an empty parsed toml file as it
        # won't be used then.

        # cxx v1.0.56 (/path/to/chromium/src/third_party/rust/cxx/v1/crate)
        r = gen._parse_cargo_tree_dependency_line(args, dict(), False, lines[0])
        self.assertEqual(r, None)  # not a dependency

        # ├── cxxbridge-macro v1.0.56 (proc-macro)
        r = gen._parse_cargo_tree_dependency_line(args, dict(), False, lines[1])
        expected = gen.CargoTreeDependency(cargo.CrateKey(
            "cxxbridge-macro", "1.0.56"),
                                           full_version="1.0.56")
        self.assertEqual(r, expected)

        # │   ├── proc-macro2 v1.0.32 default,proc-macro,span-locations
        r = gen._parse_cargo_tree_dependency_line(args, dict(), False, lines[2])
        expected = gen.CargoTreeDependency(
            cargo.CrateKey("proc-macro2", "1.0.32"),
            full_version="1.0.32",
            features=["default", "proc-macro", "span-locations"])
        self.assertEqual(r, expected)

        # │   │   └── unicode-xid v0.2.2 default
        r = gen._parse_cargo_tree_dependency_line(args, dict(), False, lines[3])
        expected = gen.CargoTreeDependency(cargo.CrateKey(
            "unicode-xid", "0.2.2"),
                                           full_version="0.2.2",
                                           features=["default"])
        self.assertEqual(r, expected)

        # │   ├── quote v1.0.10 (/path/to/chromium/src/third_party/rust/quote/v1/crate) default,proc-macro
        r = gen._parse_cargo_tree_dependency_line(args, dict(), False, lines[4])
        expected = gen.CargoTreeDependency(
            cargo.CrateKey("quote", "1.0.10"),
            full_version="1.0.10",
            crate_path="/path/to/chromium/src/third_party/rust/quote/v1/crate",
            features=["default", "proc-macro"])
        self.assertEqual(r, expected)

        # │   │   └── proc-macro2 v1.0.32 default,proc-macro,span-locations (*)
        r = gen._parse_cargo_tree_dependency_line(args, dict(), False, lines[5])
        expected = gen.CargoTreeDependency(
            cargo.CrateKey("proc-macro2", "1.0.32"),
            full_version="1.0.32",
            features=["default", "proc-macro", "span-locations"])
        self.assertEqual(r, expected)

        # │   └── syn v1.0.81 clone-impls,default,derive,full,parsing,printing,proc-macro,quote
        r = gen._parse_cargo_tree_dependency_line(args, dict(), False, lines[6])
        expected = gen.CargoTreeDependency(cargo.CrateKey("syn", "1.0.81"),
                                           full_version="1.0.81",
                                           features=[
                                               "clone-impls", "default",
                                               "derive", "full", "parsing",
                                               "printing", "proc-macro", "quote"
                                           ])
        self.assertEqual(r, expected)

        # │       ├── proc-macro2 v1.0.32 default,proc-macro,span-locations (*)
        r = gen._parse_cargo_tree_dependency_line(args, dict(), False, lines[7])
        expected = gen.CargoTreeDependency(
            cargo.CrateKey("proc-macro2", "1.0.32"),
            full_version="1.0.32",
            features=["default", "proc-macro", "span-locations"])
        self.assertEqual(r, expected)

        # │       ├── quote v1.0.10 default,proc-macro (*)
        r = gen._parse_cargo_tree_dependency_line(args, dict(), False, lines[8])
        expected = gen.CargoTreeDependency(cargo.CrateKey("quote", "1.0.10"),
                                           full_version="1.0.10",
                                           features=["default", "proc-macro"])
        self.assertEqual(r, expected)

        # │       └── unicode-xid v0.2.2
        r = gen._parse_cargo_tree_dependency_line(args, dict(), False, lines[9])
        expected = gen.CargoTreeDependency(cargo.CrateKey(
            "unicode-xid", "0.2.2"),
                                           full_version="0.2.2")
        self.assertEqual(r, expected)

        # └── link-cplusplus v1.0.5 default
        r = gen._parse_cargo_tree_dependency_line(args, dict(), False,
                                                  lines[10])
        d = gen.CargoTreeDependency(cargo.CrateKey("link-cplusplus", "1.0.5"),
                                    full_version="1.0.5",
                                    features=["default"])
        self.assertEqual(r, d)

    def test_third_party_toml(self):
        args = self.make_args()
        THIRD_PARTY_TOML = """
# This dependency has no extensions
[dependencies]
cxxbridge-macro = "1"

# This dependency has an extension, and is default-visible to first-party code.
[dependencies.link-cplusplus]
build-script-outputs = [ "src/link.rs", "src/cplusplus.rs" ]

# This dependency is not visible to first-party code thanks to the extension.
[dependencies.syn]
allow-first-party-usage = false
"""

        # Here we are simulating parsing our special third_party.toml file, so
        # we need to present the contents of that file to the
        # _parse_cargo_tree_dependency_line() function, in order for it to look
        # for extensions.
        toml_content = toml.loads(THIRD_PARTY_TOML)

        line = "├── cxxbridge-macro v1.0.56 (proc-macro)"
        r = gen._parse_cargo_tree_dependency_line(args, toml_content, True,
                                                  line)
        d = gen.CargoTreeDependency(
            cargo.CrateKey("cxxbridge-macro", "1.0.56"),
            full_version="1.0.56",
            # Deps from third_party.toml are visible to first-party code by
            # default.
            is_for_first_party_code=True)
        self.assertEqual(r, d)

        line = "├── link-cplusplus v1.0.5 default"
        r = gen._parse_cargo_tree_dependency_line(args, toml_content, True,
                                                  line)
        d = gen.CargoTreeDependency(
            cargo.CrateKey("link-cplusplus", "1.0.5"),
            full_version="1.0.5",
            features=["default"],
            is_for_first_party_code=True,
            # link-cplusplus has build script outputs listed in our
            # third_party.toml.
            build_script_outputs={"src/link.rs", "src/cplusplus.rs"})
        self.assertEqual(r, d)

        line = "└── syn v1.0.81 clone-impls,default"
        r = gen._parse_cargo_tree_dependency_line(args, toml_content, True,
                                                  line)
        d = gen.CargoTreeDependency(
            cargo.CrateKey("syn", "1.0.81"),
            full_version="1.0.81",
            features=["clone-impls", "default"],
            # Deps from third_party.toml are visible to first-party code unless
            # they opt out explicitly, which our syn dependency has done.
            is_for_first_party_code=False)
        self.assertEqual(r, d)

    def test_third_party_toml_dev_deps(self):
        args = self.make_args()
        THIRD_PARTY_TOML = """
[dependencies]
# Nothing. We're testing dev-dependencies here.

[dev-dependencies]
cxxbridge-macro = "1"

# This dependency has an extension, and is default-visible to first-party code.
[dev-dependencies.link-cplusplus]
build-script-outputs = [ "src/link.rs", "src/cplusplus.rs" ]

# This dependency is not visible to first-party code thanks to the extension.
[dev-dependencies.syn]
allow-first-party-usage = false
"""

        # Here we are simulating parsing our special third_party.toml file, so
        # we need to present the contents of that file to the
        # _parse_cargo_tree_dependency_line() function, in order for it to look
        # for extensions.
        toml_content = toml.loads(THIRD_PARTY_TOML)

        line = "├── cxxbridge-macro v1.0.56 (proc-macro)"
        r = gen._parse_cargo_tree_dependency_line(args, toml_content, True,
                                                  line)
        d = gen.CargoTreeDependency(
            cargo.CrateKey("cxxbridge-macro", "1.0.56"),
            full_version="1.0.56",
            # Deps from third_party.toml are visible to first-party code by
            # default.
            is_for_first_party_code=True)
        self.assertEqual(r, d)

        line = "├── link-cplusplus v1.0.5 default"
        r = gen._parse_cargo_tree_dependency_line(args, toml_content, True,
                                                  line)
        d = gen.CargoTreeDependency(
            cargo.CrateKey("link-cplusplus", "1.0.5"),
            full_version="1.0.5",
            features=["default"],
            is_for_first_party_code=True,
            # link-cplusplus has build script outputs listed in our
            # third_party.toml.
            build_script_outputs={"src/link.rs", "src/cplusplus.rs"})
        self.assertEqual(r, d)

        line = "└── syn v1.0.81 clone-impls,default"
        r = gen._parse_cargo_tree_dependency_line(args, toml_content, True,
                                                  line)
        d = gen.CargoTreeDependency(
            cargo.CrateKey("syn", "1.0.81"),
            full_version="1.0.81",
            features=["clone-impls", "default"],
            # Deps from third_party.toml are visible to first-party code unless
            # they opt out explicitly, which our syn dependency has done.
            is_for_first_party_code=False)
        self.assertEqual(r, d)

    def test_get_archs_third_party_toml(self):
        crate_key = None  # For third_party.toml there's none.
        usage_data = None  # For third_party.toml there's none.

        # Test what happens when there is a `target` on a dependency.
        toml_content = toml.loads("""
[dependencies]
all-platform-crate = "1"

[target."cfg(windows)".dependencies]
windows-only-crate = "2"
        """)

        # Test case without a target specified on the command line. Since
        # there's differences per-architecture in the TOML, we will have to test
        # all architectures.
        arch_specific = gen._get_archs_of_interest(toml_content, usage_data,
                                                   None)
        # Not true as there's a arch-specific dependency.
        self.assertFalse(arch_specific.is_single_arch())
        self.assertSetEqual(self.all_archs(), arch_specific.archs_to_test())

        # Test case with a target specified on the command line that matches
        # the target. We only need to test the target the user specified.
        arch_specific = gen._get_archs_of_interest(toml_content, usage_data,
                                                   "x86_64-pc-windows-msvc")
        # We only ever care about one arch, so everything is single-arch.
        self.assertTrue(arch_specific.is_single_arch())
        self.assertSetEqual({"x86_64-pc-windows-msvc"},
                            arch_specific.archs_to_test())

        # Test case with a target specified on the command line that does not
        # match the target. We only need to test the target the user specified,
        # and we're not smart enough to prune it yet..
        arch_specific = gen._get_archs_of_interest(toml_content, usage_data,
                                                   "x86_64-unknown-linux-gnu")
        # We only ever care about one arch, so everything is single-arch.
        self.assertTrue(arch_specific.is_single_arch())
        self.assertSetEqual({"x86_64-unknown-linux-gnu"},
                            arch_specific.archs_to_test())

        # Test what happens when there is no `target` on a dependency.
        toml_content = toml.loads("""
[dependencies]
all-platform-crate = "1"
        """)

        # Test case without a target specified on the command line.
        arch_specific = gen._get_archs_of_interest(toml_content, usage_data,
                                                   None)
        # Is true since we can apply a single architecture's result to other
        # ones.
        self.assertTrue(arch_specific.is_single_arch())
        # We only need to test one architecture and copy its results to the
        # rest.
        self.assertEqual(len(arch_specific.archs_to_test()), 1)

        # We don't care which architecture will be tested, but in the next test
        # case we do, so make sure the arbitrary architecture here isn't doesn't
        # unluckily collide with our next test.
        self.assertFalse({"x86_64-pc-windows-msvc"
                          }.issubset(arch_specific.archs_to_test()))

        # Test case with a target specified on the command line.
        arch_specific = gen._get_archs_of_interest(toml_content, usage_data,
                                                   "x86_64-pc-windows-msvc")
        self.assertTrue(arch_specific.is_single_arch())
        # We should test the architecture specified.
        self.assertSetEqual({"x86_64-pc-windows-msvc"},
                            arch_specific.archs_to_test())

        # Ensure we are OK with a [target] section with no dependencies
        toml_content = toml.loads("""
[dependencies]
all-platform-crate = "1"

[target."cfg(windows)"]
        """)

        arch_specific = gen._get_archs_of_interest(toml_content, usage_data,
                                                   None)
        self.assertTrue(arch_specific.is_single_arch())

    def make_fake_parent(self, child_name: str,
                         archset_where_parent_used: compiler.ArchSet,
                         parent_is_arch_specific: bool,
                         parent_has_arch_specific_deps: bool
                         ) -> gen.ArchSpecific:
        if not parent_has_arch_specific_deps:
            toml_content = toml.loads("""
[dependencies]
{} = "1"
          """.format(child_name))
        else:
            toml_content = toml.loads("""
[target."cfg(windows)".dependencies]
{} = "1"
        """.format(child_name))
        parent_crate_data = gen.PerCrateData()
        parent_crate_data.arch_specific = parent_is_arch_specific
        parent_crate_data.used_on_archs = archset_where_parent_used
        return gen._get_archs_of_interest(toml_content, parent_crate_data, None)

    def make_fake_dep(self, dep_key: cargo.CrateKey,
                      parent_requested_features_of_dep: list[str],
                      dep_for_first_party_code: bool,
                      dep_build_script_outputs: set[str]):
        return gen.CargoTreeDependency(
            dep_key,
            features=parent_requested_features_of_dep,
            is_for_first_party_code=dep_for_first_party_code,
            build_script_outputs=dep_build_script_outputs)

    def add_fake_dependency_edges(self, build_data: gen.BuildData,
                                  new_keys: set[cargo.CrateKey], **kwargs):
        parent_name: str = kwargs["parent_name"]
        parent_arch_specific = self.make_fake_parent(
            kwargs["dep_name"], kwargs["archset_where_parent_used"],
            kwargs["parent_is_arch_specific"],
            kwargs["parent_has_arch_specific_deps"])
        dep_key = cargo.CrateKey(kwargs["dep_name"], "1.2.3")
        dep = self.make_fake_dep(dep_key,
                                 kwargs["parent_requested_features_of_dep"],
                                 kwargs["dep_for_first_party_code"],
                                 kwargs["dep_build_script_outputs"])

        parent_crate_key = cargo.CrateKey(parent_name,
                                          "1.2.3") if parent_name else None

        gen._add_edges_for_dep_on_target_arch(build_data, parent_crate_key,
                                              parent_arch_specific,
                                              kwargs["parent_usage"],
                                              kwargs["parent_output"], dep,
                                              kwargs["computing_arch"],
                                              new_keys)
        return dep_key

    def test_add_edges(self):
        build_data = gen.BuildData()
        new_keys = set()

        for_first_party_code = True
        build_script_outputs = {"src/child-outs.rs"}

        dep_key = self.add_fake_dependency_edges(
            build_data,
            new_keys,
            parent_name=None,  # No parent crate from third_party.toml.
            # Parent used everywhere.
            archset_where_parent_used=compiler.ArchSet.ALL(),
            parent_is_arch_specific=False,
            parent_has_arch_specific_deps=False,
            # computing_arch is chosen arbitrarily since the result will apply
            # to all as parent_has_arch_specific_deps is False
            computing_arch="aarch64-apple-darwin",
            # Parent used for a binary.
            parent_usage=cargo.CrateUsage.FOR_NORMAL,
            # Adding a build-dependency.
            parent_output=cargo.CrateBuildOutput.BUILDRS,
            dep_name="child-name",
            parent_requested_features_of_dep=["feature1", "feature2"],
            dep_for_first_party_code=for_first_party_code,
            dep_build_script_outputs=build_script_outputs)

        # The new crate was added to new_keys
        self.assertSetEqual(new_keys, {dep_key})

        # The child crate should be in the BuildData
        self.assertSetEqual(build_data.for_buildrs.all_crates(), {dep_key})
        self.assertSetEqual(build_data.for_normal.all_crates(), {dep_key})
        self.assertSetEqual(build_data.for_tests.all_crates(), {dep_key})
        # In buildrs usage, (since the parent was building BUILDRS output),
        # the dependency will be used and have data populated.
        with build_data.for_buildrs.per_crate(dep_key) as crate:
            # The requested features will be used on _all_ architectures, even
            # since the ArchSpecific tests one but applies to all.
            features = crate.features.all_archsets()
            self.assertListSortedEqual(features, [
                ("feature1", compiler.ArchSet.ALL()),
                ("feature2", compiler.ArchSet.ALL()),
            ])
            # Similarly, the crate is used everywhere. This should always be a
            # superset of the architectures where features are enabled.
            self.assertEqual(crate.used_on_archs, compiler.ArchSet.ALL())
            # Since the ArchSpecific said the parent is not arch-specific, and
            # there's no arch-specific deps, the dependency will not be arch-
            # specific.
            self.assertFalse(crate.arch_specific)
            self.assertEqual(crate.for_first_party, for_first_party_code)
            self.assertEqual(crate.build_script_outputs, build_script_outputs)
            # There's no edges from the new dependency since we haven't added
            # those.
            for o in cargo.CrateBuildOutput:
                self.assertSetEqual(crate.deps[o].all_deps(),
                                    set(),
                                    msg="For output type {}".format(o))

        # Other usages are not used, and have no features enabled, since the
        # parent crate is only using this dependency for tests so far, as those
        # are the only edges we added.
        with build_data.for_normal.per_crate(dep_key) as crate:
            self.assertListSortedEqual(crate.features.all_archsets(), [])
            self.assertEqual(crate.used_on_archs, compiler.ArchSet.EMPTY())
            self.assertFalse(crate.arch_specific)
            self.assertEqual(crate.for_first_party, for_first_party_code)
            self.assertEqual(crate.build_script_outputs, build_script_outputs)
        with build_data.for_tests.per_crate(dep_key) as crate:
            self.assertListSortedEqual(crate.features.all_archsets(), [])
            self.assertEqual(crate.used_on_archs, compiler.ArchSet.EMPTY())
            self.assertFalse(crate.arch_specific)
            self.assertEqual(crate.for_first_party, for_first_party_code)
            self.assertEqual(crate.build_script_outputs, build_script_outputs)

        # The parent would have edges to the child but this was a dep from the
        # third_party.toml, so there's no parent.

        # Now add a transitive dependency, which will have a parent: the child
        # that we added above. We will only add edges to the grand child on
        # Windows.
        parent_key = dep_key
        for_first_party_code = False
        build_script_outputs = {"src/grand-child-outs.rs"}
        dep_key = self.add_fake_dependency_edges(
            build_data,
            new_keys,
            parent_name="child-name",  # The parent crate of the grand-child.
            # Parent used everywhere.
            archset_where_parent_used=compiler.ArchSet.ALL(),
            parent_is_arch_specific=False,
            parent_has_arch_specific_deps=True,
            # Act as if cargo-tree sees the grand-child on Windows, so we're
            # adding an edge there. Since parent_has_arch_specific_deps is True,
            # the edge won't be added to other architectures automatically.
            computing_arch="x86_64-pc-windows-msvc",
            # Parent was used for buildrs.
            parent_usage=cargo.CrateUsage.FOR_BUILDRS,
            # Adding a build-dependency.
            parent_output=cargo.CrateBuildOutput.BUILDRS,
            dep_name="grand-child-name",
            parent_requested_features_of_dep=["featureA", "featureB"],
            dep_for_first_party_code=False,
            dep_build_script_outputs=build_script_outputs)
        current_archset = compiler.ArchSet(initial={"x86_64-pc-windows-msvc"})

        # The new crate was added to new_keys
        self.assertSetEqual(new_keys, {parent_key, dep_key})

        # The grand-child crate should be in the BuildData
        self.assertSetEqual(build_data.for_buildrs.all_crates(),
                            {parent_key, dep_key})
        self.assertSetEqual(build_data.for_normal.all_crates(),
                            {parent_key, dep_key})
        self.assertSetEqual(build_data.for_tests.all_crates(),
                            {parent_key, dep_key})
        # In buildrs usage, (since its parent was building BUILDRS output),
        # the dependency will be used and have data populated.
        with build_data.for_buildrs.per_crate(dep_key) as crate:
            # The requested features will be used on the architecture we're
            # processing, since they can't be generalized to all architectures.
            features = crate.features.all_archsets()
            self.assertListSortedEqual(features,
                                       [("featureA", current_archset),
                                        ("featureB", current_archset)])
            # Similarly, the crate is used on the architecture we're processing.
            self.assertEqual(crate.used_on_archs, current_archset)
            # Since the dep is in a target rule (parent_has_arch_specific_deps),
            # the crate is considered arch-specific.
            self.assertTrue(crate.arch_specific)
            self.assertEqual(crate.for_first_party, for_first_party_code)
            self.assertEqual(crate.build_script_outputs, build_script_outputs)
            # There's no edges from the new dependency since we haven't added
            # those.
            for o in cargo.CrateBuildOutput:
                self.assertSetEqual(crate.deps[o].all_deps(),
                                    set(),
                                    msg="For output type {}".format(o))

        # Other usages are not used, and have no features enabled, since the
        # parent crate is only using this dependency for tests so far, as those
        # are the only edges we added.
        with build_data.for_normal.per_crate(dep_key) as crate:
            self.assertListSortedEqual(crate.features.all_archsets(), [])
            self.assertEqual(crate.used_on_archs, compiler.ArchSet.EMPTY())
            self.assertFalse(crate.arch_specific)
            self.assertEqual(crate.for_first_party, for_first_party_code)
            self.assertEqual(crate.build_script_outputs, build_script_outputs)
        with build_data.for_tests.per_crate(dep_key) as crate:
            self.assertListSortedEqual(crate.features.all_archsets(), [])
            self.assertEqual(crate.used_on_archs, compiler.ArchSet.EMPTY())
            self.assertFalse(crate.arch_specific)
            self.assertEqual(crate.for_first_party, for_first_party_code)
            self.assertEqual(crate.build_script_outputs, build_script_outputs)

        # The child crate now depends on the grand-child crate on the computed.
        # architecture, only for it's buildrs script.
        with build_data.for_buildrs.per_crate(parent_key) as crate:
            self.assertSetEqual(
                crate.deps[cargo.CrateBuildOutput.BUILDRS].all_deps(),
                {dep_key})
            self.assertSetEqual(
                crate.deps[cargo.CrateBuildOutput.NORMAL].all_deps(), set())
            self.assertSetEqual(
                crate.deps[cargo.CrateBuildOutput.TESTS].all_deps(), set())
        # We haven't considered the parent crate being built into a crate's
        # lib/bin as a normal dependency, or tests as a dev-dependency so
        # there's no edges.
        with build_data.for_normal.per_crate(parent_key) as crate:
            for o in cargo.CrateBuildOutput:
                self.assertSetEqual(crate.deps[o].all_deps(),
                                    set(),
                                    msg="For output type {}".format(o))
        with build_data.for_tests.per_crate(parent_key) as crate:
            for o in cargo.CrateBuildOutput:
                self.assertSetEqual(crate.deps[o].all_deps(),
                                    set(),
                                    msg="For output type {}".format(o))

        new_keys = set()
        # Now we again compute edges between the child and grand-child, for
        # another architecture, and for the child's normal build output. It
        # can use different features of the grand-child there.
        dep_key = self.add_fake_dependency_edges(
            build_data,
            new_keys,
            parent_name="child-name",  # The parent crate of the grand-child.
            # Parent used everywhere.
            archset_where_parent_used=compiler.ArchSet.ALL(),
            parent_is_arch_specific=False,
            parent_has_arch_specific_deps=True,
            # Act as if cargo-tree sees the grand-child on Fuchsia, so we're
            # adding an edge there. Since parent_has_arch_specific_deps is True,
            # the edge won't be added to other architectures automatically.
            computing_arch="x86_64-fuchsia",
            # Parent was used for a lib/bin.
            parent_usage=cargo.CrateUsage.FOR_NORMAL,
            # Adding a build-dependency.
            parent_output=cargo.CrateBuildOutput.BUILDRS,
            dep_name="grand-child-name",
            parent_requested_features_of_dep=["featureB", "featureC"],
            dep_for_first_party_code=False,
            dep_build_script_outputs=build_script_outputs)
        current_archset = compiler.ArchSet(initial={"x86_64-fuchsia"})
        before_archset = compiler.ArchSet(initial={"x86_64-pc-windows-msvc"})
        union_archset = compiler.ArchSet(
            initial={"x86_64-pc-windows-msvc", "x86_64-fuchsia"})

        # The grand-child is being used on a new platform, and is arch-specific,
        # so it will need to compute its deps again.
        self.assertSetEqual(new_keys, {dep_key})

        # In buildrs usage, (since its parent was building BUILDRS output),
        # the dependency will be used and have data populated.
        with build_data.for_buildrs.per_crate(dep_key) as crate:
            # The requested features will be used on the architecture we're
            # processing, since they can't be generalized to all architectures.
            features = crate.features.all_archsets()
            self.assertListSortedEqual(features, [
                ("featureA", before_archset),
                ("featureB", union_archset),
                ("featureC", current_archset),
            ])
            # The crate is used on the union architectures.
            self.assertEqual(crate.used_on_archs, union_archset)
            # There's no edges from the new dependency since we haven't added
            # those.
            for o in cargo.CrateBuildOutput:
                self.assertSetEqual(crate.deps[o].all_deps(),
                                    set(),
                                    msg="For output type {}".format(o))

        # Other usages are not used, and have no features enabled, since the
        # parent crate is only using this dependency for tests so far, as those
        # are the only edges we added.
        with build_data.for_normal.per_crate(dep_key) as crate:
            self.assertListSortedEqual(crate.features.all_archsets(), [])
            self.assertEqual(crate.used_on_archs, compiler.ArchSet.EMPTY())
            self.assertFalse(crate.arch_specific)
            self.assertEqual(crate.for_first_party, for_first_party_code)
            self.assertEqual(crate.build_script_outputs, build_script_outputs)
        with build_data.for_tests.per_crate(dep_key) as crate:
            self.assertListSortedEqual(crate.features.all_archsets(), [])
            self.assertEqual(crate.used_on_archs, compiler.ArchSet.EMPTY())
            self.assertFalse(crate.arch_specific)
            self.assertEqual(crate.for_first_party, for_first_party_code)
            self.assertEqual(crate.build_script_outputs, build_script_outputs)

        # The child crate still depends on the grand-child crate for its buildrs
        # script when building built as a build-dependency.
        with build_data.for_buildrs.per_crate(parent_key) as crate:
            self.assertSetEqual(
                crate.deps[cargo.CrateBuildOutput.BUILDRS].all_deps(),
                {dep_key})
            self.assertSetEqual(
                crate.deps[cargo.CrateBuildOutput.NORMAL].all_deps(), set())
            self.assertSetEqual(
                crate.deps[cargo.CrateBuildOutput.TESTS].all_deps(), set())
        # The child crate now _also_ depends on the grand-child crate for its
        # buildrs script when building built as a normal dependency.
        with build_data.for_normal.per_crate(parent_key) as crate:
            self.assertSetEqual(
                crate.deps[cargo.CrateBuildOutput.BUILDRS].all_deps(),
                {dep_key})
            self.assertSetEqual(
                crate.deps[cargo.CrateBuildOutput.NORMAL].all_deps(), set())
            self.assertSetEqual(
                crate.deps[cargo.CrateBuildOutput.TESTS].all_deps(), set())
        # We haven't considered the parent crate being built into a crate's
        # tests as a dev-dependency so there's no edges.
        with build_data.for_tests.per_crate(parent_key) as crate:
            for o in cargo.CrateBuildOutput:
                self.assertSetEqual(crate.deps[o].all_deps(),
                                    set(),
                                    msg="For output type {}".format(o))

    def test_copyright_year(self):
        modern = b"# Copyright 2001 The Chromium Authors"
        with tempfile.NamedTemporaryFile() as f:
            f.write(modern)
            f.flush()
            self.assertEqual(gen._get_copyright_year(f.name), "2001")

        ancient = b"# Copyright (c) 2001 The Chromium Authors. All rights reserved."
        with tempfile.NamedTemporaryFile() as f:
            f.write(ancient)
            f.flush()
            self.assertEqual(gen._get_copyright_year(f.name), "2001")

        self.assertEqual(gen._get_copyright_year("/file/does/not/exist"),
                         str(datetime.now().year))
