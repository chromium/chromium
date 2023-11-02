#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import logging
import os

from core import path_util
path_util.AddPyUtilsToPath()

from cli_tools.tbmv3 import trace_downloader

EXAMPLE_TRACE = ('https://storage.cloud.google.com/'
                 'chrome-telemetry-output/20201029T003106_99943/'
                 'v8.browsing_mobile/browse_shopping_amazon_2019/'
                 'retry_0/trace.html')


def Main():
  logging.basicConfig(level=logging.WARN)
  parser = argparse.ArgumentParser(
      description='Downloads the proto trace for an cloud storage html '
      'trace url.')
  parser.add_argument('html_trace_url',
                      type=str,
                      help='Looks like %s' % EXAMPLE_TRACE)
  parser.add_argument('--download-dir',
                      type=str,
                      default=trace_downloader.DEFAULT_TRACE_DIR,
                      help='Directory to download the traces to.')
  args = parser.parse_args()
  if args.html_trace_url.endswith('/'):
    raise Exception('This is a directory, not a file url')
  path = trace_downloader.DownloadProtoTrace(args.html_trace_url,
                                             download_dir=args.download_dir)
  print('Downloaded to %s' % os.path.relpath(path))


if __name__ == '__main__':
  Main()
