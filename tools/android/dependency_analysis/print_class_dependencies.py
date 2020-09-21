#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool for printing class-level dependencies."""

import argparse
from dataclasses import dataclass
from typing import List

import chrome_names
import class_dependency
import graph
import print_dependencies_helper
import serialization


@dataclass
class PrintMode:
    """Options of how and which dependencies to output."""
    inbound: bool
    outbound: bool
    build_targets: bool
    fully_qualified: bool


def get_class_name_to_display(fully_qualified_name: str,
                              print_mode: PrintMode) -> str:
    if print_mode.fully_qualified:
        return fully_qualified_name
    else:
        return chrome_names.shorten_class(fully_qualified_name)


def get_build_target_name_to_display(build_target: str,
                                     print_mode: PrintMode) -> str:
    if print_mode.fully_qualified:
        return build_target
    else:
        return chrome_names.shorten_build_target(build_target)


def print_with_indent(indent: int, message: str,
                      bullet_point: str = '') -> None:
    indents = ' ' * indent
    bullet_point_text = f'{bullet_point} ' if bullet_point else ''
    print(f'{indents}{bullet_point_text}{message}')


def print_class_nodes_grouped_by_target(
        class_nodes: List[class_dependency.JavaClass], print_mode: PrintMode,
        bullet_point: None):
    # TODO(crbug.com/1124836): This is not quite correct because
    # sets considered equal can be converted to different strings. Fix this by
    # making JavaClass.build_targets return a List instead of a Set.
    class_nodes = sorted(class_nodes, key=lambda c: str(c.build_targets))
    last_build_target = None
    for class_node in class_nodes:
        build_target = str(class_node.build_targets)
        if last_build_target != build_target:
            build_target_names = [
                get_build_target_name_to_display(target, print_mode)
                for target in class_node.build_targets
            ]
            print_with_indent(4, f'[{", ".join(build_target_names)}]')
            last_build_target = build_target
        print_with_indent(
            8, get_class_name_to_display(class_node.name, print_mode),
            bullet_point)


def print_class_nodes(class_nodes: List[class_dependency.JavaClass],
                      print_mode: PrintMode, bullet_point: None):
    if print_mode.build_targets:
        print_class_nodes_grouped_by_target(class_nodes, print_mode,
                                            bullet_point)
    else:
        for class_node in class_nodes:
            print_with_indent(
                8, get_class_name_to_display(class_node.name, print_mode),
                bullet_point)


def print_class_dependencies_for_key(
        class_graph: class_dependency.JavaClassDependencyGraph, key: str,
        print_mode: PrintMode):
    """Prints dependencies for a valid key into the class graph."""
    node: class_dependency.JavaClass = class_graph.get_node_by_key(key)
    class_name = get_class_name_to_display(node.name, print_mode)

    if print_mode.inbound:
        print(
            f'{len(node.inbound)} inbound dependency(ies) into {class_name}:')
        print_class_nodes(graph.sorted_nodes_by_name(node.inbound), print_mode,
                          '<-')

    if print_mode.outbound:
        print(
            f'{len(node.outbound)} outbound dependency(ies) from {class_name}:'
        )
        print_class_nodes(graph.sorted_nodes_by_name(node.outbound),
                          print_mode, '->')


def main():
    """Prints class-level dependencies for one or more input classes."""
    arg_parser = argparse.ArgumentParser(
        description='Given a JSON dependency graph, output '
        'the class-level dependencies for a given list of classes.')
    required_arg_group = arg_parser.add_argument_group('required arguments')
    required_arg_group.add_argument(
        '-f',
        '--file',
        required=True,
        help='Path to the JSON file containing the dependency graph. '
        'See the README on how to generate this file.')
    required_arg_group.add_argument(
        '-c',
        '--classes',
        required=True,
        dest='class_names',
        help='Case-sensitive name of the classes to print dependencies for. '
        'Matches either the simple class name without package or the fully '
        'qualified class name. For example, `AppHooks` matches '
        '`org.chromium.browser.AppHooks`. Specify multiple classes with a '
        'comma-separated list, for example '
        '`ChromeActivity,ChromeTabbedActivity`')
    direction_arg_group = arg_parser.add_mutually_exclusive_group()
    direction_arg_group.add_argument('--inbound',
                                     dest='inbound_only',
                                     action='store_true',
                                     help='Print inbound dependencies only.')
    direction_arg_group.add_argument('--outbound',
                                     dest='outbound_only',
                                     action='store_true',
                                     help='Print outbound dependencies only.')
    arg_parser.add_argument('--no-build-target',
                            action='store_true',
                            help='Do not print build target (cleaner output).')
    arg_parser.add_argument('--fully-qualified',
                            action='store_true',
                            help='Use fully qualified class names instead of '
                            'shortened ones.')
    arguments = arg_parser.parse_args()

    print_mode = PrintMode(inbound=not arguments.outbound_only,
                           outbound=not arguments.inbound_only,
                           build_targets=not arguments.no_build_target,
                           fully_qualified=arguments.fully_qualified)

    class_graph = serialization.load_class_graph_from_file(arguments.file)
    class_graph_keys = [node.name for node in class_graph.nodes]

    class_names = arguments.class_names.split(',')

    for i, class_name in enumerate(class_names):
        valid_keys = print_dependencies_helper.get_valid_class_keys_matching(
            class_graph_keys, class_name)

        if i > 0:
            print()

        if len(valid_keys) == 0:
            print(f'No class found by the name {class_name}.')
        elif len(valid_keys) > 1:
            print(f'Multiple valid keys found for the name {class_name}, '
                  'please disambiguate between one of the following options:')
            for valid_key in valid_keys:
                print(f'\t{valid_key}')
        else:  # len(valid_keys) == 1
            fully_qualified_class_name = valid_keys[0]
            class_name = get_class_name_to_display(fully_qualified_class_name,
                                                   print_mode)
            print(f'Printing class dependencies for {class_name}:')
            print_class_dependencies_for_key(class_graph,
                                             fully_qualified_class_name,
                                             print_mode)


if __name__ == '__main__':
    main()
