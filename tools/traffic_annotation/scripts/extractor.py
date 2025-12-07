#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Extracts network traffic annotation definitions from C++ source code.
"""

from __future__ import print_function

import argparse
import re
import sys
import traceback

from annotation_tools import NetworkTrafficAnnotationTools
from annotation_tokenizer import Tokenizer, SourceCodeParsingError

from enum import Enum
from pathlib import Path
from typing import List, Dict, NamedTuple


class AnnotationType(Enum):
  COMPLETE = 'Definition'
  PARTIAL = 'Partial'
  COMPLETING = 'Completing'
  BRANCHED_COMPLETING = 'BranchedCompleting'
  MUTABLE = 'Mutable'


class Language(NamedTuple):
  """Info on how to parse a given programming language's source code."""
  # Human-readable name, for debugging.
  name: str
  # Maps definition function names to the type of annotation they define.
  annotation_types: Dict[str, AnnotationType]
  # Regex that matches an annotation definition. Capture group 1 of this regex
  # should contain a function name that can be mapped via annotation_types.
  call_detection_regex: re.Pattern

# Exit code for parsing errors. Other runtime errors return 1.
EX_PARSE_ERROR = 2

# A regex that scans more quickly than the other regexen below, for
# pre-filtering. A first pass will check for these strings, and skip any files
# that don't contain them. Keep this regex simple, so it's fast.
#
# N.B.: this regex MUST match anything that would be matched by the other
# regexen below, or we will get false negatives (i.e., we will miss some
# annotations because pre-filtering is too strict).
PREFILTER_REGEX = re.compile(r'''
  TrafficAnnotation | TRAFFIC_ANNOTATION
''', re.VERBOSE | re.IGNORECASE)

# Language definition for C++ source files.
CPP_ANNOTATION_TYPES = {
    'DefineNetworkTrafficAnnotation': AnnotationType.COMPLETE,
    'DefinePartialNetworkTrafficAnnotation': AnnotationType.PARTIAL,
    'CompleteNetworkTrafficAnnotation': AnnotationType.COMPLETING,
    'BranchedCompleteNetworkTrafficAnnotation':
    AnnotationType.BRANCHED_COMPLETING,
    'CreateMutableNetworkTrafficAnnotationTag': AnnotationType.MUTABLE,
}

CPP_LANGUAGE = Language(name='C++',
                        annotation_types=CPP_ANNOTATION_TYPES,
                        call_detection_regex=re.compile(
                            r'''
    \b
    # Look for one of the tracked function names.
    # Capture group 1: function name.
    (
      ''' + ('|'.join(CPP_ANNOTATION_TYPES.keys())) + r'''
    )
    # Followed by a left-paren.
    \s*
    \(
  ''', re.VERBOSE | re.DOTALL))

# Language definition for Java source files.
JAVA_ANNOTATION_TYPES = {
    'createComplete': AnnotationType.COMPLETE,
}

JAVA_LANGUAGE = Language(name='Java',
                         annotation_types=JAVA_ANNOTATION_TYPES,
                         call_detection_regex=re.compile(
                             r'''
    \b
    # Look for a string like NetworkTrafficAnnotationTag.<methodName>
    NetworkTrafficAnnotationTag \s* \. \s*
    # Capture group 1: method name.
    (
      ''' + ('|'.join(JAVA_ANNOTATION_TYPES.keys())) + r'''
    )
    # Followed by a left-paren.
    \s*
    \(
  ''', re.VERBOSE | re.DOTALL))

# Maps file extensions to their Language definition.
LANGUAGE_MAPPING: Dict[str, Language] = {
    '.cc': CPP_LANGUAGE,
    '.mm': CPP_LANGUAGE,
    '.java': JAVA_LANGUAGE,
}

# Regex that matches an annotation that should only be used in test files.
TEST_ANNOTATION_REGEX = re.compile(
    r'\b(PARTIAL_)?TRAFFIC_ANNOTATION_FOR_TESTS\b')

