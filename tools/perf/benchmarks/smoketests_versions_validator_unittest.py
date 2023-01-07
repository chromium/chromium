# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for multiple version checking in system_health_smoke_test.py."""

import unittest

from benchmarks import system_health_smoke_test

class TestSmoketestsVersionValidator(unittest.TestCase):
  def test_version_check_pass(self):
    all_stories = [
        'a:2018', 'a', 'b:2018', 'b:2019', 'c:2018']
    disabled_stories = frozenset([
        'a:2018', 'b:2018'])
    stories = system_health_smoke_test.find_multi_version_stories(
        all_stories, disabled_stories)
    self.assertEqual(0, len(stories), "Should be no result from version check")

  def test_version_check_multi_colon_prefix_with_version(self):
    all_stories = [
        'story:name:a:2018', 'story:name:a:2019']
    disabled_stories = frozenset(['story:name:a:2018'])
    stories = system_health_smoke_test.find_multi_version_stories(
        all_stories, disabled_stories)
    self.assertEqual(0, len(stories), "Should be no result from version check")

  def test_version_check_multi_colon_prefix_without_version(self):
    all_stories = [
        'story:name:a', 'story:name:a:2019']
    disabled_stories = frozenset(['story:name:a:2019'])
    stories = system_health_smoke_test.find_multi_version_stories(
        all_stories, disabled_stories)
    self.assertEqual(0, len(stories), "Should be no result from version check")

  def test_version_check_fail_without_version(self):
    all_stories = ['a', 'a:2019', 'b:2019']
    disabled_stories = frozenset(['x'])
    stories = system_health_smoke_test.find_multi_version_stories(
        all_stories, disabled_stories)
    self.assertEqual(1, len(stories), 'Expecting 1 item in stories.')
    self.assertIn('a', stories)
    self.assertIn('a', stories['a'])
    self.assertIn('a:2019', stories['a'])

  def test_version_check_fail_with_version(self):
    all_stories = ['a:2018', 'a:2019', 'b:2019']
    disabled_stories = frozenset(['a'])
    stories = system_health_smoke_test.find_multi_version_stories(
        all_stories, disabled_stories)
    self.assertEqual(1, len(stories), 'Expecting 1 item in stories.')
    self.assertIn('a', stories)
    self.assertIn('a:2018', stories['a'])
    self.assertIn('a:2019', stories['a'])

  def test_version_check_fail_multiple_story(self):
    all_stories = ['a', 'a:2019', 'b:2018', 'b:2019', 'c:2018']
    disabled_stories = frozenset(['x'])
    stories = system_health_smoke_test.find_multi_version_stories(
        all_stories, disabled_stories)
    self.assertEqual(2, len(stories), 'Expecting 2 item in stories.')
    self.assertIn('a', stories)
    self.assertIn('a', stories['a'])
    self.assertIn('a:2019', stories['a'])
    self.assertIn('b', stories)
    self.assertIn('b:2018', stories['b'])
    self.assertIn('b:2019', stories['b'])

if __name__ == '__main__':
  unittest.main()
