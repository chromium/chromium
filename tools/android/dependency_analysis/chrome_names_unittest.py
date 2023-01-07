# Lint as: python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.print_dependencies_helper."""

import unittest
import chrome_names


class TestChromeNames_ShortenClass(unittest.TestCase):
    """Unit tests for shorten_class."""

    def test_shorten_chrome_browser_class(self):
        self.assertEqual(
            '.c.b.flags.ChromeFeatureList',
            chrome_names.shorten_class(
                'org.chromium.chrome.browser.flags.ChromeFeatureList'))

    def test_shorten_base_class(self):
        self.assertEqual(
            '.base.Callback',
            chrome_names.shorten_class('org.chromium.base.Callback'))

    def test_shorten_components_class(self):
        self.assertEqual(
            '.components.prefs.PrefService',
            chrome_names.shorten_class(
                'org.chromium.components.prefs.PrefService'))

    def test_does_not_shorten_third_party_class(self):
        self.assertEqual('org.other_project.Class',
                         chrome_names.shorten_class('org.other_project.Class'))


class TestChromeNames_ShortenBuildTarget(unittest.TestCase):
    """Unit tests for shorten_build_target."""

    def test_shorten_chrome_java(self):
        self.assertEqual(
            'chrome_java',
            chrome_names.shorten_build_target('//chrome/android:chrome_java'))

    def test_shorten_chrome_browser(self):
        self.assertEqual(
            '//c/b/flags:java',
            chrome_names.shorten_build_target('//chrome/browser/flags:java'))

    def test_does_not_shorten_other_directories(self):
        self.assertEqual('//base:base_java',
                         chrome_names.shorten_build_target('//base:base_java'))
