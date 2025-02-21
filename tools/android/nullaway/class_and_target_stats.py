#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prints out a list of targets that needs to be null-annotated.

By default the list of targets is sorted by the number of deps that a target has
that is not fully annotated. That is, a target whose deps are all annotated is
shown first, followed by targets with one dep that needs annotation, etc.
"""

import argparse
import collections
import pathlib
import subprocess
import sys

_SRC = pathlib.Path(__file__).parents[3].expanduser().resolve()
_DEP_ANALYSIS_DIR = _SRC / 'tools/android/dependency_analysis'
if str(_DEP_ANALYSIS_DIR) not in sys.path:
    sys.path.insert(0, str(_DEP_ANALYSIS_DIR))

from print_class_dependencies import serialization
import target_dependency

_DEPENDENCY_JSON_PATH = '/tmp/class_and_target_stats_deps.json'
_NOMARK_LIST_PATH = pathlib.Path('/tmp/java_file_stats_file_nomark')
_FILE_CACHE_PATH = pathlib.Path('/tmp/java_file_stats_file_cache')
_CHROME_PACKAGE_PREFIXES = ('org.chromium', 'com.google')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-n',
                        '--num',
                        type=int,
                        default=50,
                        help='Number of results to print, default 50.')
    parser.add_argument(
        '-a',
        '--all',
        action='store_true',
        help='Show all targets, by default only those with all deps annotated '
        'are shown.')
    parser.add_argument(
        '--no-cache',
        action='store_true',
        help='Skip the /tmp caches, useful when the list files changes.')
    parser.add_argument('-v',
                        '--verbose',
                        action='store_true',
                        help='Show more output.')
    args = parser.parse_args()
    subprocess.run([
        'gsutil.py', 'cp', 'gs://clank-dependency-graphs/latest/all.json',
        _DEPENDENCY_JSON_PATH
    ],
                   capture_output=not args.verbose)
    class_graph, _, _ = serialization.load_class_and_package_graphs_from_file(
        _DEPENDENCY_JSON_PATH)
    target_graph = target_dependency.JavaTargetDependencyGraph(class_graph)

    cmd = [
        str(_SRC / 'tools/android/nullaway/java_file_stats.py'),
    ]
    if not args.no_cache:
        cmd += ['--nomark-list-path', str(_NOMARK_LIST_PATH)]
    if _FILE_CACHE_PATH.exists() and not args.no_cache:
        cmd += ['--cached-file-list', str(_FILE_CACHE_PATH)]
    else:
        cmd += ['--output-file-list', str(_FILE_CACHE_PATH)]
    subprocess.run(cmd, capture_output=not args.verbose)
    nomark_paths = _NOMARK_LIST_PATH.read_text().splitlines()

    unmarked_classes = set()
    for path in nomark_paths:
        idx = path.find('/org/')
        if idx == -1:
            idx = path.find('/com/')
        if idx != -1:
            full_name = path[idx + 1:].replace('/', '.').replace('.java', '')
        else:
            # Skip all files without /org/ or /com/ since they are likely not
            # relevant for chrome packages (e.g. usually third_party stuff).
            continue
        unmarked_classes.add(full_name)

    class_nodes = [
        n for n in class_graph.nodes
        if n.package.startswith(_CHROME_PACKAGE_PREFIXES)
    ]
    class_nodes.sort(key=lambda n: len(n.inbound), reverse=True)
    filtered_class_nodes = [
        n for n in class_nodes if n._unique_key in unmarked_classes
    ]

    if args.verbose:
        print(f'{len(class_nodes)=}')
        print(f'{len(unmarked_classes)=}')
        print(f'After filtering {len(filtered_class_nodes)}')

    targets_num_unmarked = collections.defaultdict(int)
    for n in filtered_class_nodes:
        for t in n.build_targets:
            # Skip these jni ones, not interesting.
            if 'jni_headers_java' in t:
                continue
            targets_num_unmarked[t] += 1
    if args.verbose:
        print(f'{len(targets_num_unmarked)=}')

    targets_list = set(targets_num_unmarked.keys())
    targets_unmarked_deps = collections.defaultdict(set)
    targets_unmarked_dependents = collections.defaultdict(set)
    to_remove = set()
    for target in targets_list:
        node = target_graph.get_node_by_key(target)
        if node is None:
            if args.verbose:
                print(f'Removing {target=} not in target graph.')
            to_remove.add(target)
            continue
        # These are deps of this target
        for n in node.outbound:
            if n._unique_key in targets_num_unmarked:
                targets_unmarked_deps[target].add(n._unique_key)
        # These are targets that depend on this target
        for n in node.inbound:
            if n._unique_key in targets_num_unmarked:
                targets_unmarked_dependents[target].add(n._unique_key)
    targets_list = set(t for t in targets_list if t not in to_remove)

    # Account for up to 5-levels of transitive deps. This isn't perfect as some
    # targets define deps in GN that the target graph doesn't pick up on (e.g.
    # //chrome/browser/settings/internal_java), but this is good enough for this
    # script.
    for _ in range(5):
        # Allow consideration of transitive deps that do not have any unmarked.
        targets_list.update(targets_unmarked_deps.keys())
        targets_list.update(targets_unmarked_dependents.keys())
        for target in targets_list:
            node = target_graph.get_node_by_key(target)
            assert node is not None
            for n in node.outbound:
                targets_unmarked_deps[target] |= targets_unmarked_deps[
                    n._unique_key]
            for n in node.inbound:
                targets_unmarked_dependents[target] |= (
                    targets_unmarked_dependents[n._unique_key])

    if not args.all:
        targets_list = set(t for t in targets_list
                           if len(targets_unmarked_deps[t]) == 0)

    # Only keep targets that actually need to be annotated.
    targets_list = [t for t in targets_list if targets_num_unmarked[t] > 0]

    # Prefer targets that don't depend on any unmarked deps, then break ties by
    # preferring targets that many unmarked targets depend on, then break ties
    # by preferring targets with many unmarked files.
    targets_list.sort(key=lambda t: (
        len(targets_unmarked_deps[t]),
        -len(targets_unmarked_dependents[t]),
        -targets_num_unmarked[t],
    ))

    for t in targets_list[:args.num]:
        # Remove the // at the beginning for easy copy/pasting into siso.
        print(f'{t[2:]} deps={len(targets_unmarked_deps[t])} '
              f'dependent={len(targets_unmarked_dependents[t])} '
              f'classes={targets_num_unmarked[t]}')


if __name__ == '__main__':
    main()
