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
    ignore_modularized: bool
    fully_qualified: bool


INBOUND = 'inbound'
OUTBOUND = 'outbound'
ALLOWED_PREFIXES = {
    '//base/',
    '//base:',
    '//chrome/browser/',
    '//components/',
    '//content/',
    '//ui/',
    '//url:',
}


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


def is_allowed_dependency(build_target: str) -> bool:
    return any(build_target.startswith(p) for p in ALLOWED_PREFIXES)


def print_class_nodes(class_nodes: List[class_dependency.JavaClass],
                      print_mode: PrintMode, class_name: str, direction: str):
    ignore_modularized = direction == OUTBOUND and print_mode.ignore_modularized
    bullet_point = '<-' if direction == INBOUND else '->'

    print_backlog: List[Tuple[int, str]] = []

    # TODO(crbug.com/1124836): This is not quite correct because
    # sets considered equal can be converted to different strings. Fix this by
    # making JavaClass.build_targets return a List instead of a Set.
    suspect_dependencies = 0
    class_nodes = sorted(class_nodes, key=lambda c: str(c.build_targets))
    last_build_target = None
    for class_node in class_nodes:
        if ignore_modularized:
            if all(
                    is_allowed_dependency(target)
                    for target in class_node.build_targets):
                continue
            else:
                suspect_dependencies += 1
        build_target = str(class_node.build_targets)
        if last_build_target != build_target:
            build_target_names = [
                get_build_target_name_to_display(target, print_mode)
                for target in class_node.build_targets
            ]
            build_target_names_string = ", ".join(build_target_names)
            print_backlog.append((4, f'[{build_target_names_string}]'))
            last_build_target = build_target
        display_name = get_class_name_to_display(class_node.name, print_mode)
        print_backlog.append((8, f'{bullet_point} {display_name}'))

    # Print header
    if ignore_modularized:
        cleared = len(class_nodes) - suspect_dependencies
        print(
            f'{suspect_dependencies} outbound dependencies from {class_name} '
            f'may need to be broken (omitted {cleared} cleared dependencies):')
    else:
        if direction == INBOUND:
            print(
                f'{len(class_nodes)} inbound dependencies into {class_name}:')
        else:
            print(
                f'{len(class_nodes)} outbound dependencies from {class_name}:')

    # Print build targets and dependencies
    for indent, message in print_backlog:
        indents = ' ' * indent
        print(f'{indents}{message}')


def print_class_dependencies_for_key(
        class_graph: class_dependency.JavaClassDependencyGraph, key: str,
        print_mode: PrintMode):
    """Prints dependencies for a valid key into the class graph."""
    node: class_dependency.JavaClass = class_graph.get_node_by_key(key)
    class_name = get_class_name_to_display(node.name, print_mode)

    if print_mode.inbound:
        print_class_nodes(graph.sorted_nodes_by_name(node.inbound), print_mode,
                          class_name, INBOUND)

    if print_mode.outbound:
        print_class_nodes(graph.sorted_nodes_by_name(node.outbound),
                          print_mode, class_name, OUTBOUND)


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
    arg_parser.add_argument('--fully-qualified',
                            action='store_true',
                            help='Use fully qualified class names instead of '
                            'shortened ones.')
    arg_parser.add_argument('--ignore-modularized',
                            action='store_true',
                            help='Do not print outbound dependencies on '
                            'allowed (modules, components, base, etc.) '
                            'dependencies.')
    arguments = arg_parser.parse_args()

    print_mode = PrintMode(inbound=not arguments.outbound_only,
                           outbound=not arguments.inbound_only,
                           ignore_modularized=arguments.ignore_modularized,
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
            print_class_dependencies_for_key(class_graph,
                                             fully_qualified_class_name,
                                             print_mode)


if __name__ == '__main__':
    main()
