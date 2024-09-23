# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import collections
import fnmatch
import imp
import logging
import os
import sys
import zipfile

from telemetry.internal.util import command_line
from telemetry.internal.util import path
from telemetry.internal.util import path_set

try:
  from modulegraph import modulegraph  # pylint: disable=import-error
except ImportError as err:
  modulegraph = None
  import_error = err

from core import bootstrap
from core import path_util

DEPS_FILE = 'bootstrap_deps'


def FindBootstrapDependencies(base_dir):
  deps_file = os.path.join(base_dir, DEPS_FILE)
  if not os.path.exists(deps_file):
    return []
  deps_paths = bootstrap.ListAllDepsPaths(deps_file)
  return set(os.path.realpath(os.path.join(
      path_util.GetChromiumSrcDir(), '..', deps_path))
             for deps_path in deps_paths)


def FindPythonDependencies(module_path):
  logging.info('Finding Python dependencies of %s', module_path)
  if modulegraph is None:
    raise import_error

  prefixes = [sys.prefix]
  if hasattr(sys, 'real_prefix'):
    prefixes.append(sys.real_prefix)
  logging.info('Excluding Prefixes: %r', prefixes)

  sys_path = sys.path
  sys.path = list(sys_path)
  try:
    # Load the module to inherit its sys.path modifications.
    sys.path.insert(0, os.path.abspath(os.path.dirname(module_path)))
    imp.load_source(
        os.path.splitext(os.path.basename(module_path))[0], module_path)

    # Analyze the module for its imports.
    graph = modulegraph.ModuleGraph()
    graph.run_script(module_path)

    # We do a BFS instead of checking all nodes because for some reason it is
    # possible to have bogus dependencies from the Python installation to other
    # files (which may not even exist) due to the packagepath, such as to `//-`.
    # This only appears to occur when run under Python 3. By performing BFS and
    # simply ignoring anything from the Python installation, we can avoid this
    # issue.
    nodes_to_visit = _GetSourceNodes(graph)
    visited = set()

    # Filter for only imports in Chromium.
    while nodes_to_visit:
      node = nodes_to_visit.popleft()
      if node in visited:
        continue
      visited.add(node)
      if not node.filename:
        continue
      module_path = os.path.realpath(node.filename)

      incoming_edges = graph.getReferers(node)
      message = 'Discovered %s (Imported by: %s)' % (
          node.filename, ', '.join(
              d.filename for d in incoming_edges
              if d is not None and d.filename is not None))
      logging.info(message)

      # This check is done after the logging/printing above to make sure that
      # we also print out the dependency edges that include python packages
      # that are not in chromium.
      if not path.IsSubpath(module_path, path_util.GetChromiumSrcDir()):
        continue

      # Exclude any dependencies which exist in the python installation.
      if any(path.IsSubpath(module_path, pfx) for pfx in prefixes):
        continue

      for outgoing_edge in graph.getReferences(node):
        nodes_to_visit.append(outgoing_edge)

      yield module_path
      if node.packagepath is not None:
        for p in node.packagepath:
          yield p

  finally:
    sys.path = sys_path


def _GetSourceNodes(graph):
  source_nodes = collections.deque()
  for node in graph.nodes():
    incoming_edges = list(graph.getReferers(node))
    if incoming_edges == [None]:
      source_nodes.append(node)
  return source_nodes


def FindExcludedFiles(files, options):
  # Define some filters for files.
  def IsHidden(path_string):
    for pathname_component in path_string.split(os.sep):
      if pathname_component.startswith('.'):
        return True
    return False

  def IsPyc(path_string):
    return os.path.splitext(path_string)[1] == '.pyc'

  def IsInCloudStorage(path_string):
    return os.path.exists(path_string + '.sha1')

  def MatchesExcludeOptions(path_string):
    for pattern in options.exclude:
      if (fnmatch.fnmatch(path_string, pattern) or
          fnmatch.fnmatch(os.path.basename(path_string), pattern)):
        return True
    return False

  # Collect filters we're going to use to exclude files.
  exclude_conditions = [
      IsHidden,
      IsPyc,
      IsInCloudStorage,
      MatchesExcludeOptions,
  ]

  # Check all the files against the filters.
  for file_path in files:
    if any(condition(file_path) for condition in exclude_conditions):
      yield file_path


