#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool to run jdeps and process its output into a JSON file."""

import argparse
import functools
import math
import multiprocessing
import pathlib
import os
import subprocess
import sys

from typing import List, Tuple

import class_dependency
import package_dependency
import serialization

SRC_PATH = pathlib.Path(__file__).resolve().parents[3]  # src/
JDEPS_PATH = SRC_PATH.joinpath('third_party/jdk/current/bin/jdeps')
DEFAULT_ROOT_TARGET = 'chrome/android:monochrome_public_bundle'


def class_is_interesting(name: str):
    """Checks if a jdeps class is a class we are actually interested in."""
    if name.startswith('org.chromium.'):
        return True
    return False


# pylint: disable=useless-object-inheritance
class JavaClassJdepsParser(object):
    """A parser for jdeps class-level dependency output."""
    def __init__(self):  # pylint: disable=missing-function-docstring
        self._graph = class_dependency.JavaClassDependencyGraph()

    @property
    def graph(self):
        """The dependency graph of the jdeps output.

        Initialized as empty and updated using parse_raw_jdeps_output.
        """
        return self._graph

    def parse_raw_jdeps_output(self, build_target: str, jdeps_output: str):
        """Parses the entirety of the jdeps output."""
        for line in jdeps_output.split('\n'):
            self.parse_line(build_target, line)

    def parse_line(self, build_target: str, line: str):
        """Parses a line of jdeps output.

        The assumed format of the line starts with 'name_1 -> name_2'.
        """
        parsed = line.split()
        if len(parsed) <= 3:
            return
        if parsed[2] == 'not' and parsed[3] == 'found':
            return
        if parsed[1] != '->':
            return

        dep_from = parsed[0]
        dep_to = parsed[2]
        if not class_is_interesting(dep_from):
            return

        key_from, nested_from = class_dependency.split_nested_class_from_key(
            dep_from)
        from_node: class_dependency.JavaClass = self._graph.add_node_if_new(
            key_from)
        from_node.add_build_target(build_target)

        if not class_is_interesting(dep_to):
            return

        key_to, nested_to = class_dependency.split_nested_class_from_key(
            dep_to)

        self._graph.add_node_if_new(key_to)
        if key_from != key_to:  # Skip self-edges (class-nested dependency)
            self._graph.add_edge_if_new(key_from, key_to)
        if nested_from is not None:
            from_node.add_nested_class(nested_from)
        if nested_to is not None:
            from_node.add_nested_class(nested_to)



def _run_command(command: List[str]) -> str:
    """Runs a command and returns the output.

    Raises an exception and prints the command output if the command fails."""
    try:
        run_result = subprocess.run(command,
                                    capture_output=True,
                                    text=True,
                                    check=True)
    except subprocess.CalledProcessError as e:
        print(f'{command} failed with code {e.returncode}.', file=sys.stderr)
        print(f'\nSTDERR:\n{e.stderr}', file=sys.stderr)
        print(f'\nSTDOUT:\n{e.stdout}', file=sys.stderr)
        raise
    return run_result.stdout


def _run_jdeps(jdeps_path: str, filepath: pathlib.Path) -> str:
    """Runs jdeps on the given filepath and returns the output."""
    print(f'Running jdeps and parsing output for {filepath}')
    return _run_command([jdeps_path, '-R', '-verbose:class', filepath])


def _run_gn_desc_list_dependencies(build_output_dir: str, target: str,
                                   gn_path: str) -> str:
    """Runs gn desc to list all jars that a target depends on.

    This includes direct and indirect dependencies."""
    return _run_command(
        [gn_path, 'desc', '--all', build_output_dir, target, 'deps'])


JarTargetList = List[Tuple[str, pathlib.Path]]


