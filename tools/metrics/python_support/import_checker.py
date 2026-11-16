# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper to find imports of classes and functions in Python files.

The Google Python style guide asks to import modules rather than the classes
and functions in them, so that every use shows which module a name comes from:
https://google.github.io/styleguide/pyguide.html#22-imports
"""

import ast
import collections.abc
import dataclasses
import importlib.util
import pathlib

# Modules whose classes and functions can be imported directly: typing helpers,
# as the style guide allows, and the parameterized test decorator, whose
# documented usage is `from parameterized import parameterized`.
_EXEMPT_MODULES = frozenset(
  {
    '__future__',
    'collections.abc',
    'parameterized',
    'typing',
    'typing_extensions',
  }
)


@dataclasses.dataclass(frozen=True)
class SymbolImport:
  # The line (1-indexed) of the imported name
  line: int
  # The module that the name is imported from
  module: str
  # The imported name, which isn't a module
  name: str


def FindSymbolImports(
  file_text: str,
  changed_lines: set[int],
  src_path: pathlib.Path,
) -> list[SymbolImport]:
  """Returns the names on changed lines that are imported but aren't modules.

  Only `from X import Y` imports are checked. A name is reported only if X can
  be found: chromium_src modules in src_path, other modules in the current
  Python environment. That way a third-party package that isn't installed here
  can't cause false reports.

  Raises:
    SyntaxError: If file_text isn't valid Python.
  """
  return [
    SymbolImport(line=alias.lineno, module=module, name=alias.name)
    for module, alias in _AbsoluteFromImports(ast.parse(file_text))
    if alias.lineno in changed_lines
    and module not in _EXEMPT_MODULES
    and _ModuleExists(module, src_path)
    and not _ModuleExists(f'{module}.{alias.name}', src_path)
  ]


def _AbsoluteFromImports(
  tree: ast.Module,
) -> collections.abc.Iterator[tuple[str, ast.alias]]:
  """Yields the module and each imported name of absolute `from` imports."""
  for node in ast.walk(tree):
    # Relative imports aren't checked: they don't name a module to look up.
    if isinstance(node, ast.ImportFrom) and node.module and not node.level:
      yield from ((node.module, alias) for alias in node.names)


def _ModuleExists(name: str, src_path: pathlib.Path) -> bool:
  """Returns whether the dotted name is a module or a package.

  chromium_src names are looked up in src_path, which setup_modules.py maps
  them to. Other names are looked up in the current Python environment.
  """
  package, _, submodules = name.partition('.')
  if package == 'chromium_src':
    path = src_path.joinpath(*submodules.split('.'))
    return path.is_dir() or path.with_suffix('.py').is_file()
  try:
    return importlib.util.find_spec(name) is not None
  except ImportError:
    # Raised when a parent of the name isn't a package, e.g. for pathlib.Path.
    return False