# Regex that matches a placeholder annotation for a few whitelisted files.
MISSING_ANNOTATION_REGEX = re.compile(r'\bMISSING_TRAFFIC_ANNOTATION\b')

# Regex that matches placeholder annotations for unsupported platforms that
# don't require Network Traffic Annotations compliance. (e.g. iOS)
NO_ANNOTATION_REGEX = re.compile(r'\bNO_TRAFFIC_ANNOTATION_YET\b')

# List of supported file extensions for source code.
SUPPORTED_EXTENSIONS = set(LANGUAGE_MAPPING.keys())


class Annotation:
  """A network annotation definition in C++ code."""

  def __init__(self,
               language: Language,
               file_path: Path,
               line_number: int,
               type_name: AnnotationType,
               unique_id='',
               extra_id='',
               text=''):
    """Constructs an Annotation object with the given field values.

    Args:
      file_path: Path to the file that contains this annotation.
    """
    self.language = language
    self.file_path = file_path
    self.line_number = line_number
    self.type_name = type_name
    self.unique_id = unique_id
    self.extra_id = extra_id
    self.text = text

  def parse_definition(self, re_match: re.Match):
    """Parses the annotation and populates object fields.

    Args:
      re_match: A Match obtained from the Language's call_detection_regex.
    """
    definition_function = re_match.group(1)
    self.type_name = self.language.annotation_types[definition_function]

    # Parse the arguments given to the definition function, populating
    # |unique_id|, |text| and (possibly) |extra_id|.
    body = re_match.string[re_match.end():]
    self._parse_body(body)


  def extractor_output_string(self) -> str:
    """Returns a string formatted for output."""
    return '\n'.join(
        map(str, [
            '==== NEW ANNOTATION ====',
            self.file_path,
            self.line_number,
            self.type_name.value,
            self.unique_id,
            self.extra_id,
            self.text,
            '==== ANNOTATION ENDS ====',
        ]))

  def _parse_body(self, body: str):
    """Tokenizes and parses the arguments given to the definition function."""
    # Don't bother parsing CreateMutableNetworkTrafficAnnotationTag(), we don't
    # care about its arguments anyways.
    if self.type_name == AnnotationType.MUTABLE:
      return

    tokenizer = Tokenizer(body, self.file_path, self.line_number)

    # unique_id
    self.unique_id = tokenizer.advance('string_literal')
    tokenizer.advance('comma')

    # extra_id (Partial/BranchedCompleting)
    if self.type_name in [
        AnnotationType.PARTIAL, AnnotationType.BRANCHED_COMPLETING
    ]:
      self.extra_id = tokenizer.advance('string_literal')
      tokenizer.advance('comma')

    # partial_annotation (Completing/BranchedCompleting)
    if self.type_name in [
        AnnotationType.COMPLETING, AnnotationType.BRANCHED_COMPLETING
    ]:
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
  # TODO(crbug.com/40629071): Add multi-line comment support.


def may_contain_annotations(file_contents: str) -> bool:
  """Returns False if |file_path| is guaranteed not to contain annotations.

  This runs much faster than extract_annotations(), and is meant for
  pre-filtering. If this returns True, then |file_path| *might* contain
  annotations. Call extract_annotations() to know for sure."""
  return bool(PREFILTER_REGEX.search(file_contents))


