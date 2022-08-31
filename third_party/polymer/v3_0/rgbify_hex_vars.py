# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


_VAR_HEX_REG = r'(\s*--)([a-zA-Z0-9_-]+\s*)(:\s*)#([a-fA-F0-9]+)(\s*;\s*)'


def Rgbify(content, prefix='', replace=False):
  lines = content.splitlines()
  rgb_version = []

  for line in lines:
    match = re.match(_VAR_HEX_REG, line)
    if not match or not match.group(2).startswith(prefix):
      rgb_version.append(line)
      continue

    before, name, during, hex, after = match.groups()
    r, g, b = int(hex[0:2], 16), int(hex[2:4], 16), int(hex[4:6], 16)
    rgb = '%d, %d, %d' % (r, g, b)

    to_add = line
    if replace:
      use_rgb = 'rgb(var(--%s-rgb))' % name
      to_add = ''.join([before, name, during, use_rgb, after])

    rgb_var = [before, '%s-rgb' % name, during, rgb, after]
    if replace:
      # Leave the original #hex as a comment for searchability.
      rgb_var.append('  /* #%s */' % hex.lower())

    rgb_version += [''.join(rgb_var), to_add]

  return '\n'.join(rgb_version)


def RgbifyFileInPlace(path, **kwargs):
  rgbified = Rgbify(open(path, 'r').read(), **kwargs)
  with open(path, 'w') as path_file:
    path_file.write(rgbified)


if __name__ == '__main__':
  import argparse
  import sys
  parser = argparse.ArgumentParser('Add an -rgb equivalent to --var: #hex;')
  parser.add_argument('--filter-prefix', type=str, default='',
      help='Only affect --vars that start with this string')
  parser.add_argument('paths', nargs='+', help='File path to add -rgb vars to')
  parser.add_argument('--replace', action='store_true', default=False,
      help='Replace --var: #hex; with --var: rgb(var(--var-rgb));')
  opts = parser.parse_args(sys.argv[1:])

  for path in opts.paths:
    RgbifyFileInPlace(path, prefix=opts.filter_prefix, replace=opts.replace)
