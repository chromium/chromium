#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool for generating modularization stats."""

import argparse
import json
from typing import Dict, List

import class_dependency
import count_cycles
import graph
import os
import package_dependency
import print_dependencies_helper
import serialization
import sys

_SRC_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(
    os.path.join(_SRC_PATH, 'tools', 'android', 'modularization', 'loc'))

import modularization_loc_stat as loc_stat

CLASSES_TO_COUNT_INBOUND = ['ChromeActivity', 'ChromeTabbedActivity']


def _copy_metadata(metadata: Dict) -> Dict[str, str]:
    if metadata is None:
        return {}
    return {f'meta_{key}': value for key, value in metadata.items()}


def _generate_graph_sizes(
        class_graph: class_dependency.JavaClassDependencyGraph,
        package_graph: package_dependency.JavaPackageDependencyGraph
) -> Dict[str, int]:
    return {
        'class_nodes': class_graph.num_nodes,
        'class_edges': class_graph.num_edges,
        'package_nodes': package_graph.num_nodes,
        'package_edges': package_graph.num_edges
    }


def _generate_inbound_stats(
        class_graph: class_dependency.JavaClassDependencyGraph,
        class_names: List[str]) -> Dict[str, int]:
    valid_class_names = \
        print_dependencies_helper.get_valid_classes_from_class_list(
            class_graph, class_names)

    result = {}
    for class_name, valid_class_name in zip(class_names, valid_class_names):
        node: class_dependency.JavaClass = class_graph.get_node_by_key(
            valid_class_name)
        result[f'inbound_{class_name}'] = len(node.inbound)
    return result


def _generate_package_cycle_stats(
        package_graph: package_dependency.JavaPackageDependencyGraph
) -> Dict[str, int]:
    all_cycles = count_cycles.find_cycles(package_graph, 4)
    cycles_size_2 = len(all_cycles[2])
    cycles_size_up_to_4 = sum(map(len, all_cycles[2:]))
    return {
        'package_cycles_size_equals_2': cycles_size_2,
        'package_cycles_size_up_to_4': cycles_size_up_to_4
    }


def _generate_chrome_java_size(
        class_graph: class_dependency.JavaClassDependencyGraph
) -> Dict[str, int]:
    count = 0

    class_node: class_dependency.JavaClass
    for class_node in class_graph.nodes:
        if '//chrome/android:chrome_java' in class_node.build_targets:
            count += 1
    return {'chrome_java_class_count': count}


def _generate_loc_stats(git_dir: str) -> Dict[str, object]:
    start_date, end_date = loc_stat.GetDateRange(past_days=7)
    loc_result_json: str = loc_stat.GenerateLOCStats(start_date,
                                                     end_date,
                                                     quiet=True,
                                                     json_format=True,
                                                     git_dir=git_dir)

    loc_result: Dict = json.loads(loc_result_json)

    loc_modularized = loc_result.get(loc_stat.KEY_LOC_MODULARIZED, 0)
    loc_chrome_android = loc_result.get(loc_stat.KEY_LOC_LEGACY, 0)
    total = loc_modularized + loc_chrome_android
    percentage_modularized: float = loc_modularized / total if total > 0 else 0

    return {
        'loc_modularized': loc_modularized,
        'loc_chrome_android': loc_chrome_android,
        'loc_modularized_percentage': percentage_modularized,
        'loc_start_date': loc_result.get(loc_stat.KEY_START_DATE, ''),
        'loc_end_date': loc_result.get(loc_stat.KEY_END_DATE, ''),
    }


def main():
    arg_parser = argparse.ArgumentParser(
        description='Given a JSON dependency graph, output a JSON with a '
        'number of metrics to track progress of modularization.')
    required_arg_group = arg_parser.add_argument_group('required arguments')
    required_arg_group.add_argument(
        '-f',
        '--file',
        required=True,
        help='Path to the JSON file containing the dependency graph. '
        'See the README on how to generate this file.')
    arg_parser.add_argument(
        '--git-dir',
        type=str,
        help='Root directory of the git repo to look into. '
        'If not specified, use the current directory.')
    arg_parser.add_argument(
        '-o',
        '--output',
        help='File to write the result json to. In not specified, outputs to '
        'stdout.')
    arguments = arg_parser.parse_args()

    class_graph, package_graph, graph_metadata = \
        serialization.load_class_and_package_graphs_from_file(arguments.file)

    stats = {}
    stats.update(_copy_metadata(graph_metadata))
    stats.update(_generate_graph_sizes(class_graph, package_graph))
    stats.update(_generate_inbound_stats(class_graph,
                                         CLASSES_TO_COUNT_INBOUND))
    stats.update(_generate_package_cycle_stats(package_graph))
    stats.update(_generate_chrome_java_size(class_graph))
    stats.update(_generate_loc_stats(arguments.git_dir))

    if arguments.output:
        with open(arguments.output, 'w') as f:
            json.dump(stats, f, sort_keys=True)
    else:
        print(json.dumps(stats, sort_keys=True))


if __name__ == '__main__':
    main()
