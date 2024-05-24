# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code specific to disabling GTest tests."""

import os
import re
import subprocess
import sys
from typing import List, Optional, Tuple, Union, Any, Dict, TypeVar

import conditions
from conditions import Condition
import collections
import errors

A = TypeVar('A')
B = TypeVar('B')

CHROMIUM_SRC = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", ".."))
CLANG_FORMAT = os.path.join(CHROMIUM_SRC, 'third_party', 'depot_tools',
                            'clang_format.py')

# The full set of macros which we consider when looking for test definitions.
# Any such macro wrapping a GTest macro can be added to this list, so we can
# detect it for disabling tests. This list is likely incomplete.
TEST_MACROS = {
    'TEST',
    'TEST_F',
    'TYPED_TEST',
    'IN_PROC_BROWSER_TEST',
    'IN_PROC_BROWSER_TEST_F',
}


def disabler(full_test_name: str, source_file: str, new_cond: Condition,
             message: Optional[str]) -> str:
  """Disable a GTest test within the given file.

  Args:
    test_name: The name of the test, in the form TestSuite.TestName
    lines: The existing file, split into lines. Note that each line ends with a
      newline character.
    new_cond: The additional conditions under which to disable the test. These
      will be merged with any existing conditions.

  Returns:
    The new contents to write into the file, with the test disabled.
  """

  lines = source_file.split('\n')

  test_name = full_test_name.split('.')[1]

  disabled = 'DISABLED_' + test_name
  maybe = 'MAYBE_' + test_name
  current_name = None
  src_range = None

  # Search backwards from the end of the file, looking for the given test.
  # TODO: We should do more to avoid false positives. These could be caused by
  # the test name being mentioned in a comment or string, or test names that are
  # substrings of the given test name.
  for i in range(len(lines) - 1, -1, -1):
    line = lines[i]

    idents = find_identifiers(line)

    if maybe in idents:
      # If the test is already conditionally disabled, search backwards to find
      # the preprocessor conditional block that disables it, and parse out the
      # conditions.
      existing_cond, src_range = find_conditions(lines, i, test_name)
      current_name = maybe
      break
    if disabled in idents:
      existing_cond = conditions.ALWAYS
      current_name = disabled
      break
    if test_name in idents:
      existing_cond = conditions.NEVER
      current_name = test_name
      break
  else:
    raise Exception(f"Couldn't find test definition for {full_test_name}")

  test_name_index = i

  merged = conditions.merge(existing_cond, new_cond)

  comment = None
  if message:
    comment = f'// {message}'

  # Keep track of the line numbers of the lines which have been modified. These
  # line numbers will be fed to clang-format to ensure any modified lines are
  # correctly formatted.
  modified_lines = []

  # Ensure that we update modified_lines upon inserting new lines into the file,
  # as any lines after the insertion point will be shifted over.
  def insert_lines(start_index, end_index, new_lines):
    nonlocal lines
    nonlocal modified_lines

    prev_len = len(lines)
    lines[start_index:end_index] = new_lines

    len_diff = len(lines) - prev_len
    i = 0
    while i < len(modified_lines):
      line_no = modified_lines[i]
      if line_no >= start_index:
        if line_no < end_index:
          # Any existing lines with indices in [start_index, end_index) have
          # been removed, so remove them from modified_lines too.
          modified_lines.pop(i)
          continue

        modified_lines[i] += len_diff
      i += 1

    modified_lines += list(range(start_index, start_index + len(new_lines)))

  def insert_line(index, new_line):
    insert_lines(index, index, [new_line])

  def replace_line(index, new_line):
    insert_lines(index, index + 1, [new_line])

  def delete_lines(start_index, end_index):
    insert_lines(start_index, end_index, [])

  # If it's not conditionally disabled, we don't need a pre-processor block to
  # conditionally define the name. We just change the name within the test macro
  # to its appropriate value, and delete any existing preprocessor block.
  if isinstance(merged, conditions.BaseCondition):
    if merged == conditions.ALWAYS:
      replacement_name = disabled
    elif merged == conditions.NEVER:
      replacement_name = test_name

    replace_line(test_name_index,
                 lines[test_name_index].replace(current_name, replacement_name))

    if src_range:
      delete_lines(src_range[0], src_range[1] + 1)

    if comment:
      insert_line(test_name_index, comment)

    return clang_format('\n'.join(lines), modified_lines)

  # => now conditionally disabled
  replace_line(test_name_index,
               lines[test_name_index].replace(current_name, maybe))

  condition_impl = cc_format_condition(merged)

  condition_block = [
      f'#if {condition_impl}',
      f'#define {maybe} {disabled}',
      '#else',
      f'#define {maybe} {test_name}',
      '#endif',
  ]

  if src_range:
    # Replace the existing condition.
    insert_lines(src_range[0], src_range[1] + 1, condition_block)
    comment_index = src_range[0]
  else:
    # No existing condition, so find where to add a new one.
    for i in range(test_name_index, -1, -1):
      if any(test_macro in lines[i] for test_macro in TEST_MACROS):
        break
    else:
      raise Exception("Couldn't find where to insert test conditions")

    insert_lines(i, i, condition_block)
    comment_index = i

  if comment:
    insert_line(comment_index, comment)

  # Insert includes.
  # First find the set of headers we need for the given condition.
  necessary_includes = {
      include
      for var in conditions.find_terminals(merged)
      if (include := var.gtest_info.header) is not None
  }

  # Then scan through the existing set of headers, finding which are already
  # included, and where to insert the #includes for those that aren't.
  to_insert: Dict[str, int] = {}
  last_include = None
  if len(necessary_includes) > 0:
    # Track the path of the previous include, so we can find where to insert it
    # alphabetically.
    prev_path = ''
    i = 0
    while i < len(lines):
      match = get_directive(lines, i)
      i += 1
      if match is None:
        continue

      name, args = match
      if name != 'include':
        continue

      last_include = i

      # Strip both forms of delimiter around the include path.
      path = args[0].strip('<>"')
      # If this include path exactly matches one we need, then it's already
      # included and we don't need to add it. We remove it from both
      # necessary_includes, to not consider it, and from to_insert, in case we
      # already found a place to insert it.
      try:
        necessary_includes.remove(path)
      except KeyError:
        pass
      to_insert.pop(path, None)

      for include in necessary_includes:
        # Optimistically assume the includes are in sorted order, and try to
        # find a spot where we can insert each one.
        if prev_path < include < path:
          to_insert[include] = i
          necessary_includes.remove(include)
          prev_path = include
          i -= 1
          break

  if last_include is None:
    # This should never really happen outside of our tests, as the file will
    # need to at least include the GTest headers.
    last_include = 0

  # Deal with any includes that we couldn't find a spot for by putting them at
  # the end of the list of includes.
  for include in necessary_includes:
    assert last_include is not None
    to_insert[include] = last_include

  # Finally, insert all the includes in the positions we decided on. Do so from
  # higher to lower indices, so we don't need to adjust later positions to
  # account for previously-inserted lines.
  for path, i in sorted(to_insert.items(), key=lambda x: x[1], reverse=True):
    insert_line(i, f'#include "{path}"')

  return clang_format('\n'.join(lines), modified_lines)


