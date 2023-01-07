#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.print_dependencies_helper."""

import unittest
import print_dependencies_helper


class TestHelperFunctions(unittest.TestCase):
    """Unit tests for the helper functions in the module."""

    def test_package_multiple_matches(self):
        """Tests getting all valid keys for the given input."""
        test_keys = ['o.c.another.test', 'o.c.test', 'o.c.testing']
        valid_keys = print_dependencies_helper.get_valid_package_keys_matching(
            test_keys, 'test')
        self.assertEqual(valid_keys, ['o.c.another.test', 'o.c.test'])

    def test_package_no_match(self):
        """Tests getting no valid keys when there is no matching key."""
        test_keys = ['o.c.another.test', 'o.c.test', 'o.c.testing']
        valid_keys = print_dependencies_helper.get_valid_package_keys_matching(
            test_keys, 'nomatch')
        self.assertEqual(valid_keys, [])

    def test_class_multiple_matches(self):
        """Tests getting multiple valid keys that match the given input."""
        test_keys = ['o.c.test.Test', 'o.c.testing.Test', 'o.c.test.Wrong']
        valid_keys = print_dependencies_helper.get_valid_class_keys_matching(
            test_keys, 'Test')
        self.assertEqual(valid_keys, ['o.c.test.Test', 'o.c.testing.Test'])

    def test_class_full_match(self):
        """Tests getting a valid key when there is an exact fully-qualified
        match."""
        test_keys = ['o.c.test.Test', 'o.c.testing.Test', 'o.c.test.Wrong']
        valid_keys = print_dependencies_helper.get_valid_class_keys_matching(
            test_keys, 'o.c.test.Test')
        self.assertEqual(valid_keys, ['o.c.test.Test'])

    def test_class_no_match_lower_case(self):
        """Tests getting no valid keys when there would only be a match if the
        input was case-insensitive."""
        test_keys = ['o.c.test.Test', 'o.c.testing.Test', 'o.c.test.Wrong']
        valid_keys = print_dependencies_helper.get_valid_class_keys_matching(
            test_keys, 'test')
        self.assertEqual(valid_keys, [])

    def test_class_no_match_partial(self):
        """Tests getting no valid keys when the match is only partial in the
        class name."""
        test_keys = ['o.c.test.Test', 'o.c.testing.Test', 'o.c.test.Wrong']
        valid_keys = print_dependencies_helper.get_valid_class_keys_matching(
            test_keys, 'est')
        self.assertEqual(valid_keys, [])

    def test_class_no_match_partial_qualified(self):
        """Tests getting no valid keys when the match is only partial in the
        fully qualified name."""
        test_keys = ['o.c.test.Test', 'o.c.testing.Test', 'o.c.test.Wrong']
        valid_keys = print_dependencies_helper.get_valid_class_keys_matching(
            test_keys, '.test.Test')
        self.assertEqual(valid_keys, [])
