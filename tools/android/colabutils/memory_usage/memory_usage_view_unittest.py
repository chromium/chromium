# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Tests for memory_usage_view.py"""

import json
import os
import pandas as pd
import pathlib
import sys
import unittest

_SRC_PATH = pathlib.Path(__file__).resolve().parents[4]
sys.path.append(str(_SRC_PATH / 'tools/android'))
from colabutils.memory_usage.memory_usage_view import (
    MemoryUsageView, TreeNode, _aggregate_nodes, _zip_by_name,
    _compare_node_lists, _prettify_size)

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
        view = MemoryUsageView.from_df(df, demangle=False, aggregate=False)
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
        view = MemoryUsageView.from_df(df, demangle=False, aggregate=True)
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

    def test_zip_by_name_both_empty(self):
        self.assertEqual([], list(_zip_by_name([], [])))

    def test_zip_by_name_one_empty(self):
        node = TreeNode(name='abc')
        self.assertEqual([(node, None)], list(_zip_by_name([node], [])))
        self.assertEqual([(None, node)], list(_zip_by_name([], [node])))
        node2 = TreeNode(name='def')
        self.assertEqual([(None, node), (None, node2)],
                         list(_zip_by_name([], [node, node2])))

    def test_zip_by_name_simple(self):
        left_node1 = TreeNode(name='l1')
        left_node2 = TreeNode(name='l2')
        left_nodes = [left_node1, left_node2]
        right_node1 = TreeNode(name='r1')
        right_node2 = TreeNode(name='r2')
        right_nodes = [right_node1, right_node2]

        result = list(_zip_by_name(left_nodes, right_nodes))

        self.assertEqual(4, len(result))
        self.assertIn((left_node1, None), result)
        self.assertIn((left_node2, None), result)
        self.assertIn((None, right_node1), result)
        self.assertIn((None, right_node2), result)

    def test_zip_by_name_with_intersection(self):
        left_node1 = TreeNode(name='l1')
        left_abc = TreeNode(name='abc')
        left_nodes = [left_node1, left_abc]
        right_abc = TreeNode(name='abc')
        right_node2 = TreeNode(name='r2')
        right_nodes = [right_abc, right_node2]

        result = list(_zip_by_name(left_nodes, right_nodes))

        self.assertEqual(3, len(result))
        self.assertIn((left_abc, right_abc), result)
        self.assertIn((left_node1, None), result)
        self.assertIn((None, right_node2), result)

    def test_compare_nothing(self):
        self.assertEqual([], _compare_node_lists([], []))

    def test_compare_one_node_to_nothing(self):
        node = TreeNode(name='one', value=123)
        result_node = TreeNode(name='one', value=123, delta=-123)

        self.assertEqual([result_node], _compare_node_lists([node], []))

        node.value = 456
        result_node.value = 0
        result_node.delta = 456

        self.assertEqual([result_node], _compare_node_lists([], [node]))

    def test_compare_same_name_nodes(self):
        node_base = TreeNode(name='pikachu', value=123)
        node_new = TreeNode(name='pikachu', value=456)

        result = _compare_node_lists([node_base], [node_new])
        self.assertEqual(1, len(result))
        result_node = result[0]
        self.assertEqual(123, result_node.value)
        self.assertEqual('pikachu', result_node.name)
        self.assertEqual(456 - 123, result_node.delta)

    def test_compare_children(self):
        base_root = TreeNode(name='root', value=30)
        new_root = TreeNode(name='root', value=80)
        base_root.children = [
            TreeNode(name='common_child', value=10),
            TreeNode(name='base_only_child', value=20),
        ]
        new_root.children = [
            TreeNode(name='common_child', value=30),
            TreeNode(name='new_only_child', value=50),
        ]

        result_nodes = _compare_node_lists([base_root], [new_root])
        self.assertEqual(1, len(result_nodes))
        result_root = result_nodes[0]

        self.assertEqual('root', result_root.name)
        self.assertEqual(30, result_root.value)
        self.assertEqual(80 - 30, result_root.delta)

        self.assertEqual(3, len(result_root.children))
        new_only, common_child, base_only = result_root.children

        self.assertEqual('new_only_child', new_only.name)
        self.assertEqual(0, new_only.value)
        self.assertEqual(50, new_only.delta)

        self.assertEqual('common_child', common_child.name)
        self.assertEqual(10, common_child.value)
        self.assertEqual(30 - 10, common_child.delta)

        self.assertEqual('base_only_child', base_only.name)
        self.assertEqual(20, base_only.value)
        self.assertEqual(-20, base_only.delta)

    def test_compare_children_and_single(self):
        base_root = TreeNode(name='root', value=30)
        new_root = TreeNode(name='root', value=80)
        base_root.children = [
            TreeNode(name='child10', value=10),
            TreeNode(name='child20', value=20),
        ]
        result_nodes = _compare_node_lists([base_root], [new_root])
        self.assertEqual(1, len(result_nodes))
        result_root = result_nodes[0]

        self.assertEqual('root', result_root.name)
        self.assertEqual(30, result_root.value)
        self.assertEqual(80 - 30, result_root.delta)

        self.assertEqual(2, len(result_root.children))
        child_10, child_20 = result_root.children
        self.assertEqual(10, child_10.value)
        self.assertEqual(-10, child_10.delta)
        self.assertEqual(20, child_20.value)
        self.assertEqual(-20, child_20.delta)

    def test_prettify_size(self):
        tests = [
            (500, 500),
            (1024, '1 KiB'),
            (1536, '1.5 KiB'),
            (1500, '1.46 KiB'),
            (1048576, '1 MiB'),
            (123456789, '117.74 MiB'),
            (1099511627776, '1024 GiB'),
            (2000000000000, 2000000000000),
        ]

        test_lines = []
        for inp, expected in tests:
            self.assertEqual(expected, _prettify_size(inp))


if __name__ == '__main__':
    unittest.main()
