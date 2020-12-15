# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
from css_generator import CSSStyleGenerator
from proto_generator import ProtoStyleGenerator, ProtoJSONStyleGenerator
from views_generator import ViewsStyleGenerator
from base_generator import Modes


def main():
    parser = argparse.ArgumentParser(
        description='Generate style variables from JSON5 color file.')

    generators = [
        CSSStyleGenerator, ViewsStyleGenerator, ProtoStyleGenerator,
        ProtoJSONStyleGenerator
    ]

    parser.add_argument('--generator',
                        choices=[g.GetName() for g in generators],
                        required=True,
                        help='type of file to generate')
    parser.add_argument('--generate-single-mode',
                        choices=Modes.ALL,
                        help='generates output for a single mode')
    parser.add_argument('--out-file', help='file to write output to')
    parser.add_argument('targets', nargs='+', help='source json5 color files')

    args = parser.parse_args()

    for g in generators:
        if args.generator == g.GetName():
            style_generator = g()

    for t in args.targets:
        style_generator.AddJSONFileToModel(t)

    style_generator.generate_single_mode = args.generate_single_mode
    style_generator.out_file_path = args.out_file

    if args.out_file:
        with open(args.out_file, 'w') as f:
            f.write(style_generator.Render())
    else:
        print(style_generator.Render())

    return 0


if __name__ == '__main__':
    sys.exit(main())
