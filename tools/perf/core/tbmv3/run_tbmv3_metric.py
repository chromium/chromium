#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import json
import logging
import os
import sys

# TODO(crbug.com/40102479): Adding tools/perf to path. We can remove this when
# we have a wrapper script under tools/perf that sets up import paths more
# nicely.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from core import path_util
path_util.AddPyUtilsToPath()
path_util.AddTracingToPath()

from core.tbmv3 import trace_processor


_CHROMIUM_SRC_PATH  = os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..')


def _WriteHistogramSetToFile(histograms, outfile):
  with open(outfile, 'w') as f:
    json.dump(histograms.AsDicts(), f, indent=2, sort_keys=True,
              separators=(',', ': '))
    f.write("\n")


def Main(cli_args):
  parser = argparse.ArgumentParser(
    description='[Experimental] Runs TBMv3 metrics on local traces and '
    'produces histogram json.')
  parser.add_argument('--trace', required=True,
                      help='Trace file you want to compute metric on')
  parser.add_argument('--metric', required=True,
                      help=('Name of the metric you want to run'))
  parser.add_argument(
      '--trace-processor-path',
      help='Path to trace processor shell. '
      'Default: Binary downloaded from cloud storage.')
  parser.add_argument('--outfile', default='results.json',
                      help='Path to output file. Default: %(default)s')
  args = parser.parse_args(cli_args)

  histograms = trace_processor.RunMetric(args.trace_processor_path,
                                         args.trace, args.metric)
  _WriteHistogramSetToFile(histograms, args.outfile)
  logging.info('JSON result created in file://%s' % (args.outfile))


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
