# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def ChangeToStr(change):
  """Turn a pinpoint change dict into a string id."""
  change_id = ','.join(
      '{repository}@{git_hash}'.format(**commit)
      for commit in change['commits'])
  if 'patch' in change:
    change_id += '+' + change['patch']['url']
  return change_id


def IterTestOutputIsolates(job, only_differences=False):
  """Iterate over test execution results for all changes tested in the job.

  Args:
    job: A pinpoint job dict with state.

  Yields:
    (change_id, isolate_hash) pairs for each completed test execution found in
    the job.
  """
  quests = job['quests']
  for change_state in job['state']:
    if only_differences and not any(
        v == 'different' for v in change_state['comparisons'].itervalues()):
      continue
    change_id = ChangeToStr(change_state['change'])
    for attempt in change_state['attempts']:
      executions = dict(zip(quests, attempt['executions']))
      if 'Test' not in executions:
        continue
      test_run = executions['Test']
      if not test_run['completed']:
        continue
      try:
        isolate_hash = next(
            d['value'] for d in test_run['details'] if d['key'] == 'isolate')
      except StopIteration:
        continue
      yield change_id, isolate_hash
