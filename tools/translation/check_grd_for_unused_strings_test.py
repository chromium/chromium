#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import textwrap
import unittest
from pyfakefs import fake_filesystem_unittest

import check_grd_for_unused_strings

_GrdParseResult = check_grd_for_unused_strings.GrdParseResult


class ParseIdsTest(fake_filesystem_unittest.TestCase):

  def setUp(self):
    self.setUpPyfakefs()

  def test_parse_cpp(self):
    for suffix in ['.h', '.cc', '.mm', '.swift']:
      path = pathlib.Path(f'/test{suffix}')
      content = 'IDS_FOO IDS_BAR\nIDS_BAZ_123'
      self.fs.create_file(path, contents=content)
      ids = check_grd_for_unused_strings.parse_used_ids(path)
      self.assertEqual({'IDS_FOO', 'IDS_BAR', 'IDS_BAZ_123'}, ids)

  def test_parse_cpp_embedded_rc(self):
    path = pathlib.Path('/test.cc')
    content = 'IDS_FOO_BASE'
    self.fs.create_file(path, contents=content)
    ids = check_grd_for_unused_strings.parse_used_ids(path)
    self.assertEqual({'IDS_FOO', 'IDS_FOO_BASE'}, ids)

  def test_parse_java(self):
    path = pathlib.Path('/Test.java')
    content = 'R.string.foo_bar R.string.baz R . string\n  .zzz'
    self.fs.create_file(path, contents=content)
    ids = check_grd_for_unused_strings.parse_used_ids(path)
    self.assertEqual({'IDS_FOO_BAR', 'IDS_BAZ', 'IDS_ZZZ'}, ids)

  def test_parse_android_manifest(self):
    path = pathlib.Path('/AndroidManifest.xml')
    content = """
        <manifest xmlns:android="http://schemas.android.com/apk/res/android">
          <application android:label="@string/app_name">
            <activity android:name=".MainActivity"
                      android:label="@string/main_title">
            </activity>
          </application>
        </manifest>
        """
    self.fs.create_file(path, contents=content)
    ids = check_grd_for_unused_strings.parse_used_ids(path)
    self.assertEqual({'IDS_APP_NAME', 'IDS_MAIN_TITLE'}, ids)

  def test_parse_grd(self):
    for suffix in ['.grd', '.grdp']:
      path = pathlib.Path(f'/test{suffix}')
      content = """
          <grit>
            <release>
              <messages>
                <part file="sub.grdp" />

                <message name="IDS_MSG_1">Message 1</message>
                <message name="IDS_MSG_2">Message 2</message>
                <!-- Should be ignored -->
                <message name="NON_IDS_MSG">Not a message</message>
              </messages>
            </release>
            <outputs>
              <output filename="test.pak" type="data_package" />
            </outputs>
          </grit>
          """
      self.fs.create_file(path, contents=content)
      result = check_grd_for_unused_strings.parse_grd(path)
      self.assertEqual({'IDS_MSG_1', 'IDS_MSG_2'}, result.ids)
      self.assertEqual({'sub.grdp'}, result.parts)
      self.assertFalse(result.generates_runtime_strings)

  def test_parse_unknown_extension(self):
    path = pathlib.Path('/test.txt')
    self.fs.create_file(path, contents='IDS_FOO')
    ids = check_grd_for_unused_strings.parse_used_ids(path)
    self.assertEqual(set(), ids)


class FilterGrdsTest(unittest.TestCase):

  def test_filter_grds(self):
    a_path = pathlib.Path('a.grd')
    b_path = pathlib.Path('b.grdp')
    c_path = pathlib.Path('c.grdp')
    results_by_path = {
        a_path:
        _GrdParseResult(
            ids={'IDS_A'},
            parts={'b.grdp'},
            generates_runtime_strings=True,
        ),
        b_path:
        _GrdParseResult(ids={'IDS_B'}),
        c_path:
        _GrdParseResult(ids={'IDS_C'}),
    }
    filtered = check_grd_for_unused_strings.filter_grds(results_by_path)
    self.assertEqual({pathlib.Path('c.grdp'): _GrdParseResult(ids={'IDS_C'})},
                     filtered)

  def test_filter_grds_recursive(self):
    a_path = pathlib.Path('a.grd')
    b_path = pathlib.Path('b.grdp')
    c_path = pathlib.Path('c.grdp')
    results_by_path = {
        a_path: _GrdParseResult(
            parts={'b.grdp'},
            generates_runtime_strings=True,
        ),
        b_path: _GrdParseResult(parts={'c.grdp'}),
        c_path: _GrdParseResult(ids={'IDS_C'}),
    }
    filtered = check_grd_for_unused_strings.filter_grds(results_by_path)
    self.assertEqual({}, filtered)


class RemoveStringsTest(fake_filesystem_unittest.TestCase):

  def setUp(self):
    self.setUpPyfakefs()

  def test_remove_strings(self):
    grd_path = pathlib.Path('/a/test.grd')
    content = textwrap.dedent("""\
        <grit>
          <release>
            <messages>
              <message name = "IDS_REMOVE">Remove</message>
              <!-- Keep -->
              <message name="IDS_KEEP">Keep</message>
            </messages>
          </release>
        </grit>
        """)
    self.fs.create_file(grd_path, contents=content)
    self.fs.create_file('/a/test_grd/IDS_REMOVE.png.sha1', contents='67676767')
    self.fs.create_file('/a/test_grd/IDS_KEEP.png.sha1', contents='67676767')

    check_grd_for_unused_strings.remove_strings(grd_path, ['IDS_REMOVE'])
    with grd_path.open() as grd_file:
      new_content = grd_file.read()
    expected = textwrap.dedent("""\
        <grit>
          <release>
            <messages>
              <!-- Keep -->
              <message name="IDS_KEEP">Keep</message>
            </messages>
          </release>
        </grit>
        """)
    self.assertEqual(expected, new_content)
    self.assertFalse(pathlib.Path('/a/test_grd/IDS_REMOVE.png.sha1').exists())
    self.assertTrue(pathlib.Path('/a/test_grd/IDS_KEEP.png.sha1').exists())

  def test_remove_empty_tags(self):
    grd_path = pathlib.Path('/test.grd')
    content = textwrap.dedent("""\
        <grit>
          <releases>
            <messages>
              <!-- Keep this comment -->
              <if expr="is_android">
                <message name="IDS_REMOVE">
                  Remove
                </message>
              </if>
              <message name="IDS_KEEP">Keep</message>
            </messages>
          <releases>
        </grit>
        """)
    self.fs.create_file(grd_path, contents=content)

    check_grd_for_unused_strings.remove_strings(grd_path, ['IDS_REMOVE'])
    with grd_path.open() as grd_file:
      new_content = grd_file.read()
    expected = textwrap.dedent("""\
        <grit>
          <releases>
            <messages>
              <!-- Keep this comment -->
              <message name="IDS_KEEP">Keep</message>
            </messages>
          <releases>
        </grit>
        """)
    self.assertEqual(expected, new_content)


if __name__ == '__main__':
  unittest.main()
