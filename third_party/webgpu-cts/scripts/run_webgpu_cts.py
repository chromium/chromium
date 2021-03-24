# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys

# This script is run via //third_party/blink/tools/run_webgpu_cts.py which
# adds blinkpy to the Python path.
from blinkpy.web_tests import run_web_tests


def main(args, stderr):
    parser = argparse.ArgumentParser(
        description=
        'Performs additional setup for running the WebGPU CTS, '
        'then forward arguments to run_web_tests.py.'
    )
    parser.add_argument('--webgpu-cts-expectations', required=True)

    options, rest_args = parser.parse_known_args(args)

    forwarded_args = rest_args + [
        '--ignore-default-expectations', '--additional-expectations',
        options.webgpu_cts_expectations
    ]

    run_web_tests.main(forwarded_args, stderr)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:], sys.stderr))
