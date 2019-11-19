# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import filecmp
import json
import logging
from logging import handlers
import os
import posixpath
import shutil
import subprocess

from core.external_modules import pandas as pd
from core.external_modules import numpy as np
from core import gsutil
from py_utils import tempfile_ext


PINBOARD_DIR = os.path.abspath(os.path.dirname(__file__))
TOOLS_PERF_DIR = os.path.normpath(os.path.join(PINBOARD_DIR, '..', '..'))
CACHED_DATA_DIR = os.path.join(TOOLS_PERF_DIR, '_cached_data', 'pinboard')

PINPOINT_CLI = os.path.join(TOOLS_PERF_DIR, 'pinpoint_cli')
JOB_CONFIGS_PATH = os.path.join(PINBOARD_DIR, 'job_configs.json')

JOBS_STATE_FILE = 'jobs_state.json'
DATASET_PKL_FILE = 'dataset.pkl'
DATASET_CSV_FILE = 'dataset.csv'

CLOUD_STORAGE_DIR = 'gs://chrome-health-tvdata/pinboard'
TZ = 'America/Los_Angeles'  # MTV-time.


# Only these are exported and uploaded to the Cloud Storage dataset.
MEASUREMENTS = set([
    # V8 metrics.
    'JavaScript:duration',
    'Optimize-Background:duration',
    'Optimize:duration',
    'RunsPerMinute',
    'Total-Main-Thread:duration',
    'Total:duration',
    'V8-Only-Main-Thread:duration',
    'V8-Only:duration',
    'memory:chrome:renderer_processes:reported_by_chrome:v8:effective_size',
    'total:500ms_window:renderer_eqt:v8',

    # Startup metrics.
    'experimental_content_start_time',
    'experimental_navigation_start_time',
    'first_contentful_paint_time',
    'messageloop_start_time',
    'navigation_commit_time',
])

# Compute averages over a fixed set of active stories. These may need to be
# periodically updated.
ACTIVE_STORIES = set([
    'browse:chrome:newtab',
    'browse:chrome:omnibox',
    'browse:media:facebook_photos',
    'browse:media:googleplaystore:2019',
    'browse:media:imgur',
    'browse:media:youtube',
    'browse:news:cricbuzz',
    'browse:news:toi',
    'browse:shopping:amazon',
    'browse:shopping:lazada',
    'browse:social:facebook',
    'browse:social:instagram',
    'browse:tools:maps',
    'load:media:facebook_photos',
    'load:media:youtube:2018',
    'load:news:irctc',
    'load:news:wikipedia:2018',
    'intent:coldish:bbc',
    'Speedometer2',
])


def StartPinpointJobs(state, date):
  """Start new pinpoint jobs for the last commit on the given date."""
  revision, timestamp = GetLastCommitOfDate(date)
  if any(item['revision'] == revision for item in state):
    logging.info('No new jobs to start.')
    return

  # Add a new item to the state with info about jobs for this revision.
  logging.info('Starting jobs for %s (%s):', timestamp[:10], revision)
  item = {'revision': revision, 'timestamp': timestamp, 'jobs': []}
  configs = LoadJsonFile(JOB_CONFIGS_PATH)
  for config in configs:
    config['start_git_hash'] = revision
    config['end_git_hash'] = revision
    with tempfile_ext.NamedTemporaryFile() as tmp:
      json.dump(config, tmp)
      tmp.close()
      output = subprocess.check_output(
          ['vpython', PINPOINT_CLI, 'start-job', tmp.name],
          universal_newlines=True).strip()
    logging.info(output)
    assert 'https://pinpoint' in output
    item['jobs'].append({'id': output.split('/')[-1], 'status': 'queued'})
  state.append(item)
  state.sort(key=lambda p: p['timestamp'])  # Keep items sorted by date.


def IsJobFinished(job):
  return job['status'] in ['completed', 'failed']


def CollectPinpointResults(state):
  """Check the status of pinpoint jobs and collect their results."""
  # First iterate over all running jobs, and update their status.
  for item in state:
    active = [job['id'] for job in item['jobs'] if not IsJobFinished(job)]
    if not active:
      continue
    cmd = ['vpython', PINPOINT_CLI, 'status']
    cmd.extend(active)
    output = subprocess.check_output(cmd, universal_newlines=True)
    updates = dict(line.split(': ', 1) for line in output.splitlines())
    logging.info('Got job updates: %s.', updates)
    for job in item['jobs']:
      if job['id'] in updates:
        job['status'] = updates[job['id']]

  # Now iterate over all completed jobs, and download their results if needed.
  for item in state:
    if _SkipProcessing(item):  # Skip if not ready or all failed.
      continue
    output_file = RevisionResultsFile(item)
    if not os.path.exists(output_file):
      cmd = ['vpython', PINPOINT_CLI, 'get-csv', '--output', output_file, '--']
      job_ids = [j['id'] for j in item['jobs'] if j['status'] == 'completed']
      logging.info('Getting csv data for commit: %s.', item['revision'])
      subprocess.check_output(cmd + job_ids)


