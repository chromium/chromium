# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for viewing and diffing Sum Trees of memory usage."""

# TODO(crbug.com/73768497): Move this file and other files related to memory
# usage into a separate subdirectory.

import json
from dataclasses import dataclass, field


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
    def from_heap_dump(cls) -> 'MemoryUsageView':
        """Placeholder for creating a MemoryUsageView from a heap dump."""
        raise NotImplementedError()

    def to_json(self) -> str:
        """Converts the MemoryUsageView to a JSON string."""
        return json.dumps(self.roots, cls=TreeNodeEncoder, indent=0)


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
