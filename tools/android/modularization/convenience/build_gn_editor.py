# Lint as: python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
r'''Helper code to handle editing BUILD.gn files.'''

from __future__ import annotations

import difflib
import pathlib
import re
import subprocess
from typing import List, Optional, Tuple


def _find_block(source: str, start: int, open_delim: str,
                close_delim: str) -> Tuple[int, int]:
  open_delim_pos = source[start:].find(open_delim)
  if open_delim_pos < 0:
    return (-1, -1)

  baseline = start + open_delim_pos
  delim_count = 1
  for i, char in enumerate(source[baseline + 1:]):
    if char == open_delim:
      delim_count += 1
      continue
    if char == close_delim:
      delim_count -= 1
      if delim_count == 0:
        return (baseline, baseline + i + 1)
  return (baseline, -1)


def _find_line_end(source: str, start: int) -> int:
  pos = source[start:].find('\n')
  if pos < 0:
    return -1
  return start + pos


class BuildFileUpdateError(Exception):
  """Represents an error updating the build file."""

  def __init__(self, message: str):
    super().__init__()
    self._message = message

  def __str__(self):
    return self._message


class VariableContentList(object):
  """Contains the elements of a list assigned to a variable in a gn target.

  Example:
  target_type("target_name") {
    foo = [
      "a",
      "b",
      "c",
    ]
  }

  This class represents the elements "a", "b", "c" for foo.
  """

  def __init__(self):
    self._elements = []

  def parse_from(self, content: str) -> bool:
    """Parses list elements from content and returns True on success.

    The expected list format must be a valid gn list. i.e.
    1. []
    2. [ "foo" ]
    3. [
         "foo",
         "bar",
         ...
       ]
    """
    start = content.find('[')
    if start < 0:
      return False

    end = start + content[start:].find(']')
    if end <= start:
      return False

    bracketless_content = content[start + 1:end].strip()
    if not bracketless_content:
      return True

    whitespace = re.compile(r'^\s+', re.MULTILINE)
    comma = re.compile(r',$', re.MULTILINE)
    self._elements = list(
        dict.fromkeys(
            re.sub(comma, '', re.sub(whitespace, '',
                                     bracketless_content)).split('\n')))
    return True

  def get_elements(self) -> List[str]:
    return self._elements

  def add_elements(self, elements: List[str]) -> None:
    """Appends unique elements to the existing list."""
    if not self._elements:
      self._elements = list(dict.fromkeys(elements))
      return

    all_elements = list(self._elements)
    all_elements.extend(elements)
    self._elements = list(dict.fromkeys(all_elements))

  def add_list(self, other: VariableContentList) -> None:
    """Appends unique elements to the existing list."""
    self.add_elements(other.get_elements())

  def serialize(self) -> str:
    if not self._elements:
      return '[]\n'
    return '[\n' + ',\n'.join(self._elements) + ',\n]'


class TargetVariable:
  """Contains the name of a variable and its contents in a gn target.

  Example:
  target_type("target_name") {
    variable_name = variable_content
  }

  This class represents the variable_name and variable_content.
  """

  def __init__(self, name: str, content: str):
    self._name = name
    self._content = content

  def get_name(self) -> str:
    return self._name

  def get_content(self) -> str:
    return self._content

  def get_content_as_list(self) -> Optional[VariableContentList]:
    """Returns the variable's content if it can be represented as a list."""
    content_list = VariableContentList()
    if content_list.parse_from(self._content):
      return content_list
    return None

  def is_list(self) -> bool:
    """Returns whether the variable's content is represented as a list."""
    return self.get_content_as_list() is not None

  def set_content_from_list(self, content_list: VariableContentList) -> None:
    self._content = content_list.serialize()

  def set_content(self, content: str) -> None:
    self._content = content

  def serialize(self) -> str:
    return f'\n{self._name} = {self._content}\n'


