#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Prints all of the Chrome tracing stdlib as an amalgamation (a single
# PerfettoSQL source which includes all transitive dependencies).
#
# All "CREATE PERFETTO" instructions are also going to be replaced with
# "CREATE OR REPLACE PERFETTO" instructions to ensure that the amalgamation
# can be used on top of existing stdlib.
#
# Usage: print_stdlib_amalgamation.py [chrome.module1 chrome.module2 ...]

from dataclasses import dataclass
from typing import Collection, Dict, List, Set
from pathlib import Path
import os
import re
import sys

CHROME_STDLIB_DIR = Path(
    os.path.dirname(__file__)) / '../../base/tracing/stdlib'


def ReadStdlib(path: Path) -> Dict[str, str]:
  """Reads all of the SQL modules in the given directory.

  Args:
    path: The path to the directory containing the SQL modules.

  Returns:
    A dictionary mapping module names to their contents.
  """
  result = {}
  for root, _, files in os.walk(path):
    for file in files:
      if not file.endswith('.sql'):
        continue
      with open(os.path.join(root, file)) as f:
        module = os.path.join(os.path.relpath(root, path),
                              file.removesuffix('.sql')).replace(
                                  os.path.sep, '.')
        result[module] = f.read()
  return result


@dataclass
class Module:
  """A class representing a parsed SQL module."""
  name: str
  includes: List[str]
  content: str


def ParseModule(name: str, content: str,
                known_stdlib_modules: Collection[str]) -> Module:
  """
  Parses a given SQL modules, resolving includes and updating
  CREATE PERFETTO ... statements to CREATE OR REPLACE.

  Args:
    name: name of the module.
    content: raw contents of the module.
    known_stdlib_modules: a list of known stdlib modules.

  Returns:
    A parsed Module object.
  """
  includes = []

  def ReplaceInclude(match):
    included_module = match.group(1)
    if included_module in known_stdlib_modules:
      includes.append(included_module)
      return ''
    return match.group(0)

  content = re.sub(r'INCLUDE PERFETTO MODULE ([\w\.]*);', ReplaceInclude,
                   content)
  content = content.replace('CREATE PERFETTO', 'CREATE OR REPLACE PERFETTO')
  # If the module doesn't end with a semicolon, add one to terminate the statement.
  if not content.strip().endswith(';'):
    content += ';'
  return Module(name, includes, content)


def ParseModules(stdlib: Dict[str, str]) -> List[Module]:
  """
  Parses the given SQL modules, resolving includes and updating
  CREATE PERFETTO ... statements to CREATE OR REPLACE.

  Args:
    stdlib: A dictionary mapping module names to their contents.

  Returns:
    A list of Module objects, each representing a parsed module.
  """
  return [
      ParseModule(name, content, stdlib.keys())
      for name, content in stdlib.items()
  ]


def SortModules(stdlib: List[Module],
                targets: List[str] | None = None) -> List[Module]:
  """
  Topologically sorts the given modules based on their includes.
  Assumes that there are no circular includes -- otherwise the behaviour is undefined.

  Args:
    stdlib: A list of Module objects.
    targets: A list of module names to sort. If None, all modules will be sorted.

  Returns:
    A list of Module objects, sorted in topological order.
  """
  modules_by_name = {m.name: m for m in stdlib}
  result = []
  visited = set()

  def AddModule(module: Module):
    if module.name in visited:
      return
    visited.add(module.name)
    for include in module.includes:
      AddModule(modules_by_name[include])
    result.append(module)

  if targets is None:
    targets = stdlib.keys()
  for target in targets:
    AddModule(modules_by_name[target])
  return result


def PrintModules(modules: List[Module]):
  for m in modules:
    print()
    print()
    print('--')
    print('--', m.name)
    print('--')
    print()
    print()
    print(m.content)


if __name__ == '__main__':
  chrome_stdlib = ReadStdlib(CHROME_STDLIB_DIR)
  PrintModules(SortModules(ParseModules(chrome_stdlib), sys.argv[1:] or None))
