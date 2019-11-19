#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Extracts network traffic annotation definitions from C++ source code.
"""

from __future__ import print_function

import argparse
import os
import re
import sys

from annotation_tools import NetworkTrafficAnnotationTools
from annotation_tokenizer import Tokenizer

ANNOTATION_TYPES = {
    'DefineNetworkTrafficAnnotation': 'Definition',
    'DefinePartialNetworkTrafficAnnotation': 'Partial',
    'CompleteNetworkTrafficAnnotation': 'Completing',
    'BranchedCompleteNetworkTrafficAnnotation': 'BranchedCompleting',
    'CreateMutableNetworkTrafficAnnotationTag': 'Mutable',
}

# Regex that matches an annotation definition.
CALL_DETECTION_REGEX = re.compile(r'''
  \b
  # Look for one of the tracked function names.
  # Capture group 1.
  (
    ''' + ('|'.join(ANNOTATION_TYPES.keys())) + r'''
  )
  # Followed by a left-paren.
  \s*
  \(
''', re.VERBOSE | re.DOTALL)

# Regex that matches an annotation that should only be used in test files.
TEST_ANNOTATION_REGEX = re.compile(
    r'\b(PARTIAL_)?TRAFFIC_ANNOTATION_FOR_TESTS\b')

# Regex that matches a placeholder annotation for a few whitelisted files.
MISSING_ANNOTATION_REGEX = re.compile(r'\bMISSING_TRAFFIC_ANNOTATION\b')

class Annotation:
  """A network annotation definition in C++ code."""

  def __init__(self, file_path, line_number, type_name='', unique_id='',
               extra_id='', text=''):
    """Constructs an Annotation object with the given field values.

    Args:
      file_path: Path to the file that contains this annotation.
    """
    self.file_path = file_path
    self.line_number = line_number
    self.type_name = type_name
    self.unique_id = unique_id
    self.extra_id = extra_id
    self.text = text

  def parse_definition(self, re_match):
    """Parses the annotation and populates object fields.

    Args:
      file_path: Path to the file that contains this annotation.
      re_match: A MatchObject obtained from CALL_DETECTION_REGEX.
    """
    definition_function = re_match.group(1)
    self.type_name = ANNOTATION_TYPES[definition_function]

    # Parse the arguments given to the definition function, populating
    # |unique_id|, |text| and (possibly) |extra_id|.
    body = re_match.string[re_match.end():]
    self._parse_body(body)


  def clang_tool_output_string(self):
    """Returns a string formatted for clang-tool-style output."""
    return "\n".join(map(str, [
        "==== NEW ANNOTATION ====",
        self.file_path,
        self.line_number,
        self.type_name,
        self.unique_id,
        self.extra_id,
        self.text,
        "==== ANNOTATION ENDS ====",
    ]))

  def _parse_body(self, body):
    """Tokenizes and parses the arguments given to the definition function."""
    # Don't bother parsing CreateMutableNetworkTrafficAnnotationTag(), we don't
    # care about its arguments anyways.
    if self.type_name == 'Mutable':
      return

    tokenizer = Tokenizer(body, self.file_path, self.line_number)

    # unique_id
    self.unique_id = tokenizer.advance('string_literal')
    tokenizer.advance('comma')

    # extra_id (Partial/BranchedCompleting)
    if self.type_name == 'Partial' or self.type_name == 'BranchedCompleting':
      self.extra_id = tokenizer.advance('string_literal')
      tokenizer.advance('comma')

    # partial_annotation (Completing/BranchedCompleting)
    if self.type_name == 'Completing' or self.type_name == 'BranchedCompleting':
      # Skip the |partial_annotation| argument. It can be a variable_name, or a
      # FunctionName(), so skip the parentheses if they're there.
      tokenizer.advance('symbol')
      if tokenizer.maybe_advance('left_paren'):
        tokenizer.advance('right_paren')
      tokenizer.advance('comma')

    # proto text
    self.text = tokenizer.advance('string_literal')

    # The function call should end here without any more arguments.
    assert tokenizer.advance('right_paren')


def get_line_number_at(string, pos):
  """Find the line number for the char at position |pos|. 1-indexed."""
  # This is inefficient: O(n). But we only run it once for each annotation
  # definition, so the effect on total runtime is negligible.
  return 1 + len(re.compile(r'\n').findall(string[:pos]))


def is_inside_comment(string, pos):
  """Checks if the position |pos| within string seems to be inside a comment.

  This is a bit naive. Only checks for single-line comments (// ...), not block
  comments (/* ...  */).

  Args:
    string: string to scan.
    pos: position within the string.

  Returns:
    True if |string[pos]| looks like it's inside a C++ comment.
  """
  # Look for "//" on the same line in the reversed string.
  return bool(re.match(r'[^\n]*//', string[pos::-1]))
  # TODO(crbug/966883): Add multi-line comment support.


def extract_annotations(file_path):
  """Extracts and returns annotations from the file at |file_path|."""
  with open(file_path) as f:
    contents = f.read()

  defs = []

  # Check for function calls (e.g. DefineNetworkTrafficAnnotation(...))
  for re_match in CALL_DETECTION_REGEX.finditer(contents):
    if is_inside_comment(re_match.string, re_match.start()):
      continue
    line_number = get_line_number_at(contents, re_match.start())
    annotation = Annotation(file_path, line_number)
    annotation.parse_definition(re_match)
    defs.append(annotation)

  # Check for test annotations (e.g. TRAFFIC_ANNOTATION_FOR_TESTS)
  for re_match in TEST_ANNOTATION_REGEX.finditer(contents):
    if is_inside_comment(re_match.string, re_match.start()):
      continue
    line_number = get_line_number_at(contents, re_match.start())

    is_partial = bool(re_match.group(1))
    if is_partial:
      type_name = 'Partial'
      unique_id = 'test_partial'
      extra_id = 'test'
    else:
      type_name = 'Definition'
      unique_id = 'test'
      extra_id = ''

    annotation = Annotation(
        file_path, line_number, type_name=type_name,
        unique_id=unique_id, extra_id=extra_id,
        text='Traffic annotation for unit, browser and other tests')
    defs.append(annotation)

  # Check for MISSING_TRAFFIC_ANNOTATION.
  for re_match in MISSING_ANNOTATION_REGEX.finditer(contents):
    if is_inside_comment(re_match.string, re_match.start()):
      continue
    line_number = get_line_number_at(contents, re_match.start())

    annotation = Annotation(
        file_path, line_number, type_name='Definition', unique_id='missing',
        text='Function called without traffic annotation.')
    defs.append(annotation)

  return defs


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--options-file',
      help='optional file to read options from')
  args, argv = parser.parse_known_args()
  if args.options_file:
    argv = open(args.options_file).read().split()

  parser.add_argument(
      '--build-path',
      help='Specifies a compiled build directory, e.g. out/Debug.')
  parser.add_argument(
      '--generate-compdb', action='store_true',
      help='Generate a new compile_commands.json before running')
  parser.add_argument(
      '--no-filter', action='store_true',
      help='Do not filter files based on compdb entries')
  parser.add_argument(
      'file_paths', nargs='+', help='List of files to process.')

  args = parser.parse_args(argv)

  tools = NetworkTrafficAnnotationTools(args.build_path)
  compdb_files = tools.GetCompDBFiles(args.generate_compdb)

  annotation_definitions = []

  # Parse all the files.
  # TODO(crbug/966883): Do this in parallel.
  for file_path in args.file_paths:
    if not args.no_filter and os.path.abspath(file_path) not in compdb_files:
      continue
    annotation_definitions.extend(extract_annotations(file_path))

  # Print output.
  for annotation in annotation_definitions:
    print(annotation.clang_tool_output_string())

  # If all files were successfully checked for annotations but none of them had
  # any, print something so that the traffic_annotation_auditor knows there was
  # no error so that the files get checked for deleted annotations.
  if not annotation_definitions:
    print('No annotations in these files.')
  return 0


if '__main__' == __name__:
  sys.exit(main())
