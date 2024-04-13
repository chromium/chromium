# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from benchmarks import memory
from contrib.shared_storage import page_set
from contrib.shared_storage import utils
from core import perf_benchmark

from telemetry import benchmark
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement

# Features to enable via command line.
_ENABLED_FEATURES = [
    'SharedStorageAPI:ExposeDebugMessageForSettingsStatus/true',
    'SharedStorageAPIM118', 'SharedStorageAPIM125',
    'SharedStorageAPIEnableWALForDatabase',
    'FencedFrames:implementation_type/mparch', 'FencedFramesDefaultMode',
    'PrivacySandboxAdsAPIsOverride', 'DefaultAllowPrivacySandboxAttestations'
]

# Default number of times to run each shared storage action in a story.
_DEFAULT_NUM_ITERATIONS = 10

# Maximum number of times to run each shared storage action in a story.
_MAX_NUM_ITERATIONS = 10

# Default number of times to run each story.
_DEFAULT_NUM_REPEAT = 10

# Use maximum allowed trace buffer size (in KB).
_TRACE_BUFFER_SIZE = 2**32 - 1

# Timeout in seconds allowed for the browser to shutdown when asked, before
# it is killed.
_SHUTDOWN_TIMEOUT = 90

class SharedStoragePerfBase(perf_benchmark.PerfBenchmark):
  SIZE = 0
  verbose_cpu_metrics = False
  verbose_memory_metrics = False
  iterations = _DEFAULT_NUM_ITERATIONS
  verbosity = 0
  xvfb_process = None

  options = {'pageset_repeat': _DEFAULT_NUM_REPEAT}

  @property
  def URL(self):
    return "file://setup_worklet_and_db.html?size=%s" % self.SIZE

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_argument('--xvfb',
                        action='store_true',
                        default=False,
                        help='Run with Xvfb server if possible.')
    parser.add_argument(
        '--user-agent',
        default='desktop',
        help='Options are "desktop" (the default) and "mobile".')
    parser.add_argument('--verbose-cpu-metrics',
                        action='store_true',
                        help='Enables non-UMA CPU metrics.')
    parser.add_argument('--verbose-memory-metrics',
                        action='store_true',
                        help='Enables non-UMA memory metrics.')
    iter_help = (f'Number of times (default {_DEFAULT_NUM_ITERATIONS}, max '
                 f'{_MAX_NUM_ITERATIONS}) to repeat action for each story run.')
    parser.add_argument('--iterations',
                        type=int,
                        default=_DEFAULT_NUM_ITERATIONS,
                        help=iter_help)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    cls.verbose_cpu_metrics = args.verbose_cpu_metrics
    cls.verbose_memory_metrics = args.verbose_memory_metrics
    cls.iterations = args.iterations
    if cls.iterations <= 0:
      raise ValueError('Got invalid value %d for iterations' % cls.iterations)
    if cls.iterations > _MAX_NUM_ITERATIONS:
      logging.warning('The maximum allowed number of iterations is 10. ' +
                      'Increase pageset_repeat instead.')
      cls.iterations = _MAX_NUM_ITERATIONS
    if args.xvfb and utils.ShouldStartXvfb():
      cls.xvfb_process = utils.StartXvfb()

  def SetExtraBrowserOptions(self, options):
    # `options` is an instance of `browser_options.BrowserOptions`.
    if self.verbose_memory_metrics:
      memory.SetExtraBrowserOptionsForMemoryMeasurement(options)

    extra_args = [
        '--enable-features=' + ','.join(_ENABLED_FEATURES),
        '--enable-privacy-sandbox-ads-apis'
    ]
    if self.xvfb_process:
      extra_args.append('--disable-gpu')
    options.AppendExtraBrowserArgs(extra_args)

    # Increase the default shutdown timeout due to some long-running tests.
    os.environ['CHROME_SHUTDOWN_TIMEOUT'] = str(_SHUTDOWN_TIMEOUT)

  def CustomizeOptions(self, finder_options, possible_browser=None):
    #`finder_options` is an instance of `browser_options.BrowserFinderOptions`.
    #
    # Normally, a subclass of `perf_benchmark.PerfBenchmark` should only
    # override  SetExtraBrowserOptions to add more browser options rather than
    # overriding CustomizeOptions. We need to access the `finder_options` to
    # read the verbosity level, however, and this seems to be the best way to
    # do it.
    super(SharedStoragePerfBase, self).CustomizeOptions(finder_options)
    self.verbosity = finder_options.verbosity

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string="benchmark")
    if self.verbose_memory_metrics:
      tbm_options = memory.CreateCoreTimelineBasedMemoryMeasurementOptions()

      # The memory options only include the filters needed for memory
      # measurement. We reintroduce the filters required for other metrics.
      tbm_options.ExtendTraceCategoryFilter(
          category_filter.filter_string.split(','))
    else:
      tbm_options = timeline_based_measurement.Options(category_filter)

    tbm_options.config.chrome_trace_config.SetTraceBufferSizeInKb(
        _TRACE_BUFFER_SIZE)

    for histogram in utils.GetSharedStorageUmaHistograms():
      tbm_options.config.chrome_trace_config.EnableUMAHistograms(histogram)

    tbm_options.AddTimelineBasedMetric('umaMetric')
    if self.verbose_cpu_metrics:
      tbm_options.AddTimelineBasedMetric('limitedCpuTimeMetric')
    return tbm_options

  def CreateStorySet(self, options):
    # `options` is an instance of `timeline_based_measurement.Options`.
    return page_set.SharedStorageStorySet(
        url=self.URL,
        size=self.SIZE,
        enable_memory_metric=self.verbose_memory_metrics,
        user_agent=options.user_agent,
        iterations=self.iterations,
        verbosity=self.verbosity,
        xvfb_process=self.xvfb_process)


@benchmark.Info(emails=['cammie@chromium.org'],
                component='Blink>Storage>SharedStorage',
                documentation_url='')
class SharedStoragePerfFreshDB(SharedStoragePerfBase):
  SIZE = 0

  @classmethod
  def Name(cls):
    return 'shared_storage.fresh'


@benchmark.Info(emails=['cammie@chromium.org'],
                component='Blink>Storage>SharedStorage',
                documentation_url='')
class SharedStoragePerfSmallDB(SharedStoragePerfBase):
  SIZE = 10

  @classmethod
  def Name(cls):
    return 'shared_storage.small'


@benchmark.Info(emails=['cammie@chromium.org'],
                component='Blink>Storage>SharedStorage',
                documentation_url='')
class SharedStoragePerfMediumDB(SharedStoragePerfBase):
  SIZE = 1000

  @classmethod
  def Name(cls):
    return 'shared_storage.medium'


@benchmark.Info(emails=['cammie@chromium.org'],
                component='Blink>Storage>SharedStorage',
                documentation_url='')
class SharedStoragePerfLargeDB(SharedStoragePerfBase):
  # TODO(cammie): Update the size for 'shared_storage.large' to 10000 when
  # M124 reaches stable (and with it the change in quota enfocrement).
  SIZE = 9000

  @classmethod
  def Name(cls):
    return 'shared_storage.large'
