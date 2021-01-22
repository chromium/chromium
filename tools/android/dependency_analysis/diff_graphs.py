#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool for finding differences in dependency graphs."""

import argparse

from typing import List, Set

import graph
import serialization


def diff_num_graph_nodes(graph1: graph.Graph, graph2: graph.Graph, label: str):
    before: int = graph1.num_nodes
    after: int = graph2.num_nodes
    diff: int = after - before
    print(f'{label}: {diff:+} ({before} -> {after})')


def diff_node_list(graph1: graph.Graph, graph2: graph.Graph, label: str):
    print(f'{label} added (+) and removed (-):')
    before_nodes: Set[str] = set(node.name for node in graph1.nodes)
    after_nodes: Set[str] = set(node.name for node in graph2.nodes)
    added_nodes: List[str] = sorted(after_nodes - before_nodes)
    removed_nodes: List[str] = sorted(before_nodes - after_nodes)
    for i, added_node in enumerate(added_nodes, start=1):
        print(f'+ [{i:4}] {added_node}')
    for i, removed_node in enumerate(removed_nodes, start=1):
        print(f'- [{i:4}] {removed_node}')


def main():
    arg_parser = argparse.ArgumentParser(
        description='Given two JSON dependency graphs, output the differences '
        'between them.')
    required_arg_group = arg_parser.add_argument_group('required arguments')
    required_arg_group.add_argument(
        '-b',
        '--before',
        required=True,
        help='Path to the JSON file containing the "before" dependency graph. '
        'See the README on how to generate this file.')
    required_arg_group.add_argument(
        '-a',
        '--after',
        required=True,
        help='Path to the JSON file containing the "after" dependency graph.')
    arguments = arg_parser.parse_args()

    class_graph_before, package_graph_before, _ = \
        serialization.load_class_and_package_graphs_from_file(arguments.before)
    class_graph_after, package_graph_after, _ = \
        serialization.load_class_and_package_graphs_from_file(arguments.after)
    diff_num_graph_nodes(class_graph_before, class_graph_after,
                         'Total Java class count')
    diff_num_graph_nodes(package_graph_before, package_graph_after,
                         'Total Java package count')

    print()
    diff_node_list(class_graph_before, class_graph_after, 'Java classes')
    print()
    diff_node_list(package_graph_before, package_graph_after, 'Java packages')


if __name__ == '__main__':
    main()
