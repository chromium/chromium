# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import collections

from core import path_util
from core import perf_benchmark

from page_sets import webgl_supported_shared_state

from telemetry import benchmark
from telemetry import page as page_module
from telemetry.page import legacy_page_test
from telemetry.page import shared_page_state
from telemetry import story
from telemetry.timeline import bounds
from telemetry.timeline import model as model_module
from telemetry.timeline import tracing_config


BLINK_PERF_BASE_DIR = os.path.join(path_util.GetChromiumSrcDir(),
                                   'third_party', 'blink', 'perf_tests')
SKIPPED_FILE = os.path.join(BLINK_PERF_BASE_DIR, 'Skipped')

EventBoundary = collections.namedtuple('EventBoundary',
                                       ['type', 'wall_time', 'thread_time'])

MergedEvent = collections.namedtuple('MergedEvent',
                                     ['bounds', 'thread_or_wall_duration'])


class _BlinkPerfPage(page_module.Page):
  def RunPageInteractions(self, action_runner):
    action_runner.ExecuteJavaScript('testRunner.scheduleTestRun()')
    action_runner.WaitForJavaScriptCondition('testRunner.isDone', timeout=600)

def StoryNameFromUrl(url, prefix):
  filename = url[len(prefix):].strip('/')
  baseName, extension = filename.split('.')
  if extension.find('?') != -1:
    query = extension.split('?')[1]
    baseName += "_" + query # So that queried page-names don't collide
  return "{b}.{e}".format(b=baseName, e=extension)

def CreateStorySetFromPath(path, skipped_file,
                           shared_page_state_class=(
                               shared_page_state.SharedPageState),
                           append_query=None):
  assert os.path.exists(path)

  page_urls = []
  serving_dirs = set()

  def _AddPage(path):
    if not path.endswith('.html'):
      return
    if '../' in open(path, 'r').read():
      # If the page looks like it references its parent dir, include it.
      serving_dirs.add(os.path.dirname(os.path.dirname(path)))
    page_url = 'file://' + path.replace('\\', '/')
    if append_query:
      page_url += '?' + append_query
    page_urls.append(page_url)


  def _AddDir(dir_path, skipped):
    for candidate_path in os.listdir(dir_path):
      if candidate_path == 'resources':
        continue
      candidate_path = os.path.join(dir_path, candidate_path)
      if candidate_path.startswith(skipped):
        continue
      if os.path.isdir(candidate_path):
        _AddDir(candidate_path, skipped)
      else:
        _AddPage(candidate_path)

  if os.path.isdir(path):
    skipped = []
    if os.path.exists(skipped_file):
      for line in open(skipped_file, 'r').readlines():
        line = line.strip()
        if line and not line.startswith('#'):
          skipped_path = os.path.join(os.path.dirname(skipped_file), line)
          skipped.append(skipped_path.replace('/', os.sep))
    _AddDir(path, tuple(skipped))
  else:
    _AddPage(path)
  ps = story.StorySet(base_dir=os.getcwd() + os.sep,
                      serving_dirs=serving_dirs)

  all_urls = [p.rstrip('/') for p in page_urls]
  common_prefix = os.path.dirname(os.path.commonprefix(all_urls))
  for url in sorted(page_urls):
    name = StoryNameFromUrl(url, common_prefix)
    ps.AddStory(_BlinkPerfPage(
        url, ps, ps.base_dir,
        shared_page_state_class=shared_page_state_class,
        name=name))
  return ps

