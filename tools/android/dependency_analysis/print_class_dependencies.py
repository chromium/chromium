#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool for printing class-level dependencies."""

import argparse
from dataclasses import dataclass
from typing import List, Set, Tuple

import chrome_names
import class_dependency
import graph
import package_dependency
import print_dependencies_helper
import serialization


@dataclass
class PrintMode:
    """Options of how and which dependencies to output."""
    inbound: bool
    outbound: bool
    ignore_modularized: bool
    fully_qualified: bool


class TargetDependencies:
    """Build target dependencies that the set of classes depends on."""

    def __init__(self):
        # Build targets usable by modularized code that need to be depended on.
        self.cleared: Set[str] = set()

        # Build targets usable by modularized code that might need to be
        # depended on. This happens rarely, when a class dependency is in
        # multiple build targets (Android resource .R classes, or due to
        # bytecode rewriting).
        self.dubious: Set[str] = set()

    def update_with_class_node(self, class_node: class_dependency.JavaClass):
        if len(class_node.build_targets) == 1:
            self.cleared.update(class_node.build_targets)
        else:
            self.dubious.update(class_node.build_targets)

    def merge(self, other: 'TargetDependencies'):
        self.cleared.update(other.cleared)
        self.dubious.update(other.dubious)

    def print(self):
        if self.cleared:
            print()
            print('Cleared dependencies:')
            for dep in sorted(self.cleared):
                print(dep)
        if self.dubious:
            print()
            print('Dubious dependencies due to classes with multiple build '
                  'targets. Only some of these are required:')
            for dep in sorted(self.dubious):
                print(dep)


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
IGNORED_CLASSES = {'org.chromium.base.natives.GEN_JNI'}


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


def is_allowed_target_dependency(build_target: str) -> bool:
    return any(build_target.startswith(p) for p in ALLOWED_PREFIXES)


def is_ignored_class_dependency(class_name: str) -> bool:
    return class_name in IGNORED_CLASSES


def print_class_nodes(class_nodes: List[class_dependency.JavaClass],
                      print_mode: PrintMode, class_name: str,
                      direction: str) -> TargetDependencies:
    """Prints the class dependencies to or from a class, grouped by target.

    If direction is OUTBOUND and print_mode.ignore_modularized is True, omits
    modularized outbound dependencies and returns the build targets that need
    to be added for those dependencies. In other cases, returns an empty
    TargetDependencies.
    """
    ignore_modularized = direction == OUTBOUND and print_mode.ignore_modularized
    bullet_point = '<-' if direction == INBOUND else '->'

    print_backlog: List[Tuple[int, str]] = []

    # TODO(crbug.com/1124836): This is not quite correct because
    # sets considered equal can be converted to different strings. Fix this by
    # making JavaClass.build_targets return a List instead of a Set.
    suspect_dependencies = 0

    target_dependencies = TargetDependencies()
    class_nodes = sorted(class_nodes, key=lambda c: str(c.build_targets))
    last_build_target = None
    for class_node in class_nodes:
        if is_ignored_class_dependency(class_node.name):
            continue
        if ignore_modularized:
            if all(
                    is_allowed_target_dependency(target)
                    for target in class_node.build_targets):
                target_dependencies.update_with_class_node(class_node)
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
        print(f'{class_name} has {suspect_dependencies} outbound dependencies '
              f'that may need to be broken (omitted {cleared} cleared '
              f'dependencies):')
    else:
        if direction == INBOUND:
            print(f'{class_name} has {len(class_nodes)} inbound dependencies:')
        else:
            print(
                f'{class_name} has {len(class_nodes)} outbound dependencies:')

    # Print build targets and dependencies
    for indent, message in print_backlog:
        indents = ' ' * indent
        print(f'{indents}{message}')

    return target_dependencies


def print_class_dependencies_for_key(
        class_graph: class_dependency.JavaClassDependencyGraph, key: str,
        print_mode: PrintMode) -> TargetDependencies:
    """Prints dependencies for a valid key into the class graph."""
    target_dependencies = TargetDependencies()
    node: class_dependency.JavaClass = class_graph.get_node_by_key(key)
    class_name = get_class_name_to_display(node.name, print_mode)

    if print_mode.inbound:
        print_class_nodes(graph.sorted_nodes_by_name(node.inbound), print_mode,
                          class_name, INBOUND)

    if print_mode.outbound:
        target_dependencies = print_class_nodes(
            graph.sorted_nodes_by_name(node.outbound), print_mode, class_name,
            OUTBOUND)
    return target_dependencies


