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
from typing import List, Dict, Set, Optional
import re
from pathlib import Path

_IMPORT_PATTERNS = [
    (re.compile(r"^import chromium_src\.tools\.metrics\.?(.*) as .*$"), False),
    (re.compile(r"^import chromium_src\.tools\.metrics\.?([^ ]*)$"), False),
    (re.compile(r"^from chromium_src\.tools\.metrics\.?(.*) import (.*)$"),
     True)
]


def _get_py_files_recursive(directory: str):
  """Recursively finds all .py files in a given directory."""
  py_files = []
  for root, _, files in os.walk(directory):
    for file in files:
      if file.endswith(".py"):
        full_path = os.path.join(root, file)
        py_files.append(full_path)
  return py_files


def _resolve_fs_path(path_str: str) -> List[str]:
  """Resolve path to either single .py file or all files in directory

  It checks if the path is a file (by appending .py) - if so, it returns a list
  containing fully constructed path to that file.

  If it's not a py file, it tries to resolve it as directory - returning paths
  of all files within that directory.

  If the directory under this path doesn't exist either - throws an exception.
  """
  file_candidate = path_str + ".py"
  if os.path.isfile(file_candidate):
    return [file_candidate]

  if os.path.isdir(path_str):
    return _get_py_files_recursive(path_str)

  raise FileNotFoundError(
      f"Could not resolve import. Neither '{file_candidate}' exists,"
      f"nor is '{path_str}' a directory.")


def _process_import_match(root_path: str,
                          path_group: str,
                          symbol: Optional[str] = None):
  """Constructs the final path from the parsed import statement.

  If a symbol is present, it tries to append it first.
  If that fails, it falls back to checking the path without the symbol.
  """
  full_path = os.path.join(root_path, path_group.replace('.', os.sep))

  if not symbol:
    return _resolve_fs_path(full_path)

  path_with_symbol = os.path.join(full_path, symbol)
  try:
    return _resolve_fs_path(path_with_symbol)
  except FileNotFoundError:
    # Fallback: Ignore the symbol and check if the module itself matches a file
    return _resolve_fs_path(full_path)


def _parse_line_dependencies(root_path: str, line: str,
                             pattern: re.Pattern) -> List[str]:
  """Returns dependencies for the line (if any) using pattern."""
  match = pattern.match(line)
  if not match:
    return []

  path_suffix = match.group(1)

  return [
      str(Path(p).relative_to(root_path))
      for p in _process_import_match(root_path, path_suffix, None)
  ]


def _parse_line_dependencies_with_symbol(root_path: str, line: str,
                                         pattern: re.Pattern) -> List[str]:
  """Returns dependencies for the line (if any) using pattern."""
  match = pattern.match(line)
  if not match:
    return []
  path_suffix = match.group(1)
  symbols_str = match.group(2).strip()
  symbols = [s.strip() for s in symbols_str.split(',')]\

  imports: List[str] = []
  for symbol in symbols:
    imports.extend(
        str(Path(p).relative_to(root_path))
        for p in _process_import_match(root_path, path_suffix, symbol))
  return imports


def _dependencies_of(root_path: str, relative_path: str) -> List[str]:
  """Returns a list of dependencies (as path relative to root_path) for a file

  The file is identified by relative_path within root_path.
  """
  full_path = os.path.join(root_path, relative_path)
  if not os.path.exists(full_path):
    raise FileNotFoundError(f"Input file not found: {full_path}")

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


def _as_path_relative_to(path, root, reporting_root) -> str:
  return os.path.relpath(os.path.join(root, path), reporting_root)


def scan_directory_dependencies(
    root_path: str,
    report_relative_to: Optional[str] = None) -> Dict[str, List[str]]:
  """Scans the directory for .py files and builds a dependency map.

  Returns:
    Dict[str, List[str]]: Keys are relative file paths, values
    are lists of dependencies as relative file paths.
  """
  reporting_root = report_relative_to if report_relative_to else root_path
  return {
      _as_path_relative_to(path, root_path, reporting_root): [
          _as_path_relative_to(path, root_path, reporting_root)
          for path in _dependencies_of(root_path, path)
      ]
      for path in _get_py_files_recursive(root_path)
  }


def print_dependency_graph(dependency_graph: Dict[str, List[str]]) -> None:
  """Prints a visual representation of direct dependencies for each file."""
  print(f"{'Script':<50} | {'Dependencies'}")
  print("-" * 80)

  for script_path in dependency_graph:
    deps = dependency_graph[script_path]

    if not deps:
      print(f"{script_path:<50} | (None)")
    else:
      print(f"{script_path:<50} | -> {deps[0]}")
      for dep in deps[1:]:
        print(f"{'':<50} | -> {dep}")
    print("-" * 80)


def get_all_dependencies(dependency_graph: Dict[str, List[str]],
                         target_script: str) -> Set[str]:
  """Returns a list of ALL dependencies (direct and indirect) for a script.

  Detects and handles circular dependencies safely.
  """
  if target_script not in dependency_graph:
    raise ValueError(
        f"Script '{target_script}' not found in the scanned graph.")

  all_dependencies = set()
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

  return set(list(all_dependencies))