def _CreateMergedEventsBoundaries(events, max_start_time):
  """ Merge events with the given |event_name| and return a list of MergedEvent
  objects. All events that are overlapping are megred together. Same as a union
  operation.

  Note: When merging multiple events, we approximate the thread_duration:
  duration = (last.thread_start + last.thread_duration) - first.thread_start

  Args:
    events: a list of TimelineEvents
    max_start_time: the maximum time that a TimelineEvent's start value can be.
    Events past this this time will be ignored.

  Returns:
    a sorted list of MergedEvent objects which contain a Bounds object of the
    wall time boundary and a thread_or_wall_duration which contains the thread
    duration if possible, and otherwise the wall duration.
  """
  event_boundaries = []  # Contains EventBoundary objects.
  merged_event_boundaries = []     # Contains MergedEvent objects.

  # Deconstruct our trace events into boundaries, sort, and then create
  # MergedEvents.
  # This is O(N*log(N)), although this can be done in O(N) with fancy
  # datastructures.
  # Note: When merging multiple events, we approximate the thread_duration:
  # dur = (last.thread_start + last.thread_duration) - first.thread_start
  for event in events:
    # Handle events where thread duration is None (async events).
    thread_start = None
    thread_end = None
    if event.thread_start and event.thread_duration:
      thread_start = event.thread_start
      thread_end = event.thread_start + event.thread_duration
    event_boundaries.append(
        EventBoundary("start", event.start, thread_start))
    event_boundaries.append(
        EventBoundary("end", event.end, thread_end))
  event_boundaries.sort(key=lambda e: e[1])

  # Merge all trace events that overlap.
  event_counter = 0
  curr_bounds = None
  curr_thread_start = None
  for event_boundary in event_boundaries:
    if event_boundary.type == "start":
      event_counter += 1
    else:
      event_counter -= 1
    # Initialization
    if curr_bounds is None:
      assert event_boundary.type == "start"
      # Exit early if we reach the max time.
      if event_boundary.wall_time > max_start_time:
        return merged_event_boundaries
      curr_bounds = bounds.Bounds()
      curr_bounds.AddValue(event_boundary.wall_time)
      curr_thread_start = event_boundary.thread_time
      continue
    # Create a the final bounds and thread duration when our event grouping
    # is over.
    if event_counter == 0:
      curr_bounds.AddValue(event_boundary.wall_time)
      thread_or_wall_duration = curr_bounds.bounds
      if curr_thread_start and event_boundary.thread_time:
        thread_or_wall_duration = event_boundary.thread_time - curr_thread_start
      merged_event_boundaries.append(
          MergedEvent(curr_bounds, thread_or_wall_duration))
      curr_bounds = None
  return merged_event_boundaries

def _ComputeTraceEventsThreadTimeForBlinkPerf(
    model, renderer_thread, trace_events_to_measure):
  """ Compute the CPU duration for each of |trace_events_to_measure| during
  blink_perf test.

  Args:
    renderer_thread: the renderer thread which run blink_perf test.
    trace_events_to_measure: a list of string names of trace events to measure
    CPU duration for.

  Returns:
    a dictionary in which each key is a trace event' name (from
    |trace_events_to_measure| list), and value is a list of numbers that
    represents to total cpu time of that trace events in each blink_perf test.
  """
  trace_cpu_time_metrics = {}

  # Collect the bounds of "blink_perf.runTest" events.
  test_runs_bounds = []
  for event in renderer_thread.async_slices:
    if event.name == "blink_perf.runTest":
      test_runs_bounds.append(bounds.Bounds.CreateFromEvent(event))
  test_runs_bounds.sort(key=lambda b: b.min)

  for t in trace_events_to_measure:
    trace_cpu_time_metrics[t] = [0.0] * len(test_runs_bounds)

  # Handle case where there are no tests.
  if not test_runs_bounds:
    return trace_cpu_time_metrics

  for event_name in trace_events_to_measure:
    merged_event_boundaries = _CreateMergedEventsBoundaries(
        model.IterAllEventsOfName(event_name), test_runs_bounds[-1].max)

    curr_test_runs_bound_index = 0
    for b in merged_event_boundaries:
      if b.bounds.bounds == 0:
        continue
      # Fast forward (if needed) to the first relevant test.
      while (curr_test_runs_bound_index < len(test_runs_bounds) and
             b.bounds.min > test_runs_bounds[curr_test_runs_bound_index].max):
        curr_test_runs_bound_index += 1
      if curr_test_runs_bound_index >= len(test_runs_bounds):
        break
      # Add metrics for all intersecting tests, as there may be multiple
      # tests that intersect with the event bounds.
      start_index = curr_test_runs_bound_index
      while (curr_test_runs_bound_index < len(test_runs_bounds) and
             b.bounds.Intersects(
                 test_runs_bounds[curr_test_runs_bound_index])):
        intersect_wall_time = bounds.Bounds.GetOverlapBetweenBounds(
            test_runs_bounds[curr_test_runs_bound_index], b.bounds)
        intersect_cpu_or_wall_time = (
            intersect_wall_time * b.thread_or_wall_duration / b.bounds.bounds)
        trace_cpu_time_metrics[event_name][curr_test_runs_bound_index] += (
            intersect_cpu_or_wall_time)
        curr_test_runs_bound_index += 1
      # Rewind to the last intersecting test as it might intersect with the
      # next event.
      curr_test_runs_bound_index = max(start_index,
                                       curr_test_runs_bound_index - 1)
  return trace_cpu_time_metrics


