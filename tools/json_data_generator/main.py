#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

sys.path += [os.path.dirname(os.path.dirname(__file__))]

from json_data_generator.generator import JSONDataGenerator
from json_data_generator.util import (GetFileNameWithoutExtensionFromPath,
                                      JoinPath)


def main():
    parser = argparse.ArgumentParser(
        description='Generate data from JSON5 file.')

    parser.add_argument('--templates',
                        nargs='+',
                        help="Jinja template files (*.jinja)")
    parser.add_argument(
        '--template-helper',
        help='additional python file to provide custom Jinja globals/filters')
    parser.add_argument('--out-dir', help='directory to write output to')
    parser.add_argument('--sources', nargs='+', help='source json5 data files')

    args = parser.parse_args()

    generator = JSONDataGenerator(args.out_dir)
    generator.AddJSONFilesToModel(args.sources)
    generator.out_dir = args.out_dir

    os.makedirs(args.out_dir, exist_ok=True)

    for template_path in args.templates:
        out_file_path = JoinPath(
            args.out_dir, GetFileNameWithoutExtensionFromPath(template_path))

        with open(out_file_path, 'w') as f:
            f.write(
                generator.RenderTemplate(template_path, args.template_helper))

    return 0


if __name__ == '__main__':
    sys.exit(main())
