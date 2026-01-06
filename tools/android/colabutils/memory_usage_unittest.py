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
from colabutils.memory_usage import (MemoryUsageView, TreeNode,
                                     _aggregate_nodes)

_REALISTIC_JSON = """
[
  {
    "name": "main",
    "value": 0,
    "children": [
      {
        "name": "foo",
        "value": 0,
        "children": [
          {
            "name": "malloc",
            "value": 8
          }
        ]
      },
      {
        "name": "malloc",
        "value": 16
      },
      {
        "name": "bar",
        "value": 0,
        "children": [
          {
            "name": "malloc",
            "value": 48
          }
        ]
      }
    ]
  }
]
"""

_SIMPLE_JSON_WITH_DELTAS = """
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


class MemoryUsageViewTest(unittest.TestCase):

    def test_json_preserved(self):
        view = MemoryUsageView.from_json(_SIMPLE_JSON_WITH_DELTAS)
        dumped_json = view.to_json()
        self.assertEqual(json.loads(_SIMPLE_JSON_WITH_DELTAS),
                         json.loads(dumped_json))

    def test_from_json_defaults(self):
        json_data = '''[{"name": "root"}]'''
        view = MemoryUsageView.from_json(json_data)
        self.assertEqual(len(view.roots), 1)
        self.assertEqual(view.roots[0].name, 'root')
        self.assertEqual(view.roots[0].value, 0)
        self.assertEqual(view.roots[0].delta, 0)
        self.assertEqual(view.roots[0].children, [])

    def test_from_json_empty_name_stays_empty(self):
        json_data = '''[{"name": ""}]'''
        view = MemoryUsageView.from_json(json_data)
        self.assertEqual(len(view.roots), 1)
        self.assertEqual(view.roots[0].name, '')

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
        view = MemoryUsageView.from_df(df, demangler=None, aggregate=False)
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

    def test_from_df_empty_name_converts_to_unknown(self):
        df_data = {
            'callsite_id': [1, 2, 3],
            'depth': [0, 0, 1],
            'frame_name': ['root_a', 'root_b', ''],
            'parent_callsite_id': [None, None, 1],
            'total_size_bytes': [100, 200, 50],
        }
        df = pd.DataFrame(df_data)
        view = MemoryUsageView.from_df(df, demangler=None, aggregate=True)
        self.assertEqual(2, len(view.roots))
        root_b_node, root_a_node = view.roots
        self.assertEqual('root_a', root_a_node.name)
        self.assertEqual(100 + 50, root_a_node.value)
        self.assertEqual(1, len(root_a_node.children))
        empty_name_node = root_a_node.children[0]
        self.assertEqual('unknown', empty_name_node.name)
        self.assertEqual(50, empty_name_node.value)

    def test_aggregation_sorting(self):
        # Check all cumulative values and the order.
        view = MemoryUsageView.from_json(_REALISTIC_JSON)
        view.roots = _aggregate_nodes(view.roots)
        self.assertEqual(1, len(view.roots))
        root_node = view.roots[0]
        self.assertEqual('main', root_node.name)
        self.assertEqual(48 + 16 + 8, root_node.value)
        self.assertEqual(3, len(root_node.children))
        bar_node, malloc_node, foo_node = root_node.children
        self.assertEqual('bar', bar_node.name)
        self.assertEqual(48, bar_node.value)
        self.assertEqual('malloc', malloc_node.name)
        self.assertEqual(16, malloc_node.value)
        self.assertEqual('foo', foo_node.name)
        self.assertEqual(8, foo_node.value)

    def test_aggregation_sorting2(self):
        # Change the order of the children and verify that
        # it leads to the same order after aggregation.
        view = MemoryUsageView.from_json(_REALISTIC_JSON)
        foo_node, malloc_node, bar_node = view.roots[0].children
        view.roots[0].children = [foo_node, bar_node, malloc_node]
        view.roots = _aggregate_nodes(view.roots)
        root_node = view.roots[0]
        bar_node, malloc_node, foo_node = root_node.children
        self.assertEqual('bar', bar_node.name)
        self.assertEqual(48, bar_node.value)
        self.assertEqual('malloc', malloc_node.name)
        self.assertEqual(16, malloc_node.value)
        self.assertEqual('foo', foo_node.name)
        self.assertEqual(8, foo_node.value)

    def test_aggregation_merge_duplicate_names(self):
        view = MemoryUsageView.from_json(_REALISTIC_JSON)
        # Make two adjacent node names match for merging.
        foo_node, _, bar_node = view.roots[0].children
        self.assertEqual('bar', bar_node.name)
        foo_node.name = 'bar'
        view.roots = _aggregate_nodes(view.roots)
        self.assertEqual(1, len(view.roots))
        root_node = view.roots[0]
        self.assertEqual(2, len(root_node.children))
        bar_node, malloc_node = root_node.children
        self.assertEqual('bar', bar_node.name)
        self.assertEqual(48 + 8, bar_node.value)
        self.assertEqual('malloc', malloc_node.name)
        self.assertEqual(16, malloc_node.value)

if __name__ == '__main__':
    unittest.main()