def find_identifiers(line: str) -> List[str]:
  # Strip C++-style comments.
  line = re.sub('//.*$', '', line)

  # Strip strings.
  line = re.sub(r'"[^"]*[^\\]"', '', line)

  # Remainder is identifiers.
  # There are probably many corner cases this doesn't handle. We accept this
  # trade-off for simplicity of implementation, and because occurrences of the
  # test name in such corner case contexts are likely very rare.
  return re.findall('[a-zA-Z_][a-zA-Z_0-9]*', line)


def find_conditions(lines: List[str], start_line: int, test_name: str):
  """Starting from a given line, find the conditions relating to this test.

  We step backwards until we find a preprocessor conditional block which defines
  the MAYBE_Foo macro for this test. The logic is fairly rigid - there are many
  ways in which test disabling could be expressed that we don't handle. We rely
  on the fact that there is a common convention that people stick to very
  consistently.

  The format we recognise looks like:

  #if <some preprocessor condition>
  #define MAYBE_TEST DISABLED_Test
  #else
  #define MAYBE_Test Test
  #endif

  We also allow for the branches to be swapped, i.e. for the false branch to
  define the disabled case. We don't handle anything else (e.g. nested #ifs,
  indirection through other macro definitions, wrapping the whole test, etc.).

  Args:
    lines: The lines of the file, in which to search.
    start_line: The starting point of the search. This should be the line at
      which the MAYBE_Foo macro is used to define the test.
    test_name: The name of the test we're searching for. This is only the test
      name, it doesn't include the suite name.
  """

  # State machine - step backwards until we find a line defining a macro with
  # the given name, keeping track of the most recent #endif we've encountered.
  # Once we've seen such a macro, terminate at the first #if or #ifdef we see.
  #
  # The range between the #if/#ifdef and the #endif is the range defining the
  # conditions under which this test is disabled.
  #
  # We also keep track of which branch disables the test, so we know whether to
  # negate the condition.

  disabled_test = 'DISABLED_' + test_name
  maybe_test = 'MAYBE_' + test_name
  start = None
  found_define = False
  in_disabled_branch = False
  most_recent_endif = None

  disabled_on_true = None

  for i in range(start_line, 0, -1):
    match = get_directive(lines, i)
    if not match:
      continue

    name, args = match
    if name == 'endif':
      most_recent_endif = i
    elif name == 'define' and args[0] == maybe_test:
      if most_recent_endif is None:
        raise Exception(
            f'{maybe_test} is defined outside of a preprocessor conditional')

      found_define = True
      if args[1] == disabled_test:
        in_disabled_branch = True
    elif name == 'else' and in_disabled_branch:
      disabled_on_true = False
      in_disabled_branch = False
    elif name in {'if', 'ifdef'} and found_define:
      if in_disabled_branch:
        disabled_on_true = True

      existing_conds = args[0]

      start = i
      break

  assert start is not None
  assert most_recent_endif is not None

  if not disabled_on_true:
    # TODO: Maybe 'not' should still wrap its args in a list, for consistency?
    existing_conds = ('not', existing_conds)

  return canonicalise(existing_conds), (start, most_recent_endif)