def get_valid_classes_from_class_input(
        class_graph: class_dependency.JavaClassDependencyGraph,
        class_names_input: str) -> List[str]:
    """Parses classes given as input into fully qualified, valid classes."""
    result = []

    class_graph_keys = [node.name for node in class_graph.nodes]

    class_names = class_names_input.split(',')

    for class_name in class_names:
        valid_keys = print_dependencies_helper.get_valid_class_keys_matching(
            class_graph_keys, class_name)

        check_only_one_valid_key(valid_keys, class_name, 'class')

        result.append(valid_keys[0])

    return result


def get_valid_classes_from_package_input(
        package_graph: package_dependency.JavaPackageDependencyGraph,
        package_names_input: str) -> List[str]:
    """Parses packages given as input into fully qualified, valid classes."""
    result = []

    package_graph_keys = [node.name for node in package_graph.nodes]

    package_names = package_names_input.split(',')

    for package_name in package_names:
        valid_keys = print_dependencies_helper.get_valid_package_keys_matching(
            package_graph_keys, package_name)

        check_only_one_valid_key(valid_keys, package_name, 'package')

        package_key: str = valid_keys[0]
        package_node: package_dependency.JavaPackage = \
            package_graph.get_node_by_key(package_key)
        classes_in_package: List[str] = sorted(package_node.classes.keys())
        result.extend(classes_in_package)

    return result


def check_only_one_valid_key(valid_keys: List[str], key_input: str,
                             entity: str) -> None:
    if len(valid_keys) == 0:
        raise ValueError(f'No {entity} found by the name {key_input}.')
    elif len(valid_keys) > 1:
        print(f'Multiple valid keys found for the name {key_input}, '
              'please disambiguate between one of the following options:')
        for valid_key in valid_keys:
            print(f'\t{valid_key}')
        raise ValueError(
            f'Multiple valid keys found for the name {key_input}.')
    else:  # len(valid_keys) == 1
        return


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
    required_arg_group_either = arg_parser.add_argument_group(
        'required arguments (at least one)')
    required_arg_group_either.add_argument(
        '-c',
        '--classes',
        dest='class_names',
        help='Case-sensitive name of the classes to print dependencies for. '
        'Matches either the simple class name without package or the fully '
        'qualified class name. For example, `AppHooks` matches '
        '`org.chromium.browser.AppHooks`. Specify multiple classes with a '
        'comma-separated list, for example '
        '`ChromeActivity,ChromeTabbedActivity`')
    required_arg_group_either.add_argument(
        '-p',
        '--packages',
        dest='package_names',
        help='Case-sensitive name of the packages to print dependencies for, '
        'such as `org.chromium.browser`. Specify multiple packages with a '
        'comma-separated list.`')
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

    if not arguments.class_names and not arguments.package_names:
        raise ValueError('Either -c/--classes or -p/--packages need to be '
                         'specified.')

    print_mode = PrintMode(inbound=not arguments.outbound_only,
                           outbound=not arguments.inbound_only,
                           ignore_modularized=arguments.ignore_modularized,
                           fully_qualified=arguments.fully_qualified)

    class_graph, package_graph = \
        serialization.load_class_and_package_graphs_from_file(arguments.file)

    valid_class_names = []
    if arguments.class_names:
        valid_class_names.extend(
            get_valid_classes_from_class_input(class_graph,
                                               arguments.class_names))
    if arguments.package_names:
        valid_class_names.extend(
            get_valid_classes_from_package_input(package_graph,
                                                 arguments.package_names))

    target_dependencies = TargetDependencies()
    for i, fully_qualified_class_name in enumerate(valid_class_names):
        if i > 0:
            print()

        new_target_deps = print_class_dependencies_for_key(
            class_graph, fully_qualified_class_name, print_mode)
        target_dependencies.merge(new_target_deps)

    target_dependencies.print()


if __name__ == '__main__':
    main()
