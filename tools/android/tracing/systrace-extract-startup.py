#!/usr/bin/env python
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script parses result of build/android/adb_profile_chrome_startup and
# prints various information.

from __future__ import print_function

import argparse
import collections
import glob
import itertools
import json
import os
import re


def HumanSortingKey(string):
  # From https://goo.gl/65xrva
  def _ToInt(string):
    return int(string) if string.isdigit() else string

  return [_ToInt(c) for c in re.split('(\d+)', string)]


class LogIndenter(object):
  _indentation = 0

  def __init__(self, message=None, *arguments):
    self._indented = False
    if message is not None:
      log(message, *arguments)

  def __enter__(self):
    self.indent()
    return self

  def __exit__(self, type, value, traceback):
    self.unindent()

  def indent(self):
    if not self._indented:
      LogIndenter._indentation += 1
      self._indented = True

  def unindent(self):
    if self._indented:
      LogIndenter._indentation -= 1
      self._indented = False

  @classmethod
  def indentation(self):
    return LogIndenter._indentation


def log(message, *arguments):
  if not message:
    print()
    return

  if arguments:
    message = message.format(*arguments)
  if LogIndenter.indentation() > 0:
    message = '  ' * LogIndenter.indentation() + message
  print(message)


def ParseTraceDatas(trace_file_path):
  start_tag = re.compile(
      '^\s*<script class="trace-data" type="application/text">$')
  end_tag = re.compile('^(?P<line>.*?)\s*</script>$')

  trace_datas = []

  current_trace_lines = None
  with open(trace_file_path) as trace_file:
    for line in trace_file:
      line = line.rstrip()
      if current_trace_lines is None:
        if start_tag.match(line):
          current_trace_lines = []
      else:
        match = end_tag.match(line)
        if match:
          current_trace_lines.append(match.group('line'))
          trace_datas.append('\n'.join(current_trace_lines))
          current_trace_lines = None
        else:
          current_trace_lines.append(line)

  return trace_datas


class Event(object):
  PHASE_BEGIN = 'B'
  PHASE_END = 'E'
  PHASE_COMPLETE = 'X'
  PHASE_ASYNC_BEGIN = 'S'
  PHASE_ASYNC_END = 'F'

  def __init__(self, node):
    self._node = node

  @property
  def pid(self):
    return int(self._node['pid'])

  @property
  def tid(self):
    return int(self._node['tid'])

  @property
  def name(self):
    return self._node.get('name')

  @property
  def phase(self):
    return self._node['ph']

  @property
  def category(self):
    return self._node['cat']

  @property
  def timestamp_us(self):
    return long(self._node['ts'])

  @property
  def duration_us(self):
    return long(self._node['dur'])

  @property
  def args(self):
    return self._node['args']


class EventInterval(object):
  def __init__(self, from_event=None, to_event=None):
    self.from_event = from_event
    self.to_event = to_event

  def SetFromEventOnce(self, from_event):
    if self.from_event is None:
      self.from_event = from_event

  def SetToEventOnce(self, to_event):
    if self.to_event is None:
      self.to_event = to_event

  def FormatAsMilliseconds(self):
    if not self.from_event:
      time_string = "[missing the start of of the interval]"
    elif not self.to_event:
      time_string = "[missing the end of the interval]"
    else:
      interval_us = self.to_event.timestamp_us - self.from_event.timestamp_us
      time_string = str(interval_us / 1000.0)
    return time_string

  def UpTo(self, other_iterval):
    return EventInterval(self.from_event, other_iterval.to_event)


class Process(object):

  BROWSER_NAME = 'Browser'

  def __init__(self, pid):
    self.pid = pid
    self.name = None
    self.events_by_name = collections.defaultdict(list)
    self.time_ns_by_histogram = {}
    self.malloc_counter_by_name = {}
    # TODO: move these into Trace
    self.startup_interval = EventInterval()
    self.first_ui_interval = EventInterval()