class _BlinkPerfMeasurement(legacy_page_test.LegacyPageTest):
  """Runs a blink performance test and reports the results."""

  def __init__(self):
    super(_BlinkPerfMeasurement, self).__init__()
    with open(os.path.join(os.path.dirname(__file__),
                           'blink_perf.js'), 'r') as f:
      self._blink_perf_js = f.read()
    self._is_tracing = False
    self._extra_chrome_categories = None
    self._enable_systrace = None

  def WillNavigateToPage(self, page, tab):
    del tab  # unused
    page.script_to_evaluate_on_commit = self._blink_perf_js

  def DidNavigateToPage(self, page, tab):
    tab.WaitForJavaScriptCondition('testRunner.isWaitingForTelemetry')
    self._StartTracingIfNeeded(tab)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--js-flags=--expose_gc',
        '--enable-experimental-web-platform-features',
        '--autoplay-policy=no-user-gesture-required'
    ])

  def SetOptions(self, options):
    super(_BlinkPerfMeasurement, self).SetOptions(options)
    browser_type = options.browser_options.browser_type
    if browser_type and 'content-shell' in browser_type:
      options.AppendExtraBrowserArgs('--expose-internals-for-testing')
    if options.extra_chrome_categories:
      self._extra_chrome_categories = options.extra_chrome_categories
    if options.enable_systrace:
      self._enable_systrace = True

  def _StartTracingIfNeeded(self, tab):
    tracing_categories = tab.EvaluateJavaScript('testRunner.tracingCategories')
    if (not tracing_categories and not self._extra_chrome_categories and
        not self._enable_systrace):
      return

    self._is_tracing = True
    config = tracing_config.TracingConfig()
    config.enable_chrome_trace = True
    config.chrome_trace_config.category_filter.AddFilterString(
        'blink.console')  # This is always required for js land trace event
    if tracing_categories:
      config.chrome_trace_config.category_filter.AddFilterString(
          tracing_categories)
    if self._extra_chrome_categories:
      config.chrome_trace_config.category_filter.AddFilterString(
          self._extra_chrome_categories)
    if self._enable_systrace:
      config.chrome_trace_config.SetEnableSystrace()
    tab.browser.platform.tracing_controller.StartTracing(config)


  def PrintAndCollectTraceEventMetrics(self, trace_cpu_time_metrics, results):
    unit = 'ms'
    print()
    for trace_event_name, cpu_times in trace_cpu_time_metrics.iteritems():
      print('CPU times of trace event "%s":' % trace_event_name)
      cpu_times_string = ', '.join(['{0:.10f}'.format(t) for t in cpu_times])
      print('values %s %s' % (cpu_times_string, unit))
      avg = 0.0
      if cpu_times:
        avg = sum(cpu_times)/len(cpu_times)
      print('avg', '{0:.10f}'.format(avg), unit)
      results.AddMeasurement(trace_event_name, unit, cpu_times)
      print()
    print('\n')

  def ValidateAndMeasurePage(self, page, tab, results):
    trace_cpu_time_metrics = {}
    if self._is_tracing:
      trace_data = tab.browser.platform.tracing_controller.StopTracing()
      results.AddTraces(trace_data)

      trace_events_to_measure = tab.EvaluateJavaScript(
          'window.testRunner.traceEventsToMeasure')
      if trace_events_to_measure:
        model = model_module.TimelineModel(trace_data)
        renderer_thread = model.GetFirstRendererThread(tab.id)
        trace_cpu_time_metrics = _ComputeTraceEventsThreadTimeForBlinkPerf(
            model, renderer_thread, trace_events_to_measure)

    log = tab.EvaluateJavaScript('document.getElementById("log").innerHTML')

    for line in log.splitlines():
      if line.startswith("FATAL: "):
        print(line)
        continue
      if not line.startswith('values '):
        continue
      parts = line.split()
      values = [float(v.replace(',', '')) for v in parts[1:-1]]
      units = parts[-1]
      metric = page.name.split('.')[0].replace('/', '_')
      if values:
        results.AddMeasurement(metric, units, values)
      else:
        raise legacy_page_test.MeasurementFailure('Empty test results')

      break

    print(log)

    self.PrintAndCollectTraceEventMetrics(trace_cpu_time_metrics, results)


class _BlinkPerfBenchmark(perf_benchmark.PerfBenchmark):

  test = _BlinkPerfMeasurement

  def CreateStorySet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, self.SUBDIR)
    return CreateStorySetFromPath(path, SKIPPED_FILE)


@benchmark.Info(emails=['dmazzoni@chromium.org'],
                component='Blink>Accessibility',
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfAccessibility(_BlinkPerfBenchmark):
  SUBDIR = 'accessibility'

  @classmethod
  def Name(cls):
    return 'blink_perf.accessibility'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--force-renderer-accessibility',
    ])


