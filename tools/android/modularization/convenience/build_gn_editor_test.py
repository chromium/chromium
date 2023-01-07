#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import build_gn_editor


class BuildTargetTest(unittest.TestCase):
  def test_get_variable_considers_whole_variable_name(self):
    build_target = build_gn_editor.BuildTarget(
        'type', 'name', '''
        type(name) {
          srcjar_deps = [
            "hi",
          ]

          deps = [
            "bye",
          ]
        }
    ''')

    variable = build_target.get_variable('deps')
    assert variable is not None
    content_list = variable.get_content_as_list()
    assert content_list is not None
    self.assertEqual(content_list.get_elements(), ['"bye"'])


if __name__ == '__main__':
  unittest.main()