def get_directive(lines: List[str], i: int) -> Optional[Tuple[str, Any]]:
  """Scans for a preprocessor directive at the given line.

  We don't just pass the single line at lines[i], as the line might end with a
  backslash and hence continue over to the next line.

  Args:
    lines: The lines of the file to look for directives.
    i: The point at which to look from

  Returns:
    None if this lines doesn't contain a preprocessor directive.
    If it does, a tuple of (directive_name, [args])
    The args are parsed into an AST.
  """

  full_line = lines[i]

  # Handle any backslash line continuations to get the full line.
  while full_line.endswith('\\'):
    i += 1
    full_line = full_line[:-2] + lines[i]

  # TODO: Pre-compile regexes.
  # Strip comments.
  # C-style. Note that C-style comments don't nest, so we can just match them
  # with a regex.
  full_line = re.sub(r'/\*.*\*/', '', full_line)

  # C++-style
  full_line = re.sub('//.*$', '', full_line)

  # Preprocessor directives begin with a '#', which *must* be at the start of
  # the line, with only whitespace allowed to appear before them.
  match = re.match('^[ \t]*#[ \t]*(\\w*)(.*)', full_line)
  if not match:
    return None

  directive = match.group(1)
  # NOTE: This is a subset of all valid preprocessing tokens, as this matches
  # the set that are typically used. We may need to expand this in the future,
  # e.g. to more fully match integer literals.
  tokens = re.findall('"[^"]*"|<[^>]*>|\\w+|\\d+|&&|\|\||[,()!]',
                      match.group(2))

  # Reverse the token list, so we can pop from the end non-quadratically. We
  # could also maintain an index, but it would have to be shared down the whole
  # call stack so this is easier.
  tokens.reverse()
  args = []
  while tokens:
    args.append(parse_arg(tokens))

  return (directive, args)


