#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a version header from a meson version-header template.

Fills in the @<PREFIX>_VERSION@, @<PREFIX>_VERSION_MAJOR@,
@<PREFIX>_VERSION_MINOR@ and @<PREFIX>_VERSION_MICRO@ placeholders using the
project version from meson.build, as meson's configure_file() would. This
keeps the header in sync with the DEPS revision without a checked-in copy.
"""

import argparse
import re
import shutil
import sys


def get_project_version(meson_build):
    with open(meson_build) as f:
        match = re.search(r"project\(.*?version\s*:\s*'([\d.]+)'", f.read(),
                          re.DOTALL)
    if not match:
        raise ValueError(f'No project version found in {meson_build}')
    return match.group(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--meson-build', required=True)
    parser.add_argument('--template', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--prefix', required=True)
    parser.add_argument(
        '--copy',
        nargs=2,
        action='append',
        default=[],
        metavar=('SRC', 'DST'),
        help='Also copy SRC to DST, e.g. to mirror meson\'s build layout next '
        'to the generated header.')
    args = parser.parse_args()

    version = get_project_version(args.meson_build)
    major, minor, micro = version.split('.')
    values = {
        f'{args.prefix}_VERSION': version,
        f'{args.prefix}_VERSION_MAJOR': str(int(major)),
        f'{args.prefix}_VERSION_MINOR': str(int(minor)),
        f'{args.prefix}_VERSION_MICRO': str(int(micro)),
    }

    with open(args.template) as f:
        content = f.read()
    for key, value in values.items():
        content = content.replace(f'@{key}@', value)
    leftover = re.search(r'@[A-Z_]+@', content)
    if leftover:
        raise ValueError(f'Unsubstituted placeholder {leftover.group(0)} in '
                         f'{args.template}')

    with open(args.output, 'w') as f:
        f.write(content)
    for src, dst in args.copy:
        shutil.copyfile(src, dst)
    return 0


if __name__ == '__main__':
    sys.exit(main())
