#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for parse_gerrit_url.py."""

import unittest
import parse_gerrit_url


class ParseGerritUrlTest(unittest.TestCase):
  """Tests for parse_gerrit_url."""

  def test_standard_url_without_patchset(self):
    url = 'https://chromium-review.googlesource.com/c/chromium/src/+/7916168'
    res = parse_gerrit_url.parse_gerrit_url(url)
    self.assertEqual(res['host'], 'chromium-review.googlesource.com')
    self.assertEqual(res['project'], 'chromium/src')
    self.assertEqual(res['change'], 7916168)
    self.assertEqual(res['patchset'], 1)

  def test_standard_url_with_patchset(self):
    url = 'https://chromium-review.googlesource.com/c/chromium/src/+/7916168/3'
    res = parse_gerrit_url.parse_gerrit_url(url)
    self.assertEqual(res['host'], 'chromium-review.googlesource.com')
    self.assertEqual(res['project'], 'chromium/src')
    self.assertEqual(res['change'], 7916168)
    self.assertEqual(res['patchset'], 3)

  def test_url_with_file_path(self):
    url = 'https://chromium-review.googlesource.com/c/chromium/src/+/7916168/2/chrome/browser/tab.cc'
    res = parse_gerrit_url.parse_gerrit_url(url)
    self.assertEqual(res['host'], 'chromium-review.googlesource.com')
    self.assertEqual(res['project'], 'chromium/src')
    self.assertEqual(res['change'], 7916168)
    self.assertEqual(res['patchset'], 2)

  def test_url_with_query_and_fragment(self):
    url = 'https://chromium-review.googlesource.com/c/chromium/src/+/7916168/4?tab=checks#message'
    res = parse_gerrit_url.parse_gerrit_url(url)
    self.assertEqual(res['host'], 'chromium-review.googlesource.com')
    self.assertEqual(res['project'], 'chromium/src')
    self.assertEqual(res['change'], 7916168)
    self.assertEqual(res['patchset'], 4)

  def test_url_with_trailing_slash(self):
    url = 'https://chromium-review.googlesource.com/c/chromium/src/+/7916168/'
    res = parse_gerrit_url.parse_gerrit_url(url)
    self.assertEqual(res['host'], 'chromium-review.googlesource.com')
    self.assertEqual(res['project'], 'chromium/src')
    self.assertEqual(res['change'], 7916168)
    self.assertEqual(res['patchset'], 1)

  def test_different_project_and_host(self):
    url = 'https://skia-review.googlesource.com/c/skia/+/12345/6'
    res = parse_gerrit_url.parse_gerrit_url(url)
    self.assertEqual(res['host'], 'skia-review.googlesource.com')
    self.assertEqual(res['project'], 'skia')
    self.assertEqual(res['change'], 12345)
    self.assertEqual(res['patchset'], 6)

  def test_invalid_url(self):
    with self.assertRaises(ValueError):
      parse_gerrit_url.parse_gerrit_url('https://example.com/invalid')

  def test_invalid_url_no_c_prefix(self):
    with self.assertRaises(ValueError):
      parse_gerrit_url.parse_gerrit_url(
          'https://chromium-review.googlesource.com/q/status:open')

  def test_invalid_url_no_change_number(self):
    with self.assertRaises(ValueError):
      parse_gerrit_url.parse_gerrit_url(
          'https://chromium-review.googlesource.com/c/chromium/src/+/invalid')

  def test_invalid_empty_url(self):
    with self.assertRaises(ValueError):
      parse_gerrit_url.parse_gerrit_url('')


if __name__ == '__main__':
  unittest.main()
