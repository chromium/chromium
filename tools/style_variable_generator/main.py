#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys
import os

sys.path += [os.path.dirname(os.path.dirname(__file__))]

from style_variable_generator.css_generator import CSSStyleGenerator
from style_variable_generator.ts_generator import TSStyleGenerator
from style_variable_generator.proto_generator import ProtoStyleGenerator, ProtoJSONStyleGenerator
from style_variable_generator.views_generator import ViewsCCStyleGenerator, ViewsHStyleGenerator
from style_variable_generator.base_generator import Modes


def parseGeneratorOptionList(options):
    result = {}
    if options is None:
        return result
    for key_value_pair in options:
        key, value = key_value_pair.split("=", 1)
        key = key.strip()
        result[key] = value
    return result


def main():
    parser = argparse.ArgumentParser(
        description='Generate style variables from JSON5 color file.')

    generators = [
        CSSStyleGenerator, ViewsCCStyleGenerator, ViewsHStyleGenerator,
        ProtoStyleGenerator, ProtoJSONStyleGenerator, TSStyleGenerator
    ]

    parser.add_argument('--generator',
                        choices=[g.GetName() for g in generators],
                        required=True,
                        help='type of file to generate')
    parser.add_argument('--generate-single-mode',
                        choices=Modes.ALL,
                        help='generates output for a single mode')
    parser.add_argument('--out-file', help='file to write output to')
    parser.add_argument("--generator-option",
                        metavar="KEY=VALUE",
                        action="append",
                        help="Set a option specific to the selected generator"
                        "via a key value pair. See the README.md for a"
                        "full list of generator specific options.")
    parser.add_argument('targets', nargs='+', help='source json5 color files')

    args = parser.parse_args()

    for g in generators:
        if args.generator == g.GetName():
            style_generator = g()

    style_generator.AddJSONFilesToModel(args.targets)

    style_generator.generate_single_mode = args.generate_single_mode
    style_generator.out_file_path = args.out_file
    style_generator.generator_options = parseGeneratorOptionList(
        args.generator_option)

    if args.out_file:
        with open(args.out_file, 'w') as f:
            f.write(style_generator.Render())
    else:
        print(style_generator.Render())

    return 0


if __name__ == '__main__':
    sys.exit(main())
