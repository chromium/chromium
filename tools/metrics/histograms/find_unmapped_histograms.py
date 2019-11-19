# Copyright 2013 The Chromium Authors. All rights reserved.
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
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

import extract_histograms


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
HISTOGRAM_REGEX = re.compile(r"""
    (\w*           # Capture the whole macro name
    UMA_HISTOGRAM_ # Match the shared prefix for standard UMA histogram macros
    (\w*))         # Match the rest of the macro name, e.g. '_ENUMERATION'
    \(             # Match the opening parenthesis for the macro
    \s*            # Match any whitespace -- especially, any newlines
    ([^,)]*)       # Capture the first parameter to the macro
    [,)]           # Match the comma/paren that delineates the first parameter
    """, re.VERBOSE)
STANDARD_HISTOGRAM_SUFFIXES = frozenset(['TIMES', 'MEDIUM_TIMES', 'LONG_TIMES',
                                         'LONG_TIMES_100', 'CUSTOM_TIMES',
                                         'COUNTS', 'COUNTS_100', 'COUNTS_1000',
                                         'COUNTS_10000', 'CUSTOM_COUNTS',
                                         'MEMORY_KB', 'MEMORY_MB',
                                         'MEMORY_LARGE_MB', 'PERCENTAGE',
                                         'BOOLEAN', 'ENUMERATION',
                                         'CUSTOM_ENUMERATION'])
OTHER_STANDARD_HISTOGRAMS = frozenset(['SCOPED_UMA_HISTOGRAM_TIMER',
                                       'SCOPED_UMA_HISTOGRAM_LONG_TIMER'])
# The following suffixes are not defined in //base/metrics but the first
# argument to the macro is the full name of the histogram as a literal string.
STANDARD_LIKE_SUFFIXES = frozenset(['SCROLL_LATENCY_SHORT',
                                    'SCROLL_LATENCY_LONG',
                                    'TOUCH_TO_SCROLL_LATENCY',
                                    'LARGE_MEMORY_MB', 'MEGABYTES_LINEAR',
                                    'LINEAR', 'ALLOCATED_MEGABYTES',
                                    'CUSTOM_TIMES_MICROS',
                                    'TIME_IN_MINUTES_MONTH_RANGE',
                                    'TIMES_16H', 'MINUTES', 'MBYTES',
                                    'ASPECT_RATIO', 'LOCATION_RESPONSE_TIMES',
                                    'LOCK_TIMES', 'OOM_KILL_TIME_INTERVAL'])
OTHER_STANDARD_LIKE_HISTOGRAMS = frozenset(['SCOPED_BLINK_UMA_HISTOGRAM_TIMER'])


def RunGit(command):
  """Run a git subcommand, returning its output."""
  # On Windows, use shell=True to get PATH interpretation.
  command = ['git'] + command
  logging.info(' '.join(command))
  shell = (os.name == 'nt')
  proc = subprocess.Popen(command, shell=shell, stdout=subprocess.PIPE)
  out = proc.communicate()[0].strip()
  return out


class DirectoryNotFoundException(Exception):
  """Base class to distinguish locally defined exceptions from standard ones."""
  def __init__(self, msg):
    self.msg = msg

  def __str__(self):
    return self.msg


def keepOnlyNewlines(match_object):
  """Remove everything from a matched string except for the newline characters.
  Takes a MatchObject argument so that it can be used directly as the repl
  argument to re.sub().

  Args:
    match_object: A MatchObject referencing the string to be substituted, e.g.
    '  // My histogram\n   '

  Returns:
     The string with non-newlines removed, eg.
     '\n'
  """
  return NON_NEWLINE.sub('', match_object.group(0))


def removeComments(string):
  """Remove any comments from an expression, including leading and trailing
  whitespace. This does not correctly ignore comments embedded in strings, but
  that shouldn't matter for this script. Newlines in the removed text are
  preserved so that line numbers don't change.

  Args:
    string: The string to remove comments from, e.g.
    '  // My histogram\n   "My.Important.Counts" '

  Returns:
    The string with comments removed, e.g. '"\nMy.Important.Counts" '

  """
  return CPP_COMMENT.sub(keepOnlyNewlines, string)


def collapseAdjacentCStrings(string):
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


