# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A library allowing to solve dependencies within the tools/metrics
# setup. It only detects internal dependencies imported through
#
# ```
# import chromium_src.tools.metrics...
# ```
#
# or similar import.
#
# It's used to make sure that all affected files are tested when
# running presubmits.

import os
import pathlib
import re
from typing import Any, Dict, List, Optional, Set

_IMPORT_PATTERNS = [
    (re.compile(r'^import chromium_src\.tools\.metrics\.?(.*) as .*$'), False),
    (re.compile(r'^import chromium_src\.tools\.metrics\.?([^ ]*)$'), False),
    (re.compile(r'^from chromium_src\.tools\.metrics\.?(.*) import (.*)$'),
     True)
]


def _get_py_files_recursive(directory: pathlib.Path) -> List[pathlib.Path]:
  """Recursively finds all .py files in a given directory."""
  return list(directory.rglob('*.py'))


def _resolve_fs_path(path: pathlib.Path) -> List[pathlib.Path]:
  """Resolves a path to either a single .py file or all files in a directory.

  It checks if the path is a file (by appending .py) - if so, it returns a list
  containing the Path to that file.

  If it's not a py file, it tries to resolve it as a directory - returning paths
  of all files within that directory recursively.

  If neither exists, throws FileNotFoundError.
  """
  file_candidate = pathlib.Path(str(path) + '.py')
  if file_candidate.is_file():
    return [file_candidate]

  if path.is_dir():
    return _get_py_files_recursive(path)

  raise FileNotFoundError(
      f'Could not resolve import. Neither \'{file_candidate}\' exists, '
      f'nor is \'{path}\' a directory.')


def _process_import_match(root_path: pathlib.Path,
                          path_group: str,
                          symbol: Optional[str] = None) -> List[pathlib.Path]:
  """Constructs the final Path from the parsed import statement.

  If a symbol is present, it tries to append it first as a file/sub-module.
  If that fails, it falls back to checking the path without the symbol.
  """
  full_path = root_path / path_group.replace('.', os.sep)

  if not symbol:
    return _resolve_fs_path(full_path)

  path_with_symbol = full_path / symbol
  try:
    return _resolve_fs_path(path_with_symbol)
  except FileNotFoundError:
    return _resolve_fs_path(full_path)


def _parse_line_dependencies(root_path: pathlib.Path, line: str,
                             pattern: re.Pattern) -> List[pathlib.Path]:
  """Returns dependencies for the line (if any) using pattern."""
  match = pattern.match(line)
  if not match:
    return []

  path_suffix = match.group(1)

  return [
      p.relative_to(root_path)
      for p in _process_import_match(root_path, path_suffix, None)
  ]


def _parse_line_dependencies_with_symbol(
    root_path: pathlib.Path, line: str,
    pattern: re.Pattern) -> List[pathlib.Path]:
  """Returns dependencies for the line (if any) using pattern."""
  match = pattern.match(line)
  if not match:
    return []
  path_suffix = match.group(1)
  symbols_str = match.group(2).strip()
  symbols = [s.strip() for s in symbols_str.split(',')]

  imports: List[pathlib.Path] = []
  for symbol in symbols:
    imports.extend(
        p.relative_to(root_path)
        for p in _process_import_match(root_path, path_suffix, symbol))
  return imports


def _dependencies_of(root_path: pathlib.Path,
                     relative_path: pathlib.Path) -> List[pathlib.Path]:
  """Returns a list of dependencies (as Paths relative to root_path) for a file.

  The file is identified by relative_path within root_path.
  """
  full_path = root_path / relative_path
  if not full_path.exists():
    raise FileNotFoundError(f'Input file not found: {full_path}')

  with open(full_path, 'r', encoding='utf-8') as f:
    lines = f.readlines()
  dependencies = []

  for line in lines:
    line = line.strip()
    for pattern, has_symbol in _IMPORT_PATTERNS:
      if has_symbol:
        dependencies.extend(
            _parse_line_dependencies_with_symbol(root_path, line, pattern))
      else:
        dependencies.extend(_parse_line_dependencies(root_path, line, pattern))

  return dependencies


def scan_directory_dependencies(
    root_path: pathlib.Path,
    report_relative_to: Optional[pathlib.Path] = None
) -> Dict[pathlib.Path, List[pathlib.Path]]:
  """Scans the directory for .py files and builds a dependency map.

  Returns:
    Dict[pathlib.Path, List[pathlib.Path]]: Keys are relative file Paths,
    values are lists of dependencies as relative file Paths.
  """
  root_path = pathlib.Path(root_path)
  reporting_root = pathlib.Path(
      report_relative_to) if report_relative_to else root_path
  rel_files = [
      p.relative_to(root_path) for p in _get_py_files_recursive(root_path)
  ]
  return {
      (root_path / path).relative_to(reporting_root):
      [(root_path / dep_path).relative_to(reporting_root)
       for dep_path in _dependencies_of(root_path, path)]
      for path in rel_files
  }


def print_dependency_graph(
    dependency_graph: Dict[pathlib.Path, List[pathlib.Path]]) -> None:
  """Prints a visual representation of direct dependencies for each file."""
  header_script = 'Script'
  print(f'{header_script:<50} | Dependencies')
  print('-' * 80)

  empty_prefix = ''
  for script_path in dependency_graph:
    deps = dependency_graph[script_path]

    if not deps:
      print(f'{script_path:<50} | (None)')
    else:
      print(f'{script_path:<50} | -> {deps[0]}')
      for dep in deps[1:]:
        print(f'{empty_prefix:<50} | -> {dep}')
    print('-' * 80)


def get_all_dependencies(dependency_graph: Dict[pathlib.Path,
                                                List[pathlib.Path]],
                         target_script: pathlib.Path) -> Set[pathlib.Path]:
  """Returns a set of ALL dependencies (direct and indirect) for a script.

  Detects and handles circular dependencies safely.
  """
  if not dependency_graph:
    return set()

  if target_script not in dependency_graph:
    raise ValueError(
        f'Script \'{target_script}\' not found in the scanned graph.')

  all_dependencies: Set[pathlib.Path] = set()
  queue = [target_script]
  visited = set()

  while queue:
    current_script = queue.pop(0)

    if current_script in visited:
      continue
    visited.add(current_script)
    direct_deps = dependency_graph.get(current_script, [])

    for dep in direct_deps:
      all_dependencies.add(dep)
      queue.append(dep)

  return all_dependencies