class Trace(object):
  def __init__(self, file_path):
    self.file_path = file_path
    self.process_by_pid = {}
    self.startup_event = None
    self.navigation_start_event = None
    # TODO: convert these to properties over events
    self.navigation_to_contentul_paint_interval = EventInterval()
    self.navigation_to_meaningful_paint_interval = EventInterval()
    self.navigation_to_commit_interval = None

  @property
  def startup_to_navigation_interval(self):
    return EventInterval(self.startup_event, self.navigation_start_event)

  def Finalize(self):
    self.startup_event = self.FindFirstEvent(*Trace.STARTUP_EVENT_NAMES)
    self.navigation_start_event = self.FindFirstEvent(
        Trace.NAVIGATION_START_EVENT_NAME)

    def _FindNavigationToCommitInterval():
      events = self.FindAllEvents(Trace.NAVIGATION_COMMIT_EVENT_NAME)
      interval = EventInterval()
      for event in events:
        if event.phase == Event.PHASE_ASYNC_BEGIN:
          interval.SetFromEventOnce(event)
        elif event.phase == Event.PHASE_ASYNC_END:
          interval.SetToEventOnce(event)
      return interval
    self.navigation_to_commit_interval = _FindNavigationToCommitInterval()

  def FindAllEvents(self, *names):
    events = []
    for process in self.process_by_pid.itervalues():
      for name in names:
        process_events = process.events_by_name.get(name)
        if process_events:
          events.extend(process_events)
    events.sort(key=lambda e: e.timestamp_us)
    return events

  def FindFirstEvent(self, *names):
    events = self.FindAllEvents(*names)
    return events[0] if events else None

  NAVIGATION_START_EVENT_NAME = 'NavigationTiming navigationStart'
  NAVIGATION_COMMIT_EVENT_NAME = 'Navigation StartToCommit'

  STARTUP_EVENT_NAMES = [
      'Startup.BrowserMainEntryPoint', 'ChromeApplication.onCreate',
      'ContentShellApplication.onCreate'
  ]


def ParseTrace(file_path):
  trace_datas = ParseTraceDatas(file_path)
  if not trace_datas:
    raise Exception("The file doesn't have any trace-data elements.")

  trace_json = None
  for trace_data in trace_datas:
    try:
      trace_json = json.loads(trace_data)
    except ValueError:
      continue

  if not trace_json:
    raise Exception("Couldn't parse trace-data json.")

  trace = Trace(file_path)

  for event_node in trace_json['traceEvents']:
    event = Event(event_node)

    pid = event.pid
    process = trace.process_by_pid.get(event.pid)
    if not process:
      process = Process(pid)
      trace.process_by_pid[pid] = process

    name = event.name
    if not name:
      continue

    process.events_by_name[name].append(event)

    phase = event.phase
    category = event.category

    if name == 'process_name':
      process.name = event.args['name']

    if (category == 'disabled-by-default-uma-addtime' and
        name not in process.time_ns_by_histogram):
      process.time_ns_by_histogram[name] = int(event.args['value_ns'])

    if name in Trace.STARTUP_EVENT_NAMES:
      process.startup_interval.SetFromEventOnce(event)
    elif name == 'BenchmarkInstrumentation::ImplThreadRenderingStats':
      process.startup_interval.SetToEventOnce(event)

    if name == Trace.NAVIGATION_START_EVENT_NAME:
      trace.navigation_to_contentul_paint_interval.SetFromEventOnce(event)
      trace.navigation_to_meaningful_paint_interval.SetFromEventOnce(event)
    elif name == 'firstContentfulPaint':
      trace.navigation_to_contentul_paint_interval.SetToEventOnce(event)
    elif name == 'firstMeaningfulPaint':
      trace.navigation_to_meaningful_paint_interval.SetToEventOnce(event)

    if (name == 'AsyncInitializationActivity.onCreate()' and
        phase == Event.PHASE_END):
      process.first_ui_interval.SetFromEventOnce(event)
    elif name == 'ChromeBrowserInitializer.startChromeBrowserProcessesAsync':
      process.first_ui_interval.SetToEventOnce(event)

    if category == 'malloc' and name == 'malloc_counter':
      counter_name, counter_value = next(event.args.iteritems())
      process.malloc_counter_by_name[counter_name] = long(counter_value)

  trace.Finalize()
  return trace


EventSummary = collections.namedtuple('EventSummary', [
    'trace',
    'event',
    'startup_to_event_ms',
    'navigation_to_event_ms',
    'duration_ms'
])

def SummarizeEvents(event_name_regex, trace, process):
  summaries = []
  def _AddSummary(event, start_us, duration_us):
    startup_to_event_ms = (
        None if trace.startup_event is None else
        (start_us - trace.startup_event.timestamp_us) / 1000.0)
    navigation_to_event_ms = (
        None if trace.navigation_start_event is None else
        (start_us - trace.navigation_start_event.timestamp_us) / 1000.0)
    summaries.append(EventSummary(
        trace, event, startup_to_event_ms, navigation_to_event_ms,
        duration_us / 1000.0))

  for name, events in process.events_by_name.iteritems():
    if event_name_regex.search(name):
      sorted_events = sorted(events,
                             key=lambda e: (e.tid, e.timestamp_us))
      begin_event = None
      for event in sorted_events:
        if event.phase == Event.PHASE_COMPLETE:
          _AddSummary(event, event.timestamp_us, event.duration_us)
        elif (event.phase == Event.PHASE_BEGIN or
              event.phase == Event.PHASE_ASYNC_BEGIN):
          begin_event = event
        elif (event.phase == Event.PHASE_END or
              event.phase == Event.PHASE_ASYNC_END):
          if begin_event is not None:
            duration_us = event.timestamp_us - begin_event.timestamp_us
            _AddSummary(event, begin_event.timestamp_us, duration_us)
          begin_event = None

  return summaries