def logNonLiteralHistogram(filename, histogram):
  """Logs a statement warning about a non-literal histogram name found in the
  Chromium source.

  Filters out known acceptable exceptions.

  Args:
    filename: The filename for the file containing the histogram, e.g.
              'chrome/browser/memory_details.cc'
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
  if histogram == '':
    return

  # String concatenations involving macros are always constant.
  if MACRO_STRING_CONCATENATION_REGEX.match(histogram):
    return

  # TODO(isherman): This is still a little noisy... needs further filtering to
  # reduce the noise.
  logging.warning('%s contains non-literal histogram name <%s>', filename,
                  histogram)


def readChromiumHistograms():
  """Searches the Chromium source for all histogram names.

  Also prints warnings for any invocations of the UMA_HISTOGRAM_* macros with
  names that might vary during a single run of the app.

  Returns:
    A tuple of
      a set containing any found literal histogram names, and
      a set mapping histogram name to first filename:line where it was found
  """
  logging.info('Scanning Chromium source for histograms...')

  # Use git grep to find all invocations of the UMA_HISTOGRAM_* macros.
  # Examples:
  #   'path/to/foo.cc:420:  UMA_HISTOGRAM_COUNTS_100("FooGroup.FooName",'
  #   'path/to/bar.cc:632:  UMA_HISTOGRAM_ENUMERATION('
  locations = RunGit(['gs', 'UMA_HISTOGRAM']).split('\n')
  all_filenames = set(location.split(':')[0] for location in locations);
  filenames = [f for f in all_filenames
               if C_FILENAME.match(f) and not TEST_FILENAME.match(f)]

  histograms = set()
  location_map = dict()
  unknown_macros = set()
  all_suffixes = STANDARD_HISTOGRAM_SUFFIXES | STANDARD_LIKE_SUFFIXES
  all_others = OTHER_STANDARD_HISTOGRAMS | OTHER_STANDARD_LIKE_HISTOGRAMS
  for filename in filenames:
    contents = ''
    with open(filename, 'r') as f:
      contents = removeComments(f.read())

    # TODO(isherman): Look for histogram function calls like
    # base::UmaHistogramSparse() in addition to macro invocations.
    for match in HISTOGRAM_REGEX.finditer(contents):
      line_number = contents[:match.start()].count('\n') + 1
      if (match.group(2) not in all_suffixes and
          match.group(1) not in all_others):
        full_macro_name = match.group(1)
        if (full_macro_name not in unknown_macros):
          logging.warning('%s:%d: Unknown macro name: <%s>' %
                          (filename, line_number, match.group(1)))
          unknown_macros.add(full_macro_name)

        continue

      histogram = match.group(3).strip()
      histogram = collapseAdjacentCStrings(histogram)

      # Must begin and end with a quotation mark.
      if not histogram or histogram[0] != '"' or histogram[-1] != '"':
        logNonLiteralHistogram(filename, histogram)
        continue

      # Must not include any quotation marks other than at the beginning or end.
      histogram_stripped = histogram.strip('"')
      if '"' in histogram_stripped:
        logNonLiteralHistogram(filename, histogram)
        continue

      if histogram_stripped not in histograms:
        histograms.add(histogram_stripped)
        location_map[histogram_stripped] = '%s:%d' % (filename, line_number)

  return histograms, location_map


def readXmlHistograms(histograms_file_location):
  """Parses all histogram names from histograms.xml.

  Returns:
    A set cotaining the parsed histogram names.
  """
  logging.info('Reading histograms from %s...' % histograms_file_location)
  histograms = extract_histograms.ExtractHistograms(histograms_file_location)
  return set(extract_histograms.ExtractNames(histograms))


def hashHistogramName(name):
  """Computes the hash of a histogram name.

  Args:
    name: The string to hash (a histogram name).

  Returns:
    Histogram hash as a string representing a hex number (with leading 0x).
  """
  return '0x' + hashlib.md5(name).hexdigest()[:16]


def output_csv(unmapped_histograms, location_map):
  for histogram in sorted(unmapped_histograms):
    parts = location_map[histogram].split(':')
    assert len(parts) == 2
    (filename, line_number) = parts
    print('%s,%s,%s,%s' % (filename, line_number, histogram,
                           hashHistogramName(histogram)))


def output_log(unmapped_histograms, location_map, verbose):
  if len(unmapped_histograms):
    logging.info('')
    logging.info('')
    logging.info('Histograms in Chromium but not in XML files:')
    logging.info('-------------------------------------------------')
    for histogram in sorted(unmapped_histograms):
      if verbose:
        logging.info('%s: %s - %s', location_map[histogram], histogram,
                     hashHistogramName(histogram))
      else:
        logging.info('  %s - %s', histogram, hashHistogramName(histogram))
  else:
    logging.info('Success!  No unmapped histograms found.')


def main():
  # Find default paths.
  default_root = path_util.GetInputFile('/')
  default_histograms_path = path_util.GetInputFile(
      'tools/metrics/histograms/histograms.xml')
  default_extra_histograms_path = path_util.GetInputFile(
      'tools/histograms/histograms.xml')

  # Parse command line options
  parser = optparse.OptionParser()
  parser.add_option(
    '--root-directory', dest='root_directory', default=default_root,
    help='scan within DIRECTORY for histograms [optional, defaults to "%s"]' %
        default_root,
    metavar='DIRECTORY')
  parser.add_option(
    '--histograms-file', dest='histograms_file_location',
    default=default_histograms_path,
    help='read histogram definitions from FILE (relative to --root-directory) '
         '[optional, defaults to "%s"]' % default_histograms_path,
    metavar='FILE')
  parser.add_option(
    '--extra_histograms-file', dest='extra_histograms_file_location',
    default=default_extra_histograms_path,
    help='read additional histogram definitions from FILE (relative to '
         '--root-directory) [optional, defaults to "%s"]' %
         default_extra_histograms_path,
    metavar='FILE')
  parser.add_option(
      '--csv', action='store_true', dest='output_as_csv', default=False,
      help=(
          'output as csv for ease of parsing ' +
          '[optional, defaults to %default]'))
  parser.add_option(
      '--verbose', action='store_true', dest='verbose', default=False,
      help=(
          'print file position information with histograms ' +
          '[optional, defaults to %default]'))

  (options, args) = parser.parse_args()
  if args:
    parser.print_help()
    sys.exit(1)

  logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.INFO)

  try:
    os.chdir(options.root_directory)
  except EnvironmentError as e:
    logging.error("Could not change to root directory: %s", e)
    sys.exit(1)
  chromium_histograms, location_map = readChromiumHistograms()
  xml_histograms = readXmlHistograms(options.histograms_file_location)
  unmapped_histograms = chromium_histograms - xml_histograms

  if os.path.isfile(options.extra_histograms_file_location):
    xml_histograms2 = readXmlHistograms(options.extra_histograms_file_location)
    unmapped_histograms -= xml_histograms2
  else:
    logging.warning('No such file: %s', options.extra_histograms_file_location)

  if options.output_as_csv:
    output_csv(unmapped_histograms, location_map)
  else:
    output_log(unmapped_histograms, location_map, options.verbose)


if __name__ == '__main__':
  main()
