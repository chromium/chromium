#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import checkxmlstyle

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
    self.assertEqual(4, len(list(checkxmlstyle.IncludedFiles(mock_input_api))))

  def testFileExcluded(self):
    lines = []
    mock_input_api = MockInputApi()
    mock_input_api.files = [
        MockFile('chrome/res_test/test.xml', lines),
        MockFile('ui/test/test.xml', lines),
        MockFile('ui/java/res/test.java', lines),
        MockFile('content/java/res.xml', lines),
        MockFile('components/java/test.xml', lines),
        MockFile('test/java/res/test.xml', lines)
    ]
    self.assertEqual(0, len(list(checkxmlstyle.IncludedFiles(mock_input_api))))


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
    mock_input_api.files = [MockFile('chrome/java/res_test/colors.xml', lines)]
    errors = checkxmlstyle._CheckColorReferences(
        mock_input_api, MockOutputApi())
    self.assertEqual(0, len(errors))


class DuplicateColorsTest(unittest.TestCase):

  def testFailure(self):
    lines = ['<color name="color1">#61000000</color>',
             '<color name="color2">#61000000</color>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/colors.xml', lines)]
    errors = checkxmlstyle._CheckDuplicateColors(
        mock_input_api, MockOutputApi())
    self.assertEqual(1, len(errors))
    self.assertEqual(2, len(errors[0].items))
    self.assertEqual('  chrome/java/res_test/colors.xml:1',
                     errors[0].items[0].splitlines()[0])
    self.assertEqual('  chrome/java/res_test/colors.xml:2',
                     errors[0].items[1].splitlines()[0])

  def testSucess(self):
    lines = ['<color name="color1">#61000000</color>',
             '<color name="color1">#FFFFFF</color>']
    mock_input_api = MockInputApi()
    mock_input_api.files = [MockFile('chrome/java/res_test/colors.xml', lines)]
    errors = checkxmlstyle._CheckDuplicateColors(
        mock_input_api, MockOutputApi())
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

  def testSucess(self):
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


if __name__ == '__main__':
  unittest.main()
