#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Library and tool to expand command lines that mention thin archives
# into command lines that mention the contained object files.

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import argparse
import sys

from goma_link import GomaLinkWindows
from goma_ld import GomaLinkUnix


def main(argv):
  ap = argparse.ArgumentParser(
      description=('Expand command lines that mention thin archives into'
                   ' command lines that mention the contained object files.'),
      usage='%(prog)s [options] -- command line')
  ap.add_argument('-o', '--output',
                  help=('Write new command line to named file'
                        ' instead of standard output.'))
  ap.add_argument('-p', '--linker-prefix',
                  help='String to prefix linker flags with.',
                  default='')
  ap.add_argument('cmdline',
                  nargs=argparse.REMAINDER,
                  help='Command line to expand. Should be preceeded by \'--\'.')
  args = ap.parse_args(argv[1:])
  if not args.cmdline:
    ap.print_help(sys.stderr)
    return 1

  cmdline = args.cmdline
  if cmdline[0] == '--':
    cmdline = cmdline[1:]
  linker_prefix = args.linker_prefix

  if linker_prefix == '-Wl,':
    linker = GomaLinkUnix()
  else:
    linker = GomaLinkWindows()

  rsp_expanded = list(linker.expand_args_rsps(cmdline))
  expanded_args = list(linker.expand_thin_archives(rsp_expanded))

  if args.output:
    output = open(args.output, 'w')
  else:
    output = sys.stdout
  for arg in expanded_args:
    output.write('%s\n' % (arg,))
  if args.output:
    output.close()
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
