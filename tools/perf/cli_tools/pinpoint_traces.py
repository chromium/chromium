#!/usr/bin/env vpython
# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import csv
import datetime
import errno
import json
import multiprocessing
import os
import platform
import re
import shutil
import subprocess
import sys
import traceback

DESCRIPTION = (
    """Tool to download and aggregate traces from a given Pinpoint job.

The tool is intended to be used in two steps:

Step 1: Fetch all traces from a Pinpoint job. Example:

> pinpoint_traces.py fetch \\
    --job=https://pinpoint-dot-chromeperf.appspot.com/job/1709f5ff920000

Step 2: Extract all events with a matching name into a CSV file. Example:

> pinpoint_traces.py extract --event='.*Looper.dispatch.*' --output events.csv


NOTE: Currently only supported on Linux.

WARNING: Uses multiprocessing with heavy use of memory and CPU resources in each
         subprocess. This causes problems when handling SIGINT (when ^C is
         pressed), instead it is recommended to stop by ^Z and using the shell
         job control to kill or continue the job (like: kill %1).
""")

THIS_DIRNAME = os.path.dirname(os.path.abspath(__file__))
PINPOINT_CLI = os.path.join(THIS_DIRNAME, os.pardir, 'pinpoint_cli')
HTML2TRACE = os.path.join(THIS_DIRNAME, os.pardir, os.pardir, os.pardir,
                          'third_party/catapult/tracing/bin/html2trace')
TRACE_PREFIX = 'https://console.developers.google.com/m/cloudstorage/b/chrome-telemetry-output/o/'
SUPPORTED_PHASES = set(['B', 'E', 'X'])


def ParseArgs():
  parser = argparse.ArgumentParser(
      prog='PINPOINT_TRACES',
      formatter_class=argparse.RawTextHelpFormatter,
      description=DESCRIPTION)
  subparsers = parser.add_subparsers(dest='command')

  # Subcommand to fetch traces created by a Pinpoint job.
  parser_fetch_traces = subparsers.add_parser(
      'fetch', help='Fetches job_results.csv and the traces mentioned in it')
  required_named_fetch = parser_fetch_traces.add_argument_group(
      'required named arguments')
  required_named_fetch.add_argument('--job',
                                    required=True,
                                    help='Pinpoint job ID or URL')

  # Subcommand to extract events from all fetched traces.
  parser_extract_event = subparsers.add_parser(
      'extract',
      help=('Extracts information about an event from all downloaded traces'
            'into a CSV file'))
  required_named_extract = parser_extract_event.add_argument_group(
      'required named arguments')
  required_named_extract.add_argument('-e',
                                      '--event',
                                      required=True,
                                      help='Regex to match event names against')
  required_named_extract.add_argument('-o',
                                      '--output',
                                      required=True,
                                      help='CSV output file name')

  return parser.parse_args()


def EnsureDirectoryExists(path):
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise
  return path


def LastAfterSlash(name):
  assert platform.system() == 'Linux'
  return name.rsplit('/', 1)[-1]


def FetchTraces(job_id):
  """Fetches traces from the cloud storage and stores then in a new directory.

  In order to discover the trace files, fetches the 'job results' file
  (job_results.csv) using pinpoint_cli. Extracts all the trace file names from
  the job results and fetches them from the predefined cloud storage bucket.

  The structure of the directory is like:
    ~/.local/share/pinpoint_traces/latest/
    ├── job_results.csv
    ├── chromium@4efc7af
    │   ├── trace1.html
    │   ├── trace2.html
    ├── chromium@5707819
    │   ├── trace3.html
    │   │
    │   ...
    ...

  Args:
    job_id: (str) Pinpoint job ID, extracted from the job URL.

  Returns:
    Full path to the job results file.
  """
  timestamp = datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
  top_dir = os.path.expanduser('~/.local/share/pinpoint_traces')
  working_dir = os.path.join(top_dir, timestamp)
  assert not os.path.exists(working_dir), ('Frequent invocations are not '
                                           'supported')
  EnsureDirectoryExists(working_dir)
  subprocess.check_call([PINPOINT_CLI, 'get-csv', job_id], cwd=working_dir)
  tmp_latest = os.path.join(top_dir, 'tmp-latest')
  real_latest = os.path.join(top_dir, 'latest')
  try:
    os.remove(tmp_latest)
  except OSError:
    pass
  os.symlink(timestamp, tmp_latest)
  os.rename(tmp_latest, real_latest)
  job_results = os.path.join(real_latest, 'job_results.csv')
  assert os.path.exists(job_results)
  cloud_traces = set()
  with open(job_results) as csv_file:
    reader = csv.DictReader(csv_file)
    for row in reader:
      cloud_traces.add((row['run_label'], row['trace_url']))
  for run_label, cloud_path in cloud_traces:
    assert cloud_path.startswith(TRACE_PREFIX)
    cloud_path = cloud_path[len(TRACE_PREFIX):]
    assert LastAfterSlash(cloud_path) == 'trace.html'
    trace_file_name = cloud_path.replace('/', '_')
    cloud_path = 'gs://chrome-telemetry-output/' + cloud_path
    tmp_dir = EnsureDirectoryExists(os.path.join(working_dir, 'tmp'))
    subprocess.check_call(['gsutil.py', 'cp', cloud_path, tmp_dir])
    run_label_dir = EnsureDirectoryExists(os.path.join(working_dir, run_label))
    full_trace_file_name = os.path.join(run_label_dir, trace_file_name)
    os.rename(os.path.join(tmp_dir, 'trace.html'), full_trace_file_name)
  return job_results