def PrintReport(file_paths, options):
  # TODO: don't accumulate traces, build report on the fly
  traces = []
  for file_path in file_paths:
    log('Parsing {}...', file_path)
    try:
      traces.append(ParseTrace(file_path))
    except Exception as e:
      log('Oops: {}', e.message)

  log('Parsed {} trace(s).', len(traces))

  event_name_regex = None
  event_summaries_by_name = collections.defaultdict(list)
  if options.print_events:
    event_name_regex = re.compile(options.print_events)

  def _TraceSortingKey(trace):
    return HumanSortingKey(os.path.basename(trace.file_path))

  traces.sort(key=lambda t: _TraceSortingKey(t))

  if options.csv:
    separator = ','
    gap = ''
  else:
    separator = '\t'
    # Make it less likely for terminals to eat tabs when wrapping a line.
    gap = '    '

  table = [[
      'File',
      'Startup (ms)',
      'StartupToNavigation (ms)',
      'NavigationToCommit (ms)',
      'NavigationToContentfulPaint (ms)',
      'StartupToContentfulPaint (ms)',
      'NavigationToMeaningfulPaint (ms)',
      'StartupToMeaningfulPaint (ms)'
  ]]
  for trace in traces:
    browser_process = None
    for process in trace.process_by_pid.itervalues():
      if process.name == Process.BROWSER_NAME:
        browser_process = process
        break
    if browser_process is None:
      continue

    table.append([
        os.path.basename(trace.file_path),
        browser_process.startup_interval.FormatAsMilliseconds(),
        trace.startup_to_navigation_interval.FormatAsMilliseconds(),
        trace.navigation_to_commit_interval.FormatAsMilliseconds(),
        trace.navigation_to_contentul_paint_interval.FormatAsMilliseconds(),
        browser_process.startup_interval.UpTo(trace.navigation_to_contentul_paint_interval).\
            FormatAsMilliseconds(),
        trace.navigation_to_meaningful_paint_interval.FormatAsMilliseconds(),
        browser_process.startup_interval.UpTo(trace.navigation_to_meaningful_paint_interval).\
            FormatAsMilliseconds()
    ])

    if event_name_regex:
      event_summaries = SummarizeEvents(
          event_name_regex, trace, browser_process)
      for summary in event_summaries:
        event_summaries_by_name[summary.event.name].append(summary)

  for name, event_summaries in event_summaries_by_name.iteritems():
    table.append([])

    summaries_by_trace = collections.defaultdict(list)
    for summary in event_summaries:
      summaries_by_trace[summary.trace].append(summary)

    width = max(len(s) for s in summaries_by_trace.itervalues())
    summary_headers = [
        'StartupToEvent (ms)',
        'NavigationToEvent (ms)',
        'Duration (ms)'
    ]

    table.append(
        [name] +
        ([gap] * len(summary_headers)) +
        list(itertools.chain.from_iterable(
            ['#{}'.format(i)] + [gap] * (len(summary_headers) - 1)
                for i in range(1, width))))
    table.append(
        ['File'] +
        summary_headers * width)

    trace_summaries = sorted(summaries_by_trace.iteritems(),
                             key=lambda t_s: _TraceSortingKey(t_s[0]))
    for trace, summaries in trace_summaries:
      row = [os.path.basename(trace.file_path)]
      for summary in summaries:
        row += [
            (gap if summary.startup_to_event_ms is None
                else summary.startup_to_event_ms),
            (gap if summary.navigation_to_event_ms is None
                else summary.navigation_to_event_ms),
            summary.duration_ms
        ]
      table.append(row)

  print()
  print('\n'.join(separator.join(str(v) for v in row) for row in table))


