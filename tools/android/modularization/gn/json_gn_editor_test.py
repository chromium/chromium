#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest
import tempfile
import textwrap
import os

import json_gn_editor


class BuildFileTest(unittest.TestCase):
    def test_avoid_reformatting_gn_file_if_no_ast_changed(self):
        text = textwrap.dedent('''\
        android_library("target_name") {
          deps =[":local_dep"]} #shouldn't change
        ''')
        with tempfile.NamedTemporaryFile(mode='w') as f:
            f.write(text)
            f.flush()
            with json_gn_editor.BuildFile(f.name, '/') as build_file:
                pass
            with open(f.name, 'r') as f_after:
                self.assertEqual(f_after.read(), text)

    def test_split_dep_works_for_full_relative_abs_deps(self):
        with tempfile.TemporaryDirectory() as rootdir:
            java_subdir = os.path.join(rootdir, 'java')
            os.mkdir(java_subdir)
            build_gn_path = os.path.join(java_subdir, 'BUILD.gn')
            with open(build_gn_path, 'w') as f:
                f.write(
                    textwrap.dedent('''\
            android_library("java") {
            }

            android_library("target1") {
              deps = [ "//java:java" ]
            }

            android_library("target2") {
              deps += [ ":java" ]
            }

            android_library("target3") {
              public_deps = [ "//java" ]
            }
            '''))
            with json_gn_editor.BuildFile(build_gn_path,
                                          rootdir) as build_file:
                # Test both explicit and implied dep resolution works.
                build_file.split_dep('//java:java', '//other_dir:other_dep')
                build_file.split_dep('//java', '//other_dir:other_dep2')
            with open(build_gn_path, 'r') as f:
                self.assertEqual(
                    f.read(),
                    textwrap.dedent('''\
            android_library("java") {
            }

            android_library("target1") {
              deps = [
                "//java:java",
                "//other_dir:other_dep",
                "//other_dir:other_dep2",
              ]
            }

            android_library("target2") {
              deps += [
                ":java",
                "//other_dir:other_dep",
                "//other_dir:other_dep2",
              ]
            }

            android_library("target3") {
              public_deps = [
                "//java",
                "//other_dir:other_dep",
                "//other_dir:other_dep2",
              ]
            }
            '''))

    def test_split_dep_does_not_duplicate_deps(self):
        with tempfile.TemporaryDirectory() as rootdir:
            java_subdir = os.path.join(rootdir, 'java')
            os.mkdir(java_subdir)
            build_gn_path = os.path.join(java_subdir, 'BUILD.gn')
            with open(build_gn_path, 'w') as f:
                f.write(
                    textwrap.dedent('''\
            android_library("target") {
              deps = [
                "//java",
                "//other_dir:other_dep",
              ]
            }
            '''))
            with json_gn_editor.BuildFile(build_gn_path,
                                          rootdir) as build_file:
                build_file.split_dep('//java:java', '//other_dir:other_dep')
            with open(build_gn_path, 'r') as f:
                self.assertEqual(
                    f.read(),
                    textwrap.dedent('''\
            android_library("target") {
              deps = [
                "//java",
                "//other_dir:other_dep",
              ]
            }
            '''))


if __name__ == '__main__':
    unittest.main()
