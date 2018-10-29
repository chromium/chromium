# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contrib.cluster_telemetry import ct_benchmarks_util
from contrib.cluster_telemetry import page_set
from contrib.cluster_telemetry import repaint_helpers

from core import perf_benchmark
from measurements import rendering


class RepaintCT(perf_benchmark.PerfBenchmark):
  """Measures repaint performance for Cluster Telemetry."""

  test = rendering.Rendering

  @classmethod
  def Name(cls):
    return 'repaint_ct'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--mode', type='string',
                      default='viewport',
                      help='Invalidation mode. '
                      'Supported values: fixed_size, layer, random, viewport.')
    parser.add_option('--width', type='int',
                      default=None,
                      help='Width of invalidations for fixed_size mode.')
    parser.add_option('--height', type='int',
                      default=None,
                      help='Height of invalidations for fixed_size mode.')
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    ct_benchmarks_util.ValidateCommandLineArgs(parser, args)

  def CreateStorySet(self, options):
    return page_set.CTPageSet(
        options.urls_list, options.user_agent, options.archive_data_file,
        run_page_interaction_callback=repaint_helpers.WaitThenRepaint)
