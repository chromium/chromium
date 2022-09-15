#!/usr/bin/env vpython3
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Analyze benchmark results for Instant start."""

from __future__ import print_function

import argparse
import pickle
import sys

import stats.analyze


def main():
    """Main program"""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('pickles',
                        nargs='+',
                        help='The pickle files saved by benchmark.py.')
    args = parser.parse_args()

    runs = []
    for filename in args.pickles:
        with open(filename, 'rb') as file:
            metadata = pickle.load(file)
            print('Reading "%s" with %s' % (filename, metadata))
            runs.extend(pickle.load(file))
    stats.analyze.print_report(runs, metadata['model'])


if __name__ == '__main__':
    sys.exit(main())
