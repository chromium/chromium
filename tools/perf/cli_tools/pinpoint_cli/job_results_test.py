# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from cli_tools.pinpoint_cli import job_results


def Change(*args, **kwargs):
  commits = []
  for arg in args:
    repository, git_hash = arg.split('@')
    commits.append({'repository': repository, 'git_hash': git_hash})
  change = {'commits': commits}
  patch = kwargs.pop('patch', None)
  if patch is not None:
    change['patch'] = {'url': patch}
  return change


def Execution(**kwargs):
  execution = {'completed': kwargs.pop('completed', True)}
  execution['details'] = [
      {'key': k, 'value': v} for k, v in kwargs.iteritems()]
  return execution


class TestJobResults(unittest.TestCase):
  def testChangeToStr(self):
    self.assertEqual(
        job_results.ChangeToStr(Change('src@1234')),
        'src@1234')
    self.assertEqual(
        job_results.ChangeToStr(
            Change('src@1234', 'v8@4567', patch='crrev.com/c/123')),
        'src@1234,v8@4567+crrev.com/c/123')

  def testIterTestOutputIsolates(self):
    job = {
        'quests': ['Build', 'Test', 'Get results'],
        'state': [
            {
                'change': Change('src@1234'),
                'attempts': [
                    {
                        'executions': [
                            Execution(),  # Build
                            Execution(isolate='results1'),  # Test
                            Execution()  # Get results
                        ]
                    },
                    {
                        'executions': [
                            Execution(),  # Build
                            Execution(),  # Test (completed but failed)
                        ]
                    },
                    {
                        'executions': [
                            Execution(),  # Build
                            Execution(isolate='results3'),  # Test
                            Execution(completed=False)  # Get results
                        ]
                    }
                ]
            },
            {
                'change': Change('src@1234', patch='crrev.com/c/123'),
                'attempts': [
                    {
                        'executions': [
                            Execution(),  # Build
                            Execution(isolate='results4'),  # Test
                            Execution()  # Get results
                        ]
                    },
                    {
                        'executions': [
                            Execution(),  # Build
                            Execution(completed=False)  # Test
                        ]
                    },
                    {
                        'executions': [
                            Execution(completed=False)  # Build
                        ]
                    }
                ]
            }
        ]
    }
    self.assertSequenceEqual(
        list(job_results.IterTestOutputIsolates(job)),
        [
            ('src@1234', 'results1'),
            ('src@1234', 'results3'),
            ('src@1234+crrev.com/c/123', 'results4')
        ])