def LoadJobsState():
  """Load the latest recorded state of pinpoint jobs."""
  local_path = CachedFilePath(JOBS_STATE_FILE)
  if os.path.exists(local_path):
    return LoadJsonFile(local_path)
  else:
    return []


def UpdateJobsState(state):
  """Write back the updated state of pinpoint jobs.

  If there were any changes to the state, i.e. new jobs were created or
  existing ones completed, both the local cached copy and the backup in cloud
  storage are updated.
  """
  local_path = CachedFilePath(JOBS_STATE_FILE)
  with tempfile_ext.NamedTemporaryFile() as tmp:
    json.dump(state, tmp, sort_keys=True, indent=2, separators=(',', ': '))
    tmp.close()
    if not os.path.exists(local_path) or not filecmp.cmp(tmp.name, local_path):
      shutil.copyfile(tmp.name, local_path)
      UploadToCloudStorage(local_path)


def AggregateAndUploadResults(state):
  """Aggregate results collected and upload them to cloud storage."""
  cached_results = CachedFilePath(DATASET_PKL_FILE)
  dfs = []

  keep_revisions = set(item['revision'] for item in state)
  if os.path.exists(cached_results):
    # To speed things up, we take the cache computed from previous results.
    df = pd.read_pickle(cached_results)
    # Drop possible old data from revisions no longer in recent state.
    df = df[df['revision'].isin(keep_revisions)]
    dfs.append(df)
    known_revisions = set(df['revision'])
  else:
    known_revisions = set()

  found_new = False
  for item in state:
    if item['revision'] in known_revisions or _SkipProcessing(item):
      # Revision is already in cache, jobs are not ready, or all have failed.
      continue
    if not found_new:
      logging.info('Processing data from new results:')
      found_new = True
    logging.info('- %s (%s)', item['timestamp'][:10], item['revision'])
    dfs.append(GetRevisionResults(item))

  if not found_new:
    logging.info('No new data found.')
    return

  # Otherwise update our cache and upload.
  df = pd.concat(dfs, ignore_index=True)
  df.to_pickle(cached_results)

  # Drop revisions with no results and mark the last result for each metric,
  # both with/without patch, as a 'reference'. This allows making score cards
  # comparing their most recent results in Data Studio dashboards.
  df = df[df['count'] > 0].copy()
  latest_result = df.groupby(
      ['label', 'benchmark', 'name'])['timestamp'].transform('max')
  df['reference'] = df['timestamp'] == latest_result

  dataset_file = CachedFilePath(DATASET_CSV_FILE)
  df.to_csv(dataset_file, index=False)
  UploadToCloudStorage(dataset_file)
  logging.info('Total %s rows of data uploaded.' % len(df.index))


def GetRevisionResults(item):
  """Aggregate the results from jobs that ran on a particular revision."""
  # First load pinpoint csv results into a DataFrame. The dtype arg is needed
  # to ensure that job_id's are always read a strings (even if some of them
  # look like large numbers).
  df = pd.read_csv(RevisionResultsFile(item), dtype={'job_id': str})
  assert df['change'].str.contains(item['revision']).all(), (
      'Not all results match the expected git revision')

  # Filter out and keep only the measurements and stories that we want.
  df = df[df['name'].isin(MEASUREMENTS)]
  df = df[df['story'].isin(ACTIVE_STORIES)]

  if not df.empty:
    # Aggregate over the results of individual stories.
    df = df.groupby(['change', 'name', 'benchmark', 'unit'])['mean'].agg(
        ['mean', 'count']).reset_index()
  else:
    # Otherwise build a single row with an "empty" aggregate for this revision.
    # This is needed so we can remember in the cache that this revision has
    # been processed.
    df = pd.DataFrame(index=[0])
    df['change'] = item['revision']
    df['name'] = '(missing)'
    df['benchmark'] = '(missing)'
    df['unit'] = ''
    df['mean'] = np.nan
    df['count'] = 0

  # Convert time units from milliseconds to seconds. This is what Data Studio
  # dashboards expect.
  is_ms_unit = df['unit'].str.startswith('ms_')
  df.loc[is_ms_unit, 'mean'] = df['mean'] / 1000

  # Distinguish jobs that ran with/without the tested patch.
  df['label'] = df['change'].str.contains(r'\+').map(
      {False: 'without_patch', True: 'with_patch'})

  # Add timestamp and revision information. We snap the date to noon and make
  # it naive (i.e. no timezone), so the dashboard doesn't get confused with
  # dates close to the end of day.
  date = item['timestamp'].split('T')[0] + 'T12:00:00'
  df['timestamp'] = pd.Timestamp(date)
  df['revision'] = item['revision']

  # Fake the timestamp of jobs without the patch to appear as if they ran a
  # year ago; this makes it easier to visualize and compare timeseries from
  # runs with/without the patch in Data Studio dashboards.
  df.loc[df['label'] == 'without_patch', 'timestamp'] = (
      df['timestamp'] - pd.DateOffset(years=1))

  return df[['revision', 'timestamp', 'label',
             'benchmark', 'name', 'mean', 'count']]


