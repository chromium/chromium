#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints a report of symbols stripped by the linker due to being unused.

To use, build with these linker flags:
  -Wl,--gc-sections
  -Wl,--print-gc-sections
the first one is the default in Release; search build/common.gypi for it
and to see where to add the other.

Then build, saving the output into a file:
  make chrome 2>&1 | tee buildlog
and run this script on it:
  ./tools/unused-symbols-report.py buildlog > report.html
"""

from __future__ import print_function

import cgi
import optparse
import os
import re
import subprocess
import sys

cppfilt_proc = None
def Demangle(sym):
  """Demangle a C++ symbol by passing it through c++filt."""
  global cppfilt_proc
  if cppfilt_proc is None:
    cppfilt_proc = subprocess.Popen(['c++filt'], stdin=subprocess.PIPE,
                                    stdout=subprocess.PIPE)
  print(sym, file=cppfilt_proc.stdin)
  return cppfilt_proc.stdout.readline().strip()


def Unyuck(sym):
  """Attempt to prettify a C++ symbol by some basic heuristics."""
  sym = sym.replace('std::basic_string<char, std::char_traits<char>, '
                    'std::allocator<char> >', 'std::string')
  sym = sym.replace('std::basic_string<wchar_t, std::char_traits<wchar_t>, '
                    'std::allocator<wchar_t> >', 'std::wstring')
  sym = sym.replace('std::basic_string<unsigned short, '
                    'base::string16_internals::'
                    'string16_char_traits, '
                    'std::allocator<unsigned short> >', 'string16')
  sym = re.sub(r', std::allocator<\S+\s+>', '', sym)
  return sym


def Parse(input, skip_paths=None, only_paths=None):
  """Parse the --print-gc-sections build output.

  Args:
    input: iterable over the lines of the build output

  Yields:
    (target name, path to .o file, demangled symbol)
  """
  symbol_re = re.compile(r"'\.text\.(\S+)' in file '(\S+)'$")
  path_re = re.compile(r"^out/[^/]+/[^/]+/([^/]+)/(.*)$")
  for line in input:
    match = symbol_re.search(line)
    if not match:
      continue
    symbol, path = match.groups()
    symbol = Unyuck(Demangle(symbol))
    path = os.path.normpath(path)
    if skip_paths and skip_paths in path:
      continue
    if only_paths and only_paths not in path:
      continue
    match = path_re.match(path)
    if not match:
      print("Skipping weird path", path, file=sys.stderr)
      continue
    target, path = match.groups()
    yield target, path, symbol


# HTML header for our output page.
TEMPLATE_HEADER = """<!DOCTYPE html>
<head>
<style>
body {
  font-family: sans-serif;
  font-size: 0.8em;
}
h1, h2 {
  font-weight: normal;
  margin: 0.5em 0;
}
h2 {
  margin-top: 1em;
}
tr:hover {
  background: #eee;
}
.permalink {
  padding-left: 1ex;
  font-size: 80%;
  text-decoration: none;
  color: #ccc;
}
.symbol {
  font-family: WebKitWorkAround, monospace;
  margin-left: 4ex;
  text-indent: -4ex;
  padding: 0.5ex 1ex;
}
.file {
  padding: 0.5ex 1ex;
  padding-left: 2ex;
  font-family: WebKitWorkAround, monospace;
  font-size: 90%;
  color: #777;
}
</style>
</head>
<body>
<h1>chrome symbols deleted at link time</h1>
"""


def Output(iter):
  """Print HTML given an iterable of (target, path, symbol) tuples."""
  targets = {}
  for target, path, symbol in iter:
    entries = targets.setdefault(target, [])
    entries.append((symbol, path))

  print(TEMPLATE_HEADER)
  print("<p>jump to target:")
  print("<select onchange='document.location.hash = this.value'>")
  for target in sorted(targets.keys()):
    print("<option>%s</option>" % target)
  print("</select></p>")

  for target in sorted(targets.keys()):
    print("<h2>%s" % target)
    print("<a class=permalink href='#%s' name='%s'>#</a>" % (target, target))
    print("</h2>")
    print("<table width=100% cellspacing=0>")
    for symbol, path in sorted(targets[target]):
      htmlsymbol = cgi.escape(symbol).replace('::', '::<wbr>')
      print("<tr><td><div class=symbol>%s</div></td>" % htmlsymbol)
      print("<td valign=top><div class=file>%s</div></td></tr>" % path)
    print("</table>")


def main():
  parser = optparse.OptionParser(usage='%prog [options] buildoutput\n\n' +
                                 __doc__)
  parser.add_option("--skip-paths", metavar="STR", default="third_party",
                    help="skip paths matching STR [default=%default]")
  parser.add_option("--only-paths", metavar="STR",
                    help="only include paths matching STR [default=%default]")
  opts, args = parser.parse_args()

  if len(args) < 1:
    parser.print_help()
    sys.exit(1)

  iter = Parse(open(args[0]),
               skip_paths=opts.skip_paths,
               only_paths=opts.only_paths)
  Output(iter)


if __name__ == '__main__':
  main()
