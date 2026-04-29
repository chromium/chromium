#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--gettext-domain', default='')
    # Mutter specific
    parser.add_argument('--xwayland-grab-default-access-rules', default='')
    args = parser.parse_args()

    with open(args.input, 'r', encoding='utf-8') as f:
        content = f.read()

    # Perform common replacements
    content = content.replace('@GETTEXT_DOMAIN@', args.gettext_domain)

    # Mutter specific replacements
    content = content.replace('@XWAYLAND_GRAB_DEFAULT_ACCESS_RULES@',
                              args.xwayland_grab_default_access_rules)

    with open(args.output, 'w', encoding='utf-8') as f:
        f.write(content)
    return 0


if __name__ == '__main__':
    sys.exit(main())
