# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A simple C++ preprocessor."""

import ast
from collections.abc import Iterable
import dataclasses
import re

_COMMENT_OR_STRING_RE = re.compile(
  '|'.join(
    [
      r'//.*?$',
      r'/\*.*?\*/',
      r'R"([^\s\\()]{0,16})\((?:(?!\)\1").)*\)\1"',
      r"'(?:\\.|[^\\'])*'",
      r'"(?:\\.|[^\\"])*"',
    ]
  ),
  flags=re.DOTALL | re.MULTILINE,
)

# A single token of a C++ boolean condition such as
# `!BUILDFLAG(IS_IOS) && (IS_ANDROID || defined(OFFICIAL_BUILD))`.
_CONDITION_TOKEN_RE = re.compile(
  r'\s*(?:(?:BUILDFLAG|defined)\s*\(\s*(?P<flag>\w+)\s*\)'
  r'|defined\s+(?P<defined_flag>\w+)'
  r'|(?P<ident>\w+)'
  r'|(?P<op>&&|\|\||!|\(|\)))'
)
_CONDITION_OPERATORS = {'&&': 'and', '||': 'or', '!': 'not', '(': '(', ')': ')'}
_CONDITION_LITERALS = {'0': 'False', '1': 'True'}


def remove_comments(code: str, preserve_newlines: bool = False) -> str:
  """Removes comments from the given C++ code.

  Args:
    code: The C++ source code to strip comments from.
    preserve_newlines: Whether to replace comments with equivalent newlines.
      Callers validating line numbers (e.g. error reporters) may need to
      preserve 1-to-1 line correspondence, whereas preprocessors evaluating
      multi-line `#if` directives require complete comment removal so multi-line
      block comments inside a directive do not split the directive across lines.

  Returns:
    The C++ code with comments removed.
  """

  def _replacer(match: re.Match[str]) -> str:
    s = match.group(0)
    # Comments start with '/'. String literals are also matched in the regex so
    # that any '//' or '/*' sequences inside string literals are matched as
    # complete string tokens and returned unchanged.
    if s.startswith('/'):
      if preserve_newlines:
        return '\n' * s.count('\n')
      return ''
    return s

  return _COMMENT_OR_STRING_RE.sub(_replacer, code)


# Alias for Chromium style compatibility.
RemoveComments = remove_comments  # pylint: disable=invalid-name


@dataclasses.dataclass
class _PreprocessorState:
  """Represents the state of the preprocessor for a given #if block.

  Attributes:
    active: Whether the current block is active.
    met: Whether a condition has been met in the current block.
  """

  active: bool
  met: bool


