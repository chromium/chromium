# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for viewing and diffing Sum Trees of memory usage."""

import copy
import jinja2
import json
import os
import pandas as pd
import pathlib
import string
import sys
from dataclasses import dataclass, field

_HAS_IPYTHON = False
try:
    if not 'unittest' in sys.modules:
        # Do not import IPython when running unit tests to avoid module name
        # conflicts. There is `import cProfile as profile` in IPython.
        from IPython.display import HTML, display
        _HAS_IPYTHON = True
except ImportError:
    _HAS_IPYTHON = False

from . import demangler

_MEMORY_USAGE_DIR = pathlib.Path(__file__).resolve().parent
_SRC_PATH = _MEMORY_USAGE_DIR.parents[3]
sys.path.append(str(_SRC_PATH / 'third_party/perfetto/python'))
from perfetto.trace_processor import TraceProcessor


@dataclass
class TreeNode:
    """Node in a memory usage hierarchy."""

    name: str
    value: int = 0
    delta: int = 0
    children: list['TreeNode'] = field(default_factory=list)

    @classmethod
    def from_dict(cls, data: dict) -> 'TreeNode':
        raw_children = data.get('children', [])
        children = [TreeNode.from_dict(child) for child in raw_children]
        params = {k: v for k, v in data.items() if k != 'children'}
        return cls(**params, children=children)


class MemoryUsageView:
    """Holds memory usage information possibly with multiple root nodes."""

    def __init__(self, roots: list[TreeNode]):
        self.roots = roots

    @classmethod
    def from_json(cls, json_data: str) -> 'MemoryUsageView':
        """Creates a MemoryUsageView from a JSON string.

        Provides a simple way to initialize the structure for testing.
        """
        data = json.loads(json_data)
        roots = [TreeNode.from_dict(item) for item in data]
        return cls(roots)

    @classmethod
    def from_heap_dump(cls,
                       trace_file_name: str,
                       demangle: bool = True,
                       aggregate: bool = True) -> 'MemoryUsageView':
        """Creates a MemoryUsageView from a trace file with a heap dump."""
        abs_path = os.path.abspath(trace_file_name)
        with TraceProcessor(trace=abs_path) as trace_processor:
            df = _extract_heap_dump(trace_processor)
            return cls.from_df(df, demangle=demangle, aggregate=aggregate)
        return None

    def to_json(self, **kwargs) -> str:
        """Converts the MemoryUsageView to a JSON string."""
        return json.dumps(self.roots, cls=TreeNodeEncoder, indent=0, **kwargs)

    @classmethod
    def from_df(cls,
                df: pd.DataFrame,
                demangle: bool = True,
                aggregate: bool = True) -> 'MemoryUsageView':
        """Creates a MemoryUsageView from a DataFrame."""
        roots = []
        if demangle:
            with demangler.Demangler() as d:
                roots = _load_df(df, d, aggregate)
        else:
            roots = _load_df(df, None, aggregate)
        return cls(roots)

    @classmethod
    def from_comparison(cls, base: 'MemoryUsageView',
                        new: 'MemoryUsageView') -> 'MemoryUsageView':
        """Duplicates the base view with deltas as compared to the new view."""
        return cls(_compare_node_lists(base.roots, new.roots))

    def toplevel_names(self) -> list[str]:
        return [root.name for root in self.roots]

    def toplevel_pretty_report(self) -> list[str]:
        """Generates pretty-printed memory usage for toplevel frames."""
        result = []
        for root in self.roots:
            pretty_size = _prettify_size(root.value)
            result.append((root.name, root.value, pretty_size))
        for triplet in sorted(result, key=lambda t: t[1], reverse=True):
            yield f'{triplet[0]}: {triplet[2]}'

    def display(self):
        if not _HAS_IPYTHON:
            print('Error: Could not import IPython module')
            return
        saved_roots = self.roots
        unique_root_id = f'metrics-root-{id(self)}'
        env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(_MEMORY_USAGE_DIR))
        env.policies['json.dumps_function'] = MemoryUsageView.to_json
        try:
            # Common heap profiles are a too deep for the default recursion
            # limit.
            previous_recursion_limit = sys.getrecursionlimit()
            sys.setrecursionlimit(10000)
            self.roots = copy.deepcopy(self.roots)
            _prettify_nodes(self.roots)
            template = env.get_template('memory_usage_table.html.j2')
            html_str = template.render(json_data=self, root_id=unique_root_id)
        finally:
            sys.setrecursionlimit(previous_recursion_limit)
            self.roots = saved_roots
        display(HTML(html_str))


