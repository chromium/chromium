# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import csv
import json
import logging
import ntpath
import posixpath
import sys

from cli_tools.pinpoint_cli import histograms_df
from cli_tools.pinpoint_cli import job_results
from core.services import isolate_service
from core.services import pinpoint_service


def StartJobFromConfig(config_path):
  """Start a pinpoint job based on a config file."""
  src = sys.stdin if config_path == '-' else open(config_path)
  with src as f:
    config = json.load(f)

  if not isinstance(config, dict):
    raise ValueError('Invalid job config')

  # An absent comparison_mode denotes a tryjob configuration.
  if 'comparison_mode' not in config:
    config['comparison_mode'] = 'try'

  # As of crrev.com/c/1965875 try jobs must specify a base git hash.
  if config['comparison_mode'] == 'try' and 'base_git_hash' not in config:
    config['base_git_hash'] = config['start_git_hash']
    del config['start_git_hash']
    del config['end_git_hash']

  response = pinpoint_service.NewJob(**config)
  print('Started:', response['jobUrl'])


def CheckJobStatus(job_ids):
  for job_id in job_ids:
    job = pinpoint_service.Job(job_id)
    print('%s: %s' % (job_id, job['status'].lower()))


def DownloadJobResultsAsCsv(job_ids, only_differences, output_file):
  """Download the perf results of a job as a csv file."""
  with open(output_file, 'wb') as f:
    writer = csv.writer(f)
    writer.writerow(('job_id', 'change', 'isolate') + histograms_df.COLUMNS)
    num_rows = 0
    for job_id in job_ids:
      job = pinpoint_service.Job(job_id, with_state=True)
      os_path = _OsPathFromJob(job)
      results_file = os_path.join(
          job['arguments']['benchmark'], 'perf_results.json')
      print('Fetching results for %s job %s:' % (job['status'].lower(), job_id))
      for change_id, isolate_hash in job_results.IterTestOutputIsolates(
          job, only_differences):
        print('- isolate: %s ...' % isolate_hash)
        try:
          histograms = isolate_service.RetrieveFile(isolate_hash, results_file)
        except KeyError:
          logging.warning('Skipping over isolate, results not found.')
          continue
        for row in histograms_df.IterRows(json.loads(histograms)):
          writer.writerow((job_id, change_id, isolate_hash) + row)
          num_rows += 1
  print('Wrote data from %d histograms in %s.' % (num_rows, output_file))


def _OsPathFromJob(job):
  if job['arguments']['configuration'].lower().startswith('win'):
    return ntpath
  else:
    return posixpath