class CppPreprocessor:
  """A simple C++ preprocessor.

  Handles #if, #ifdef, #ifndef, #else, #elif, #elifdef, #elifndef,
  and #endif directives.
  """

  def __init__(self, defines: Iterable[str], known_symbols: Iterable[str] = ()):
    """Initializes the preprocessor with a set of active preprocessor defines.

    Args:
      defines: A set of strings representing the active preprocessor defines.
      known_symbols: A set of strings representing known preprocessor symbols.
        Any other symbol encountered during preprocessing will be considered
        unknown and will evaluate to True.
    """
    self._defines = set(defines)
    self._known_symbols = set(known_symbols)

  def _extract_unknown_symbols(self, code: str) -> set[str]:
    """Extracts preprocessor symbols from the given C++ code."""
    symbols = set()
    # BUILDFLAG(SYMBOL)
    symbols.update(re.findall(r'BUILDFLAG\s*\((\w+)\)', code))
    # #ifdef SYMBOL, #ifndef SYMBOL, #elifdef SYMBOL, #elifndef SYMBOL
    for match in re.finditer(
      r'#(?:ifdef|ifndef|elifdef|elifndef)\s+(.*)', code
    ):
      symbols.add(self._extract_flag(match.group(1).strip()))
    # #if or #elif conditions
    for match in re.finditer(r'#(?:if|elif)\s+(.*)', code):
      expression = match.group(1)
      # Remove BUILDFLAG(...) content from expression because
      # it is already covered by the first pattern.
      expression = re.sub(r'BUILDFLAG\s*\(\w+\)', '', expression)
      tokens = re.findall(r'\(|\)|\w+|&&|\|\||!', expression)
      for token in tokens:
        if token.isidentifier() and token not in ('True', 'False', 'defined'):
          symbols.add(token)
    return symbols - self._known_symbols - self._defines

  def preprocess(self, code: str) -> str:
    """Preprocesses the given C++ code.

    Args:
      code: A string containing the C++ code.

    Returns:
      The preprocessed code with the inactive blocks removed.
    """
    # Handle `\` line continuations.
    code = code.replace('\\\n', '')
    code = remove_comments(code)
    unknown_symbols = self._extract_unknown_symbols(code)
    all_true_symbols = self._defines | unknown_symbols
    lines = code.splitlines()
    processed_lines = []
    if_stack: list[_PreprocessorState] = []

    def _push_if_state(condition: bool):
      """Pushes a new state onto the if_stack."""
      # A block is active if its parent block is active and the condition
      # is true.
      # The parent is active if it's at the top level (if_stack is empty)
      # or if the enclosing #if block was itself active.
      parent_active = not if_stack or if_stack[-1].active
      if_stack.append(
        _PreprocessorState(active=parent_active and condition, met=condition)
      )

    for line in lines:
      stripped_line = line.strip()

      if not stripped_line:
        continue

      if re.fullmatch(r'#if\s+.*', stripped_line):
        condition_str = stripped_line.removeprefix('#if').strip()
        _push_if_state(
          self._evaluate_condition(condition_str, all_true_symbols)
        )
      elif re.fullmatch(r'#ifdef\s+.*', stripped_line):
        condition_str = stripped_line.removeprefix('#ifdef').strip()
        _push_if_state(self._extract_flag(condition_str) in all_true_symbols)
      elif re.fullmatch(r'#ifndef\s+.*', stripped_line):
        condition_str = stripped_line.removeprefix('#ifndef').strip()
        _push_if_state(
          self._extract_flag(condition_str) not in all_true_symbols
        )
      elif re.fullmatch(r'#elifdef\s+.*', stripped_line):
        if not if_stack:
          raise ValueError('Unexpected #elifdef')

        if if_stack[-1].met:
          if_stack[-1].active = False
        else:
          condition_str = stripped_line.removeprefix('#elifdef').strip()
          condition = self._extract_flag(condition_str) in all_true_symbols
          # The parent is active if the grand parent block was active.
          parent_active = len(if_stack) == 1 or if_stack[-2].active
          if_stack[-1].active = parent_active and condition
          if_stack[-1].met = condition
      elif re.fullmatch(r'#elifndef\s+.*', stripped_line):
        if not if_stack:
          raise ValueError('Unexpected #elifndef')

        if if_stack[-1].met:
          if_stack[-1].active = False
        else:
          condition_str = stripped_line.removeprefix('#elifndef').strip()
          condition = self._extract_flag(condition_str) not in all_true_symbols
          # The parent is active if the grand parent block was active.
          parent_active = len(if_stack) == 1 or if_stack[-2].active
          if_stack[-1].active = parent_active and condition
          if_stack[-1].met = condition
      elif re.fullmatch(r'#elif\s+.*', stripped_line):
        if not if_stack:
          raise ValueError('Unexpected #elif')

        if if_stack[-1].met:
          if_stack[-1].active = False
        else:
          condition_str = stripped_line.removeprefix('#elif').strip()
          condition = self._evaluate_condition(condition_str, all_true_symbols)
          # The parent is active if the grand parent block was active.
          parent_active = len(if_stack) == 1 or if_stack[-2].active
          if_stack[-1].active = parent_active and condition
          if_stack[-1].met = condition

      elif re.fullmatch(r'#else', stripped_line):
        if not if_stack:
          raise ValueError('Unexpected #else')

        if if_stack[-1].met:
          if_stack[-1].active = False
        else:
          # The parent is active if the grand parent block was active.
          parent_active = len(if_stack) == 1 or if_stack[-2].active
          if_stack[-1].active = parent_active
          if_stack[-1].met = True

      elif re.fullmatch(r'#endif', stripped_line):
        if not if_stack:
          raise ValueError('Unexpected #endif')
        if_stack.pop()

      else:
        if not if_stack or if_stack[-1].active:
          processed_lines.append(line)

    if if_stack:
      raise ValueError('Unterminated #if block')

    return '\n'.join(processed_lines)

  def _extract_flag(self, condition_str: str) -> str:
    """Extracts the flag name from a condition string."""
    match = re.search(r'BUILDFLAG\s*\((\w+)\)', condition_str)
    if match:
      return match.group(1)
    return condition_str

  def _evaluate_condition(
    self, expression: str, all_true_symbols: set[str]
  ) -> bool:
    """Evaluates a preprocessor condition string."""
    result = evaluate_condition(expression, all_true_symbols)
    return bool(result)


