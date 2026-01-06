# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for viewing and diffing Sum Trees of memory usage."""

# TODO(crbug.com/73768497): Move this file and other files related to memory
# usage into a separate subdirectory.

import jinja2
import json
import os
import pandas as pd
import pathlib
import string
import sys
from dataclasses import dataclass, field
try:
    from IPython.display import HTML, display
    _HAS_IPYTHON = True
except ImportError:
    _HAS_IPYTHON = False

from . import demangler

_MEMORY_USAGE_DIR = pathlib.Path(__file__).resolve().parent
_SRC_PATH = _MEMORY_USAGE_DIR.parents[2]
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
                       demangler: demangler.Demangler | None = None,
                       aggregate: bool = True) -> 'MemoryUsageView':
        """Creates a MemoryUsageView from a trace file with a heap dump."""
        abs_path = os.path.abspath(trace_file_name)
        with TraceProcessor(trace=abs_path) as trace_processor:
            df = _extract_heap_dump(trace_processor)
            return cls.from_df(df, demangler=demangler, aggregate=aggregate)
        return None

    def to_json(self, **kwargs) -> str:
        """Converts the MemoryUsageView to a JSON string."""
        return json.dumps(self.roots, cls=TreeNodeEncoder, indent=0, **kwargs)

    @classmethod
    def from_df(cls,
                df: pd.DataFrame,
                demangler: demangler.Demangler | None,
                aggregate: bool = True) -> 'MemoryUsageView':
        """Creates a MemoryUsageView from a DataFrame."""
        # Nodes indexed by callsite_id.
        nodes: dict[int, TreeNode] = {}
        roots: list[TreeNode] = []

        for row in df.itertuples():
            frame_name = row.frame_name
            if pd.isna(frame_name) or not frame_name:
                frame_name = 'unknown'
            elif demangler:
                frame_name = demangler.demangle(frame_name)

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
        return cls(roots)

    def toplevel_names(self) -> list[str]:
        return [root.name for root in self.roots]

    def display(self):
        if not _HAS_IPYTHON:
            print('Error: Could not import IPython module')
            return
        unique_root_id = f'metrics-root-{id(self)}'
        env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(_MEMORY_USAGE_DIR))
        try:
            # Common heap profiles are a too deep for the default recursion
            # limit.
            previous_recursion_limit = sys.getrecursionlimit()
            sys.setrecursionlimit(10000)
            env.policies['json.dumps_function'] = MemoryUsageView.to_json
            template = env.get_template('memory_usage_table.html.j2')
            html_str = template.render(json_data=self, root_id=unique_root_id)
        finally:
            sys.setrecursionlimit(previous_recursion_limit)
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
