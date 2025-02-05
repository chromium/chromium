#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import pathlib
import subprocess
import sys

_SRC = pathlib.Path(__file__).parents[3].expanduser().resolve()
_DEP_ANALYSIS_DIR = _SRC / 'tools/android/dependency_analysis'
if str(_DEP_ANALYSIS_DIR) not in sys.path:
    sys.path.insert(0, str(_DEP_ANALYSIS_DIR))

from print_class_dependencies import serialization

_DEPENDENCY_JSON_PATH = '/tmp/class_and_target_stats_deps.json'
_NOMARK_LIST_PATH = pathlib.Path('/tmp/java_file_stats_file_nomark')
_FILE_CACHE_PATH = pathlib.Path('/tmp/java_file_stats_file_cache')
_CHROME_PACKAGE_PREFIXES = ('org.chromium', 'com.google')
_TOP_N_RESULTS = 50


def main():
    subprocess.run([
        'gsutil.py', 'cp', 'gs://clank-dependency-graphs/latest/all.json',
        _DEPENDENCY_JSON_PATH
    ])
    class_graph, _, _ = serialization.load_class_and_package_graphs_from_file(
        _DEPENDENCY_JSON_PATH)

    cmd = [
        str(_SRC / 'tools/android/nullaway/java_file_stats.py'),
        '--nomark-list-path',
        str(_NOMARK_LIST_PATH)
    ]
    if _FILE_CACHE_PATH.exists():
        cmd += ['--cached-file-list', str(_FILE_CACHE_PATH)]
    else:
        cmd += ['--output-file-list', str(_FILE_CACHE_PATH)]
    subprocess.run(cmd)
    nomark_paths = _NOMARK_LIST_PATH.read_text().splitlines()

    unmarked_classes = set()
    for path in nomark_paths:
        idx = path.find('/org/')
        if idx == -1:
            idx = path.find('/com/')
        if idx != -1:
            full_name = path[idx + 1:].replace('/', '.').replace('.java', '')
        unmarked_classes.add(full_name)

    class_nodes = [
        n for n in class_graph.nodes
        if n.package.startswith(_CHROME_PACKAGE_PREFIXES)
    ]
    class_nodes.sort(key=lambda n: len(n.inbound), reverse=True)

    print(f'{len(class_nodes)=}')
    print(f'{len(unmarked_classes)=}')
    class_nodes = [n for n in class_nodes if n._unique_key in unmarked_classes]
    print(f'After filtering {len(class_nodes)}')

    targets = collections.defaultdict(int)
    for n in class_nodes:
        for t in n.build_targets:
            # Skip these jni ones, not interesting.
            if 'jni_headers_java' in t:
                continue
            targets[t] += len(n.inbound)
    print(f'{len(targets)=}')

    targets_list = list(targets.items())
    targets_list.sort(key=lambda t: t[1], reverse=True)

    for n in class_nodes[:_TOP_N_RESULTS]:
        print(f'{len(n.inbound)=} {n.name} {n.build_targets}')

    for t, num in targets_list[:_TOP_N_RESULTS]:
        print(f'{t} {num=}')


if __name__ == '__main__':
    main()