def FindDependencies(target_paths, options):
  # Verify arguments.
  for target_path in target_paths:
    if not os.path.exists(target_path):
      raise ValueError('Path does not exist: %s' % target_path)

  dependencies = path_set.PathSet()

  # Including Telemetry's major entry points will (hopefully) include Telemetry
  # and all its dependencies. If the user doesn't pass any arguments, we just
  # have Telemetry.
  dependencies |= FindPythonDependencies(os.path.realpath(
      os.path.join(path_util.GetTelemetryDir(),
                   'telemetry', 'command_line', 'parser.py')))
  dependencies |= FindPythonDependencies(os.path.realpath(
      os.path.join(path_util.GetTelemetryDir(),
                   'telemetry', 'testing', 'run_tests.py')))

  # Add dependencies.
  for target_path in target_paths:
    base_dir = os.path.dirname(os.path.realpath(target_path))

    dependencies.add(base_dir)
    dependencies |= FindBootstrapDependencies(base_dir)
    dependencies |= FindPythonDependencies(target_path)

  # Remove excluded files.
  dependencies -= FindExcludedFiles(set(dependencies), options)

  return dependencies


def ZipDependencies(target_paths, dependencies, options):
  base_dir = os.path.dirname(os.path.realpath(path_util.GetChromiumSrcDir()))

  with zipfile.ZipFile(options.zip, 'w', zipfile.ZIP_DEFLATED) as zip_file:
    # Add dependencies to archive.
    for dependency_path in dependencies:
      path_in_archive = os.path.join(
          'telemetry', os.path.relpath(dependency_path, base_dir))
      zip_file.write(dependency_path, path_in_archive)

    # Add symlinks to executable paths, for ease of use.
    for target_path in target_paths:
      link_info = zipfile.ZipInfo(
          os.path.join('telemetry', os.path.basename(target_path)))
      link_info.create_system = 3  # Unix attributes.
      # 010 is regular file, 0111 is the permission bits rwxrwxrwx.
      link_info.external_attr = 0o0100777 << 16  # Octal.

      relative_path = os.path.relpath(target_path, base_dir)
      link_script = (
          '#!/usr/bin/env vpython\n\n'
          'import os\n'
          'import sys\n\n\n'
          'script = os.path.join(os.path.dirname(__file__), \'%s\')\n'
          'os.execv(sys.executable, [sys.executable, script] + sys.argv[1:])'
          % relative_path)

      zip_file.writestr(link_info, link_script)


class FindDependenciesCommand(command_line.Command):
  """Prints all dependencies"""

  @classmethod
  def AddCommandLineArgs(cls, parser):
    parser.add_argument('-v',
                        '--verbose',
                        action='count',
                        dest='verbosity',
                        default=0,
                        help='Increase verbosity level (repeat as needed).')

    parser.add_argument(
        '-e',
        '--exclude',
        action='append',
        default=[],
        help='Exclude paths matching EXCLUDE. Can be used multiple times.')

    parser.add_argument('-z',
                        '--zip',
                        help='Store files in a zip archive at ZIP.')

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    if args.verbosity >= 2:
      logging.getLogger().setLevel(logging.DEBUG)
    elif args.verbosity:
      logging.getLogger().setLevel(logging.INFO)
    else:
      logging.getLogger().setLevel(logging.WARNING)

  def Run(self, args):
    target_paths = args.positional_args
    dependencies = FindDependencies(target_paths, args)
    if args.zip:
      ZipDependencies(target_paths, dependencies, args)
      print('Zip archive written to %s.' % args.zip)
    else:
      print('\n'.join(sorted(dependencies)))
    return 0
