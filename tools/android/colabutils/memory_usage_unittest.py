# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Tests for memory_usage.py"""

import json
import os
import pandas as pd
import pathlib
import sys
import unittest

_SRC_PATH = pathlib.Path(__file__).resolve().parents[3]
sys.path.append(str(_SRC_PATH / 'tools/android'))
from colabutils.memory_usage import (MemoryUsageView, TreeNode)


class MemoryUsageViewTest(unittest.TestCase):

    def test_json_preserved(self):
        simple_json = """
[
  {
    "name": "root1",
    "value": 100,
    "delta": 10,
    "children": [
      {
        "name": "child1_1",
        "value": 50,
        "delta": 5
      },
      {
        "name": "child1_2",
        "value": 30,
        "delta": 3
      }
    ]
  },
  {
    "name": "root2",
    "value": 200,
    "delta": 20
  }
]
"""
        view = MemoryUsageView.from_json(simple_json)
        dumped_json = view.to_json()
        self.assertEqual(json.loads(simple_json), json.loads(dumped_json))

    def test_from_json_defaults(self):
        json_data = '''[{"name": "root"}]'''
        view = MemoryUsageView.from_json(json_data)
        self.assertEqual(len(view.roots), 1)
        self.assertEqual(view.roots[0].name, 'root')
        self.assertEqual(view.roots[0].value, 0)
        self.assertEqual(view.roots[0].delta, 0)
        self.assertEqual(view.roots[0].children, [])

    def test_to_json_no_children(self):
        node = TreeNode(name='test_node')
        view = MemoryUsageView(roots=[node])
        dumped_json = json.loads(view.to_json())
        expected_json = {'name': 'test_node', 'value': 0, 'delta': 0}
        self.assertEqual(dumped_json, [expected_json])

    def test_from_df(self):
        df_data = {
            'callsite_id': [1, 2, 3],
            'depth': [0, 0, 1],
            'frame_name': ['root_a', 'root_b', 'child_a_1'],
            'parent_callsite_id': [None, None, 1],
            'total_size_bytes': [100, 200, 50],
        }
        df = pd.DataFrame(df_data)
        view = MemoryUsageView.from_df(df, demangler=None)
        dumped_json = view.to_json()
        expected_json = """
[
  {
    "name": "root_a",
    "value": 100,
    "delta": 0,
    "children": [
      {
        "name": "child_a_1",
        "value": 50,
        "delta": 0
      }
    ]
  },
  {
    "name": "root_b",
    "value": 200,
    "delta": 0
  }
]
"""
        self.assertEqual(json.loads(expected_json), json.loads(dumped_json))
        self.assertEqual(['root_a', 'root_b'], sorted(view.toplevel_names()))


if __name__ == '__main__':
    unittest.main()