def evaluate_condition(
  expression: str,
  true_symbols: Iterable[str],
  known_symbols: Iterable[str] | None = None,
) -> bool | None:
  """Evaluates a C++ boolean condition expression.

  Translates the C++ condition expression into an equivalent Python boolean
  expression (mapping '&&'/'||'/'!' to 'and'/'or'/'not', '0'/'1' to
  'False'/'True', and identifiers, BUILDFLAG(X), defined(X) and defined X to
  their truth values).
  Parsing with ast.parse allows delegating operator precedence, short-
  circuiting structure, and nested parentheses to Python's built-in parser,
  after which _eval_boolean_ast recursively walks the AST safely without
  using arbitrary code evaluation (eval()).

  Args:
    expression: The C++ condition expression string.
    true_symbols: Symbols that evaluate to True.
    known_symbols: Optional set of all allowed symbols. When provided, any
      symbol not in `known_symbols` causes the function to return None instead
      of evaluating it to False.

  Returns:
    The boolean result of evaluating the condition, or None if the expression
    contains unsupported syntax or unknown symbols.
  """
  true_set = set(true_symbols)
  known_set = set(known_symbols) if known_symbols is not None else None
  expression = expression.strip()
  py_tokens = []
  pos = 0
  while pos < len(expression):
    match = _CONDITION_TOKEN_RE.match(expression, pos)
    if not match:
      return None
    pos = match.end()
    flag = match.group('flag') or match.group('defined_flag')
    if symbol := flag or match.group('ident'):
      if not flag and symbol in _CONDITION_LITERALS:
        py_tokens.append(_CONDITION_LITERALS[symbol])
      elif known_set is not None and symbol not in known_set:
        return None
      else:
        py_tokens.append(str(symbol in true_set))
    else:
      py_tokens.append(_CONDITION_OPERATORS[match.group('op')])

  try:
    return _eval_boolean_ast(ast.parse(' '.join(py_tokens), mode='eval').body)
  except (ValueError, SyntaxError):
    return None


# Alias for Chromium style compatibility.
EvaluateCondition = evaluate_condition  # pylint: disable=invalid-name


def _eval_boolean_ast(node: ast.AST) -> bool:
  """Evaluates an AST node containing only True, False, and, or, not.

  Safely evaluates boolean AST expressions generated from translated C++
  preprocessor conditions without executing arbitrary Python code. Only
  boolean constants, unary 'not', and boolean operations ('and', 'or') are
  supported. Any unsupported node (e.g. non-boolean constants or function calls)
  raises ValueError.

  Args:
    node: An ast.AST node representing the boolean expression.

  Returns:
    The boolean result of evaluating the node.

  Raises:
    ValueError: If an unsupported node type or non-boolean constant is
      encountered.
  """
  if isinstance(node, ast.Constant) and isinstance(node.value, bool):
    return node.value
  if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.Not):
    return not _eval_boolean_ast(node.operand)
  if isinstance(node, ast.BoolOp):
    if isinstance(node.op, ast.And):
      return all(_eval_boolean_ast(v) for v in node.values)
    if isinstance(node.op, ast.Or):
      return any(_eval_boolean_ast(v) for v in node.values)
  raise ValueError(f'Unsupported boolean AST node: {type(node).__name__}')
