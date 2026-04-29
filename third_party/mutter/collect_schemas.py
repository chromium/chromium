#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-dir', required=True)
    parser.add_argument('--stamp', required=True)
    parser.add_argument('--schemas', nargs='+', required=True)
    args = parser.parse_args()

    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)

    for schema in args.schemas:
        # Copy schema file to output directory
        dest = os.path.join(args.output_dir, os.path.basename(schema))
        # Remove destination if it exists to avoid issues
        if os.path.exists(dest):
            os.remove(dest)
        shutil.copy2(schema, dest)

    # Write stamp file
    with open(args.stamp, 'w', encoding='utf-8') as f:
        f.write('done')

    return 0


if __name__ == '__main__':
    sys.exit(main())
