#!/usr/bin/env python
#
# Copyright The Closure Library Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tool to insert JsDoc before a function.

This script attempts to find the first function passed in to stdin, generate
JSDoc for it (with argument names and possibly return value), and inject
it in the string.  This is intended to be used as a subprocess by editors
such as emacs and vi.
"""

import re
import sys


# Matches a typical Closure-style function definition.
_FUNCTION_REGEX = re.compile(r"""
# Start of line
^

# Indentation
(?P<indentation>[ ]*)

# Identifier (handling split across line)
(?P<identifier>\w+(\s*\.\s*\w+)*)

# "= function"
\s* = \s* function \s*

# opening paren
\(

# Function arguments
(?P<arguments>(?:\s|\w+|,)*)

# closing paren
\)

# opening bracket
\s* {

""", re.MULTILINE | re.VERBOSE)


def _MatchFirstFunction(script):
  """Match the first function seen in the script."""
  return _FUNCTION_REGEX.search(script)


def _ParseArgString(arg_string):
  """Parse an argument string (inside parens) into parameter names."""
  for arg in arg_string.split(','):
    arg = arg.strip()
    if arg:
      yield arg


def _ExtractFunctionBody(script, indentation=0):
  """Attempt to return the function body."""

  # Real extraction would require a token parser and state machines.
  # We look for first bracket at the same level of indentation.
  regex_str = r'{(.*?)^[ ]{%d}}' % indentation

  function_regex = re.compile(regex_str, re.MULTILINE | re.DOTALL)
  match = function_regex.search(script)
  if match:
    return match.group(1)


def _ContainsReturnValue(function_body):
  """Attempt to determine if the function body returns a value."""
  return_regex = re.compile(r'\breturn\b[^;]')

  # If this matches, we assume they're returning something.
  return bool(return_regex.search(function_body))


def _InsertString(original_string, inserted_string, index):
  """Insert a string into another string at a given index."""
  return original_string[0:index] + inserted_string + original_string[index:]


def _GenerateJsDoc(args, return_val=False):
  """Generate JSDoc for a function.

  Args:
    args: A list of names of the argument.
    return_val: Whether the function has a return value.

  Returns:
    The JSDoc as a string.
  """

  lines = []
  lines.append('/**')

  lines += [' * @param {} %s' % arg for arg in args]

  if return_val:
    lines.append(' * @return')

  lines.append(' */')

  return '\n'.join(lines) + '\n'


def _IndentString(source_string, indentation):
  """Indent string some number of characters."""
  lines = [(indentation * ' ') + line
           for line in source_string.splitlines(True)]
  return ''.join(lines)


def InsertJsDoc(script):
  """Attempt to insert JSDoc for the first seen function in the script.

  Args:
    script: The script, as a string.

  Returns:
    Returns the new string if function was found and JSDoc inserted. Otherwise
      returns None.
  """

  match = _MatchFirstFunction(script)
  if not match:
    return

  # Add argument flags.
  args_string = match.group('arguments')
  args = _ParseArgString(args_string)

  start_index = match.start(0)
  function_to_end = script[start_index:]

  lvalue_indentation = len(match.group('indentation'))

  return_val = False
  function_body = _ExtractFunctionBody(function_to_end, lvalue_indentation)
  if function_body:
    return_val = _ContainsReturnValue(function_body)

  jsdoc = _GenerateJsDoc(args, return_val)
  if lvalue_indentation:
    jsdoc = _IndentString(jsdoc, lvalue_indentation)

  return _InsertString(script, jsdoc, start_index)


if __name__ == '__main__':
  stdin_script = sys.stdin.read()
  result = InsertJsDoc(stdin_script)

  if result:
    sys.stdout.write(result)
  else:
    sys.stdout.write(stdin_script)