@benchmark.Info(
    component='Blink>Bindings',
    emails=['jbroman@chromium.org', 'yukishiino@chromium.org',
            'haraken@chromium.org'],
    documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfBindings(_BlinkPerfBenchmark):
  SUBDIR = 'bindings'

  @classmethod
  def Name(cls):
    return 'blink_perf.bindings'


@benchmark.Info(emails=['futhark@chromium.org', 'andruud@chromium.org'],
                documentation_url='https://bit.ly/blink-perf-benchmarks',
                component='Blink>CSS')
class BlinkPerfCSS(_BlinkPerfBenchmark):
  SUBDIR = 'css'

  @classmethod
  def Name(cls):
    return 'blink_perf.css'

@benchmark.Info(emails=['aaronhk@chromium.org', 'fserb@chromium.org'],
                documentation_url='https://bit.ly/blink-perf-benchmarks',
                component='Blink>Canvas')
class BlinkPerfCanvas(_BlinkPerfBenchmark):
  SUBDIR = 'canvas'

  @classmethod
  def Name(cls):
    return 'blink_perf.canvas'

  def CreateStorySet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, self.SUBDIR)
    story_set = CreateStorySetFromPath(
        path, SKIPPED_FILE,
        shared_page_state_class=(
            webgl_supported_shared_state.WebGLSupportedSharedState))
    raf_story_set = CreateStorySetFromPath(
        path, SKIPPED_FILE,
        shared_page_state_class=(
            webgl_supported_shared_state.WebGLSupportedSharedState),
        append_query="RAF")
    for raf_story in raf_story_set:
      story_set.AddStory(raf_story)
    # WebGLSupportedSharedState requires the skipped_gpus property to
    # be set on each page.
    for page in story_set:
      page.skipped_gpus = []
    return story_set

@benchmark.Info(emails=['masonfreed@chromium.org'],
                component='Blink>DOM',
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfDOM(_BlinkPerfBenchmark):
  SUBDIR = 'dom'

  @classmethod
  def Name(cls):
    return 'blink_perf.dom'


@benchmark.Info(emails=['masonfreed@chromium.org'],
                component='Blink>DOM',
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfEvents(_BlinkPerfBenchmark):
  SUBDIR = 'events'

  @classmethod
  def Name(cls):
    return 'blink_perf.events'

  # TODO(yoichio): Migrate EventsDispatching tests to V1 and remove this flags
  # crbug.com/937716.
  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--enable-blink-features=ShadowDOMV0'])


@benchmark.Info(emails=['cblume@chromium.org'],
                component='Internals>Images>Codecs',
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfImageDecoder(_BlinkPerfBenchmark):
  SUBDIR = 'image_decoder'

  @classmethod
  def Name(cls):
    return 'blink_perf.image_decoder'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-blink-features=JSImageDecode',
    ])


@benchmark.Info(emails=['eae@chromium.org'],
                component='Blink>Layout',
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfLayout(_BlinkPerfBenchmark):
  SUBDIR = 'layout'

  @classmethod
  def Name(cls):
    return 'blink_perf.layout'


@benchmark.Info(emails=['dmurph@chromium.org'],
                component='Blink>Storage',
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfOWPStorage(_BlinkPerfBenchmark):
  SUBDIR = 'owp_storage'

  @classmethod
  def Name(cls):
    return 'blink_perf.owp_storage'

  # This ensures that all blobs >= 20MB will be transported by files.
  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--blob-transport-by-file-trigger=307300',
        '--blob-transport-min-file-size=2048',
        '--blob-transport-max-file-size=10240',
        '--blob-transport-shared-memory-max-size=30720'
    ])


@benchmark.Info(emails=['pdr@chromium.org', 'wangxianzhu@chromium.org'],
                component='Blink>Paint',
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfPaint(_BlinkPerfBenchmark):
  SUBDIR = 'paint'

  @classmethod
  def Name(cls):
    return 'blink_perf.paint'


@benchmark.Info(component='Blink>Bindings',
                emails=['jbroman@chromium.org',
                         'yukishiino@chromium.org',
                         'haraken@chromium.org'],
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfParser(_BlinkPerfBenchmark):
  SUBDIR = 'parser'

  @classmethod
  def Name(cls):
    return 'blink_perf.parser'


@benchmark.Info(emails=['fs@opera.com', 'pdr@chromium.org'],
                component='Blink>SVG',
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfSVG(_BlinkPerfBenchmark):
  SUBDIR = 'svg'

  @classmethod
  def Name(cls):
    return 'blink_perf.svg'


@benchmark.Info(emails=['masonfreed@chromium.org'],
                component='Blink>DOM>ShadowDOM',
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfShadowDOM(_BlinkPerfBenchmark):
  SUBDIR = 'shadow_dom'

  @classmethod
  def Name(cls):
    return 'blink_perf.shadow_dom'

  # TODO(yoichio): Migrate shadow-style-share tests to V1 and remove this flags
  # crbug.com/937716.
  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--enable-blink-features=ShadowDOMV0'])

@benchmark.Info(emails=['vmpstr@chromium.org'],
                component='Blink>Paint',
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfDisplayLocking(_BlinkPerfBenchmark):
  SUBDIR = 'display_locking'

  @classmethod
  def Name(cls):
    return 'blink_perf.display_locking'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(
      ['--enable-blink-features=DisplayLocking,CSSContentSize'])