class LeanEvent(object):
  """Event that holds only necessary information extracted from JSON."""

  def __init__(self, trace_event):
    self.name = trace_event['name']
    self.phase = trace_event['ph']
    self.ts = trace_event['ts']
    self.tdur = 0
    if self.phase == 'X':
      self.dur = trace_event['dur']
      if 'tdur' in trace_event:
        self.tdur = trace_event['tdur']


def LoadJsonFile(json_file, compiled_re):
  """From the JSON file takes only the events that match.

  Note: This function loads a typical JSON file very slowly, for several
  seconds. Most time is spent in the Python json module.

  Args:
    json_file: (str) Path to the JSON file to load
    compiled_re: A compiled regular expression that selects event by |name| if
        the latter matches.

  Returns:
    A dict with this internal structure:
        { tid: { (int) ts: [event, ...] } },
    where:
      tid: (int) Thread ID of all the events it maps to.
      ts: (int) Trace timestamp (in microseconds) of the beginning of the event.
      event: (LeanEvent) One extracted event only containing the fields for
          writing to the CSV file, pairing 'begin' and 'end' events into a
          single one.

  """
  with open(json_file) as f:
    events = json.load(f)
  tids = {}
  tid_to_pid = {}
  for event in events['traceEvents']:
    assert 'ph' in event
    assert 'ts' in event
    phase = event['ph']
    if not phase in SUPPORTED_PHASES:
      continue
    assert 'tid' in event
    assert 'pid' in event
    tid = event['tid']
    pid = event['pid']
    old_pid = tid_to_pid.setdefault(tid, pid)
    assert pid == old_pid, 'Two TIDs match one PID'
    event_name = event['name']
    if compiled_re.match(event_name):
      ts_to_events = tids.setdefault(tid, {})
      ts = event['ts']
      event_list = ts_to_events.setdefault(ts, [])
      event_list.append(LeanEvent(event))
  return tids


def ExtractFromOneTraceThrowing(args):
  """Extracts events from a given trace file.

  First, converts the trace (fetched in HTML form) by the Catapult
  tool 'html2trace' into a number of JSON files (usually 2). The JSON files
  left after conversion are cached for later invocations of this function.

  With JSON cache the structure of the working directory becomes as follows:

    ~/.local/share/pinpoint_traces/latest/
    ├── job_results.csv
    ├── chromium@4efc7af
    │   ├── trace1.html
    │   ├── trace2.html
    │   └── json_cache
    │       ├── trace1
    │       │   ├── json
    │       │   └── json.1
    │       ├── trace2
    │       │   ├── json
    │       │   ├── json.1
    │       │   └── json.2
    ├── chromium@5707819
    │   │
    │   ...
    ...

  Since multiple JSON files are produced for a single HTML file, the loaded
  contents from the JSON files is merged. The merged representation is then
  scanned to find pairs of begin- and end- events to allow calculating their
  duration.

  Args: (passed as a single tuple to allow easier posting to subprocess.Pool)
    label: (str) A label corresponding to the revision of the code, for example,
        'chromium@4efc7af'.
    trace_index: (int) The number uniquely identifying the trace html that the
        JSON was generated from.
    html_trace: (str) Path to the original downloaded trace file.
    json_dir: (str) A directory where to look for the cached json files
    compiled_regex: Compiled regular expression, as in LoadJsonFile().

  Returns:
    A tuple (label, trace_index, events), where |events| is a tuple:
      (event_name, ts, duration, thread_duration) - the fields that will be
      written to the final CSV file.
  """
  (label, trace_index, html_trace, json_dir, compiled_regex) = args
  if not os.path.exists(json_dir):
    print 'Converting trace to json: {}'.format(json_dir)
    tmp_json_dir = json_dir + '.tmp'
    if os.path.exists(tmp_json_dir) and os.path.isdir(tmp_json_dir):
      shutil.rmtree(tmp_json_dir)
    EnsureDirectoryExists(tmp_json_dir)
    json_file = os.path.join(tmp_json_dir, 'json')
    output = subprocess.check_output([HTML2TRACE, html_trace, json_file])
    for line in output.splitlines():
      assert LastAfterSlash(line).startswith('json')
    # Rename atomically to make sure that the JSON cache is not consumed
    # partially.
    os.rename(tmp_json_dir, json_dir)

  # Since several json files are produced from the same trace, an event could
  # begin and end in different jsons.
  # Step 1: Merge events from all traces.
  merged_tids = None
  print 'Taking trace: {}'.format(html_trace)
  for json_file in os.listdir(json_dir):
    if not json_file.startswith('json'):
      print '{} not starting with json'.format(json_file)
      continue
    full_json_file = os.path.join(json_dir, json_file)
    tids = LoadJsonFile(full_json_file, compiled_regex)
    if not merged_tids:
      merged_tids = tids
    else:
      for tid, ts_to_events in tids.iteritems():
        merged_ts_to_events = merged_tids.setdefault(tid, {})
        for ts, events in ts_to_events.iteritems():
          merged_event_list = merged_ts_to_events.setdefault(ts, [])
          merged_event_list.extend(events)

  # Step 2: Sort by ts and compute event durations.
  csv_events = []
  for tid, ts_to_events in merged_tids.iteritems():
    begin_events = {}
    for ts, events in sorted(ts_to_events.iteritems()):
      for event in events:
        duration = 0
        thread_duration = 0
        assert ts == event.ts
        if event.phase == 'B':
          assert not event.name in begin_events
          begin_events[event.name] = event
          continue
        elif event.phase == 'E':
          begin_event = begin_events[event.name]
          duration = event.ts - begin_event.ts
          del begin_events[event.name]
        else:
          assert event.phase == 'X'
          duration = event.dur
          thread_duration = event.tdur
        csv_events.append((event.name, ts, duration, thread_duration))
  return (label, trace_index, csv_events)


