#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import crate_utils

from run_presubmits import (
    # Functions under test:
    _GetExtraKvForCrateName,
    CheckCargoTomlIsSorted,
    CheckGnrtConfigTomlIsSorted,
    CheckExplicitAllowUnsafeForAllCrates,
    CheckMultiversionCrates,
    CheckNonapplicableGnrtConfigEntries,
)


class CheckCargoTomlIsSortedTests(unittest.TestCase):

    def testSortedSimple(self):
        cargo_toml = {
            "dependencies": {
                "abc": "1.2.3",
                "def": "1.2.3",
                "xyz": "1.2.3",
            }
        }
        self.assertEqual("", CheckCargoTomlIsSorted(cargo_toml))

    def testNotSortedSimple(self):
        cargo_toml = {
            "dependencies": {
                "def": "1.2.3",
                "xyz": "1.2.3",
                "abc": "1.2.3",
            }
        }
        msg = CheckCargoTomlIsSorted(cargo_toml)
        self.assertTrue("`abc` should appear before `xyz`" in msg)

    def testNotSortedElaborateMixedIn(self):
        cargo_toml = {
            "dependencies": {
                "abc": "1.2.3",
                "def": {
                    "version": "1.2.3"
                },
                "xyz": "1.2.3",
            }
        }
        msg = CheckCargoTomlIsSorted(cargo_toml)
        self.assertTrue(
            "Simple string entries should appear before table entries" in msg)
        self.assertTrue("`xyz` should appear after `def`" in msg)

    def testSortedSimpleAndElaborate(self):
        cargo_toml = {
            "dependencies": {
                "abc": "1.2.3",
                "def": "1.2.3",
                "yyy": {
                    "version": "1.2.3"
                },
                "zzz": {
                    "version": "1.2.3"
                },
            }
        }
        self.assertEqual("", CheckCargoTomlIsSorted(cargo_toml))

    def testNotSortedElaborate(self):
        cargo_toml = {
            "dependencies": {
                "abc": "1.2.3",
                "def": "1.2.3",
                "zzz": {
                    "version": "1.2.3"
                },
                "yyy": {
                    "version": "1.2.3"
                },
            }
        }
        msg = CheckCargoTomlIsSorted(cargo_toml)
        self.assertTrue("`yyy` should appear before `zzz`" in msg)


class CheckExplicitAllowUnsafeForAllCratesTests(unittest.TestCase):

    def testAllowUnsafeMissing(self):
        crate_ids = set(["foo@1.2.3"])
        gnrt_config = {}
        msg = CheckExplicitAllowUnsafeForAllCrates(crate_ids, gnrt_config)
        self.assertTrue("explicitly specifies `allow_unsafe = ...`" in msg)
        self.assertTrue(
            "all crates that `chromium_crates_io` depends on" in msg)
        self.assertTrue("gnrt_config.toml" in msg)
        self.assertTrue("[crate.foo.extra_kv]" in msg)
        self.assertTrue("allow_unsafe = " in msg)

    def testAllowUnsafePresent(self):
        crate_ids = set(["foo@1.2.3"])
        gnrt_config = {"crate": {"foo": {"extra_kv": {"allow_unsafe": True}}}}
        self.assertEqual(
            "", CheckExplicitAllowUnsafeForAllCrates(crate_ids, gnrt_config))

    def testFakeRootCrateIsIgnored(self):
        crate_ids = set(["chromium@1.2.3"])
        gnrt_config = {}
        self.assertEqual(
            "", CheckExplicitAllowUnsafeForAllCrates(crate_ids, gnrt_config))

    def testPlaceholderCratesAreIgnored(self):
        crate_ids = set([crate_utils.GetPlaceholderCrateIdForTesting()])
        gnrt_config = {}
        self.assertEqual(
            "", CheckExplicitAllowUnsafeForAllCrates(crate_ids, gnrt_config))


class CheckGnrtConfigTomlIsSortedTests(unittest.TestCase):

    def testSorted(self):
        gnrt_config = {
            "crate": {
                "aaa": {
                    "extra_kv": {}
                },
                "bbb": {
                    "extra_kv": {}
                },
            }
        }
        self.assertEqual("", CheckGnrtConfigTomlIsSorted(None, gnrt_config))

    def testNotSortedElaborate(self):
        gnrt_config = {
            "crate": {
                "bbb": {
                    "extra_kv": {}
                },
                "aaa": {
                    "extra_kv": {}
                },
            }
        }
        msg = CheckGnrtConfigTomlIsSorted(None, gnrt_config)
        self.assertTrue("`aaa` should appear before `bbb`" in msg)


