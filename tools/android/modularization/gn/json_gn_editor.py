# Lint as: python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Helper script to use GN's JSON interface to make changes.'''

from __future__ import annotations

import copy
import json
import os
import pathlib
import sys
from typing import Dict, List, Optional

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).parents[2].resolve()
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import subprocess_utils

# Refer to parse_tree.cc for GN AST implementation details:
# https://gn.googlesource.com/gn/+/refs/heads/main/src/gn/parse_tree.cc
# These constants should match corresponding entries in parse_tree.cc.
# TODO: Add high-level details for the expected data structure.
NODE_CHILD = 'child'
NODE_TYPE = 'type'
NODE_VALUE = 'value'


class BuildFile:
    """Represents the contents of a BUILD.gn file."""
    def __init__(self,
                 build_gn_path: str,
                 root_gn_path: str,
                 *,
                 dryrun: bool = False):
        self._root = root_gn_path
        rel_path = os.path.relpath(build_gn_path, root_gn_path)
        self._gn_rel_path = '//' + os.path.dirname(rel_path)
        self._full_path = os.path.abspath(build_gn_path)
        self._skip_write_content = dryrun

    def __enter__(self):
        output = subprocess_utils.run_command(
            ['gn', 'format', '--dump-tree=json', self._full_path])
        self._content = json.loads(output)
        self._original_content = json.dumps(self._content)
        return self

    def __exit__(self, exc, value, tb):
        if not self._skip_write_content:
            self.write_content_to_file()

    # See: https://gist.github.com/sgraham/bd9ffee312f307d5f417019a9c0f0777
    def _find_all(self, match_fn):
        results = []

        def recursive_find(root):
            matched = match_fn(root)
            if matched is not None:
                results.append(matched)
                return
            children = root.get(NODE_CHILD)
            if children:
                for child in children:
                    recursive_find(child)

        recursive_find(self._content)
        return results

    def split_dep(self, original_dep_name: str, new_dep_name: str) -> bool:
        """Add |new_dep_name| to GN deps that contains |original_dep_name|.

        Supports deps and public_deps.

        Works for explicitly assigning a list to deps:
        deps = [ ..., "original_dep", ...]
        # Becomes
        deps = [ ..., "original_dep", "new_dep", ...]
        Also works for appending a list to deps:
        public_deps += [ ..., "original_dep", ...]
        # Becomes
        public_deps += [ ..., "original_dep", "new_dep", ...]

        Does not work for assigning or appending variables to deps:
        deps = other_list_of_deps # Does NOT check other_list_of_deps.
        # Becomes (no changes)
        deps = other_list_of_deps

        Does not work with parameter expansion, i.e. $variables.

        Returns whether the new dep was added one or more times.
        """
        assert original_dep_name.startswith('//'), (
            f'Absolute GN path required, starting with //: {original_dep_name}'
        )
        assert new_dep_name.startswith('//'), (
            f'Absolute GN path required, starting with //: {new_dep_name}')

        def match_deps_list(node):
            r"""Matches and returns the list for a deps assignment.

            Binary node
             /       \
            /         \
            deps      list of nodes

            Returns the list of nodes.
            """
            if node.get(NODE_TYPE) != 'BINARY':
                return None
            children = node.get(NODE_CHILD)
            assert len(children) == 2, (
                'Binary nodes should have two child nodes, but the node is: '
                f'{node}')
            left_child, right_child = children
            if left_child.get(NODE_TYPE) != 'IDENTIFIER':
                return None
            if left_child.get(NODE_VALUE) not in ('deps', 'public_deps'):
                return None
            if right_child.get(NODE_TYPE) != 'LIST':
                return None
            return right_child.get(NODE_CHILD)

        all_deps_lists = self._find_all(match_deps_list)

        def normalize(name: str):
            if not name:
                return ''
            if name.startswith('"'):
                name = name[1:-1]
            if not name.startswith('//'):
                name = self._gn_rel_path + name
            if not ':' in name:
                name += ':' + os.path.basename(name)
            return name

        def add_quotes(name: str):
            if name.startswith('"'):
                return name
            return f'"{name}"'

        added_new_dep = False
        normalized_original_dep_name = normalize(original_dep_name)
        normalized_new_dep_name = normalize(new_dep_name)
        for deps_list in all_deps_lists:
            original_dep = None
            new_dep_already_exists = False
            for child in deps_list:
                dep_name = normalize(child.get(NODE_VALUE))
                if dep_name == normalized_original_dep_name:
                    original_dep = child
                if dep_name == normalized_new_dep_name:
                    new_dep_already_exists = True
            if original_dep and not new_dep_already_exists:
                new_dep = copy.deepcopy(original_dep)
                new_dep[NODE_VALUE] = add_quotes(new_dep_name)
                deps_list.append(new_dep)
                added_new_dep = True

        return added_new_dep

    def write_content_to_file(self) -> None:
        current_content = json.dumps(self._content)
        if current_content != self._original_content:
            subprocess_utils.run_command(
                ['gn', 'format', '--read-tree=json', self._full_path],
                cmd_input=current_content)