class TreeNodeEncoder(json.JSONEncoder):
    """A JSON encoder for TreeNode objects."""

    def default(self, obj):
        if isinstance(obj, TreeNode):
            data = {
                'name': obj.name,
                'value': obj.value,
                'delta': obj.delta,
            }
            if obj.children:
                data['children'] = [
                    self.default(child) for child in obj.children
                ]
            return data
        return super().default(obj)


def _extract_heap_dump(trace_processor: TraceProcessor) -> pd.DataFrame:
    """Extracts a heap dump from a trace file.

    In the returned DataFrame rows with parent callsite_id must precede
    the child rows referencing them.
    """
    # The SQL schema is documented here:
    #   https://perfetto.dev/docs/analysis/sql-tables#stack_profile_callsite
    query = r"""
    WITH AggregatedAllocations AS (
      SELECT
        callsite_id,
        SUM(size) AS total_size_bytes
      FROM
        heap_profile_allocation
      WHERE
        size >= 0
      GROUP BY
        callsite_id
    )
    SELECT
      callsite.id AS callsite_id,
      callsite.parent_id AS parent_callsite_id,
      callsite.depth AS depth,

      frame.name AS frame_name,
      frame.id AS frame_id,

      -- Ensures 0 is returned instead of NULL when the callsite had no recorded
      -- allocations.
      COALESCE(allocations.total_size_bytes, 0) AS total_size_bytes
    FROM
      stack_profile_callsite AS callsite
      JOIN stack_profile_frame AS frame
      ON callsite.frame_id = frame.id

      -- Allocations are only available for leaf callsites. Normally allocations
      -- are callsites with name 'malloc' and 'posix_memalign'.
      LEFT JOIN AggregatedAllocations AS allocations
      ON callsite.id = allocations.callsite_id
    ORDER BY
      depth ASC
    """
    df = trace_processor.query(query).as_pandas_dataframe()
    return df


def _load_df(df: pd.DataFrame, d: demangler.Demangler,
             aggregate: bool) -> list[TreeNode]:
    """Loads a DataFrame into a MemoryUsageView.roots."""
    # Nodes indexed by callsite_id.
    nodes: dict[int, TreeNode] = {}
    roots: list[TreeNode] = []
    for row in df.itertuples():
        frame_name = row.frame_name
        if pd.isna(frame_name) or not frame_name:
            frame_name = 'unknown'
        elif d:
            frame_name = d.demangle(frame_name)

        callsite_id = int(row.callsite_id)
        assert callsite_id not in nodes, (
            f'Duplicate callsite id: {row.callsite_id}')

        node = TreeNode(name=frame_name, value=int(row.total_size_bytes))
        nodes[callsite_id] = node

        if int(row.depth) == 0:
            assert pd.isna(row.parent_callsite_id), (
                f'Row with zero depth and {row.parent_callsite_id=}')
            roots.append(node)
            continue
        parent_id = int(row.parent_callsite_id)
        assert parent_id in nodes, (
            f'Missing parent callsite for {callsite_id}: {parent_id}')
        parent_node = nodes[parent_id]
        parent_node.children.append(node)
    if aggregate:
        roots = _aggregate_nodes(roots)
    return roots


