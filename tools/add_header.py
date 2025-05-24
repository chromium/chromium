#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper for adding or removing an include to/from source file(s).

clang-format already provides header sorting functionality; however, the
functionality is limited to sorting headers within a block of headers surrounded
by blank lines (these are a heuristic to avoid clang breaking ordering for
headers sensitive to inclusion order, e.g. <windows.h>).

As a result, inserting a new header is a bit more complex than simply inserting
the new header at the top and running clang-format.

This script implements additional logic to:
- classify different blocks of headers by type (C system, C++ system, user)
- find the appropriate insertion point for the new header
- creating a new header block if necessary

As a bonus, it does *also* sort the includes, though any sorting disagreements
with clang-format should be resolved in favor of clang-format.

It also supports removing a header with option `--remove`.

Usage:
tools/add_header.py --header '<utility>' foo/bar.cc foo/baz.cc foo/baz.h
tools/add_header.py --header '<vector>' --remove foo/bar.cc foo/baz.cc foo/baz.h
"""

import argparse
import difflib
import os.path
import re
import sys

# The specific values of these constants are also used as a sort key for
# ordering different header types in the correct relative order.
_HEADER_TYPE_C_SYSTEM = 0
_HEADER_TYPE_CXX_SYSTEM = 1
_HEADER_TYPE_USER = 2
_HEADER_TYPE_INVALID = -1


def ClassifyHeader(decorated_name):
  if IsCSystemHeader(decorated_name):
    return _HEADER_TYPE_C_SYSTEM
  elif IsCXXSystemHeader(decorated_name):
    return _HEADER_TYPE_CXX_SYSTEM
  elif IsUserHeader(decorated_name):
    return _HEADER_TYPE_USER
  else:
    return _HEADER_TYPE_INVALID


def UndecoratedName(decorated_name):
  """Returns the undecorated version of decorated_name by removing "" or <>."""
  assert IsSystemHeader(decorated_name) or IsUserHeader(decorated_name)
  return decorated_name[1:-1]


def IsSystemHeader(decorated_name):
  """Returns true if decorated_name looks like a system header."""
  return decorated_name[0] == '<' and decorated_name[-1] == '>'


def IsCSystemHeader(decorated_name):
  """Returns true if decoraed_name looks like a C system header."""
  return IsSystemHeader(decorated_name) and UndecoratedName(
      decorated_name).endswith('.h')


def IsCXXSystemHeader(decorated_name):
  """Returns true if decoraed_name looks like a C++ system header."""
  return IsSystemHeader(
      decorated_name) and not UndecoratedName(decorated_name).endswith('.h')


def IsUserHeader(decorated_name):
  """Returns true if decoraed_name looks like a user header."""
  return decorated_name[0] == '"' and decorated_name[-1] == '"'


_EMPTY_LINE_RE = re.compile(r'\s*$')
_COMMENT_RE = re.compile(r'\s*//(.*)$')
_INCLUDE_RE = re.compile(
    r'\s*#(import|include)\s+([<"].+?[">])\s*?(?://(.*))?$')


def FindIncludes(lines):
  """Finds the block of #includes, assuming Google+Chrome C++ style source.

  Note that this doesn't simply return a slice of the input lines, because
  having the actual indices simplifies things when generatingn the updated
  source text.

  Args:
    lines: The source text split into lines.

  Returns:
    A tuple of begin, end indices that can be used to slice the input lines to
        contain the includes to process. Returns -1, -1 if no such block of
        input lines could be found.
  """
  begin = end = -1
  for idx, line in enumerate(lines):
    # Skip over any initial comments (e.g. the copyright boilerplate) or empty
    # lines.
    # TODO(dcheng): This means that any preamble comment associated with the
    # first header will be dropped. So far, this hasn't broken anything, but
    # maybe this needs to be more clever.
    # TODO(dcheng): #define and #undef should probably also be allowed.
    if _EMPTY_LINE_RE.match(line) or _COMMENT_RE.match(line):
      continue
    m = _INCLUDE_RE.match(line)
    if not m:
      if begin < 0:
        # No match, but no #includes have been seen yet. Keep scanning for the
        # first #include.
        continue
      # Give up, it's something weird that probably requires manual
      # intervention.
      break

    if begin < 0:
      begin = idx
    end = idx + 1
  return begin, end


class Include(object):
  """Represents an #include/#import and any interesting metadata for it.

  Attributes:
    decorated_name: The name of the header file, decorated with <> for system
      headers or "" for user headers.

    directive: 'include' or 'import'
      TODO(dcheng): In the future, this may need to support C++ modules.

    preamble: Any comment lines that precede this include line, e.g.:

        // This is a preamble comment
        // for a header file.
        #include <windows.h>

      would have a preamble of

        ['// This is a preamble comment', '// for a header file.'].

    inline_comment: Any comment that comes after the #include on the same line,
      e.g.

        #include <windows.h>  // For CreateWindowExW()

      would be parsed with an inline comment of ' For CreateWindowExW'.

    header_type: The header type corresponding to decorated_name as determined
      by ClassifyHeader().

    is_primary_header: True if this is the primary related header of a C++
      implementation file. Any primary header will be sorted to the top in its
      own separate block.
  """

  def __init__(self, decorated_name, directive, preamble, inline_comment):
    self.decorated_name = decorated_name
    assert directive == 'include' or directive == 'import'
    self.directive = directive
    self.preamble = preamble
    self.inline_comment = inline_comment
    self.header_type = ClassifyHeader(decorated_name)
    assert self.header_type != _HEADER_TYPE_INVALID
    self.is_primary_header = False

  def __repr__(self):
    return str((self.decorated_name, self.directive, self.preamble,
                self.inline_comment, self.header_type, self.is_primary_header))

  def ShouldInsertNewline(self, previous_include):
    # Per the Google C++ style guide, different blocks of headers should be
    # separated by an empty line.
    return (self.is_primary_header != previous_include.is_primary_header
            or self.header_type != previous_include.header_type)

  def ToSource(self):
    """Generates a C++ source representation of this include."""
    source = []
    source.extend(self.preamble)
    include_line = '#%s %s' % (self.directive, self.decorated_name)
    if self.inline_comment:
      include_line = include_line + '  //' + self.inline_comment
    source.append(include_line)
    return [line.rstrip() for line in source]


def ParseIncludes(lines):
  """Parses lines into a list of Include objects. Returns None on failure.

  Args:
    lines: A list of strings representing C++ source text.

  Returns:
    A list of Include objects representing the parsed input lines, or None if
    the input lines could not be parsed.
  """
  includes = []
  preamble = []
  for line in lines:
    if _EMPTY_LINE_RE.match(line):
      if preamble:
        # preamble contents are flushed when an #include directive is matched.
        # If preamble is non-empty, that means there is a preamble separated
        # from its #include directive by at least one newline. Just give up,
        # since the sorter has no idea how to preserve structure in this case.
        return None
      continue
    m = _INCLUDE_RE.match(line)
    if not m:
      preamble.append(line)
      continue
    includes.append(Include(m.group(2), m.group(1), preamble, m.group(3)))
    preamble = []
  # In theory, the caller should never pass a list of lines with a dangling
  # preamble. But there's a test case that exercises this, and just in case it
  # actually happens, fail a bit more gracefully.
  if preamble:
    return None
  return includes


def _DecomposePath(filename):
  """Decomposes a filename into a list of directories and the basename.

  Args:
    filename: A filename!

  Returns:
    A tuple of a list of directories and a string basename.
  """
  dirs = []
  dirname, basename = os.path.split(filename)
  while dirname:
    dirname, last = os.path.split(dirname)
    dirs.append(last)
  dirs.reverse()
  # Remove the extension from the basename.
  basename = os.path.splitext(basename)[0]
  return dirs, basename


_PLATFORM_SUFFIX = (
    r'(?:_(?:android|aura|chromeos|fuchsia|ios|linux|mac|ozone|posix|win|x11))?'
)
_TEST_SUFFIX = r'(?:_(?:browser|interactive_ui|perf|ui|unit)?test)?'


def MarkPrimaryInclude(includes, filename):
  """Finds the primary header in includes and marks it as such.

  Per the style guide, if moo.cc's main purpose is to implement or test the
  functionality in moo.h, moo.h should be ordered first in the includes.

  Args:
    includes: A list of Include objects.
    filename: The filename to use as the basis for finding the primary header.
  """
  # Header files never have a primary include.
  if filename.endswith('.h'):
    return

  # First pass. Looking for exact match primary header.
  exact_match_primary_header = f'{os.path.splitext(filename)[0]}.h'
  for include in includes:
    if IsUserHeader(include.decorated_name) and UndecoratedName(
        include.decorated_name) == exact_match_primary_header:
      include.is_primary_header = True
      return

  basis = _DecomposePath(filename)

  # Second pass. The list of includes is searched in reverse order of length.
  # Even though matching is fuzzy, moo_posix.h should take precedence over moo.h
  # when considering moo_posix.cc.
  includes.sort(key=lambda i: -len(i.decorated_name))
  for include in includes:
    if include.header_type != _HEADER_TYPE_USER:
      continue
    to_test = _DecomposePath(UndecoratedName(include.decorated_name))

    # If the basename to test is longer than the basis, just skip it and
    # continue. moo.c should never match against moo_posix.h.
    if len(to_test[1]) > len(basis[1]):
      continue

    # The basename in the two paths being compared need to fuzzily match.
    # This allows for situations where moo_posix.cc implements the interfaces
    # defined in moo.h.
    escaped_basename = re.escape(to_test[1])
    if not (re.match(escaped_basename + _PLATFORM_SUFFIX + _TEST_SUFFIX + '$',
                     basis[1]) or
            re.match(escaped_basename + _TEST_SUFFIX + _PLATFORM_SUFFIX + '$',
                     basis[1])):
      continue

    # The topmost directory name must match, and the rest of the directory path
    # should be 'substantially similar'.
    s = difflib.SequenceMatcher(None, to_test[0], basis[0])
    first_matched = False
    total_matched = 0
    for match in s.get_matching_blocks():
      if total_matched == 0 and match.a == 0 and match.b == 0:
        first_matched = True
      total_matched += match.size

    if not first_matched:
      continue

    # 'Substantially similar' is defined to be:
    # - no more than two differences
    # - at least one match besides the topmost directory
    total_differences = abs(total_matched -
                            len(to_test[0])) + abs(total_matched -
                                                   len(basis[0]))
    # Note: total_differences != 0 is mainly intended to allow more succinct
    # tests (otherwise tests with just a basename would always trip the
    # total_matched < 2 check).
    if total_differences != 0 and (total_differences > 2 or total_matched < 2):
      continue

    include.is_primary_header = True
    return


def SerializeIncludes(includes):
  """Turns includes back into the corresponding C++ source text.

  Args:
    includes: a list of Include objects.

  Returns:
    A list of strings representing C++ source text.
  """
  source = []

  # LINT.IfChange(winheader)
  # Headers that are sorted above others to prevent inclusion order issues.
  # NOTE: The order of these headers is the sort key and will be the order in
  # the output file. It should be set to match whatever clang-format will do.
  special_headers = [
      # Listed first because it must be before initguid.h in the block below.
      '<objbase.h>',

      # Alphabetized block that don't matter relative to each other, but need to
      # be included before any instance of the listed other header. These other
      # listed headers are non-exhaustive examples.
      '<atlbase.h>',      # atlapp.h
      '<initguid.h>',     # emi.h
      '<mmdeviceapi.h>',  # functiondiscoverykeys_devpkey.h
      '<ocidl.h>',        # commdlg.h
      '<ole2.h>',         # intshcut.h
      '<shobjidl.h>',     # propkey.h
      '<tchar.h>',        # tpcshrd.h
      '<unknwn.h>',       # intshcut.h
      '<windows.h>',      # hidclass.h, memoryapi.h, ncrypt.h, shellapi.h,
                          # versionhelpers.h, winbase.h, etc.
      '<winsock2.h>',     # ws2tcpip.h
      '<winternl.h>',     # ntsecapi.h; also needs `#define _NTDEF_`
      '<ws2tcpip.h>',     # iphlpapi.h
  ]

  # LINT.ThenChange(/.clang-format:winheader)

  # Ensure that headers are sorted as follows:
  #
  # 1. The primary header, if any, appears first.
  # 2. All headers of the same type (e.g. C system, C++ system headers, et
  #    cetera) are grouped contiguously.
  # 3. Any special sorting rules needed within each group for satisfying
  #    platform header idiosyncrasies. In practice, this only applies to C
  #    system headers.
  # 4. The remaining headers without special sorting rules are sorted
  #    lexicographically.
  #
  # The for loop below that outputs the actual source text depends on #2 above
  # to insert newlines between different groups of headers.
  def SortKey(include):
    def SpecialSortKey(include):
      lower_name = include.decorated_name.lower()
      for i in range(len(special_headers)):
        if special_headers[i] == lower_name:
          return i
      return len(special_headers)

    return (not include.is_primary_header, include.header_type,
            SpecialSortKey(include), include.decorated_name)

  includes.sort(key=SortKey)

  # Assume there's always at least one include.
  previous_include = None
  for include in includes:
    if previous_include and include.ShouldInsertNewline(previous_include):
      source.append('')
    source.extend(include.ToSource())
    previous_include = include
  return source


def AddHeaderToSource(filename, source, decorated_name, remove=False):
  """Adds or removes the specified header into/from the source text, if needed.

  Args:
    filename: The name of the source file.
    source: A string containing the contents of the source file.
    decorated_name: The decorated name of the header to add or remove.
    remove: If true, remove instead of adding.

  Returns:
    None if no changes are needed or the modified source text otherwise.
  """
  lines = source.splitlines()
  begin, end = FindIncludes(lines)

  # No #includes in this file. Just give up.
  # TODO(dcheng): Be more clever and insert it after the file-level comment or
  # include guard as appropriate.
  if begin < 0:
    print(f'Skipping {filename}: unable to find includes!')
    return None

  includes = ParseIncludes(lines[begin:end])
  if not includes:
    print(f'Skipping {filename}: unable to parse includes!')
    return None

  if remove:
    for i in includes:
      if decorated_name == i.decorated_name:
        includes.remove(i)
        break
    else:
      print(f'Skipping {filename}: unable to find {decorated_name}')
      return None
  else:
    if decorated_name in [i.decorated_name for i in includes]:
      # Nothing to do.
      print(f'Skipping {filename}: no changes required!')
      return None
    else:
      includes.append(Include(decorated_name, 'include', [], None))

  MarkPrimaryInclude(includes, filename)

  lines[begin:end] = SerializeIncludes(includes)
  lines.append('')  # To avoid eating the newline at the end of the file.
  return '\n'.join(lines)


def main():
  parser = argparse.ArgumentParser(
      description='Mass add (or remove) a new header into a bunch of files.')
  parser.add_argument(
      '--header',
      help='The decorated filename of the header to insert (e.g. "a" or <a>)',
      required=True)
  parser.add_argument('--remove',
                      help='Remove the header file instead of adding it',
                      action='store_true')
  parser.add_argument('files', nargs='+')
  args = parser.parse_args()
  if ClassifyHeader(args.header) == _HEADER_TYPE_INVALID:
    print('--header argument must be a decorated filename, e.g.')
    print('  --header "<utility>"')
    print('or')
    print('  --header \'"moo.h"\'')
    return 1
  operation = 'Removing' if args.remove else 'Inserting'
  print(f'{operation} #include {args.header}...')
  for filename in args.files:
    with open(filename, 'r') as f:
      new_source = AddHeaderToSource(os.path.normpath(filename), f.read(),
                                     args.header, args.remove)
    if not new_source:
      continue
    with open(filename, 'w', newline='\n') as f:
      f.write(new_source)


if __name__ == '__main__':
  sys.exit(main())
