#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
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


# Return values of categorize_dependency().
IGNORE = 'ignore'
CLEAR = 'clear'
PRINT = 'print'


@dataclass
class PrintMode:
    """Options of how and which dependencies to output."""
    inbound: bool
    outbound: bool
    ignore_modularized: bool
    ignore_audited_here: bool
    ignore_same_package: bool
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


def categorize_dependency(from_class: class_dependency.JavaClass,
                          to_class: class_dependency.JavaClass,
                          ignore_modularized: bool, print_mode: PrintMode,
                          audited_classes: Set[str]) -> str:
    """Decides if a class dependency should be printed, cleared, or ignored."""
    if is_ignored_class_dependency(to_class.name):
        return IGNORE
    if ignore_modularized and all(
            is_allowed_target_dependency(target)
            for target in to_class.build_targets):
        return CLEAR
    if (print_mode.ignore_same_package
            and to_class.package == from_class.package):
        return IGNORE
    if print_mode.ignore_audited_here and to_class.name in audited_classes:
        return IGNORE
    return PRINT


def print_class_dependencies(to_classes: List[class_dependency.JavaClass],
                             print_mode: PrintMode,
                             from_class: class_dependency.JavaClass,
                             direction: str,
                             audited_classes: Set[str]) -> TargetDependencies:
    """Prints the class dependencies to or from a class, grouped by target.

    If direction is OUTBOUND and print_mode.ignore_modularized is True, omits
    modularized outbound dependencies and returns the build targets that need
    to be added for those dependencies. In other cases, returns an empty
    TargetDependencies.

    If print_mode.ignore_same_package is True, omits outbound dependencies in
    the same package.
    """
    ignore_modularized = direction == OUTBOUND and print_mode.ignore_modularized
    bullet_point = '<-' if direction == INBOUND else '->'

    print_backlog: List[Tuple[int, str]] = []

    # TODO(crbug.com/40147556): This is not quite correct because
    # sets considered equal can be converted to different strings. Fix this by
    # making JavaClass.build_targets return a List instead of a Set.
    suspect_dependencies = 0

    target_dependencies = TargetDependencies()
    to_classes = sorted(to_classes, key=lambda c: str(c.build_targets))
    last_build_target = None

    for to_class in to_classes:
        # Check if dependency should be ignored due to --ignore-modularized,
        # --ignore-same-package, or due to being an ignored class.
        # Check if dependency should be listed as a cleared dep.
        ignore_allow = categorize_dependency(from_class, to_class,
                                             ignore_modularized, print_mode,
                                             audited_classes)
        if ignore_allow == CLEAR:
            target_dependencies.update_with_class_node(to_class)
            continue
        elif ignore_allow == IGNORE:
            continue

        # Print the dependency
        suspect_dependencies += 1
        build_target = str(to_class.build_targets)
        if last_build_target != build_target:
            build_target_names = [
                get_build_target_name_to_display(target, print_mode)
                for target in to_class.build_targets
            ]
            build_target_names_string = ", ".join(build_target_names)
            print_backlog.append((4, f'[{build_target_names_string}]'))
            last_build_target = build_target
        display_name = get_class_name_to_display(to_class.name, print_mode)
        print_backlog.append((8, f'{bullet_point} {display_name}'))

    # Print header
    class_name = get_class_name_to_display(from_class.name, print_mode)
    if ignore_modularized:
        cleared = len(to_classes) - suspect_dependencies
        print(f'{class_name} has {suspect_dependencies} outbound dependencies '
              f'that may need to be broken (omitted {cleared} cleared '
              f'dependencies):')
    else:
        if direction == INBOUND:
            print(f'{class_name} has {len(to_classes)} inbound dependencies:')
        else:
            print(f'{class_name} has {len(to_classes)} outbound dependencies:')

    # Print build targets and dependencies
    for indent, message in print_backlog:
        indents = ' ' * indent
        print(f'{indents}{message}')

    return target_dependencies


def print_class_dependencies_for_key(
        class_graph: class_dependency.JavaClassDependencyGraph, key: str,
        print_mode: PrintMode,
        audited_classes: Set[str]) -> TargetDependencies:
    """Prints dependencies for a valid key into the class graph."""
    target_dependencies = TargetDependencies()
    node: class_dependency.JavaClass = class_graph.get_node_by_key(key)

    if print_mode.inbound:
        print_class_dependencies(graph.sorted_nodes_by_name(node.inbound),
                                 print_mode, node, INBOUND, audited_classes)

    if print_mode.outbound:
        target_dependencies = print_class_dependencies(
            graph.sorted_nodes_by_name(node.outbound), print_mode, node,
            OUTBOUND, audited_classes)
    return target_dependencies


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
    arg_parser.add_argument('--ignore-audited-here',
                            action='store_true',
                            help='Do not print outbound dependencies on '
                            'other classes being audited in this run.')
    arg_parser.add_argument('--ignore-same-package',
                            action='store_true',
                            help='Do not print outbound dependencies on '
                            'classes in the same package.')
    arguments = arg_parser.parse_args()

    if not arguments.class_names and not arguments.package_names:
        raise ValueError('Either -c/--classes or -p/--packages need to be '
                         'specified.')

    print_mode = PrintMode(inbound=not arguments.outbound_only,
                           outbound=not arguments.inbound_only,
                           ignore_modularized=arguments.ignore_modularized,
                           ignore_audited_here=arguments.ignore_audited_here,
                           ignore_same_package=arguments.ignore_same_package,
                           fully_qualified=arguments.fully_qualified)

    class_graph, package_graph, _ = \
        serialization.load_class_and_package_graphs_from_file(arguments.file)

    valid_class_names = []
    if arguments.class_names:
        valid_class_names.extend(
            print_dependencies_helper.get_valid_classes_from_class_input(
                class_graph, arguments.class_names))
    if arguments.package_names:
        valid_class_names.extend(
            print_dependencies_helper.get_valid_classes_from_package_input(
                package_graph, arguments.package_names))

    target_dependencies = TargetDependencies()
    for i, fully_qualified_class_name in enumerate(valid_class_names):
        if i > 0:
            print()

        new_target_deps = print_class_dependencies_for_key(
            class_graph, fully_qualified_class_name, print_mode,
            set(valid_class_names))
        target_dependencies.merge(new_target_deps)

    target_dependencies.print()


if __name__ == '__main__':
    main()
