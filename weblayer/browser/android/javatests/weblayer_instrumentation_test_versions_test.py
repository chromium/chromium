#!/usr/bin/env python
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Test helper functions in skew test wrapper.

import textwrap
import unittest

import weblayer_instrumentation_test_versions


class ExpectationsTest(unittest.TestCase):

  def testMatchesImpl(self):
    tag_matches = weblayer_instrumentation_test_versions.tag_matches

    self.assertTrue(tag_matches('impl_lte_84', impl_version=83))
    self.assertTrue(tag_matches('impl_lte_83', impl_version=83))
    self.assertFalse(tag_matches('impl_lte_82', impl_version=83))

  def testMatchesClient(self):
    tag_matches = weblayer_instrumentation_test_versions.tag_matches

    self.assertTrue(tag_matches('client_lte_84', client_version=83))
    self.assertTrue(tag_matches('client_lte_83', client_version=83))
    self.assertFalse(tag_matches('client_lte_82', client_version=83))

  def testMismatchedTargets(self):
    tag_matches = weblayer_instrumentation_test_versions.tag_matches

    self.assertFalse(tag_matches('client_lte_80', impl_version=80))
    self.assertFalse(tag_matches('impl_lte_80', client_version=80))

  def testMatchesGreater(self):
    tag_matches = weblayer_instrumentation_test_versions.tag_matches

    self.assertFalse(tag_matches('client_gte_84', client_version=83))
    self.assertTrue(tag_matches('client_gte_83', client_version=83))
    self.assertTrue(tag_matches('client_gte_82', client_version=83))

  def testMatchesInvalid(self):
    tag_matches = weblayer_instrumentation_test_versions.tag_matches

    self.assertRaises(AssertionError, tag_matches, 'impl_lte_82')
    self.assertRaises(AssertionError, tag_matches, 'impl_x_82',
                      client_version=83)
    self.assertRaises(AssertionError, tag_matches, 'client_lte_82',
                      client_version='83')

  def testTestsToSkip(self):
    expectation_contents = textwrap.dedent("""
    # tags: [ impl_lte_83 client_lte_80 ]
    # results: [ Skip ]

    [ impl_lte_83 ] weblayer.NavigationTest#testIntent [ Skip ]
    [ client_lte_80 ] weblayer.ExternalTest#testUser [ Skip ]
    """)
    tests_to_skip = weblayer_instrumentation_test_versions.tests_to_skip

    self.assertEquals(
        tests_to_skip(expectation_contents, impl_version=80),
        ['weblayer.NavigationTest#testIntent'])
    self.assertEquals(
        tests_to_skip(expectation_contents, impl_version=83),
        ['weblayer.NavigationTest#testIntent'])
    self.assertEquals(tests_to_skip(expectation_contents, impl_version=84), [])

    self.assertEquals(
        tests_to_skip(expectation_contents, client_version=79),
        ['weblayer.ExternalTest#testUser'])
    self.assertEquals(
        tests_to_skip(expectation_contents, client_version=80),
        ['weblayer.ExternalTest#testUser'])
    self.assertEquals(
        tests_to_skip(expectation_contents, client_version=81), [])


if __name__ == '__main__':
  unittest.main()