def PrintTrace(trace_file_path, options):
  trace = ParseTrace(trace_file_path)

  def _PrintInterval(name, interval):
    log('{} (ms): {}', name, interval.FormatAsMilliseconds())

  def _PrintHistogramTime(process, name):
    time_ns = process.time_ns_by_histogram.get(name)
    time_ms = None if time_ns is None else time_ns / 1e6
    if time_ms is not None or options.print_none_histograms:
      log('{} (ms): {}', name, time_ms)

  histogram_names = [
      'ChromeGeneratedCustomTab.IntentToFirstCommitNavigationTime3.ZoomedIn',
      'CustomTabs.IntentToFirstCommitNavigationTime3.ZoomedIn',
      'PageLoad.PaintTiming.NavigationToFirstPaint',
      'PageLoad.PaintTiming.NavigationToFirstContentfulPaint',
      'PageLoad.Experimental.PaintTiming.NavigationToFirstMeaningfulPaint',
      'SessionRestore.ForegroundTabFirstPaint3',
  ]

  processes = sorted(trace.process_by_pid.itervalues(), key=lambda p: p.name)

  events_regex = None
  if options.print_events:
    events_regex = re.compile(options.print_events)

  for process in processes:
    log('{} timings:', process.name)
    indenter = LogIndenter()
    indenter.indent()

    if process.name == Process.BROWSER_NAME:
      _PrintInterval('Startup', process.startup_interval)
      _PrintInterval('StartupToNavigation',
          trace.startup_to_navigation_interval)
      _PrintInterval('NavigationToCommit', trace.navigation_to_commit_interval)
      _PrintInterval('NavigationToContentfulPaint',
          trace.navigation_to_contentul_paint_interval)
      _PrintInterval('StartupToContentfulPaint', process.startup_interval.UpTo(
          trace.navigation_to_contentul_paint_interval))
      _PrintInterval('NavigationToMeaningfulPaint',
          trace.navigation_to_meaningful_paint_interval)
      _PrintInterval('StartupToMeaningfulPaint', process.startup_interval.UpTo(
          trace.navigation_to_meaningful_paint_interval))

      if options.experimental:
        _PrintInterval('First UI interval', process.first_ui_interval)

    if process.malloc_counter_by_name:
      def _PrintMallocCounter(title, value_name, factor):
        value = process.malloc_counter_by_name.get(value_name)
        if value is not None:
          value /= factor
        log('{}: {}', title, value)

      log('Malloc counters:')
      with LogIndenter():
        _PrintMallocCounter('Total time (ms)', 'malloc_time_ns', 1000000)
        _PrintMallocCounter('Total allocated (KiB)', 'allocated_bytes', 1024)
        _PrintMallocCounter('Allocations', 'allocation_count', 1)
        _PrintMallocCounter('Frees', 'free_count', 1)

    for histogram_name in histogram_names:
      _PrintHistogramTime(process, histogram_name)

    if events_regex:
      event_summaries = SummarizeEvents(events_regex, trace, process)
      if event_summaries:
        with LogIndenter('Events matching "{}":', events_regex.pattern):
          for event_summary in event_summaries:
            with LogIndenter('{}:', event_summary.event.name):
              if event_summary.startup_to_event_ms is not None:
                log('StartupToEvent (ms): {}',
                    event_summary.startup_to_event_ms)
              if event_summary.navigation_to_event_ms is not None:
                log('NavigationToEvent (ms): {}',
                    event_summary.navigation_to_event_ms)
              log('Duration (ms): {}', event_summary.duration_ms)

    indenter.unindent()
    log('')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('file_or_glob')
  parser.add_argument('--print-none-histograms',
                      help='Print histograms with None values.',
                      default=False, action='store_true')
  # TODO: introduce a variant that takes a list of event names, as escaping
  #       event names can be tedious.
  # TODO: match regex against '<index>|<event name>' to allow selecting
  #       events by index (complicated for begin/end pairs).
  parser.add_argument('--print-events',
                      help='Print events matching the specified regex.')
  parser.add_argument('--experimental',
                      default=False, action='store_true',
                      help='Enable experimental stuff.')
  parser.add_argument('--report',
                      default=False, action='store_true',
                      help='Present information as a tab-separated table.')
  parser.add_argument('--csv',
                      default=False, action='store_true',
                      help=('Separate report values by commas (not tabs).'))

  options = parser.parse_args()

  globbed = False
  if os.path.isfile(options.file_or_glob):
    trace_file_paths = [options.file_or_glob]
  else:
    globbed = True
    file_pattern = options.file_or_glob
    trace_file_paths = glob.glob(file_pattern)
    if not trace_file_paths:
      file_pattern += '*html'
      trace_file_paths = glob.glob(file_pattern)
    if not trace_file_paths:
      log("'{}' didn't match anything.", file_pattern)
      return
    log("'{}' matched {} file(s).", file_pattern, len(trace_file_paths))
    log('')

  if options.report:
    PrintReport(trace_file_paths, options)
  else:
    for file_path in trace_file_paths:
      if globbed:
        log('_' * len(file_path))
        log(file_path)
      log('')
      PrintTrace(file_path, options)


if __name__ == '__main__':
  main()
