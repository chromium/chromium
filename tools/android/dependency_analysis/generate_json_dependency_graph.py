#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool to run jdeps and process its output into a JSON file."""

import argparse
import functools
import logging
import math
import multiprocessing
import pathlib
import os
import subprocess
import sys

from typing import List, Tuple, Union

import class_dependency
import git_utils
import package_dependency
import serialization
import subprocess_utils
import target_dependency

_SRC_PATH = pathlib.Path(__file__).parents[3].resolve()
sys.path.append(str(_SRC_PATH / 'build' / 'android'))
from pylib import constants

_DEFAULT_ROOT_TARGET = 'chrome/android:monochrome_public_bundle'
_DEFAULT_PREFIX = 'org.chromium.'
_TARGETS_WITH_NO_SOURCE_FILES = set([
    '//components/module_installer/android:module_interface_java',
    '//base:jni_java'
])


def _relsrc(path: Union[str, pathlib.Path]):
    return pathlib.Path(path).relative_to(_SRC_PATH)


def class_is_interesting(name: str, prefixes: Tuple[str]):
    """Checks if a jdeps class is a class we are actually interested in."""
    if not prefixes or name.startswith(prefixes):
        return True
    return False


class JavaClassJdepsParser:
    """A parser for jdeps class-level dependency output."""

    def __init__(self):
        self._graph = class_dependency.JavaClassDependencyGraph()

    @property
    def graph(self):
        """The dependency graph of the jdeps output.

        Initialized as empty and updated using parse_raw_jdeps_output.
        """
        return self._graph

    def parse_raw_jdeps_output(self, build_target: str, jdeps_output: str,
                               prefixes: Tuple[str]):
        """Parses the entirety of the jdeps output."""
        for line in jdeps_output.split('\n'):
            self.parse_line(build_target, line, prefixes)

    def parse_line(self,
                   build_target: str,
                   line: str,
                   prefixes: Tuple[str] = (_DEFAULT_PREFIX, )):
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
        if not class_is_interesting(dep_from, prefixes):
            return

        key_from, nested_from = class_dependency.split_nested_class_from_key(
            dep_from)
        from_node: class_dependency.JavaClass = self._graph.add_node_if_new(
            key_from)
        from_node.add_build_target(build_target)

        if not class_is_interesting(dep_to, prefixes):
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


def _run_jdeps(jdeps_path: pathlib.Path, filepath: pathlib.Path) -> str:
    """Runs jdeps on the given filepath and returns the output.

    Uses a simple file cache for the output of jdeps. If the jar file's mtime is
    older than the jdeps cache then just use the cached content instead.
    Otherwise jdeps is run again and the output used to update the file cache.

    Tested Nov 2nd, 2022:
    - With all cache hits, script takes 13 seconds.
    - Without the cache, script takes 1 minute 14 seconds.
    """
    assert filepath.exists(), (
        f'Jar file missing for jdeps {filepath}, perhaps some targets need to '
        'be added to _TARGETS_WITH_NO_SOURCE_FILES?')

    cache_path = filepath.with_suffix('.jdeps_cache')
    if (cache_path.exists()
            and cache_path.stat().st_mtime > filepath.stat().st_mtime):
        logging.debug(f'Found valid jdeps cache at {_relsrc(cache_path)}')
        with cache_path.open() as f:
            return f.read()

    # Cache either doesn't exist or is older than the jar file.
    logging.debug(f'Running jdeps and parsing output for {_relsrc(filepath)}')
    output = subprocess_utils.run_command(
        [str(jdeps_path), '-R', '-verbose:class',
         str(filepath)])
    with cache_path.open('w') as f:
        f.write(output)
    return output


def _run_gn_desc_list_dependencies(build_output_dir: str, target: str,
                                   gn_path: str) -> str:
    """Runs gn desc to list all jars that a target depends on.

    This includes direct and indirect dependencies."""
    return subprocess_utils.run_command(
        [gn_path, 'desc', '--all', build_output_dir, target, 'deps'])


JarTargetList = List[Tuple[str, pathlib.Path]]


def list_original_targets_and_jars(gn_desc_output: str, build_output_dir: str,
                                   cr_position: int) -> JarTargetList:
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
        jar_path = _get_jar_path_for_target(build_output_dir, build_target,
                                            cr_position)
        # Bundle module targets have no javac jars.
        if (original_build_target.endswith('_bundle_module')
                or original_build_target in _TARGETS_WITH_NO_SOURCE_FILES):
            assert not jar_path.exists(), (
                f'Perhaps a source file was added to {original_build_target}?')
            continue
        jar_tuples.append((original_build_target, jar_path))
    return jar_tuples


def _get_jar_path_for_target(build_output_dir: str, build_target: str,
                             cr_position: int) -> pathlib.Path:
    """Calculates the output location of a jar for a java build target."""
    if cr_position == 0:  # Not running on main branch, use current convention.
        subdirectory = 'obj'
    elif cr_position < 761560:  # crrev.com/c/2161205
        subdirectory = 'gen'
    else:
        subdirectory = 'obj'
    target_path, target_name = build_target.split(':')
    assert target_path.startswith('//'), \
        f'Build target should start with "//" but is: "{build_target}"'
    jar_dir = target_path[len('//'):]
    jar_name = target_name.replace('__compile_java', '.javac.jar')
    return pathlib.Path(build_output_dir) / subdirectory / jar_dir / jar_name