def parse_arg(tokens: List[str]) -> Union[Tuple, str]:
  """Parser for binary operators."""

  # First parse the LHS.
  term = parse_terminal(tokens)

  # Then check if there's an operator.
  if peek(tokens) in {'&&', '||'}:
    # And if so parse out the RHS and connect them.
    # TODO: Handle operator precedence properly.
    return (tokens.pop(), [term, parse_arg(tokens)])

  # If not then just return the LHS.
  return term


def parse_terminal(tokens: List[str]) -> Union[Tuple, str]:
  """Parser for everything else."""
  tok = tokens.pop()

  if is_ident(tok) and peek(tokens) == '(':
    # Function-style macro or builtin. Or it could be arbitrary tokens if this
    # is the definition of a macro. But we ignore this for now.
    ident = tok
    tokens.pop()
    args = []
    while (next_tok := peek(tokens)) != ')':
      if next_tok is None:
        raise Exception('End of input while parsing preprocessor macro')
      if next_tok == ',':
        tokens.pop()
      else:
        args.append(parse_arg(tokens))
    tokens.pop()

    return (ident, args)

  if tok == '(':
    # Bracketed expression. Just parse the contained expression and then ensure
    # there's a closing bracket.
    arg = parse_arg(tokens)
    if peek(tokens) != ')':
      raise Exception('Expected closing bracket')
    tokens.pop()
    return arg

  if tok == '!':
    # Prefix operator '!', which takes a single argument.
    arg = parse_arg(tokens)
    return (tok, arg)

  # Otherwise this is a terminal, so just return it.
  return tok


def peek(tokens: List[str]) -> Optional[str]:
  """Return the next token without consuming it, if tokens is non-empty."""
  if tokens:
    return tokens[-1]
  return None


def is_ident(s: str) -> bool:
  """Checks if s is a valid identifier.

  This doesn't handle the full intricacies of the spec.
  """
  return all(c.isalnum() or c == '_' for c in s)


GTestInfo = collections.namedtuple('GTestInfo', ['type', 'name', 'header'])

# Sentinel values for representing types of conditions.
MACRO_TYPE = object()
BUILDFLAG_TYPE = object()

# Extend conditions.TERMINALS with GTest-specific info.
for t_name, t_repr in [
    ('android', GTestInfo(BUILDFLAG_TYPE, 'IS_ANDROID',
                          'build/build_config.h')),
    ('chromeos', GTestInfo(BUILDFLAG_TYPE, 'IS_CHROMEOS',
                           'build/build_config.h')),
    ('fuchsia', GTestInfo(BUILDFLAG_TYPE, 'IS_FUCHSIA',
                          'build/build_config.h')),
    ('ios', GTestInfo(BUILDFLAG_TYPE, 'IS_IOS', 'build/build_config.h')),
    ('linux', GTestInfo(BUILDFLAG_TYPE, 'IS_LINUX', 'build/build_config.h')),
    ('mac', GTestInfo(BUILDFLAG_TYPE, 'IS_MAC', 'build/build_config.h')),
    ('win', GTestInfo(BUILDFLAG_TYPE, 'IS_WIN', 'build/build_config.h')),
    ('arm64', GTestInfo(MACRO_TYPE, 'ARCH_CPU_ARM64', 'build/build_config.h')),
    ('x86', GTestInfo(MACRO_TYPE, 'ARCH_CPU_X86', 'build/build_config.h')),
    ('x86-64', GTestInfo(MACRO_TYPE, 'ARCH_CPU_X86_64',
                         'build/build_config.h')),
    ('asan', GTestInfo(MACRO_TYPE, 'ADDRESS_SANITIZER', None)),
    ('msan', GTestInfo(MACRO_TYPE, 'MEMORY_SANITIZER', None)),
    ('tsan', GTestInfo(MACRO_TYPE, 'THREAD_SANITIZER', None)),
    ('lacros',
     GTestInfo(BUILDFLAG_TYPE, 'IS_CHROMEOS_LACROS',
               'build/chromeos_buildflags.h')),
    ('ash',
     GTestInfo(BUILDFLAG_TYPE, 'IS_CHROMEOS_ASH',
               'build/chromeos_buildflags.h')),
]:
  conditions.get_term(t_name).gtest_info = t_repr


