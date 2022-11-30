#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool for printing package-level dependencies."""

import argparse

import graph
import print_dependencies_helper
import serialization


def print_package_dependencies_for_edge(begin, end):
    """Prints dependencies for an edge in the package graph.

    Since these are package edges, we also print the class dependency edges
    comprising the printed package edge.
    """
    if begin == end:
        return
    print(f'\t{begin.name} -> {end.name}')
    class_deps = begin.get_class_dependencies_in_outbound_edge(end)
    print(f'\t{len(class_deps)} class edge(s) comprising the dependency:')
    for begin_class, end_class in graph.sorted_edges_by_name(class_deps):
        print(f'\t\t{begin_class.class_name} -> {end_class.class_name}')


def print_package_dependencies_for_key(package_graph, key, ignore_subpackages):
    """Prints dependencies for a valid key into the package graph.

    Since we store self-edges for the package graph
    but they aren't relevant in this case, we skip them.
    """
    node = package_graph.get_node_by_key(key)

    inbound_without_self = [other for other in node.inbound if other != node]
    print(f'{len(inbound_without_self)} inbound dependency(ies) '
          f'for {node.name}:')
    for inbound_dep in graph.sorted_nodes_by_name(inbound_without_self):
        if ignore_subpackages and inbound_dep.name.startswith(node.name):
            continue
        print_package_dependencies_for_edge(inbound_dep, node)

    outbound_without_self = [other for other in node.outbound if other != node]
    print(f'{len(outbound_without_self)} outbound dependency(ies) '
          f'for {node.name}:')
    for outbound_dep in graph.sorted_nodes_by_name(outbound_without_self):
        if ignore_subpackages and outbound_dep.name.startswith(node.name):
            continue
        print_package_dependencies_for_edge(node, outbound_dep)

def main():
    """Prints package-level dependencies for an input package."""
    arg_parser = argparse.ArgumentParser(
        description='Given a JSON dependency graph, output the package-level '
        'dependencies for a given package and the '
        'class dependencies comprising those dependencies')
    required_arg_group = arg_parser.add_argument_group('required arguments')
    required_arg_group.add_argument(
        '-f',
        '--file',
        required=True,
        help='Path to the JSON file containing the dependency graph. '
        'See the README on how to generate this file.')
    required_arg_group.add_argument(
        '-p',
        '--package',
        required=True,
        help='Case-insensitive name of the package to print dependencies for. '
        'Matches names of the form ...input, for example '
        '`browser` matches `org.chromium.browser`.')
    optional_arg_group = arg_parser.add_argument_group('optional arguments')
    optional_arg_group.add_argument(
        '-s',
        '--ignore-subpackages',
        action='store_true',
        help='If present, this tool will ignore dependencies between the '
        'given package and subpackages. For example, if given '
        'browser.customtabs, it won\'t print a dependency between '
        'browser.customtabs and browser.customtabs.content.')
    arguments = arg_parser.parse_args()

    _, package_graph, _ = serialization.load_class_and_package_graphs_from_file(
        arguments.file)
    package_graph_keys = [node.name for node in package_graph.nodes]
    valid_keys = print_dependencies_helper.get_valid_package_keys_matching(
        package_graph_keys, arguments.package)

    if len(valid_keys) == 0:
        print(f'No package found by the name {arguments.package}.')
    elif len(valid_keys) > 1:
        print(f'Multiple valid keys found for the name {arguments.package}, '
              'please disambiguate between one of the following options:')
        for valid_key in valid_keys:
            print(f'\t{valid_key}')
    else:
        print(f'Printing package dependencies for {valid_keys[0]}:')
        print_package_dependencies_for_key(package_graph, valid_keys[0],
                                           arguments.ignore_subpackages)


if __name__ == '__main__':
    main()
