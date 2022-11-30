# Lint as: python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Various helpers for printing dependencies."""

from typing import List

import class_dependency
import package_dependency


def get_valid_classes_from_class_input(
        class_graph: class_dependency.JavaClassDependencyGraph,
        class_names_input: str) -> List[str]:
    """Parses classes given as input into fully qualified, valid classes.

    Input is a comma-separated list of classes."""
    class_names = class_names_input.split(',')
    return get_valid_classes_from_class_list(class_graph, class_names)


def get_valid_classes_from_class_list(
        class_graph: class_dependency.JavaClassDependencyGraph,
        class_names: List[str]) -> List[str]:
    """Parses classes given as input into fully qualified, valid classes.

    Input is a list of class names."""
    result = []

    class_graph_keys = [node.name for node in class_graph.nodes]

    for class_name in class_names:
        valid_keys = get_valid_class_keys_matching(class_graph_keys,
                                                   class_name)

        _check_only_one_valid_key(valid_keys, class_name, 'class')

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
        valid_keys = get_valid_package_keys_matching(package_graph_keys,
                                                     package_name)

        _check_only_one_valid_key(valid_keys, package_name, 'package')

        package_key: str = valid_keys[0]
        package_node: package_dependency.JavaPackage = \
            package_graph.get_node_by_key(package_key)
        classes_in_package: List[str] = sorted(package_node.classes.keys())
        result.extend(classes_in_package)

    return result


def _check_only_one_valid_key(valid_keys: List[str], key_input: str,
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


def get_valid_package_keys_matching(all_keys: List,
                                    input_key: str) -> List[str]:
    """Return a list of keys of graph nodes that match a package input.

    For our use case (matching user input to package nodes),
    a valid key is one that ends with the input, case insensitive.
    For example, 'apphooks' matches 'org.chromium.browser.AppHooks'.
    """
    input_key_lower = input_key.lower()
    return [key for key in all_keys if key.lower().endswith(input_key_lower)]


def get_valid_class_keys_matching(all_keys: List, input_key: str) -> List[str]:
    """Return a list of keys of graph nodes that match a class input.

    For our use case (matching user input to class nodes),
    a valid key is one that matches fully the input either fully qualified or
    ignoring package, case sensitive.
    For example, the inputs 'org.chromium.browser.AppHooks' and 'AppHooks'
    match the node 'org.chromium.browser.AppHooks' but 'Hooks' does not.
    """
    if '.' in input_key:
        # Match full name with package only.
        return [input_key] if input_key in all_keys else []
    else:
        # Match class name in any package.
        return [key for key in all_keys if key.endswith(f'.{input_key}')]
