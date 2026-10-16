#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Fills in the glib version in a configuration header template.

on_update.sh checks in config.h and glibconfig.h, from meson's host
configuration checks, as templates with the version-dependent values replaced
by @...@ placeholders. This fills them in from src/meson.build, as meson
would, so the checked-in templates don't change when glib is rolled.
"""

import argparse
import re
import sys

import glib_version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--template', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()

    version = glib_version.get_version()
    major, minor, micro = (int(v) for v in version.split('.'))
    # As computed in upstream's top-level meson.build.
    interface_age = 0 if minor % 2 else micro
    binary_age = 100 * minor + micro
    values = {
        'VERSION': version,
        'GLIB_MAJOR_VERSION': major,
        'GLIB_MINOR_VERSION': minor,
        'GLIB_MICRO_VERSION': micro,
        'GLIB_INTERFACE_AGE': interface_age,
        'GLIB_BINARY_AGE': binary_age,
    }

    with open(args.template) as f:
        content = f.read()
    for key, value in values.items():
        content = content.replace(f'@{key}@', str(value))
    leftover = re.search(r'@[A-Z_]+@', content)
    if leftover:
        raise ValueError(f'Unknown placeholder {leftover.group(0)} in '
                         f'{args.template}')

    with open(args.output, 'w') as f:
        f.write(content)
    return 0


if __name__ == '__main__':
    sys.exit(main())