class BuildTarget:
  """Contains the target name, type and content of a gn target.

  Example:
  target_type("target_name") {
    <content>
  }

  This class represents target_type, target_name and arbitrary content.

  Specific variables are accessible via this class by name although only the
  basic 'foo = "bar"' and
  'foo = [
    "bar",
    "baz",
  ]'
  formats are supported, not more complex things like += or conditionals.
  """

  def __init__(self, target_type: str, target_name: str, content: str):
    self._target_type = target_type
    self._target_name = target_name
    self._content = content

  def get_name(self) -> str:
    return self._target_name

  def get_type(self) -> str:
    return self._target_type

  def get_variable(self, variable_name: str) -> Optional[TargetVariable]:
    pattern = re.compile(fr'^\s*{variable_name} = ', re.MULTILINE)
    match = pattern.search(self._content)
    if not match:
      return None

    start = match.end() - 1
    end = start
    if self._content[match.end()] == '[':
      start, end = _find_block(self._content, start, '[', ']')
    else:
      end = _find_line_end(self._content, start)

    if end <= start:
      return None

    return TargetVariable(variable_name, self._content[start:end + 1])

  def add_variable(self, variable: TargetVariable) -> None:
    """Adds the variable to the end of the content.

    Warning: this does not check for prior existence."""
    self._content += variable.serialize()

  def replace_variable(self, variable: TargetVariable) -> None:
    """Replaces an existing variable and returns True on success."""
    pattern = re.compile(fr'^\s*{variable.get_name()} =', re.MULTILINE)
    match = pattern.search(self._content)
    if not match:
      raise BuildFileUpdateError(
          f'{self._target_type}("{self._target_name}") variable '
          f'{variable.get_name()} not found. Unable to replace.')

    start = match.end()
    if variable.is_list():
      start, end = _find_block(self._content, start, '[', ']')
    else:
      end = _find_line_end(self._content, start)

    if end <= match.start():
      raise BuildFileUpdateError(
          f'{self._target_type}("{self._target_name}") variable '
          f'{variable.get_name()} invalid. Unable to replace.')

    self._content = (self._content[:match.start()] + variable.serialize() +
                     self._content[end + 1:])

  def serialize(self) -> str:
    return (f'\n{self._target_type}("{self._target_name}") {{\n' +
            f'{self._content}\n}}\n')


class BuildFile:
  """Represents the contents of a BUILD.gn file.

  This supports modifying or adding targets to the file at a basic level.
  """

  def __init__(self, build_gn_path: pathlib.Path):
    self._path = build_gn_path
    with open(self._path, 'r') as build_gn_file:
      self._content = build_gn_file.read()

  def get_target_names_of_type(self, target_type: str) -> List[str]:
    """Lists all targets in the build file of target_type."""
    pattern = re.compile(fr'^\s*{target_type}\(\"(\w+)\"\)', re.MULTILINE)
    return pattern.findall(self._content)

  def get_target(self, target_type: str,
                 target_name: str) -> Optional[BuildTarget]:
    pattern = re.compile(fr'^\s*{target_type}\(\"{target_name}\"\)',
                         re.MULTILINE)
    match = pattern.search(self._content)
    if not match:
      return None

    start, end = _find_block(self._content, match.end(), '{', '}')
    if end <= start:
      return None

    return BuildTarget(target_type, target_name, self._content[start + 1:end])

  def get_path(self) -> pathlib.Path:
    return self._path

  def get_content(self) -> str:
    return self._content

  def get_diff(self) -> str:
    with open(self._path, 'r') as build_gn_file:
      disk_content = build_gn_file.read()
    return ''.join(
        difflib.unified_diff(disk_content.splitlines(keepends=True),
                             self._content.splitlines(keepends=True),
                             fromfile=f'{self._path}',
                             tofile=f'{self._path}'))

  def add_target(self, target: BuildTarget) -> None:
    """Adds the target to the end of the content.

    Warning: this does not check for prior existence."""
    self._content += target.serialize()

  def replace_target(self, target: BuildTarget) -> None:
    """Replaces an existing target and returns True on success."""
    pattern = re.compile(fr'^\s*{target.get_type()}\(\"{target.get_name()}\"\)',
                         re.MULTILINE)
    match = pattern.search(self._content)
    if not match:
      raise BuildFileUpdateError(
          f'{target.get_type()}("{target.get_name()}") not found. '
          'Unable to replace.')

    start, end = _find_block(self._content, match.end(), '{', '}')
    if end <= start:
      raise BuildFileUpdateError(
          f'{target.get_type()}("{target.get_name()}") invalid. '
          'Unable to replace.')

    self._content = (self._content[:match.start()] + target.serialize() +
                     self._content[end + 1:])

  def format_content(self) -> None:
    process = subprocess.Popen(['gn', 'format', '--stdin'],
                               stdout=subprocess.PIPE,
                               stdin=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    stdout_data, stderr_data = process.communicate(input=self._content.encode())
    if process.returncode:
      raise BuildFileUpdateError(
          'Formatting failed. There was likely an error in the changes '
          '(this program cannot handle complex BUILD.gn files).\n'
          f'stderr: {stderr_data.decode()}')
    self._content = stdout_data.decode()

  def write_content_to_file(self) -> None:
    with open(self._path, 'w+') as build_gn_file:
      build_gn_file.write(self._content)
