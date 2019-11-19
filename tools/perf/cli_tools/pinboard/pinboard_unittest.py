# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import tempfile
import unittest

import mock

from cli_tools.pinboard import pinboard
from core.external_modules import pandas as pd


def StateItem(revision, **kwargs):
  item = {'revision': revision,
          'timestamp': kwargs.pop('timestamp', '2019-03-15'),
          'jobs': []}
  for job_id, status in sorted(kwargs.items()):
    item['jobs'].append({'id': job_id, 'status': status})
  return item


@unittest.skipIf(pd is None, 'pandas not available')
class PinboardToolTests(unittest.TestCase):
  def setUp(self):
    self.cache_dir = tempfile.mkdtemp()
    os.mkdir(os.path.join(self.cache_dir, 'job_results'))
    mock.patch(
        'cli_tools.pinboard.pinboard.CACHED_DATA_DIR',
        new=self.cache_dir).start()
    self.subprocess = mock.patch(
        'cli_tools.pinboard.pinboard.subprocess').start()
    self.upload_to_cloud = mock.patch(
        'cli_tools.pinboard.pinboard.UploadToCloudStorage').start()

  def tearDown(self):
    mock.patch.stopall()
    shutil.rmtree(self.cache_dir)

  @mock.patch('cli_tools.pinboard.pinboard.GetLastCommitOfDate')
  @mock.patch('cli_tools.pinboard.pinboard.LoadJsonFile')
  def testStartPinpointJobs(self, load_configs, get_last_commit):
    load_configs.return_value = [{'name': 'config1'}, {'name': 'config2'}]
    get_last_commit.return_value = ('2a66bac4', '2019-03-17T23:50:16-07:00')
    self.subprocess.check_output.side_effect = [
        'Started: https://pinpoint.example.com/job/14b4c451f40000\n',
        'Started: https://pinpoint.example.com/job/11fae481f40000\n']
    state = []

    pinboard.StartPinpointJobs(state, '2019-03-17')

    self.assertEqual(state, [{
        'revision': '2a66bac4',
        'timestamp': '2019-03-17T23:50:16-07:00',
        'jobs': [{'id': '14b4c451f40000', 'status': 'queued'},
                 {'id': '11fae481f40000', 'status': 'queued'}]}])

  def testCollectPinpointResults(self):
    state = [
        StateItem('a100', job1='completed', job2='completed'),
        StateItem('a200', job3='completed', job4='running'),
        StateItem('a300', job5='running', job6='running')]

    # Write some fake "previous" results for first revision.
    df = pd.DataFrame({'revision': ['a100']})
    df.to_csv(pinboard.RevisionResultsFile(state[0]), index=False)

    self.subprocess.check_output.side_effect = [
        'job4: completed\n',
        'job5: running\njob6: failed\n',
        'getting csv data ...\n'
    ]
    expected_state = [
        StateItem('a100', job1='completed', job2='completed'),
        StateItem('a200', job3='completed', job4='completed'),
        StateItem('a300', job5='running', job6='failed')]

    pinboard.CollectPinpointResults(state)

    self.assertEqual(state, expected_state)
    self.subprocess.check_output.assert_has_calls([
        mock.call(['vpython', pinboard.PINPOINT_CLI, 'status', 'job4'],
                  universal_newlines=True),
        mock.call(['vpython', pinboard.PINPOINT_CLI, 'status', 'job5', 'job6'],
                  universal_newlines=True),
        mock.call([
            'vpython', pinboard.PINPOINT_CLI, 'get-csv', '--output',
            pinboard.RevisionResultsFile(state[1]), '--', 'job3', 'job4'])
    ])

  def testUpdateJobsState(self):
    state = pinboard.LoadJobsState()
    self.assertEqual(state, [])

    # Update state with new data a couple of times.
    state.append(StateItem('a100'))
    pinboard.UpdateJobsState(state)
    state.append(StateItem('a200'))
    pinboard.UpdateJobsState(state)

    # No new data. Should be a no-op.
    pinboard.UpdateJobsState(state)

    stored_state = pinboard.LoadJobsState()
    self.assertEqual(stored_state, state)
    self.assertEqual([i['revision'] for i in stored_state], ['a100', 'a200'])
    self.assertEqual(self.upload_to_cloud.call_count, 2)

  @mock.patch('cli_tools.pinboard.pinboard.GetRevisionResults')
  def testAggregateAndUploadResults(self, get_revision_results):
    state = [
        StateItem('a100', timestamp='2019-03-15', job1='completed'),
        StateItem('a200', timestamp='2019-03-16', job2='completed'),
        StateItem('a300', timestamp='2019-03-17', job3='failed'),
        StateItem('a400', timestamp='2019-03-18', job4='completed'),
        StateItem('a500', timestamp='2019-03-19', job5='completed'),
    ]

    def GetFakeResults(item):
      df = pd.DataFrame(index=[0])
      df['revision'] = item['revision']
      df['label'] = 'with_patch'
      df['benchmark'] = 'loading'
      df['name'] = 'Total:duration'
      df['timestamp'] = pd.Timestamp(item['timestamp'])
      df['count'] = 1 if item['revision'] != 'a400' else 0
      return df

    get_revision_results.side_effect = GetFakeResults

    # Only process first few revisions.
    pinboard.AggregateAndUploadResults(state[:3])
    dataset_file = pinboard.CachedFilePath(pinboard.DATASET_CSV_FILE)
    df = pd.read_csv(dataset_file)
    self.assertEqual(set(df['revision']), set(['a100', 'a200']))
    self.assertTrue((df[df['reference']]['revision'] == 'a200').all())

    # Incrementally process the rest.
    pinboard.AggregateAndUploadResults(state)
    dataset_file = pinboard.CachedFilePath(pinboard.DATASET_CSV_FILE)
    df = pd.read_csv(dataset_file)
    self.assertEqual(set(df['revision']), set(['a100', 'a200', 'a500']))
    self.assertTrue((df[df['reference']]['revision'] == 'a500').all())

    # No new revisions. This should be a no-op.
    pinboard.AggregateAndUploadResults(state)

    self.assertEqual(get_revision_results.call_count, 4)
    self.assertEqual(self.upload_to_cloud.call_count, 2)

  def testGetRevisionResults_simple(self):
    item = StateItem('2a66ba', timestamp='2019-03-17T23:50:16-07:00')
    csv = [
        'change,benchmark,story,name,unit,mean\n',
        '2a66ba,loading,story1,Total:duration,ms_smallerIsBetter,300.0\n',
        '2a66ba,loading,story2,Total:duration,ms_smallerIsBetter,400.0\n',
        '2a66ba+patch,loading,story1,Total:duration,ms_smallerIsBetter,100.0\n',
        '2a66ba+patch,loading,story2,Total:duration,ms_smallerIsBetter,200.0\n',
        '2a66ba,loading,story1,Other:metric,count_smallerIsBetter,1.0\n']
    expected_results = [
        ('without_patch', 0.35, '2018-03-17T12:00:00'),
        ('with_patch', 0.15, '2019-03-17T12:00:00'),
    ]

    filename = pinboard.RevisionResultsFile(item)
    with open(filename, 'w') as f:
      f.writelines(csv)

    with mock.patch('cli_tools.pinboard.pinboard.ACTIVE_STORIES',
                    new=['story1', 'story2']):
      df = pinboard.GetRevisionResults(item)

    self.assertEqual(len(df.index), 2)  # Only two rows of output.
    self.assertTrue((df['revision'] == '2a66ba').all())
    self.assertTrue((df['benchmark'] == 'loading').all())
    self.assertTrue((df['name'] == 'Total:duration').all())
    self.assertTrue((df['count'] == 2).all())
    df = df.set_index('label', verify_integrity=True)
    for label, value, timestamp in expected_results:
      self.assertEqual(df.loc[label, 'mean'], value)
      self.assertEqual(df.loc[label, 'timestamp'], pd.Timestamp(timestamp))

  def testGetRevisionResults_empty(self):
    item = StateItem('2a66ba', timestamp='2019-03-17T23:50:16-07:00')
    csv = [
        'change,benchmark,story,name,unit,mean\n',
        '2a66ba,loading,story1,Other:metric,count_smallerIsBetter,1.0\n']

    filename = pinboard.RevisionResultsFile(item)
    with open(filename, 'w') as f:
      f.writelines(csv)

    df = pinboard.GetRevisionResults(item)
    self.assertEqual(len(df.index), 1)  # Only one row of output.
    row = df.iloc[0]
    self.assertEqual(row['revision'], '2a66ba')
    self.assertEqual(row['count'], 0)

  @mock.patch('cli_tools.pinboard.pinboard.FindCommit')
  def testGetLastCommitOfDate_simple(self, find_commit):
    commit_before = ('2a66bac4', '2019-03-17T23:50:16-07:00')
    commit_after = ('5aefdb31', '2019-03-18T02:41:58-07:00')
    find_commit.side_effect = [commit_after, commit_before]

    date = pd.Timestamp('2019-03-17 04:01:01', tz=pinboard.TZ)
    return_value = pinboard.GetLastCommitOfDate(date)

    cutoff_date = pd.Timestamp('2019-03-18 00:00:00', tz=pinboard.TZ)
    find_commit.assert_has_calls([
        mock.call(after_date=cutoff_date),
        mock.call(before_date=cutoff_date)])
    self.assertEqual(return_value, commit_before)

  @mock.patch('cli_tools.pinboard.pinboard.FindCommit')
  def testGetLastCommitOfDate_failed(self, find_commit):
    commit_before = ('2a66bac4', '2019-03-17T23:50:16-07:00')
    find_commit.side_effect = [None, commit_before]

    date = pd.Timestamp('2019-03-17 04:01:01', tz=pinboard.TZ)
    with self.assertRaises(ValueError):
      pinboard.GetLastCommitOfDate(date)

    cutoff_date = pd.Timestamp('2019-03-18 00:00:00', tz=pinboard.TZ)
    find_commit.assert_has_calls([
        mock.call(after_date=cutoff_date)])

  def testFindCommit_simple(self):
    self.subprocess.check_output.return_value = '2a66bac4:1552891816\n'
    date = pd.Timestamp('2019-03-18T00:00:00', tz=pinboard.TZ)
    revision, timestamp = pinboard.FindCommit(before_date=date)
    self.subprocess.check_output.assert_called_once_with(
        ['git', 'log', '--max-count', '1', '--format=format:%H:%ct',
         '--before', '2019-03-18T00:00:00-07:00', 'origin/master'],
        cwd=pinboard.TOOLS_PERF_DIR)
    self.assertEqual(revision, '2a66bac4')
    self.assertEqual(timestamp, '2019-03-17T23:50:16-07:00')

  def testFindCommit_notFound(self):
    self.subprocess.check_output.return_value = ''
    date = pd.Timestamp('2019-03-18T00:00:00', tz=pinboard.TZ)
    return_value = pinboard.FindCommit(after_date=date)
    self.subprocess.check_output.assert_called_once_with(
        ['git', 'log', '--max-count', '1', '--format=format:%H:%ct',
         '--after', '2019-03-18T00:00:00-07:00', 'origin/master'],
        cwd=pinboard.TOOLS_PERF_DIR)
    self.assertIsNone(return_value)
