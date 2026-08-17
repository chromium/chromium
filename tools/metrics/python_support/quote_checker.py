# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper to check Python files for quote consistency."""

import ast
from dataclasses import dataclass
import pathlib
from typing import List, Set


@dataclass
class ModifiedString:
  # The raw source of the string literal (including prefix and quotes)
  raw_source: str
  # Path to the source file
  file_path: pathlib.Path
  # The line numbers (1-indexed) spanned by this string literal
  lines: Set[int]


@dataclass
class _StringLiteralWithContext:
  literal: str
  line_range: tuple[int, int]


def GetExactSource(
  node: ast.AST,
  file_lines: List[str],
) -> _StringLiteralWithContext | None:
  """Retrieves the exact code source substring for the given AST node.

  Returns None if:
  - The node lacks required location attributes (e.g., dynamically generated
    or mocked AST nodes).

  Raises:
    ValueError: If the node coordinates index outside of the file's line
      boundaries.
  """
  # AST nodes representing expressions are guaranteed to have line numbers
  # when parsed from a source file by the Python compiler. However, nodes
  # created dynamically or manually in the AST (e.g., during AST mutations
  # or inside some third-party mock trees in tests) might lack line numbers.
  # We skip them to prevent crashes.
  lineno = getattr(node, 'lineno', None)
  end_lineno = getattr(node, 'end_lineno', None)
  col_offset = getattr(node, 'col_offset', None)
  end_col_offset = getattr(node, 'end_col_offset', None)
  if (
    lineno is None
    or end_lineno is None
    or col_offset is None
    or end_col_offset is None
  ):
    return None

  node_lines = file_lines[lineno - 1 : end_lineno]
  if not node_lines:
    raise ValueError(
      f'Node line range {lineno}-{end_lineno} is out of bounds for file '
      f'with {len(file_lines)} lines.'
    )

  # Slice the end first to handle single-line node slicing correctly
  node_lines[-1] = node_lines[-1][:end_col_offset]
  node_lines[0] = node_lines[0][col_offset:]
  literal = '\n'.join(node_lines)
  return _StringLiteralWithContext(
    literal=literal, line_range=(lineno, end_lineno)
  )


def IsQuoteConsistent(raw_source: str) -> bool:
  """Returns True if the string follows the quote consistency rules.

  Double quotes are allowed only when they contain single quotes (to avoid
  escaping), e.g. "don't". Triple double quotes (docstrings) are also allowed.
  """
  q_indices = [raw_source.find(q) for q in ("'", '"') if q in raw_source]
  if not q_indices:
    return True
  string_no_prefix = raw_source[min(q_indices) :]
  return ("'" in string_no_prefix) or string_no_prefix.startswith('"""')


def _GetStringConstantsWithinJoinedStr(tree: ast.AST) -> Set[int]:
  """Collects the object IDs of Constant nodes inside JoinedStr values.

  PEP 701 (Python 3.12+) parses f-string Constant components as separate
  Constant nodes inside JoinedStr values. We collect their object IDs
  so we can skip them and check the JoinedStr f-string as a single unit.
  """
  constants_to_skip = set()
  for node in ast.walk(tree):
    if not isinstance(node, ast.JoinedStr):
      continue
    for child in node.values:
      if isinstance(child, ast.Constant):
        constants_to_skip.add(id(child))
  return constants_to_skip


def GetModifiedStrings(
  filepath: pathlib.Path,
  file_text: str,
) -> List[ModifiedString]:
  """Parses a Python file and extracts string literals and f-strings."""
  tree = ast.parse(file_text)
  file_lines = file_text.splitlines()

  constants_to_skip = _GetStringConstantsWithinJoinedStr(tree)

  modified_strings = []
  for node in ast.walk(tree):
    if id(node) in constants_to_skip:
      continue

    if not isinstance(node, (ast.Constant, ast.JoinedStr)):
      continue

    # Skip non-string/non-bytes constants (like numbers, True/False/None)
    if isinstance(node, ast.Constant) and not isinstance(
      node.value, (str, bytes)
    ):
      continue

    literal_with_context = GetExactSource(node, file_lines)
    if literal_with_context is None:
      continue

    modified_strings.append(
      ModifiedString(
        raw_source=literal_with_context.literal,
        file_path=filepath,
        lines=set(
          range(
            literal_with_context.line_range[0],
            literal_with_context.line_range[1] + 1,
          )
        ),
      )
    )

  return modified_strings


def CheckQuoteConsistency(
  source_string: ModifiedString,
  changed_lines: Set[int],
) -> bool:
  """Returns True if the source_string is a valid string or wasn't modified."""
  string_modified = changed_lines.intersection(source_string.lines)
  if not string_modified:
    return True
  return IsQuoteConsistent(source_string.raw_source)