def _aggregate_nodes(nodes: list[TreeNode]) -> list[TreeNode]:
    """Recursively aggregates values the list of TreeNodes for viewing.

    Replaces same-level nodes with the same name by a single node. Sorts
    all children by value, largest first.

    Note: This function is not idempotent. Applying the aggregation twice will
    lead to wrong results.
    """
    # Group nodes by name, non-recursively.
    grouped_by_name = {}
    for node in nodes:
        name = node.name
        grouped_by_name.setdefault(name, [])
        grouped_by_name[name].append(node)

    # Replace each group of nodes with a single occurrence.
    name_to_grouped_node = {}
    for name, group_list in grouped_by_name.items():
        all_children = []

        # Sum up all individual node values.
        list_total_value = 0
        for node in group_list:
            list_total_value += node.value
            all_children.extend(node.children)

        # Recursively replace children with grouped ones.
        grouped_node = TreeNode(name=name, value=0, delta=0)
        new_children = []
        if all_children:
            new_children = _aggregate_nodes(all_children)
            grouped_node.children = new_children
            for child in new_children:
                grouped_node.value += child.value

        grouped_node.value += list_total_value
        name_to_grouped_node[name] = grouped_node

    # Sort the nodes by their cumulative value.
    return sorted(list(name_to_grouped_node.values()),
                  key=lambda node: node.value,
                  reverse=True)


def _zip_by_name(left_nodes: list[TreeNode],
                 right_nodes: list[TreeNode]) -> list[TreeNode]:
    """Iterates over two lists of nodes, producing pairs with an item from each.

    Nodes are paired by name. This function assumes that each input list has no
    nodes with duplicate names. If for an element in either list there is no
    paired node (i.e. no node with the same name) in the other list, then None
    takes place of the non-existent node.
    """
    name_to_right_node = {n.name: n for n in right_nodes}
    processed_names = set()
    for node1 in left_nodes:
        name = node1.name
        processed_names.add(name)
        node2 = name_to_right_node.get(name)
        yield (node1, node2)
    for node2 in right_nodes:
        if node2.name not in processed_names:
            yield (None, node2)


def _compare_node_lists(nodes_base: list[TreeNode],
                        nodes_new: list[TreeNode]) -> list[TreeNode]:
    """Recursively creates a copy of base nodes with deltas to the new list.

    Puts value difference (from base list to the new list) as node.delta in the
    resulting tree(s). Assumes that each list (or list of children of a node
    does not have duplicated names). This can be achieved by applying
    _aggregate_nodes() above. Toplevel lists and all lists of children are
    individually sorted by delta (biggest to smallest) for simplest
    visualisation to emphasize biggest memory regressions first.
    """
    result = []
    for base, new in _zip_by_name(nodes_base, nodes_new):
        if not base:
            result.append(TreeNode(name=new.name, value=0, delta=new.value))
            continue
        if not new:
            zero_new = TreeNode(name=base.name,
                                value=base.value,
                                delta=-base.value)
            result.append(zero_new)
            continue
        merged_node = TreeNode(name=base.name, value=base.value)
        merged_node.delta = new.value - base.value
        merged_node.children = _compare_node_lists(base.children, new.children)
        result.append(merged_node)
    return sorted(result, key=lambda node: node.delta, reverse=True)


def _prettify_size(size_bytes: int):
    KIB = 1024
    TIB = KIB**4

    if size_bytes < KIB or size_bytes > TIB:
        # The JS display table replaces zero integers with empty cells. To allow
        # removing such clutter simply do not convert the result to str.
        return size_bytes

    for unit in ['KiB', 'MiB', 'GiB']:
        size_bytes /= KIB
        if size_bytes < KIB or unit == 'GiB':
            result = f'{size_bytes:.2f}'.rstrip('0').rstrip('.')
            return f'{result} {unit}'


def _prettify_nodes(nodes: list[TreeNode]):
    for node in nodes:
        node.value = _prettify_size(node.value)
        node.delta = _prettify_size(node.delta)
        _prettify_nodes(node.children)
