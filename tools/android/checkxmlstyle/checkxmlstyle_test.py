#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import checkxmlstyle
import helpers

sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__))))))
from PRESUBMIT_test_mocks import MockFile, MockInputApi, MockOutputApi


class IncludedFilesTest(unittest.TestCase):

  def testFileIncluded(self):
    lines = []
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('chrome/java/res_test/test.xml', lines),
        MockFile('ui/test/java/res/test.xml', lines),
        MockFile('content/java/res_test/test.xml', lines),
        MockFile('components/test/java/res_test/test.xml', lines)
    ]
    self.assertEqual(4,
                     len(list(checkxmlstyle.IncludedFiles(mock_input_api))))

  def testFileExcluded(self):
    lines = []
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('chrome/res_test/test.xml', lines),
        MockFile('ui/test/test.xml', lines),
        MockFile('content/java/res.xml', lines),
        MockFile('components/java/test.xml', lines),
        MockFile('test/java/res/test.xml', lines)
    ]
    self.assertEqual(0,
                     len(list(checkxmlstyle.IncludedFiles(mock_input_api))))


class ColorFormatTest(unittest.TestCase):

  def testColorFormatIgnoredFile(self):
    lines = ['<color name="color1">#61000000</color>',
             '<color name="color2">#FFFFFF</color>',
             '<color name="color3">#CCC</color>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.java', lines)]
    errors = checkxmlstyle._CheckColorFormat(mock_input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testColorFormatTooShort(self):
    lines = ['<color name="color1">#61000000</color>',
             '<color name="color2">#FFFFFF</color>',
             '<color name="color3">#CCC</color>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.xml', lines)]
    errors = checkxmlstyle._CheckColorFormat(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(1, len(errors[0].items))
    self.assertEqual('  chrome/java/res_test/test.xml:3',
                     errors[0].items[0].splitlines()[0])

  def testColorInvalidAlphaValue(self):
    lines = ['<color name="color1">#61000000</color>',
             '<color name="color2">#FEFFFFFF</color>',
             '<color name="color3">#FFCCCCCC</color>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.xml', lines)]
    errors = checkxmlstyle._CheckColorFormat(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(1, len(errors[0].items))
    self.assertEqual('  chrome/java/res_test/test.xml:3',
                     errors[0].items[0].splitlines()[0])

  def testColorFormatLowerCase(self):
    lines = ['<color name="color1">#61000000</color>',
             '<color name="color2">#EFFFFFFF</color>',
             '<color name="color3">#CcCcCC</color>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.xml', lines)]
    errors = checkxmlstyle._CheckColorFormat(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(1, len(errors[0].items))
    self.assertEqual('  chrome/java/res_test/test.xml:3',
                     errors[0].items[0].splitlines()[0])


class ColorReferencesTest(unittest.TestCase):

  def testVectorDrawbleIgnored(self):
    lines = ['<vector',
             'tools:targetApi="21"',
             'android:fillColor="#CCCCCC">',
             '</vector>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.xml', lines)]
    result = checkxmlstyle._CheckColorReferences(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(result))
    self.assertEqual(result[0].type, 'warning')

  def testInvalidReference(self):
    lines = ['<TextView',
             'android:textColor="#FFFFFF" />']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.xml', lines)]
    errors = checkxmlstyle._CheckColorReferences(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(1, len(errors[0].items))
    self.assertEqual('  chrome/java/res_test/test.xml:2',
                     errors[0].items[0].splitlines()[0])

  def testValidReference(self):
    lines = ['<TextView',
             'android:textColor="@color/color1" />']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.xml', lines)]
    errors = checkxmlstyle._CheckColorReferences(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testValidReferenceInColorResources(self):
    lines = ['<color name="color1">#61000000</color>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile(helpers.COLOR_PALETTE_RELATIVE_PATH, lines)]
    errors = checkxmlstyle._CheckColorReferences(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testReferenceInSemanticColors(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile(helpers.COLOR_PALETTE_PATH,
                 ['<resources><color name="a">#f0f0f0</color></resources>']),
        MockFile('ui/android/java/res/values/semantic_colors_non_adaptive.xml',
                 [
                     '<resources>',
                     '<color name="b">@color/hello</color>',
                     '<color name="c">@color/a</color>',
                     '</resources>'
                 ]),
        MockFile('ui/android/java/res/values/semantic_colors_adaptive.xml',
                 ['<color name="c">@color/a</color>'])
    ]
    errors = checkxmlstyle._CheckSemanticColorsReferences(
      mock_input_api, MockOutputApi())
    self.assertEqual(1, len(errors))

  def testReferenceInColorPalette(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile(helpers.COLOR_PALETTE_PATH,
                 ['<resources><color name="foo">#f0f0f0</color></resources>']),
        MockFile('ui/android/java/res/values/semantic_colors_adaptive.xml',
                 ['<color name="b">@color/foo</color>']),
        MockFile('ui/android/java/res/values/colors.xml', [
            '<color name="c">@color/b</color>',
            '<color name="d">@color/b</color>',
            '<color name="e">@color/foo</color>'
        ])
    ]
    warnings = checkxmlstyle._CheckColorPaletteReferences(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(warnings))

  def testValidReferenceInNonAdaptive(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile(helpers.COLOR_PALETTE_PATH,
                 ['<resources><color name="a">#f0f0f0</color></resources>']),
        MockFile('ui/android/java/res/values/semantic_colors_non_adaptive.xml',
                 [
                     '<resources>',
                     '<color name="b">@color/a</color>',
                     '<color name="c">@color/b</color>',
                     '</resources>'
                 ])
    ]
    errors = checkxmlstyle._CheckSemanticColorsReferences(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(errors))


class DuplicateColorsTest(unittest.TestCase):

  def testFailure(self):
    lines = ['<color name="color1">#61000000</color>',
             '<color name="color2">#61000000</color>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile(helpers.COLOR_PALETTE_RELATIVE_PATH, lines)]
    errors = checkxmlstyle._CheckDuplicateColors(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(2, len(errors[0].items))
    self.assertEqual('  %s:1' % helpers.COLOR_PALETTE_RELATIVE_PATH,
                     errors[0].items[0].splitlines()[0])
    self.assertEqual('  %s:2' % helpers.COLOR_PALETTE_RELATIVE_PATH,
                     errors[0].items[1].splitlines()[0])

  def testSuccess(self):
    lines = ['<color name="color1">#61000000</color>',
             '<color name="color1">#FFFFFF</color>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/colors.xml', lines)]
    errors = checkxmlstyle._CheckDuplicateColors(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(errors))


class NonDynamicColorsTest(unittest.TestCase):
  class MockColorStateListSet:
    def get(self):
      return {'color_state_list'}

  def testFailure(self):
    lines = [
        'app:tint="@color/tint_color" />',
        'android:background="@color/bg_color"',
        '<color name="fake_semantic_color">@color/palettele_color</color>',
    ]
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/colors.xml', lines)]
    errors = checkxmlstyle._CheckNonDynamicColorReference(
        mock_input_api,
        MockOutputApi(),
        lazy_color_state_list_set=self.MockColorStateListSet())
    self.assertEqual(1, len(errors))
    self.assertEqual(len(lines), len(errors[0].items))

  def testSuccess(self):
    lines = [
        'app:tint="@color/color_state_list" />',
        'android:background="@color/color_state_list"',
        '<color name="fake_semantic_color">@color/color_state_list</color>',
    ]
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('chrome/java/res_test/colors.xml', lines),
    ]
    errors = checkxmlstyle._CheckNonDynamicColorReference(
        mock_input_api,
        MockOutputApi(),
        lazy_color_state_list_set=self.MockColorStateListSet())
    self.assertEqual(0, len(errors))


class XmlNamespacePrefixesTest(unittest.TestCase):

  def testFailure(self):
    lines = ['xmlns:chrome="http://schemas.android.com/apk/res-auto"']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/file.xml', lines)]
    errors = checkxmlstyle._CheckXmlNamespacePrefixes(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(1, len(errors[0].items))
    self.assertEqual('  chrome/java/res_test/file.xml:1',
                     errors[0].items[0].splitlines()[0])

  def testSuccess(self):
    lines = ['xmlns:app="http://schemas.android.com/apk/res-auto"']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/file.xml', lines)]
    errors = checkxmlstyle._CheckXmlNamespacePrefixes(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(errors))


class TextAppearanceTest(unittest.TestCase):

  def testFailure_Style(self):
    lines = [
        '<resource>',
        '<style name="TestTextAppearance">',
        '<item name="android:textColor">@color/default_text_color_link</item>',
        '<item name="android:textSize">14sp</item>',
        '<item name="android:textStyle">bold</item>',
        '<item name="android:fontFamily">some-font</item>',
        '<item name="android:textAllCaps">true</item>',
        '</style>',
        '</resource>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.xml', lines)]
    errors = checkxmlstyle._CheckTextAppearance(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(5, len(errors[0].items))
    self.assertEqual(
        ('  chrome/java/res_test/test.xml:2 contains attribute '
         'android:textColor'),
        errors[0].items[0].splitlines()[0])
    self.assertEqual(
        '  chrome/java/res_test/test.xml:2 contains attribute android:textSize',
        errors[0].items[1].splitlines()[0])
    self.assertEqual(
        ('  chrome/java/res_test/test.xml:2 contains attribute '
         'android:textStyle'),
        errors[0].items[2].splitlines()[0])
    self.assertEqual(
        ('  chrome/java/res_test/test.xml:2 contains attribute '
         'android:fontFamily'),
        errors[0].items[3].splitlines()[0])
    self.assertEqual(
        ('  chrome/java/res_test/test.xml:2 contains attribute '
         'android:textAllCaps'),
        errors[0].items[4].splitlines()[0])

  def testSuccess_Style(self):
    lines = [
        '<resource>',
        '<style name="TextAppearance.Test">',
        '<item name="android:textColor">@color/default_text_color_link</item>',
        '<item name="android:textSize">14sp</item>',
        '<item name="android:textStyle">bold</item>',
        '<item name="android:fontFamily">some-font</item>',
        '<item name="android:textAllCaps">true</item>',
        '</style>',
        '<style name="TestStyle">',
        '<item name="android:background">some_background</item>',
        '</style>',
        '</resource>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.xml', lines)]
    errors = checkxmlstyle._CheckTextAppearance(mock_input_api, MockOutputApi())
    self.assertEqual(0, len(errors))

  def testFailure_Widget(self):
    lines_top_level = [
        '<TextView',
        'xmlns:android="http://schemas.android.com/apk/res/android"',
        'android:layout_width="match_parent"',
        'android:layout_height="@dimen/snippets_article_header_height"',
        'android:textColor="@color/snippets_list_header_text_color"',
        'android:textSize="14sp" />']
    lines_subcomponent_widget = [
        '<RelativeLayout',
        'xmlns:android="http://schemas.android.com/apk/res/android"',
        'android:layout_width="match_parent"',
        'android:layout_height="wrap_content">',
        '<View',
        'android:textColor="@color/error_text_color"',
        'android:textSize="@dimen/text_size_medium"',
        'android:textAllCaps="true"',
        'android:background="@drawable/infobar_shadow_top"',
        'android:visibility="gone" />',
        '</RelativeLayout>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('chrome/java/res_test/test1.xml', lines_top_level),
        MockFile('chrome/java/res_test/test2.xml', lines_subcomponent_widget)]
    errors = checkxmlstyle._CheckTextAppearance(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(5, len(errors[0].items))
    self.assertEqual(
        ('  chrome/java/res_test/test1.xml:5 contains attribute '
         'android:textColor'),
        errors[0].items[0].splitlines()[0])
    self.assertEqual(
        ('  chrome/java/res_test/test1.xml:6 contains attribute '
         'android:textSize'),
        errors[0].items[1].splitlines()[0])
    self.assertEqual(
        ('  chrome/java/res_test/test2.xml:6 contains attribute '
         'android:textColor'),
        errors[0].items[2].splitlines()[0])
    self.assertEqual(
        ('  chrome/java/res_test/test2.xml:7 contains attribute '
         'android:textSize'),
        errors[0].items[3].splitlines()[0])
    self.assertEqual(
        ('  chrome/java/res_test/test2.xml:8 contains attribute '
         'android:textAllCaps'),
        errors[0].items[4].splitlines()[0])

  def testSuccess_Widget(self):
    lines = [
        '<RelativeLayout',
        'xmlns:android="http://schemas.android.com/apk/res/android"',
        'android:layout_width="match_parent"',
        'android:layout_height="wrap_content">',
        '<View',
        'android:background="@drawable/infobar_shadow_top"',
        'android:visibility="gone" />',
        '</RelativeLayout>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.xml', lines)]
    errors = checkxmlstyle._CheckTextAppearance(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(errors))


class NewTextAppearanceTest(unittest.TestCase):

  def testFailure(self):
    lines = [
        '<resource>',
        '<style name="TextAppearance.Test">',
        '<item name="android:textColor">@color/default_text_color_link</item>',
        '<item name="android:textSize">14sp</item>',
        '</style>',
        '</resource>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.xml', lines)]
    errors = checkxmlstyle._CheckNewTextAppearance(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(1, len(errors[0].items))
    self.assertEqual(
        '  chrome/java/res_test/test.xml:2',
        errors[0].items[0].splitlines()[0])

  def testSuccess(self):
    lines = [
        '<resource>',
        '<style name="TextAppearanceTest">',
        '<item name="android:textColor">@color/default_text_color_link</item>',
        '<item name="android:textSize">14sp</item>',
        '</style>',
        '</resource>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/test.xml', lines)]
    errors = checkxmlstyle._CheckNewTextAppearance(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(errors))


class ImageAccessibilityTextTest(unittest.TestCase):

  def testIgnoreContentDescription(self):
    xmlChanges = [
        '<ImageView',
        '    android:id="@+id/obvious_image"',
        '    tools:ignore="ContentDescription"',
        '    android:layout_width="wrap_content"',
        '    android:layout_height="match_parent"',
        '    android:gravity="center_vertical"',
        '/>'
    ]
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('chrome/android/java/res/layout/new_imageview.xml', xmlChanges)
    ]
    result = checkxmlstyle._CheckImportantForAccessibility(
        mock_input_api, MockOutputApi())

    self.assertEqual(1, len(result))
    self.assertEqual(1, len(result[0].items))
    self.assertEqual('  chrome/android/java/res/layout/new_imageview.xml:3',
                       result[0].items[0].splitlines()[0])

class UnfavoredLayoutAttributesTest(unittest.TestCase):

  def testLineSpacingAttributesUsage(self):
    xmlChanges = [
        '<TextView android:id="@+id/test"',
        '    android:lineSpacingExtra="42dp"',
        '    android:lineSpacingMultiplier="42dp"',
        '/>',
        '<TextViewWithLeading android:id="@+id/test2"',
        '    app:leading="42dp"',
        '/>'
    ]
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('ui/android/java/res/layout/new_textview.xml', xmlChanges)
    ]
    result = checkxmlstyle._CheckLineSpacingAttribute(
        mock_input_api, MockOutputApi())

    self.assertEqual(1, len(result))
    self.assertEqual(2, len(result[0].items))
    self.assertEqual('  ui/android/java/res/layout/new_textview.xml:2',
                      result[0].items[0].splitlines()[0])
    self.assertEqual('  ui/android/java/res/layout/new_textview.xml:3',
                      result[0].items[1].splitlines()[0])

class UnfavoredWidgetsTest(unittest.TestCase):

  def testButtonCompatUsage(self):
    xmlChanges = [
        '<Button',
        '   android:text="@string/hello"',
        '   android:text="@color/modern_blue_600"',
        '/>',
        '',
        '<android.support.v7.widget.AppCompatButton',
        '   android:text="@string/welcome"',
        '   android:color="@color/modern_purple_300"',
        '/>',
        '<org.chromium.ui.widget.ButtonCompat',
        '   android:id="@+id/action_button"',
        '/>'
    ]
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('ui/android/java/res/layout/dropdown_item.xml', xmlChanges)
    ]
    result = checkxmlstyle._CheckButtonCompatWidgetUsage(
        mock_input_api, MockOutputApi())

    self.assertEqual(1, len(result))
    self.assertEqual(2, len(result[0].items))
    self.assertEqual('  ui/android/java/res/layout/dropdown_item.xml:1',
                     result[0].items[0].splitlines()[0])
    self.assertEqual('  ui/android/java/res/layout/dropdown_item.xml:6',
                     result[0].items[1].splitlines()[0])