class CheckMultiversionCratesTests(unittest.TestCase):

    def testNoMultiversionCrates(self):
        crate_ids = set(["foo@1.2.3", "bar@4.5.6"])
        gnrt_config = {}
        self.assertEqual("", CheckMultiversionCrates(crate_ids, gnrt_config))

    def testBugLinkPresent(self):
        crate_ids = set(["foo@1.2.3", "foo@4.5.6"])
        gnrt_config = {
            "crate": {
                "foo": {
                    "extra_kv": {
                        "multiversion_cleanup_bug": "blah"
                    }
                }
            }
        }
        self.assertEqual("", CheckMultiversionCrates(crate_ids, gnrt_config))

    def testProblemDetection(self):
        crate_ids = set(["foo@1.2.3", "foo@4.5.6"])
        gnrt_config = {}
        msg = CheckMultiversionCrates(crate_ids, gnrt_config)
        self.assertTrue("multiple versions of the same crate" in msg)
        self.assertTrue("gnrt_config.toml" in msg)
        self.assertTrue("foo@1.2.3, foo@4.5.6" in msg)
        self.assertTrue("[crate.foo.extra_kv]" in msg)
        self.assertTrue("multiversion_cleanup_bug = " in msg)


class CheckNonapplicableGnrtConfigEntriesTests(unittest.TestCase):

    def testNoProblems(self):
        crate_ids = set(["foo@1.2.3"])
        gnrt_config = {"crate": {"foo": {"bar": 123}}}
        self.assertEqual(
            "", CheckNonapplicableGnrtConfigEntries(crate_ids, gnrt_config))

    def testProblem(self):
        crate_ids = set()
        gnrt_config = {"crate": {"foo": {"bar": 123}}}
        msg = CheckNonapplicableGnrtConfigEntries(crate_ids, gnrt_config)
        self.assertTrue("foo" in msg)
        self.assertTrue("gnrt_config.toml" in msg)

    def testPlaceholderCrate(self):
        crate_id = crate_utils.GetPlaceholderCrateIdForTesting()
        crate_name = crate_utils.ConvertCrateIdToCrateName(crate_id)
        crate_ids = set([crate_id])
        gnrt_config = {"crate": {crate_name: {"bar": 123}}}
        msg = CheckNonapplicableGnrtConfigEntries(crate_ids, gnrt_config)
        self.assertTrue(msg != "")


class GetExtraKvForCrateNameTests(unittest.TestCase):

    def testHappyPath(self):
        gnrt_config = {"crate": {"foo": {"extra_kv": {"entry": 123}}}}
        extra_kv = _GetExtraKvForCrateName("foo", gnrt_config)
        self.assertEqual({"entry": 123}, extra_kv)

    def testMissingExtraKv(self):
        """ Verify an empty dictionary is a fallback when no `extra_kv`. """
        gnrt_config = {"crate": {"foo": {"bar": 123}}}
        extra_kv = _GetExtraKvForCrateName("foo", gnrt_config)
        self.assertEqual({}, extra_kv)

    def testMissingCrateEntry(self):
        gnrt_config = {"crate": {"other_crate": {"extra_kv": {"entry": 123}}}}
        extra_kv = _GetExtraKvForCrateName("foo", gnrt_config)
        self.assertEqual({}, extra_kv)

    def testMistypedExtraKv(self):
        """ Test fallback behavior when `extra_kv` is not a dictionary. """
        gnrt_config = {"crate": {"foo": {"extra_kv": 123}}}
        extra_kv = _GetExtraKvForCrateName("foo", gnrt_config)
        self.assertEqual({}, extra_kv)

    def testEmptyConfigSuchAsUsedInSomeTestsHere(self):
        gnrt_config = {}
        extra_kv = _GetExtraKvForCrateName("foo", gnrt_config)
        self.assertEqual({}, extra_kv)


if __name__ == '__main__':
    unittest.main()