def extract_annotations(file_path: Path, contents: str) -> List[Annotation]:
  """Extracts and returns annotations from the file at |file_path|."""
  if file_path.suffix not in LANGUAGE_MAPPING:
    raise ValueError("Unrecognized extension '{}' for file '{}'.".format(
        file_path.suffix, str(file_path)))

  language = LANGUAGE_MAPPING[file_path.suffix]

  defs = []

  # Check for function calls (e.g. DefineNetworkTrafficAnnotation(...))
  for re_match in language.call_detection_regex.finditer(contents):
    if is_inside_comment(re_match.string, re_match.start()):
      continue
    line_number = get_line_number_at(contents, re_match.start())
    annotation = Annotation(language, file_path, line_number,
                            AnnotationType.COMPLETE)
    annotation.parse_definition(re_match)
    defs.append(annotation)

  # Check for test annotations (e.g. TRAFFIC_ANNOTATION_FOR_TESTS)
  for re_match in TEST_ANNOTATION_REGEX.finditer(contents):
    if is_inside_comment(re_match.string, re_match.start()):
      continue
    line_number = get_line_number_at(contents, re_match.start())

    is_partial = bool(re_match.group(1))
    if is_partial:
      type_name = AnnotationType.PARTIAL
      unique_id = 'test_partial'
      extra_id = 'test'
    else:
      type_name = AnnotationType.COMPLETE
      unique_id = 'test'
      extra_id = ''

    annotation = Annotation(
        language,
        file_path,
        line_number,
        type_name=type_name,
        unique_id=unique_id,
        extra_id=extra_id,
        text='Traffic annotation for unit, browser and other tests')
    defs.append(annotation)

  # Check for MISSING_TRAFFIC_ANNOTATION.
  for re_match in MISSING_ANNOTATION_REGEX.finditer(contents):
    if is_inside_comment(re_match.string, re_match.start()):
      continue
    line_number = get_line_number_at(contents, re_match.start())

    annotation = Annotation(language,
                            file_path,
                            line_number,
                            type_name=AnnotationType.COMPLETE,
                            unique_id='missing',
                            text='Function called without traffic annotation.')
    defs.append(annotation)

  # Check for NO_TRAFFIC_ANNOTATION_YET.
  for re_match in NO_ANNOTATION_REGEX.finditer(contents):
    if is_inside_comment(re_match.string, re_match.start()):
      continue
    line_number = get_line_number_at(contents, re_match.start())

    annotation = Annotation(language,
                            file_path,
                            line_number,
                            type_name=AnnotationType.COMPLETE,
                            unique_id='undefined',
                            text='Nothing here yet.')
    defs.append(annotation)

  return defs


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--options-file',
                      type=Path,
                      help='optional file to read options from')
  args, argv = parser.parse_known_args()
  if args.options_file is not None:
    argv = args.options_file.read_text(encoding="utf-8").split()

  parser.add_argument(
      '--build-path',
      type=Path,
      help='Specifies a compiled build directory, e.g. out/Debug.')
  parser.add_argument(
      '--generate-compdb', action='store_true',
      help='Generate a new compile_commands.json before running')
  parser.add_argument(
      '--no-filter', action='store_true',
      help='Do not filter files based on compdb entries')
  parser.add_argument('file_paths',
                      nargs='+',
                      type=Path,
                      help='List of files to process.')

  args = parser.parse_args(argv)

  if not args.no_filter:
    tools = NetworkTrafficAnnotationTools(args.build_path)
    compdb_files = tools.GetCompDBFiles(args.generate_compdb)

  annotation_definitions = []

  # Parse all the files.
  # TODO(crbug.com/40629071): Do this in parallel.
  for file_path in args.file_paths:
    if not args.no_filter and file_path.resolve() not in compdb_files:
      continue
    try:
      annotation_definitions.extend(
          extract_annotations(file_path, file_path.read_text(encoding="utf-8")))
    except SourceCodeParsingError:
      traceback.print_exc()
      return EX_PARSE_ERROR

  # Print output.
  for annotation in annotation_definitions:
    print(annotation.extractor_output_string())

  # If all files were successfully checked for annotations but none of them had
  # any, print something so that the traffic_annotation_auditor knows there was
  # no error so that the files get checked for deleted annotations.
  if not annotation_definitions:
    print('No annotations in these files.')
  return 0


if '__main__' == __name__:
  sys.exit(main())
