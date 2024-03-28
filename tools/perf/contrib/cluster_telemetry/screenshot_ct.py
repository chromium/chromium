# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from contrib.cluster_telemetry import ct_benchmarks_util
from contrib.cluster_telemetry import page_set
from contrib.cluster_telemetry import repaint_helpers
from contrib.cluster_telemetry import screenshot

class ScreenshotCT(perf_benchmark.PerfBenchmark):
  """Captures PNG screenshots of web pages for Cluster Telemetry. Screenshots
     written to local file with path-safe urls of pages as filenames. Cluster
     Telemetry is then used for aggregation and analysis."""

  @classmethod
  def Name(cls):
    return 'screenshot_ct'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)
    parser.add_argument('--png-outdir',
                        help='Output directory for the PNG files')
    parser.add_argument('--wait-time',
                        type=float,
                        default=0,
                        help='Wait time before the benchmark is started')
    parser.add_argument('--dc-detect',
                        action='store_true',
                        default=False,
                        help=('Detects dynamic content by marking'
                              'pixels that were not consistent across multiple '
                              'screenshots with cyan'))
    parser.add_argument(
        '--dc-wait-time',
        type=float,
        default=1,
        help=('Wait time in between screenshots. Only applicable '
              'if dc_detect flag is true.'))
    parser.add_argument(
        '--dc-extra-screenshots',
        type=int,
        default=1,
        help=('Number of extra screenshots taken to detect '
              'dynamic content. Only applicable if dc_detect flag is '
              'true.'))
    parser.add_argument(
        '--dc-threshold',
        type=float,
        default=0.5,
        help=('Maximum tolerable percentage of dynamic content '
              'pixels. Raises an exception if percentage of dynamic '
              'content is beyond this threshold. Only applicable if '
              'dc_detect flag is true.'))

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    ct_benchmarks_util.ValidateCommandLineArgs(parser, args)
    if not args.png_outdir:
      parser.error('Please specify --png-outdir')

  def CreatePageTest(self, options):
    return screenshot.Screenshot(options.png_outdir, options.wait_time,
      options.dc_detect, options.dc_wait_time, options.dc_extra_screenshots,
      options.dc_threshold)

  def CreateStorySet(self, options):
    return page_set.CTPageSet(
        options.urls_list, options.user_agent, options.archive_data_file,
        run_page_interaction_callback=repaint_helpers.WaitThenRepaint)
