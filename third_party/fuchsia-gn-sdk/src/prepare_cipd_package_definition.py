#!/usr/bin/env python3.8
#
# Copyright 2020 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Prepares a directory and a corresponding package definition which can be
used to create a CIPD package."""

import argparse
import os
import sys


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--pkg-name', type=str, required=True, help='Name of the CIPD package.')
    parser.add_argument(
        '--description',
        type=str,
        required=True,
        help='Description of the CIPD package.')
    parser.add_argument(
        '--pkg-root', type=str, required=True, help='Path to the package root.')
    parser.add_argument(
        '--install-mode',
        type=str,
        choices=['copy', 'symlink'],
        required=True,
        help='CIPD install mode.')
    parser.add_argument(
        '--pkg-def',
        type=str,
        required=True,
        help='Path to the output package definition.')
    parser.add_argument(
        '--depfile', type=str, required=True, help='Path to the depfile.')
    parser.add_argument(
        '--files',
        nargs='+',
        default=(),
        help='Files relative to --pkg-root to include in the '
        'package definition.')
    parser.add_argument(
        '--directories',
        nargs='+',
        default=(),
        help='Directories relative to --pkg-root to include in '
        'the package definition.')
    parser.add_argument(
        '--copy-files',
        nargs='+',
        default=(),
        help='Files to be copied into --pkg-root and included '
        'in the package definition.')
    args = parser.parse_args(args)

    pkg_def = {
        'package': args.pkg_name,
        'description': args.description,
        'root': args.pkg_root,
        'install_mode': args.install_mode,
        'data': [],
    }

    deps = set()
    # Include files and directories in the package definition.
    for relpath in args.files:
        pkg_def['data'].append({'file': relpath})
        deps.add(os.path.join(args.pkg_root, relpath))
    for relpath in args.directories:
        pkg_def['data'].append({'dir': relpath})
        deps.add(os.path.join(args.pkg_root, relpath))

    # Copy files into the root and include in the package definition.
    if args.copy_files:
        for filepath in args.copy_files:
            basename = os.path.basename(filepath)
            dest = os.path.join(args.pkg_root, basename)
            if not os.path.exists(dest):
                os.link(filepath, dest)
            pkg_def['data'].append({'file': basename})
            deps.add(dest)

    with open(args.pkg_def, 'w') as f:
        print_yaml(pkg_def, f)
    with open(args.depfile, 'w') as f:
        f.writelines('%s: %s\n' % (args.pkg_def, ' '.join(sorted(deps))))

    return 0


def print_yaml_item(item, out):
    keys = list(item.keys())
    keys.sort()
    first = True
    for key in keys:
        if first:
            out.write('- %s: %s\n' % (key, item[key]))
            first = False
        else:
            out.write('  %s: %s\n' % (key, item[key]))


def print_yaml(o, out):
    keys = list(o.keys())
    keys.sort()
    for k in keys:
        val = o[k]
        if isinstance(val, (list, set)):
            out.write('%s:\n' % k)
            for item in val:
                print_yaml_item(item, out)
        elif isinstance(val, str):
            out.write('%s: %s\n' % (k, val))


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
