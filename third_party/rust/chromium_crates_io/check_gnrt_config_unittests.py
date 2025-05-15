#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from check_gnrt_config import (
    _GetExtraKvForCrateName,
    CheckMultiversionCrates,
)


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
        self.assertTrue("foo@1.2.3, foo@4.5.6" in msg)
        self.assertTrue("[crate.foo.extra_kv]" in msg)
        self.assertTrue("multiversion_cleanup_bug = " in msg)


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
