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
import csv
import datetime
import json
import pathlib
import re
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

# These are classes that show up as unmarked but are not really (e.g.
# annotations).
_IGNORED_CLASSES_RE = [
    # These are just annotations in build_java.
    r'org.chromium.build.annotations.*',
    # Errorprone plugins don't need annotations.
    r'org.chromium.tools.errorprone.plugin.*',
    # Not sure why this one isn't annotated.
    r'org.chromium.base.test.ClangProfiler',
    # The target boundary_interface_java sets chromium_code=false in
    # //android_webview/support_library/boundary_interfaces/BUILD.gn.
    r'org.chromium.support_lib_boundary.*',
    # False positive, version used in Chrome is @NullMarked; Cronet version
    # is not marked.
    r'org.chromium.net.ConnectivityManagerShim',
]


def gen_build_target_graph(out_dir: pathlib.Path, include_testonly: bool,
                           no_cache: bool):
    project_json = out_dir / 'project.json'
    if not project_json.exists() or no_cache:
        subprocess.run(['gn', 'gen', '--ide=json', str(out_dir)])
    assert project_json.exists()
    with project_json.open() as f:
        data = json.load(f)
    outbound = collections.defaultdict(set)
    inbound = collections.defaultdict(set)
    skip = set()
    if not include_testonly:
        for target, info in data['targets'].items():
            if not info['testonly']:
                continue
            # If it exists, it should always be True.
            assert info['testonly'], f'{target} has false testonly value.'
            skip.add(target)
    for target, info in data['targets'].items():
        if target in skip:
            continue
        if '__' in target:
            target = target.split('__')[0]
        for dep in info['deps']:
            if '__' in dep:
                dep = dep.split('__')[0]
            if dep in skip:
                continue
            # After removing subtargets, A_java depends on A_java__header, but
            # we don't want that in the graph.
            if target == dep:
                continue
            outbound[target].add(dep)
            inbound[dep].add(target)
    return outbound, inbound, skip


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '-a',
        '--all',
        action='store_true',
        help='Show all targets, by default only those with all deps annotated '
        'are shown.')
    parser.add_argument(
        '--no-cache',
        action='store_true',
        help='Skip the /tmp caches and regenerate project.json, useful when '
        'the list files changes.')
    parser.add_argument(
        '-C',
        '--out-dir',
        default='out/Debug',
        type=pathlib.Path,
        help='Used to generate project.json in the outdir and use it for the '
        'target graph. Defaults to out/Debug.')
    parser.add_argument('--csv',
                        type=pathlib.Path,
                        help='Also output a .csv file for further processing')
    parser.add_argument(
        '-v',
        '--verbose',
        action='count',
        default=0,
        help='1 to print deps/dependent count, 2 to print each unmarked class, '
        '3 to print lists of deps, and 4 to print lists of dependents.')
    parser.add_argument(
        '--include-testonly',
        action='store_true',
        help='Include testonly targets, otherwise excluded by default.')
    args = parser.parse_args()
    gsutil_cmd = [
        'gsutil.py', 'cp', 'gs://clank-dependency-graphs/latest/all.json',
        _DEPENDENCY_JSON_PATH
    ]
    subprocess.run(gsutil_cmd, capture_output=True)
    class_graph, _, _ = serialization.load_class_and_package_graphs_from_file(
        _DEPENDENCY_JSON_PATH)
    target_graph = target_dependency.JavaTargetDependencyGraph(class_graph)
    target_outbound, target_inbound, skipped_targets = gen_build_target_graph(
        args.out_dir, args.include_testonly, args.no_cache)

    stats_cmd = [
        str(_SRC / 'tools/android/nullaway/java_file_stats.py'),
        '--nomark-list-path',
        str(_NOMARK_LIST_PATH)
    ]
    if _FILE_CACHE_PATH.exists() and not args.no_cache:
        stats_cmd += ['--cached-file-list', str(_FILE_CACHE_PATH)]
    else:
        stats_cmd += ['--output-file-list', str(_FILE_CACHE_PATH)]
    subprocess.run(stats_cmd, capture_output=True)
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
        if any(
                re.match(pattern, full_name)
                for pattern in _IGNORED_CLASSES_RE):
            #print(f'Ignoring {full_name}')
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
    targets_unmarked_classes = collections.defaultdict(set)
    java_targets = set()
    substrings = ['test', 'junit']
    for n in filtered_class_nodes:
        for t in n.build_targets:
            if '__' in t:
                t = t.split('__')[0]
            # Skip these jni ones, not interesting.
            if 'jni_headers_java' in t or 'jni_java' in t:
                continue
            if t in skipped_targets:
                continue
            if not args.include_testonly and any(s in t for s in substrings):
                continue
            java_targets.add(t)
            targets_num_unmarked[t] += 1
            targets_unmarked_classes[t].add(n.name)
    if args.verbose:
        print(f'{len(targets_num_unmarked)=}')

    target_transitive_outbound = collections.defaultdict(set)
    target_transitive_inbound = collections.defaultdict(set)
    for target in java_targets:
        # These are deps of this target
        target_transitive_outbound[target].update(target_outbound[target])
        target_transitive_inbound[target].update(target_inbound[target])
        node = target_graph.get_node_by_key(target)
        if node is None:
            continue
        for n in node.outbound:
            if n._unique_key not in target_transitive_outbound[target]:
                #print(target, 'inbound', n._unique_key)
                target_transitive_outbound[target].add(n._unique_key)
        # These are targets that depend on this target
        for n in node.inbound:
            if n._unique_key not in target_transitive_inbound[target]:
                #print(target, 'outbound', n._unique_key)
                target_transitive_inbound[target].add(n._unique_key)

    # Account for up to 5-levels of transitive deps (~2^5). This isn't perfect
    # as some targets define deps in GN that the target graph doesn't pick up on
    # (e.g. //chrome/browser/settings/internal_java), but this is good enough
    # for this script.
    for _ in range(5):
        # Allow consideration of transitive deps that do not have any unmarked.
        for target in java_targets:
            # These are deps of this target
            for t in target_outbound[target]:
                target_transitive_outbound[target].update(
                    target_transitive_outbound[t])
            # These are targets that depend on this target
            for t in target_inbound[target]:
                target_transitive_inbound[target].update(
                    target_transitive_inbound[t])

    targets_unmarked_deps = collections.defaultdict(set)
    targets_unmarked_dependents = collections.defaultdict(set)
    for target in java_targets:
        for dep in target_transitive_outbound[target]:
            if targets_num_unmarked[dep] > 0:
                targets_unmarked_deps[target].add(dep)
        for dep in target_transitive_inbound[target]:
            if targets_num_unmarked[dep] > 0:
                targets_unmarked_dependents[target].add(dep)

    if not args.all:
        java_targets = set(t for t in java_targets
                           if len(targets_unmarked_deps[t]) == 0)

    # Only keep targets that actually need to be annotated.
    targets_list = sorted(
        [t for t in java_targets if targets_num_unmarked[t] > 0])

    for t in targets_list:
        # Remove the // at the beginning for easy copy/pasting into siso.
        status = f'{targets_num_unmarked[t]:2}'
        if args.verbose >= 1:
            status += f' {len(targets_unmarked_dependents[t]):2}'
            # Without --all this list is all zeros.
            if args.all:
                status += f' {len(targets_unmarked_deps[t]):2}'
        print(f'[{status}] {t[2:]}')

        if args.verbose >= 2:
            for c in targets_unmarked_classes[t]:
                print(f'$ {c}')

        if args.verbose >= 3:
            for tt in targets_unmarked_deps[t]:
                print(f'> {tt}')

        if args.verbose >= 4:
            for tt in targets_unmarked_dependents[t]:
                print(f'< {tt}')

    print('''\
For the status prefix:[A B C] <target-name>:
- A is the number of unmarked classes in that target.
- B (shows up only with -v) is the number of unmarked targets that depend on \
this target.
- C (shows up only with -v and -a) is the number of unmarked targets that this \
target depends on.''')

    if args.csv:
        timestamp = datetime.datetime.now(datetime.UTC).timestamp()
        print(f"Writing csv to {args.csv}")
        with open(args.csv, 'w') as csv_output_file:
            csv_writer = csv.writer(csv_output_file)
            for t in targets_list:
                status = [
                    t[2:], targets_num_unmarked[t],
                    len(targets_unmarked_dependents[t]),
                    len(targets_unmarked_deps[t]), timestamp
                ]
                csv_writer.writerow(status)


if __name__ == '__main__':
    main()