def list_original_targets_and_jars(gn_desc_output: str,
                                   build_output_dir: str) -> JarTargetList:
    """Parses gn desc output to list original java targets and output jar paths.

    Returns a list of tuples (build_target: str, jar_path: str), where:
    - build_target is the original java dependency target in the form
      "//path/to:target"
    - jar_path is the path to the built jar in the build_output_dir,
      including the path to the output dir
    """
    jar_tuples: JarTargetList = []
    for build_target_line in gn_desc_output.split('\n'):
        if not build_target_line.endswith('__compile_java'):
            continue
        build_target = build_target_line.strip()
        original_build_target = build_target.replace('__compile_java', '')
        jar_path = _get_jar_path_for_target(build_output_dir, build_target)
        jar_tuples.append((original_build_target, jar_path))
    return jar_tuples


def _get_jar_path_for_target(build_output_dir: str, build_target: str) -> str:
    """Calculates the output location of a jar for a java build target."""
    target_path, target_name = build_target.split(':')
    assert target_path.startswith('//'), \
        f'Build target should start with "//" but is: "{build_target}"'
    jar_dir = target_path[len('//'):]
    jar_name = target_name.replace('__compile_java', '.javac.jar')
    return pathlib.Path(build_output_dir) / 'obj' / jar_dir / jar_name


def main():
    """Runs jdeps on all JARs a build target depends on.

    Creates a JSON file from the jdeps output."""
    arg_parser = argparse.ArgumentParser(
        description='Runs jdeps (dependency analysis tool) on all JARs a root '
        'build target depends on and writes the resulting dependency graph '
        'into a JSON file. The default root build target is '
        'chrome/android:monochrome_public_bundle.')
    required_arg_group = arg_parser.add_argument_group('required arguments')
    required_arg_group.add_argument('-C',
                                    '--build_output_dir',
                                    required=True,
                                    help='Build output directory.')
    required_arg_group.add_argument(
        '-o',
        '--output',
        required=True,
        help='Path to the file to write JSON output to. Will be created '
        'if it does not yet exist and overwrite existing '
        'content if it does.')
    arg_parser.add_argument('-t',
                            '--target',
                            default=DEFAULT_ROOT_TARGET,
                            help='Root build target.')
    arg_parser.add_argument('-j',
                            '--jdeps-path',
                            default=JDEPS_PATH,
                            help='Path to the jdeps executable.')
    arg_parser.add_argument('-g',
                            '--gn-path',
                            default='gn',
                            help='Path to the gn executable.')
    arguments = arg_parser.parse_args()

    # gn must be run from inside the git checkout.
    os.chdir(SRC_PATH)

    print('Getting list of dependency jars...')
    gn_desc_output = _run_gn_desc_list_dependencies(arguments.build_output_dir,
                                                    arguments.target,
                                                    arguments.gn_path)
    target_jars: JarTargetList = list_original_targets_and_jars(
        gn_desc_output, arguments.build_output_dir)

    print('Running jdeps...')
    # jdeps already has some parallelism
    jdeps_process_number = math.ceil(multiprocessing.cpu_count() / 2)
    with multiprocessing.Pool(jdeps_process_number) as pool:
        jar_paths = [target_jar for _, target_jar in target_jars]
        jdeps_outputs = pool.map(
            functools.partial(_run_jdeps, arguments.jdeps_path), jar_paths)

    print('Parsing jdeps output...')
    jdeps_parser = JavaClassJdepsParser()
    for raw_jdeps_output, (build_target, _) in zip(jdeps_outputs, target_jars):
        jdeps_parser.parse_raw_jdeps_output(build_target, raw_jdeps_output)

    class_graph = jdeps_parser.graph
    print(f'Parsed class-level dependency graph, '
          f'got {class_graph.num_nodes} nodes '
          f'and {class_graph.num_edges} edges.')

    package_graph = package_dependency.JavaPackageDependencyGraph(class_graph)
    print(f'Created package-level dependency graph, '
          f'got {package_graph.num_nodes} nodes '
          f'and {package_graph.num_edges} edges.')

    print(f'Dumping JSON representation to {arguments.output}.')
    serialization.dump_class_and_package_graphs_to_file(
        class_graph, package_graph, arguments.output)


if __name__ == '__main__':
    main()