def _SkipProcessing(item):
  """Return True if not all jobs have finished or all have failed."""
  return (not all(IsJobFinished(job) for job in item['jobs']) or
          all(job['status'] == 'failed' for job in item['jobs']))


def GetLastCommitOfDate(date):
  """"Find the the lastest commit that landed on a given date."""
  # Make sure our local git repo has up to date info on origin/master.
  logging.info('Fetching latest origin/master data.')
  subprocess.check_output(
      ['git', 'fetch', 'origin', 'master'], cwd=TOOLS_PERF_DIR,
      stderr=subprocess.STDOUT)

  # Snap the date to the end of the day.
  cutoff_date = date.replace(hour=12).ceil('D')
  logging.info('Finding latest commit before %s.', cutoff_date)
  if not FindCommit(after_date=cutoff_date):
    # We expect there to be some commits after the 'cutoff_date', otherwise
    # there isn't yet a *last* commit before that date.
    raise ValueError("Given date appears to be in the future. There isn't yet "
                     'a last commit before %s.' % cutoff_date)
  return FindCommit(before_date=cutoff_date)


def FindCommit(before_date=None, after_date=None):
  """Find latest commit with optional before/after date constraints."""
  cmd = ['git', 'log', '--max-count', '1', '--format=format:%H:%ct']
  if before_date is not None:
    cmd.extend(['--before', before_date.isoformat()])
  if after_date is not None:
    cmd.extend(['--after', after_date.isoformat()])
  cmd.append('origin/master')
  line = subprocess.check_output(cmd, cwd=TOOLS_PERF_DIR).strip()
  if line:
    revision, commit_time = line.split(':')
    commit_time = pd.Timestamp(
        int(commit_time), unit='s', tz=TZ).isoformat()
    return revision, commit_time
  else:
    return None


def RevisionResultsFile(item):
  """Get a filepath where to cache results of jobs for a single revision."""
  return CachedFilePath('job_results', item['revision'] + '.csv')


def CachedFilePath(arg, *args):
  """Get the path to a file stored in local cache."""
  return os.path.join(CACHED_DATA_DIR, arg, *args)


def UploadToCloudStorage(filepath):
  """Copy the given file to cloud storage."""
  gsutil.Copy(
      filepath, posixpath.join(CLOUD_STORAGE_DIR, os.path.basename(filepath)))


def LoadJsonFile(filename):
  with open(filename) as f:
    return json.load(f)


def TimeAgo(**kwargs):
  return pd.Timestamp.now(TZ) - pd.DateOffset(**kwargs)


def SetUpLogging(level):
  """Set up logging to log both to stderr and a file."""
  logger = logging.getLogger()
  logger.setLevel(level)
  formatter = logging.Formatter(
      '(%(levelname)s) %(asctime)s [%(module)s] %(message)s')

  h1 = logging.StreamHandler()
  h1.setFormatter(formatter)
  logger.addHandler(h1)

  h2 = handlers.TimedRotatingFileHandler(
     filename=CachedFilePath('pinboard.log'), when='W0', backupCount=5)
  h2.setFormatter(formatter)
  logger.addHandler(h2)


def SelectRecentRevisions(state):
  """Filter out old revisions from state to keep only recent (6 months) data."""
  from_date = str(TimeAgo(months=6).date())
  return [item for item in state if item['timestamp'] > from_date]


def Main():
  SetUpLogging(level=logging.INFO)
  actions = ('start', 'collect', 'upload')
  parser = argparse.ArgumentParser()
  parser.add_argument(
      'actions', metavar='ACTION', nargs='+', choices=actions + ('auto',),
      help=("select action to perform: 'start' pinpoint jobs, 'collect' job "
            "results, 'upload' aggregated data, or 'auto' to do all in "
            "sequence."))
  parser.add_argument(
      '--date', type=lambda s: pd.Timestamp(s, tz=TZ), default=TimeAgo(days=1),
      help=('Run jobs for the last commit landed on the given date (assuming '
            'MTV time). Defaults to the last commit landed yesterday.'))
  args = parser.parse_args()
  if 'auto' in args.actions:
    logging.info('=== auto run for %s ===', args.date)
    args.actions = actions

  state = LoadJobsState()
  try:
    if 'start' in args.actions:
      StartPinpointJobs(state, args.date)
    recent_state = SelectRecentRevisions(state)
    if 'collect' in args.actions:
      CollectPinpointResults(recent_state)
  finally:
    UpdateJobsState(state)

  if 'upload' in args.actions:
    AggregateAndUploadResults(recent_state)
