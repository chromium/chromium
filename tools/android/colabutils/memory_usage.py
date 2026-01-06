# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for viewing and diffing Sum Trees of memory usage."""

# TODO(crbug.com/73768497): Move this file and other files related to memory
# usage into a separate subdirectory.

import json
import os
import pandas as pd
import pathlib
import sys
from dataclasses import dataclass, field

from . import demangler

_SRC_PATH = pathlib.Path(__file__).resolve().parents[3]
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
    def from_heap_dump(
            cls,
            trace_file_name: str,
            demangler: demangler.Demangler | None = None) -> 'MemoryUsageView':
        """Creates a MemoryUsageView from a trace file with a heap dump."""
        abs_path = os.path.abspath(trace_file_name)
        with TraceProcessor(trace=abs_path) as trace_processor:
            df = _extract_heap_dump(trace_processor)
            return cls.from_df(df, demangler=demangler)
        return None

    def to_json(self) -> str:
        """Converts the MemoryUsageView to a JSON string."""
        return json.dumps(self.roots, cls=TreeNodeEncoder, indent=0)

    @classmethod
    def from_df(cls, df: pd.DataFrame,
                demangler: demangler.Demangler | None) -> 'MemoryUsageView':
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
        return cls(roots)

    def toplevel_names(self) -> list[str]:
        return [root.name for root in self.roots]


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
