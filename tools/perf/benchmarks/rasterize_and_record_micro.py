# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from measurements import rasterize_and_record_micro
import page_sets
from telemetry import benchmark


class _RasterizeAndRecordMicro(perf_benchmark.PerfBenchmark):

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_argument('--start-wait-time',
                        type=float,
                        default=2,
                        help=('Wait time before the benchmark is started '
                              '(must be long enough to load all content)'))
    parser.add_argument('--rasterize-repeat',
                        type=int,
                        default=100,
                        help=('Repeat each raster this many times. Increase '
                              'this value to reduce variance.'))
    parser.add_argument('--record-repeat',
                        type=int,
                        default=100,
                        help=('Repeat each record this many times. Increase '
                              'this value to reduce variance.'))
    parser.add_argument('--timeout',
                        type=int,
                        default=120,
                        help=('The length of time to wait for the micro '
                              'benchmark to finish, expressed in seconds.'))
    parser.add_argument('--report-detailed-results',
                        action='store_true',
                        help='Whether to report additional detailed results.')

  @classmethod
  def Name(cls):
    return 'rasterize_and_record_micro'

  def CreatePageTest(self, options):
    return rasterize_and_record_micro.RasterizeAndRecordMicro(
        options.start_wait_time, options.rasterize_repeat,
        options.record_repeat, options.timeout, options.report_detailed_results)


@benchmark.Info(
    emails=['pdr@chromium.org',
             'wangxianzhu@chromium.org'],
    component='Internals>Compositing>Rasterization',
    documentation_url='https://bit.ly/rasterize-and-record-benchmark')
class RasterizeAndRecordMicroTop25(_RasterizeAndRecordMicro):
  """Measures rasterize and record performance on the top 25 web pages.

  https://chromium.googlesource.com/chromium/src/+/HEAD/docs/speed/benchmark/harnesses/rendering.md"""
  page_set = page_sets.StaticTop25PageSet

  @classmethod
  def Name(cls):
    return 'rasterize_and_record_micro.top_25'