# TODO: Handle #ifdef properly. Probably the easiest thing to do is wrap its one
# arg in 'defined' and then treat it the same as #if. Note that as of
# 2021-10-13, only two files across all of chromium/src use #ifdef for disabling
# tests, so this is pretty low priority.
def canonicalise(parsed_condition) -> Condition:
  """Make a Condition from a raw preprocessor AST.

  Take the raw form of the condition we've parsed from the file and convert it
  into its canonical form, replacing any domain-specific stuff with its generic
  form.
  """

  if not isinstance(parsed_condition, tuple):
    return parsed_condition

  # Convert logical operators into their canonical Condition form.
  op, args = parsed_condition
  if op == '!':
    # Just one arg in this case, not wrapped in a list.
    return ('not', canonicalise(args))

  if (logical_fn := {'&&': 'and', '||': 'or'}.get(op, None)) is not None:
    return (logical_fn, [canonicalise(arg) for arg in args])

  # If not a logical operator, this must a be a function-style macro used to
  # express a condition. Find the Terminal it represents.
  assert len(args) == 1
  term = next((t for t in conditions.TERMINALS if t.gtest_info.name == args[0]),
              None)
  if term is None:
    # TODO: Should probably produce with a nicer error message here, as this is
    # a somewhat expected case where we can't parse the existing condition.
    raise Exception(f"Couldn't find any terminal corresponding to {args[0]}")

  if op == 'defined':
    assert term.gtest_info.type == MACRO_TYPE
  elif op == 'BUILDFLAG':
    assert term.gtest_info.type == BUILDFLAG_TYPE
  else:
    raise Exception(f"Don't know what to do with expr {parsed_condition}")

  return term


def cc_format_condition(cond: Condition, add_brackets=False) -> str:
  """The reverse of canonicalise - produce a C++ expression for a Condition."""

  def bracket(s: str) -> str:
    return f"({s})" if add_brackets else s

  assert cond != conditions.ALWAYS
  assert cond != conditions.NEVER

  if isinstance(cond, conditions.Terminal):
    value = cond.gtest_info.name
    if cond.gtest_info.type == MACRO_TYPE:
      return f'defined({value})'
    if cond.gtest_info.type == BUILDFLAG_TYPE:
      return f'BUILDFLAG({value})'

    raise Exception(f"Don't know how to express condition '{cond}' in C++")

  assert isinstance(cond, tuple)

  # TODO: Avoid redundant brackets? We probably want to keep them even when
  # redundant in most cases, but !(defined(X)) should be !defined(X).
  op, args = cond
  if op == 'not':
    return f'!({cc_format_condition(args)})'
  if op == 'and':
    return bracket(' && '.join(cc_format_condition(arg, True) for arg in args))
  if op == 'or':
    return bracket(' || '.join(cc_format_condition(arg, True) for arg in args))

  raise Exception(f'Unknown op "{op}"')


# TODO: Running clang-format is ~400x slower than everything else this tool does
# (excluding ResultDB RPCs). We may want to consider replacing this with
# something simpler that applies only the changes we need, and doesn't require
# shelling out to an external tool.
def clang_format(file_contents: str, modified_lines: List[int]) -> str:
  # clang-format accepts 1-based line numbers. Do the adjustment here to keep
  # things simple for the calling code.
  modified_lines = [i + 1 for i in modified_lines]

  p = subprocess.Popen(['python3', CLANG_FORMAT, '--style=file'] +
                       [f'--lines={i}:{i}' for i in modified_lines],
                       cwd=CHROMIUM_SRC,
                       stdin=subprocess.PIPE,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE,
                       text=True)

  stdout, stderr = p.communicate(file_contents)

  if p.returncode != 0:
    # TODO: We might want to distinguish between different types of error here.
    #
    # If it failed because the user doesn't have clang-format in their path, we
    # might want to raise UserError and tell them to install it. Or perhaps to
    # just return the original file contents and forgo formatting.
    #
    # But if it failed because we generated bad output and clang-format is
    # rightfully rejecting it, that should definitely be an InternalError.
    raise errors.InternalError(f'clang-format failed with: {stderr}')

  return stdout
