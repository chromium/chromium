# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Example tool to load a heap dump from a trace."""

import argparse
import os
import pathlib
import sys

_SRC_PATH = pathlib.Path(__file__).resolve().parents[4]
sys.path.append(str(_SRC_PATH / 'tools/android'))
from colabutils.memory_usage.memory_usage_view import MemoryUsageView


def main():
    parser = argparse.ArgumentParser(
        description='Load a heap dump from a trace file and print top-level '
        'memory usage.')
    parser.add_argument('trace_file', help='Path to the trace file.')
    args = parser.parse_args()

    if not os.path.exists(args.trace_file):
        print(f'Error: {args.trace_file}: file not found')
        return 1

    print(f'Loading heap dump from: {args.trace_file}')
    view = MemoryUsageView.from_heap_dump(args.trace_file)

    print('Memory usage for toplevel frames:')
    for line in view.toplevel_pretty_report():
        print(line)
    return 0


if __name__ == '__main__':
    sys.exit(main())
