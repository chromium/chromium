#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import android_res_formatter


class AndroidResFormatterTest(unittest.TestCase):

  def test_format_xml_no_changes(self):
    # Test case where we expect no changes
    input_xml = """<?xml version="1.0" encoding="utf-8"?>
<resources>
  <color name="short_name">text</color>
</resources>
"""
    formatted_xml = android_res_formatter.format_xml(input_xml)
    self.assertEqual(formatted_xml, input_xml)

  def test_format_xml_changes_needed(self):
    # Test case where we expect changes (attribute reordering)
    input_xml = """<?xml version="1.0" encoding="utf-8"?>
<resources>
  <color value="val" name="short_name">text</color>
</resources>
"""
    expected_xml = """<?xml version="1.0" encoding="utf-8"?>
<resources>
  <color name="short_name" value="val">text</color>
</resources>
"""
    formatted_xml = android_res_formatter.format_xml(input_xml)
    self.assertEqual(formatted_xml, expected_xml)

  def test_format_xml_wraps_long_line(self):
    # Test case where we expect wrapping due to line length > 100
    input_xml = """<?xml version="1.0" encoding="utf-8"?>
<resources>
  <color name="toast_bg_color_baseline_with_a_very_long_name_that_makes_it_exceed_one_hundred_characters">@color/default_bg_color_dark_elev_1_baseline</color>
</resources>
"""
    expected_xml = """<?xml version="1.0" encoding="utf-8"?>
<resources>
  <color name="toast_bg_color_baseline_with_a_very_long_name_that_makes_it_exceed_one_hundred_characters">
    @color/default_bg_color_dark_elev_1_baseline
  </color>
</resources>
"""
    formatted_xml = android_res_formatter.format_xml(input_xml)
    self.assertEqual(formatted_xml, expected_xml)

  def test_format_xml_nested_layouts(self):
    # Test case with nested layouts and attributes that need reordering
    input_xml = """<?xml version="1.0" encoding="utf-8"?>
<LinearLayout value="root_val" name="root_layout" xmlns:android="http://schemas.android.com/apk/res/android">
  <RelativeLayout android:width="match_parent" android:name="inner_layout" android:id="@+id/inner">
    <View value="leaf_val" name="leaf_view" />
  </RelativeLayout>
</LinearLayout>
"""
    expected_xml = """<?xml version="1.0" encoding="utf-8"?>
<LinearLayout
    xmlns:android="http://schemas.android.com/apk/res/android"
    name="root_layout"
    value="root_val">
  <RelativeLayout android:id="@+id/inner" android:name="inner_layout" android:width="match_parent">
    <View name="leaf_view" value="leaf_val" />
  </RelativeLayout>
</LinearLayout>
"""
    formatted_xml = android_res_formatter.format_xml(input_xml)
    self.assertEqual(formatted_xml, expected_xml)


if __name__ == '__main__':
  unittest.main()