def main():
    """Runs jdeps on all JARs a build target depends on.

    Creates a JSON file from the jdeps output."""
    arg_parser = argparse.ArgumentParser(
        description='Runs jdeps (dependency analysis tool) on all JARs a root '
        'build target depends on and writes the resulting dependency graph '
        'into a JSON file. The default root build target is '
        'chrome/android:monochrome_public_bundle and the default prefix is '
        '"org.chromium.".')
    required_arg_group = arg_parser.add_argument_group('required arguments')
    required_arg_group.add_argument(
        '-o',
        '--output',
        required=True,
        help='Path to the file to write JSON output to. Will be created '
        'if it does not yet exist and overwrite existing content if it does.')
    arg_parser.add_argument(
        '-C',
        '--build_output_dir',
        help='Build output directory, will guess if not provided.')
    arg_parser.add_argument(
        '-p',
        '--prefixes',
        default=_DEFAULT_PREFIX,
        help='A comma-separated list of prefixes to filter '
        'classes. Class paths that do not match any of the '
        'prefixes are ignored in the graph. Pass in an '
        'empty string to turn off filtering.')
    arg_parser.add_argument('-t',
                            '--target',
                            default=_DEFAULT_ROOT_TARGET,
                            help='Root build target.')
    arg_parser.add_argument('-d',
                            '--checkout-dir',
                            help='Path to the chromium checkout directory.')
    arg_parser.add_argument('-j',
                            '--jdeps-path',
                            help='Path to the jdeps executable.')
    arg_parser.add_argument('-g',
                            '--gn-path',
                            default='gn',
                            help='Path to the gn executable.')
    arg_parser.add_argument('-v',
                            '--verbose',
                            action='store_true',
                            help='Used to display detailed logging.')
    arguments = arg_parser.parse_args()

    if arguments.verbose:
        level = logging.DEBUG
    else:
        level = logging.INFO
    logging.basicConfig(
        level=level, format='%(levelname).1s %(relativeCreated)6d %(message)s')

    if arguments.checkout_dir:
        src_path = pathlib.Path(arguments.checkout_dir)
    else:
        src_path = pathlib.Path(__file__).resolve().parents[3]

    if arguments.jdeps_path:
        jdeps_path = pathlib.Path(arguments.jdeps_path)
    else:
        jdeps_path = src_path.joinpath('third_party/jdk/current/bin/jdeps')

    # gn and git must be run from inside the git checkout.
    os.chdir(src_path)

    cr_position_str = git_utils.get_last_commit_cr_position()
    cr_position = int(cr_position_str) if cr_position_str else 0

    if arguments.build_output_dir:
        constants.SetOutputDirectory(arguments.build_output_dir)
    constants.CheckOutputDirectory()
    arguments.build_output_dir = constants.GetOutDirectory()
    logging.info(f'Using output dir: {_relsrc(arguments.build_output_dir)}')

    logging.info('Getting list of dependency jars...')
    gn_desc_output = _run_gn_desc_list_dependencies(arguments.build_output_dir,
                                                    arguments.target,
                                                    arguments.gn_path)
    target_jars: JarTargetList = list_original_targets_and_jars(
        gn_desc_output, arguments.build_output_dir, cr_position)

    # Need to trim off leading // to convert gn target to ninja target.
    missing_targets = [
        target_name[2:] for target_name, path in target_jars
        if not path.exists()
    ]
    if missing_targets:
        logging.warning(
            f'Missing {len(missing_targets)} jars, re-building the targets.')
        subprocess.run(['autoninja', '-C', arguments.build_output_dir] +
                       missing_targets,
                       check=True)

    logging.info('Running jdeps...')
    # jdeps already has some parallelism
    jdeps_process_number = math.ceil(multiprocessing.cpu_count() / 2)
    with multiprocessing.Pool(jdeps_process_number) as pool:
        jar_paths = [target_jar for _, target_jar in target_jars]
        jdeps_outputs = pool.map(functools.partial(_run_jdeps, jdeps_path),
                                 jar_paths)

    logging.info('Parsing jdeps output...')
    prefixes = tuple(arguments.prefixes.split(','))
    jdeps_parser = JavaClassJdepsParser()
    for raw_jdeps_output, (build_target, _) in zip(jdeps_outputs, target_jars):
        logging.debug(f'Parsing jdeps for {build_target}')
        jdeps_parser.parse_raw_jdeps_output(build_target,
                                            raw_jdeps_output,
                                            prefixes=prefixes)

    class_graph = jdeps_parser.graph
    logging.info(f'Parsed class-level dependency graph, '
                 f'got {class_graph.num_nodes} nodes '
                 f'and {class_graph.num_edges} edges.')

    package_graph = package_dependency.JavaPackageDependencyGraph(class_graph)
    logging.info(f'Created package-level dependency graph, '
                 f'got {package_graph.num_nodes} nodes '
                 f'and {package_graph.num_edges} edges.')

    target_graph = target_dependency.JavaTargetDependencyGraph(class_graph)
    logging.info(f'Created target-level dependency graph, '
                 f'got {target_graph.num_nodes} nodes '
                 f'and {target_graph.num_edges} edges.')

    logging.info(f'Dumping JSON representation to {arguments.output}.')
    serialization.dump_class_and_package_and_target_graphs_to_file(
        class_graph, package_graph, target_graph, arguments.output)
    logging.info('Done')


if __name__ == '__main__':
    main()
