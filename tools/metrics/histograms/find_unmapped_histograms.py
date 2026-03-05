#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Scans the Chromium source for histograms that are absent from histograms.xml.

This is a heuristic scan, so a clean run of this script does not guarantee that
all histograms in the Chromium source are properly mapped.  Notably, field
trials are entirely ignored by this script.

"""

from __future__ import print_function

import hashlib
import logging
import optparse
import os
import re
import struct
import subprocess
import sys

import setup_modules

import chromium_src.tools.metrics.common.path_util as path_util
import chromium_src.tools.metrics.histograms.extract_histograms as extract_histograms
import chromium_src.tools.metrics.histograms.histogram_paths as histogram_paths
import chromium_src.tools.metrics.histograms.merge_xml as merge_xml


C_FILENAME = re.compile(r"""
    .*              # Anything
    \.(cc|cpp|h|mm) # Ending in these extensions
    $               # End of string
    """, re.VERBOSE)
TEST_FILENAME = re.compile(r"""
    .*   # Anything
    test # The word test
    \.   # A literal '.'
    """, re.VERBOSE)
NON_NEWLINE = re.compile(r'.+')
CPP_COMMENT = re.compile(r"""
    \s*             # Optional whitespace
    (?:             # Non-capturing group
        //.*        # C++-style comment
        \n          # Newline
        |           # or
        /\*         # Start C-style comment
        (?:         # Non-capturing group
            (?!\*/) # Negative lookahead for comment end
            [\s\S]  # Any character including newline
        )*          # Repeated zero or more times
        \*/         # End C-style comment
    )               # End group
    \s*             # Optional whitespace
    """, re.VERBOSE);
ADJACENT_C_STRING_REGEX = re.compile(r"""
    ("      # Opening quotation mark
    [^"]*)  # Literal string contents
    "       # Closing quotation mark
    \s*     # Any number of spaces
    "       # Another opening quotation mark
    """, re.VERBOSE)
CONSTANT_REGEX = re.compile(r"""
    (\w*::)*  # Optional namespace(s)
    k[A-Z]    # Match a constant identifier: 'k' followed by an uppercase letter
    \w*       # Match the rest of the constant identifier
    $         # Make sure there's only the identifier, nothing else
    """, re.VERBOSE)
MACRO_STRING_CONCATENATION_REGEX = re.compile(r"""
    \s*                           # Optional whitespace
    (                             # Group
        (                         # Nested group
            "[^"]*"               # Literal string
            |                     # or
            [a-zA-Z][a-zA-Z0-9_]+ # Macro constant name
        )                         # End of alternation
        \s*                       # Optional whitespace
    ){2,}                         # Group repeated 2 or more times
    $                             # End of string
    """, re.VERBOSE)
HISTOGRAM_MACRO_REGEX = re.compile(
    r"""
    (\w*           # Capture the whole macro name
    UMA_HISTOGRAM_ # Match the shared prefix for standard UMA histogram macros
    (\w*))         # Match the rest of the macro name, e.g. '_ENUMERATION'
    \(             # Match the opening parenthesis for the macro
    \s*            # Match any whitespace -- especially, any newlines
    ([^,)]*)       # Capture the first parameter to the macro
    [,)]           # Match the comma/paren that delineates the first parameter
    """, re.VERBOSE)
HISTOGRAM_FUNCTION_REGEX = re.compile(
    r"""
    (\w*           # Capture the whole function name
    UmaHistogram   # Match the shared prefix for UMA histogram functions
    (\w*))         # Match the rest of the macro name, e.g. 'Enumeration'
    \(             # Match the opening parenthesis for the macro
    \s*            # Match any whitespace -- especially, any newlines
    ([^,)]*)       # Capture the first parameter to the macro
    [,)]           # Match the comma/paren that delineates the first parameter
    """, re.VERBOSE)
HISTOGRAM_MACRO_SUFFIXES = frozenset([
    # Note: Order matches histogram_macros.h.
    'ENUMERATION',
    'SCALED_ENUMERATION',
    'BOOLEAN',
    'EXACT_LINEAR',
    'PERCENTAGE',
    'SCALED_EXACT_LINEAR',
    'COUNTS_100',
    'COUNTS_1000',
    'COUNTS_10000',
    'COUNTS_100000',
    'COUNTS_1M',
    'COUNTS_10M',
    'CUSTOM_COUNTS',
    'TIMES',
    'MEDIUM_TIMES',
    'LONG_TIMES',
    'LONG_TIMES_100',
    'CUSTOM_TIMES',
    'CUSTOM_MICROSECONDS_TIMES',
    'MEMORY_KB',
    'MEMORY_MEDIUM_MB',
    'MEMORY_LARGE_MB',
    'SPARSE',
    'COUNTS',
    'MEMORY_MB',
    'CUSTOM_ENUMERATION',
    # The following macro name suffixes are not defined in //base/metrics.
    'SCROLL_LATENCY_SHORT',
    'SCROLL_LATENCY_LONG',
    'TOUCH_TO_SCROLL_LATENCY',
    'LARGE_MEMORY_MB',
    'MEGABYTES_LINEAR',
    'LINEAR',
    'ALLOCATED_MEGABYTES',
    'CUSTOM_TIMES_MICROS',
    'TIME_IN_MINUTES_MONTH_RANGE',
    'TIMES_16H',
    'MINUTES',
    'MBYTES',
    'ASPECT_RATIO',
    'LOAD_TIME',
    'LOCATION_RESPONSE_TIMES',
    'LOCK_TIMES',
    'OOM_KILL_TIME_INTERVAL',
    'MICRO_TIMES',
    'CUSTOM_EXACT_LINEAR',
    'EVENT_LATENCY_TIMES',
    'RASTER_TIME_CUSTOM_MICROSECONDS',
])
OTHER_HISTOGRAM_MACROS = frozenset([
    'SCOPED_UMA_HISTOGRAM_TIMER',
    'SCOPED_UMA_HISTOGRAM_LONG_TIMER',
    'SCOPED_UMA_HISTOGRAM_TIMER_MICROS',
    'SCOPED_BLINK_UMA_HISTOGRAM_TIMER',
    'SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES',
    'SCOPED_UMA_HISTOGRAM_MICRO_TIMER',
    'SCOPED_UMA_HISTOGRAM_THREAD_TIMER',
    'SCOPED_UMA_HISTOGRAM_MICRO_THREAD_TIMER',
])
HISTOGRAM_FUNCTION_SUFFIXES = frozenset([
    # Note: Order matches histogram_functions.h.
    'ExactLinear',
    'Enumeration',
    'Boolean',
    'Percentage',
    'PercentageObsoleteDoNotUse',
    'CustomCounts',
    'Counts100',
    'Counts1000',
    'Counts10000',
    'Counts100000',
    'Counts1M',
    'Counts10M',
    'CustomTimes',
    'Times',
    'MediumTimes',
    'LongTimes',
    'LongTimes100',
    'CustomMicrosecondsTimes',
    'MicrosecondsTimes',
    'MemoryKB',
    'MemoryMB',
    'MemoryLargeMB',
    'Sparse',
    # The following function name suffixes are not defined in //base/metrics.
    'FileError',
])


def _git_cmd_available(command: list[str]) -> bool:
  """Checks if the specific git command is present in the current env"""
  cmd = ['git', '--no-pager']
  cmd.extend(command)
  proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
  return proc.wait() == 0


def _run_git(command: list[str]) -> str:
  """Run a git subcommand, returning its output."""
  # On Windows, use shell=True to get PATH interpretation.
  command = ['git'] + command
  logging.info(' '.join(command))
  shell = (os.name == 'nt')
  proc = subprocess.Popen(command, shell=shell, stdout=subprocess.PIPE)
  out = proc.communicate()[0].strip().decode('utf-8')
  return out


class DirectoryNotFoundException(Exception):
  """Base class to distinguish locally defined exceptions from standard ones."""

  def __init__(self, msg: str):
    self.msg = msg

  def __str__(self):
    return self.msg


def _keep_only_newlines(match_object: re.Match) -> str:
  """Remove everything from a matched string except for the newline characters.

  Takes a Match object argument so that it can be used directly as the repl
  argument to re.sub().

  Args:
    match_object: A Match object referencing the string to be substituted, e.g.
    '  // My histogram\n   '

  Returns:
     The string with non-newlines removed, eg.
     '\n'
  """
  return NON_NEWLINE.sub('', match_object.group(0))


def _remove_comments(string: str) -> str:
  """Remove comments from a string, including leading and trailing whitespace.

  This does not correctly ignore comments embedded in strings, but
  that shouldn't matter for this script. Newlines in the removed text are
  preserved so that line numbers don't change.

  Args:
    string: The string to remove comments from, e.g.
    '  // My histogram\n   "My.Important.Counts" '

  Returns:
    The string with comments removed, e.g. '"\nMy.Important.Counts" '

  """
  return CPP_COMMENT.sub(_keep_only_newlines, string)


def _collapse_adjacent_c_strings(string: str) -> str:
  """Collapses any adjacent C strings into a single string.

  Useful to re-combine strings that were split across multiple lines to satisfy
  the 80-col restriction.

  Args:
    string: The string to recombine, e.g. '"Foo"\n    "bar"'

  Returns:
    The collapsed string, e.g. "Foobar" for an input of '"Foo"\n    "bar"'
  """
  while True:
    collapsed = ADJACENT_C_STRING_REGEX.sub(r'\1', string, count=1)
    if collapsed == string:
      return collapsed

    string = collapsed


def log_non_literal_histogram(filename: str, symbol_name: str,
                              histogram: str) -> None:
  """Logs a warning about a non-literal histogram name found in the source.

  Filters out known acceptable exceptions.

  Args:
    filename: The filename for the file containing the histogram, e.g.
              'chrome/browser/memory_details.cc'
    symbol_name: The function/macro name called.
    histogram: The expression that evaluates to the name of the histogram, e.g.
               '"FakeHistogram" + variant'

  Returns:
    None
  """
  # Ignore histogram macros, which typically contain backslashes so that they
  # can be formatted across lines.
  if '\\' in histogram:
    return

  # Ignore histogram names that have been pulled out into C++ constants.
  if CONSTANT_REGEX.match(histogram):
    return

  # A blank value wouldn't compile unless it was in a comment.
  if not histogram:
    return

  # String concatenations involving macros are always constant.
  if MACRO_STRING_CONCATENATION_REGEX.match(histogram):
    return

  # TODO(isherman): This is still a little noisy... needs further filtering to
  # reduce the noise.
  logging.warning('%s: %s used with non-literal histogram name <%s>', filename,
                  symbol_name, histogram)


def _read_chromium_histograms() -> dict[str, str]:
  """Searches the Chromium source for all histogram names.

  Prints warnings for any invocations of the UMA_HISTOGRAM_* macros with
  names that might vary during a single run of the app.

  Returns:
    A dict mapping histogram name to first filename:line where it was found.
  """
  # Note: For functions, we don't require literals (no warnings printed), but
  # we also are limited to finding unmapped names coming from literals.
  logging.info('Scanning Chromium source for histograms functions...')
  location_map = _find_histograms('UmaHistogram',
                                  HISTOGRAM_FUNCTION_REGEX,
                                  HISTOGRAM_FUNCTION_SUFFIXES, [],
                                  require_literals=False)

  logging.info('Scanning Chromium source for histograms macros...')
  location_map2 = _find_histograms('UMA_HISTOGRAM',
                                   HISTOGRAM_MACRO_REGEX,
                                   HISTOGRAM_MACRO_SUFFIXES,
                                   OTHER_HISTOGRAM_MACROS,
                                   require_literals=True)

  location_map.update(location_map2)
  return location_map


def _find_histograms(grep_expression: str, regex: re.Pattern,
                     all_suffixes: list[str], all_others: list[str],
                     require_literals: bool) -> dict[str, str]:
  """Searches the Chromium source for histogram names.

  If `regex` is 'UMA_HISTOGRAM', prints warnings for any invocations the macros
  with names that might vary during a single run of the app.

  Args:
    grep_expression: Either 'UMA_HISTOGRAM' or 'UmaHistogram' as the expression
      to pass to "git gs" for searching for relevant lines.
    regex: The actual regex to find call sites.
    all_suffixes: Suffixes to look for.
    all_others: Other macros/functions to look for.
    require_literals: Whether errors should be printed about non-literal args.

  Returns:
    A dict mapping histogram name to first filename:line where it was found.
  """
  # Use git grep to find all invocations of the UMA_HISTOGRAM_* macros.
  # Examples:
  #   'path/to/foo.cc:420:  UMA_HISTOGRAM_COUNTS_100("FooGroup.FooName",'
  #   'path/to/bar.cc:632:  UMA_HISTOGRAM_ENUMERATION('
  locations = _run_git(['gs', grep_expression]).split('\n')
  all_filenames = set(location.split(':')[0] for location in locations);
  filenames = [f for f in all_filenames
               if C_FILENAME.match(f) and not TEST_FILENAME.match(f)]

  location_map = dict()
  unknown_symbols = set()
  for filename in filenames:
    with open(filename, 'r') as f:
      contents = _remove_comments(f.read())

    for match in regex.finditer(contents):
      line_number = contents[:match.start()].count('\n') + 1
      full_symbol_name = match.group(1)
      symbol_suffix = match.group(2)
      metric_name = _collapse_adjacent_c_strings(match.group(3).strip())

      if (symbol_suffix not in all_suffixes
          and full_symbol_name not in all_others):
        if (full_symbol_name not in unknown_symbols):
          logging.warning('%s:%d: Unknown symbol name: <%s>' %
                          (filename, line_number, match.group(1)))
          unknown_symbols.add(full_symbol_name)
        continue

      # Must begin and end with a quotation mark.
      if not metric_name or metric_name[0] != '"' or metric_name[-1] != '"':
        if require_literals:
          log_non_literal_histogram(filename, full_symbol_name, metric_name)
        # Not a literal name. Skip.
        continue

      # Must not include any quotation marks other than at the beginning or end.
      metric_name_stripped = metric_name.strip('"')
      if '"' in metric_name_stripped:
        if require_literals:
          log_non_literal_histogram(filename, full_symbol_name, metric_name)
        # Not a literal name. Skip.
        continue

      if metric_name_stripped not in location_map:
        location_map[metric_name_stripped] = '%s:%d' % (filename, line_number)

  return location_map


def _read_all_xml_histograms() -> set[str]:
  """Parses all histogram names defined in |histogram_paths.ALL_XMLS|.

  Returns:
    A set containing the parsed histogram names.
  """
  merged = merge_xml.MergeFiles(histogram_paths.ALL_XMLS)
  histograms, _ = extract_histograms.ExtractHistogramsFromDom(merged)
  return set(extract_histograms.ExtractNames(histograms))


def _read_xml_histograms(histograms_file_location: str) -> set[str]:
  """Parses all histogram names from |histograms_file_location|.

  Args:
    histograms_file_location: The given histograms.xml file path.

  Returns:
    A set containing the parsed histogram names.
  """
  logging.info('Reading histograms from %s...' % histograms_file_location)
  histograms = extract_histograms.ExtractHistograms(histograms_file_location)
  return set(extract_histograms.ExtractNames(histograms))


def _hash_histogram_name_to_int(t: str) -> int:
  """Computes the hash of a Chrome histogram name.

  The histogram ID is derived from the first 8 bytes of the md5 hash of the
  histogram's name. Those 64 bits are interpreted as an integer (big-endian
  unsigned long long). See HashMetricName here:
  https://source.chromium.org/chromium/chromium/src/+/main:base/metrics/metrics_hashes.cc

  Args:
    t: The string to hash (a histogram name).

  Returns:
    Histogram hash as an integer.
  """
  # >Q: 8 bytes, big endian.
  return struct.unpack('>Q', hashlib.md5(t.encode('utf-8')).digest()[:8])[0]


def _hash_histogram_name(t: str) -> str:
  """Computes the hash of a Chrome histogram name.

  Args:
    t: The string to hash (a histogram name).

  Returns:
    Histogram hash as a string representing a hex number, zero-padded to
    fixed length.
  """
  return f'0x{_hash_histogram_name_to_int(t):016x}'


def _cast_to_int32(n):
  """Simulates C++ style (int32_t) casting from a larger integer type."""
  # Mask to get the lower 32 bits
  n = n & 0xFFFFFFFF
  # Check if the sign bit (bit 31) is set
  if n & 0x80000000:
    # Negative number: apply two's complement
    return n - 0x100000000
  else:
    # Positive number
    return n


def _output_csv(unmapped_histograms: list[str],
                location_map: dict[str, str]) -> None:
  for histogram in sorted(unmapped_histograms):
    parts = location_map[histogram].split(':')
    assert len(parts) == 2
    (filename, line_number) = parts
    print('%s,%s,%s,%s' %
          (filename, line_number, histogram, _hash_histogram_name(histogram)))

def _output_log(unmapped_histograms: list[str], location_map: dict[str, str],
                verbose: bool) -> None:
  if not unmapped_histograms:
    logging.info('Success!  No unmapped histograms found.')
    return

  logging.info('')
  logging.info('')
  logging.info('Histograms in Chromium but not in XML files:')
  logging.info('-------------------------------------------------')
  for histogram in sorted(unmapped_histograms):
    line = ''
    if verbose:
      line += f'{location_map[histogram]}: '
    name_hash = _hash_histogram_name(histogram)
    name_hash_int = _hash_histogram_name_to_int(histogram)
    line += f'{histogram} - {name_hash} ({name_hash_int})'
    if verbose:
      line += f' (int32: {_cast_to_int32(name_hash_int)})'
    logging.info(line)


def main() -> None:
  # Find default paths.
  default_root = path_util.GetInputFile('/')

  # Parse command line options
  parser = optparse.OptionParser()
  parser.add_option(
    '--root-directory', dest='root_directory', default=default_root,
    help='scan within DIRECTORY for histograms [optional, defaults to "%s"]' %
        default_root,
    metavar='DIRECTORY')
  parser.add_option(
      '--extra_histograms-file',
      dest='extra_histograms_file_location',
      default=None,
      help='read additional histogram definitions from FILE (relative to '
      '--root-directory) [optional]',
      metavar='FILE')
  parser.add_option('--csv',
                    action='store_true',
                    dest='output_as_csv',
                    default=False,
                    help=('output as csv for ease of parsing ' +
                          '[optional, defaults to %default]'))
  parser.add_option('--verbose',
                    action='store_true',
                    dest='verbose',
                    default=False,
                    help=('print file location and int32 hashes for ' +
                          'histograms [optional, defaults to %default]'))

  (options, args) = parser.parse_args()
  if args:
    parser.print_help()
    sys.exit(1)

  logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.INFO)

  try:
    os.chdir(options.root_directory)
  except EnvironmentError as e:
    logging.error('Could not change to root directory: %s', e)
    sys.exit(1)

  if not _git_cmd_available(['gs', 'fake_search_term']):
    logging.error("`git gs` is not available in this environment.")
    sys.exit(1)

  location_map = _read_chromium_histograms()
  chromium_histograms = set(location_map.keys())
  xml_histograms = _read_all_xml_histograms()
  unmapped_histograms = chromium_histograms - xml_histograms

  if options.extra_histograms_file_location:
    if os.path.isfile(options.extra_histograms_file_location):
      xml_histograms2 = _read_xml_histograms(
          options.extra_histograms_file_location)
      unmapped_histograms -= xml_histograms2
    else:
      logging.warning('No such file: %s',
                      options.extra_histograms_file_location)

  if options.output_as_csv:
    _output_csv(unmapped_histograms, location_map)
  else:
    _output_log(unmapped_histograms, location_map, options.verbose)

  sys.exit(len(unmapped_histograms))


if __name__ == '__main__':
  main()
