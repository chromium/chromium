# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input')
    parser.add_argument('output')
    args = parser.parse_args()

    with open(args.input, 'rt', encoding='utf-8') as input_file:
        with open(args.output, 'wt', encoding='utf-8') as output_file:
            output_file.write(
                input_file.read().replace('tflite.proto', 'tflite'))


if __name__ == '__main__':
    main()