class StringResourcesTest(unittest.TestCase):
  def testInfavoredQuotations(self):
    xmlChanges = ('''<grit><release><messages>
      <message name="IDS_TEST_0">
          <ph><ex>Hi</ex></ph>, it\u0027s a good idea
      </message>
      <message name="IDS_TEST_1">
          <ph><ex>Yes</ex></ph>, it\u2019s a good idea
      </message>
      <message name="IDS_TEST_2">
        Go to \u0022Settings\u0022 and
        \u0022Menus\u0022
      </message>
      <message name="IDS_TEST_3">
        Go to \u201CSettings\u201D
        \u0022Menus\u0023
      </message>
      <message name="IDS_TEST_4">
        Go to \u201CSettings\u201D
        \u201CMenus\u201D
      </message>
        <part file="site_settings.grdp" />
          </messages></release></grit>''').splitlines()

    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('ui/android/string/chrome_android_string.grd', xmlChanges)
    ]
    result = checkxmlstyle._CheckStringResourceQuotesPunctuations(
        mock_input_api, MockOutputApi())

    self.assertEqual(1, len(result))
    self.assertEqual(4, len(result[0].items))
    self.assertEqual('  ui/android/string/chrome_android_string.grd:3',
                     result[0].items[0].splitlines()[0])
    self.assertEqual('  ui/android/string/chrome_android_string.grd:9',
                     result[0].items[1].splitlines()[0])
    self.assertEqual('  ui/android/string/chrome_android_string.grd:10',
                     result[0].items[2].splitlines()[0])
    self.assertEqual('  ui/android/string/chrome_android_string.grd:14',
                     result[0].items[3].splitlines()[0])


  def testInfavoredEllipsis(self):
    xmlChanges = ('''<grit><release><messages>
      <message name="IDS_TEST_0">
          <ph><ex>Hi</ex></ph>, file is downloading\u002E\u002E\u002E
      </message>
      <message name="IDS_TEST_1">
          <ph><ex>Yes</ex></ph>, file is downloading\u2026
      </message>
      <message name="IDS_TEST_2">
          <ph><ex>Oh</ex></ph>, file is downloaded\u002E
      </message>
        <part file="site_settings.grdp" />
          </messages></release></grit>''').splitlines()

    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('ui/android/string/chrome_android_string.grd', xmlChanges)
    ]
    result = checkxmlstyle._CheckStringResourceEllipsisPunctuations(
        mock_input_api, MockOutputApi())

    self.assertEqual(1, len(result))
    self.assertEqual(1, len(result[0].items))
    self.assertEqual('  ui/android/string/chrome_android_string.grd:3',
                     result[0].items[0].splitlines()[0])