def ExtractFromOneTrace(args):
  try:
    return ExtractFromOneTraceThrowing(args)
  except:
    # Print the stack trace that otherwise is not visible for this function
    # because it is invoked by multiprocessing.Pool.
    traceback.print_exc(file=sys.stderr)
    raise


def OutputCsv(trace_tuples, writer):
  for (label, trace_index, csv_events) in trace_tuples:
    for (name, ts, duration, thread_duration) in csv_events:
      writer.writerow([label, trace_index, name, ts, duration, thread_duration])


def ExtractEvents(regex, working_dir, csv_path):
  """Extracts all matching events and outputs them into a CSV file.

  Finds the trace files by scanning the working directory.

  Args:
    regex: A regular expression. Selects an event if its name matches.
    working_dir: (str) Path to a working directory with structure as explained
        in ExtractFromOneTraceThrowing().
    csv_path: (str) Name of the output file.
  """
  compiled_regex = re.compile(regex)
  tasks = []
  for run_label in os.listdir(working_dir):
    full_dir = os.path.join(working_dir, run_label)
    if not os.path.isdir(full_dir):
      continue
    trace_index = -1  # ID of the html trace, several json files merge into it.
    for html in os.listdir(full_dir):
      html_trace = os.path.join(full_dir, html)
      if not os.path.isfile(html_trace) or not html_trace.endswith('.html'):
        continue
      trace_index += 1
      json_cache = os.path.join(full_dir, 'json_cache')
      json_dir = os.path.join(json_cache, html.rstrip('.html'))
      tasks.append(
          (run_label, trace_index, html_trace, json_dir, compiled_regex))
  # Since multiprocessing accumulates all data in memory until the last task in
  # the pool has terminated, run it in batches to avoid swapping.
  batch_size = multiprocessing.cpu_count()
  pool = multiprocessing.Pool(batch_size)
  with open(csv_path, 'w') as csv_file:
    writer = csv.writer(csv_file)
    writer.writerow(
        ['label', 'trace_index', 'name', 'ts', 'duration', 'thread_duration'])
    while tasks:
      batch = tasks[:batch_size]
      tasks = tasks[batch_size:]
      OutputCsv(pool.map(ExtractFromOneTrace, batch), writer)


def main():
  args = ParseArgs()
  if args.command == 'fetch':
    print 'Fetching job_results.csv'
    csv_file = FetchTraces(LastAfterSlash(args.job))
    print 'See job results in {}'.format(csv_file)
  elif args.command == 'extract':
    top_dir = os.path.expanduser('~/.local/share/pinpoint_traces')
    working_dir = os.path.join(top_dir,
                               os.readlink(os.path.join(top_dir, 'latest')))
    assert os.path.exists(working_dir), 'Broken symlink'
    ExtractEvents(args.event, working_dir, args.output)
    print 'Wrote output: {}'.format(args.output)
  return 0


if __name__ == '__main__':
  sys.exit(main())
