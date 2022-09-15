#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool for finding differences in dependency graphs."""

import argparse

import itertools
from typing import List, Set, Tuple, Callable

import chrome_names
import count_cycles
import graph
import serialization


def _print_diff_num_nodes(graph1: graph.Graph, graph2: graph.Graph,
                          label: str):
    before: int = graph1.num_nodes
    after: int = graph2.num_nodes
    _print_diff_metric(before, after, label)


def _print_diff_num_edges(graph1: graph.Graph, graph2: graph.Graph,
                          label: str):
    before: int = graph1.num_edges
    after: int = graph2.num_edges
    _print_diff_metric(before, after, label)


def _print_diff_metric(before: int, after: int, label: str):
    diff: int = after - before
    print(f'{label}: {diff:+} ({before} -> {after})')


def _print_diff_node_list(graph1: graph.Graph, graph2: graph.Graph,
                          label: str):
    before_nodes: Set[str] = set(node.name for node in graph1.nodes)
    after_nodes: Set[str] = set(node.name for node in graph2.nodes)
    _print_set_diff(before_nodes, after_nodes, label)


def _print_diff_node_list_filtered(graph1: graph.Graph, graph2: graph.Graph,
                                   label: str,
                                   filter_fn: Callable[[graph.Node], bool]):

    before_nodes: Set[str] = set(node.name for node in graph1.nodes
                                 if filter_fn(node))
    after_nodes: Set[str] = set(node.name for node in graph2.nodes
                                if filter_fn(node))
    _print_diff_metric(len(before_nodes), len(after_nodes), label)
    _print_set_diff(before_nodes, after_nodes, label)


def _print_diff_edge_list(graph1: graph.Graph, graph2: graph.Graph,
                          label: str):
    before_edges: Set[str] = set(_edge_str(edge) for edge in graph1.edges)
    after_edges: Set[str] = set(_edge_str(edge) for edge in graph2.edges)
    _print_set_diff(before_edges, after_edges, label)


def _edge_str(edge: Tuple[graph.Node, graph.Node]) -> str:
    return f'{edge[0]} -> {edge[1]}'


def _print_diff_cycle_list(cycles1: Set[count_cycles.Cycle],
                           cycles2: Set[count_cycles.Cycle], label: str):
    before_cycles: Set[str] = set(_cycle_str(cycle) for cycle in cycles1)
    after_cycles: Set[str] = set(_cycle_str(cycle) for cycle in cycles2)
    _print_set_diff(before_cycles, after_cycles, label)


def _cycle_str(cycle: count_cycles.Cycle) -> str:
    return ' > '.join(chrome_names.shorten_class(node.name) for node in cycle)


def _print_set_diff(before_set: Set[str], after_set: Set[str], label: str):
    all_added: List[str] = sorted(after_set - before_set)
    all_removed: List[str] = sorted(before_set - after_set)
    if not all_added and not all_removed:
        print(f'{label} - no changes')
        return

    print(f'{label} added (+) and removed (-):')
    for i, added in enumerate(all_added, start=1):
        print(f'+ [{i:4}] {added}')
    for i, removed in enumerate(all_removed, start=1):
        print(f'- [{i:4}] {removed}')


def _cycle_set(graph: graph.Graph,
               max_cycle_size: int) -> Set[count_cycles.Cycle]:
    all_cycles_by_size = count_cycles.find_cycles(graph, max_cycle_size)
    return set(itertools.chain(*all_cycles_by_size))


def main():
    arg_parser = argparse.ArgumentParser(
        description='Given two JSON dependency graphs, output the differences '
        'between them. By default, outputs the differences in the sets of '
        'class and package nodes.')
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
    arg_parser.add_argument('-e',
                            '--edges',
                            action='store_true',
                            help='Also diff the set of graph edges.')
    arg_parser.add_argument(
        '--package-cycles',
        type=int,
        help='Also diff the set of package cycles up to the specified size.')
    arg_parser.add_argument(
        '--build-target',
        type=str,
        help='Also diff the set of class nodes in the given build target, e.g. '
        '"//chrome/android:chrome_java"')
    arguments = arg_parser.parse_args()

    class_graph_before, package_graph_before, _ = \
        serialization.load_class_and_package_graphs_from_file(arguments.before)
    class_graph_after, package_graph_after, _ = \
        serialization.load_class_and_package_graphs_from_file(arguments.after)
    _print_diff_num_nodes(class_graph_before, class_graph_after,
                          'Total Java class count')
    _print_diff_num_nodes(package_graph_before, package_graph_after,
                          'Total Java package count')

    print()
    _print_diff_node_list(class_graph_before, class_graph_after,
                          'Java classes')
    print()
    _print_diff_node_list(package_graph_before, package_graph_after,
                          'Java packages')

    if arguments.edges:
        print()
        _print_diff_num_edges(class_graph_before, class_graph_after,
                              'Total Java class edge count')
        _print_diff_num_edges(package_graph_before, package_graph_after,
                              'Total Java package edge count')

        print()
        _print_diff_edge_list(class_graph_before, class_graph_after,
                              'Java class edges')
        print()
        _print_diff_edge_list(package_graph_before, package_graph_after,
                              'Java package edges')

    if arguments.package_cycles:
        cycles_before = _cycle_set(package_graph_before,
                                   arguments.package_cycles)
        cycles_after = _cycle_set(package_graph_after,
                                  arguments.package_cycles)
        print()
        _print_diff_metric(
            len(cycles_before), len(cycles_after),
            'Total Java package cycle count (up to size '
            f'{arguments.package_cycles})')

        print()
        _print_diff_cycle_list(
            cycles_before, cycles_after,
            f'Java package cycles (up to size {arguments.package_cycles})')

    if arguments.build_target:

        def is_in_build_target(node):
            return arguments.build_target in node.build_targets

        print()
        _print_diff_node_list_filtered(
            class_graph_before, class_graph_after,
            f'Java classes in {arguments.build_target}', is_in_build_target)


if __name__ == '__main__':
    main()