class BadStyleReferenceTest(unittest.TestCase):
  def testFailure(self):
    lines = [
        ' android:theme="style/foo"',
        ' android:theme="@stylefoo"',
        ' android:theme="@foo"',
        ' android:theme="@foo/foo"',
        ' android:theme="attr/foo"',
        ' android:theme="?attrfoo"',
        ' android:theme="@attr/foo"',
        ' android:theme="?anroid:attrfoo"',
        ' android:theme="?foo"',
        ' android:theme="?foo/foo"',
        ' android:textAppearance="foo"',
        ' style="foo"',
    ]
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile(helpers.COLOR_PALETTE_RELATIVE_PATH, lines)
    ]
    warnings = checkxmlstyle._CheckBadStyleReference(mock_input_api,
                                                     MockOutputApi())
    self.assertEqual(1, len(warnings))
    self.assertEqual(12, len(warnings[0].items))

  def testSuccess(self):
    lines = [
        ' android:theme="@style/foo"',
        ' android:theme="?attr/foo"',
        ' android:theme="?android:attr/foo"',
        ' android:textAppearance="@style/foo"',
        ' android:textAppearance="?attr/foo"',
        ' android:textAppearance="?android:attr/foo"',
        ' style="@style/foo"',
        ' style="?attr/foo"',
        ' style="?android:attr/foo"',
        ' foo="foo/stuff"',
        ' foo="foo"',
        ' foo="@foo"',
        ' foo="?foo"',
    ]
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/colors.xml', lines)]
    warnings = checkxmlstyle._CheckBadStyleReference(mock_input_api,
                                                     MockOutputApi())
    self.assertEqual(0, len(warnings))


if __name__ == '__main__':
  unittest.main()
