#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Fills in the mutter version in the config.h template.

on_update.sh checks in config.h, from meson's host configuration checks, as a
template with the mutter version replaced by @VERSION@. This fills it in from
src/meson.build, so the checked-in template doesn't change when mutter is
rolled.
"""

import argparse
import re
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--meson-build', required=True)
    parser.add_argument('--template', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()

    with open(args.meson_build) as f:
        match = re.search(r"project\(.*?version\s*:\s*'([\w.]+)'", f.read(),
                          re.DOTALL)
    if not match:
        raise ValueError(f'No project version found in {args.meson_build}')

    with open(args.template) as f:
        content = f.read()
    content = content.replace('@VERSION@', match.group(1))
    leftover = re.search(r'@[A-Z_]+@', content)
    if leftover:
        raise ValueError(f'Unknown placeholder {leftover.group(0)} in '
                         f'{args.template}')

    with open(args.output, 'w') as f:
        f.write(content)
    return 0


if __name__ == '__main__':
    sys.exit(main())
