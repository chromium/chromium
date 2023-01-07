# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a file describing the js_library to be used by js_binary action.

This script takes in a list of sources and dependencies as described by a
js_library action.  It creates a file listing the sources and dependencies
that can later be used by a js_binary action to compile the javascript.
"""

from argparse import ArgumentParser


def main():
  parser = ArgumentParser()
  parser.add_argument('-s', '--sources', nargs='*', default=[],
                      help='List of js source files')
  parser.add_argument('-e', '--externs', nargs='*', default=[],
                      help='List of js source files')
  parser.add_argument('-o', '--output', help='Write list to output')
  parser.add_argument('-d', '--deps', nargs='*', default=[],
                      help='List of js_library dependencies')
  args = parser.parse_args()

  with open(args.output, 'w') as out:
    out.write('sources:\n')
    for s in args.sources:
      out.write('%s\n' % s)
    out.write('deps:\n')
    for d in args.deps:
      out.write('%s\n' % d)
    out.write('externs:\n')
    for e in args.externs:
      out.write('%s\n' % e)

if __name__ == '__main__':
  main()
