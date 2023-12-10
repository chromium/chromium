#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to remove the assets and strings from a specific
version of what's new.

Sample use:

python3 tools/whats_new/remove_whats_new_milestone.py --milestone="M123"

Run with --help to get a complete list of options this script runs with.
"""

import subprocess
import sys
import whats_new_util
import argparse


def main():
    parser = parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        '--milestone',
        required=True,
        help='Specify the the What\'s New milestone you want to remove.')
    args = parser.parse_args()
    if not args.milestone:
        raise ValueError('Missing input through --milestone.')

    milestone = args.milestone.lower()
    whats_new_util.RemoveStringsForMilestone(milestone)
    whats_new_util.RemoveAnimationAssetsForMilestone(milestone)

    command_git_format = ['git', 'cl', 'format']
    error = subprocess.Popen(command_git_format,
                             universal_newlines=True,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE).communicate()
    if error:
        print(error)


if __name__ == '__main__':
    sys.exit(main())
