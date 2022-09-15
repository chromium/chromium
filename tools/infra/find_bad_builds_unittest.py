#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for find_bad_builds."""

from __future__ import division
from __future__ import print_function

import contextlib
import datetime
import json

import argparse
import unittest

import git_utils
import find_bad_builds

import mock


class Pool:
  """Simple object used to mock out multiprocessing.Pool."""

  def map(self, fn, lst):
    return map(fn, lst)


@mock.patch('find_bad_builds._get_pool')
class FindBadBuildsIntegrationTest(unittest.TestCase):
  """Integration tests for find_bad_builds.

  This only tests that the script works from an end to end perspective. It mocks
  all the interactions with git and buildbucket in the script. That
  functionality is fairly simple, and doesn't need to be tested.
  """

  def _gen_build(self,
                 bid,
                 rev,
                 running_time,
                 builder='linux-rel',
                 bucket='try',
                 project='chromium'):
    """Generates a buildbucket dict representing a single build."""
    return {
        'id': str(bid),
        'revision': rev,
        'running_time': running_time,
        'builder': {
            'builder': builder,
            'bucket': bucket,
            'project': project
        },
    }

  @contextlib.contextmanager
  def _setup_git(self, commits, first_commit_date=None, time_gap=1):
    """Sets up git mocks.

    Args:
      commits: An ordered list of commit hashes. The first commit is assumed to
        be the most recent.
      first_commit_date: When the first commit starts. Datetime object.
      time_gap: How long between git commits (minutes).
    Returns:
      Nothing. Yields to the caller, to allow the patches to be used during the
        test.
    """
    if not first_commit_date:
      first_commit_date = datetime.datetime(2020, 1, 1, 0, 0, 0)
    with mock.patch('git_utils.get_revisions_between') as rev_btwn:

      def find_btwn(commit1, commit2):
        # technically inefficient, but probably fine.
        first = commits.index(commit1)
        last = commits.index(commit2)
        # Only want commits in between these two
        return commits[first + 1:last]

      rev_btwn.side_effect = find_btwn

      with mock.patch('git_utils.get_commit_date') as rev_date:

        def calc_date(rev):
          ind = commits.index(rev)
          return first_commit_date + datetime.timedelta(minutes=ind * time_gap)

        rev_date.side_effect = calc_date

        yield

  @contextlib.contextmanager
  def _setup_buildbucket(self, builds):
    """Sets up buildbucket mocks.

    Args:
      builds: A list of buildbucket builds (dict).

    Yields:
      The mock object for '_find_builds'. Callers can modify its side_effect to
      verify attributes of the predicate passed to buildbucket.
    """
    with mock.patch('find_bad_builds._find_builds') as find_builds:
      find_builds.return_value = [json.dumps(b) for b in builds]
      with mock.patch('find_bad_builds._fetch_build_revision') as fetch_rev:

        def find_rev(bid):
          for b in builds:
            if b['id'] == bid:
              return b['revision']
          self.fail('build %s is missing a revision' % bid)
          return None

        fetch_rev.side_effect = find_rev
        with mock.patch('find_bad_builds._get_build_running_time') as b_runtime:
          b_runtime.side_effect = lambda build: datetime.timedelta(minutes=int(
              build['running_time']))
          # Yield find_builds so callers can assert on the predicate being
          # passed to find_builds.
          yield find_builds

  def _run(self, args):
    """Runs the script. Returns the exit code and printed lines."""
    lines = []
    return find_bad_builds.main(args, lines.append), set(lines)

  def test_simple(self, get_pool):
    get_pool.return_value = Pool()
    with self._setup_git(['1,', '2', '3', '4', '5']):
      with self._setup_buildbucket([
          self._gen_build(11, '1', 5),
          self._gen_build(22, '2', 4),
          self._gen_build(33, '3', 3),
          self._gen_build(44, '4', 2),
          self._gen_build(55, '5', 1),
      ]) as find_builds:
        rval = find_builds.return_value

        def se(predicate):
          self.assertEqual(
              predicate['createTime']['startTime'],
              # commit '2' commit time.
              '2020-01-01T00:01:00+00:00')
          self.assertEqual(
              predicate['createTime']['endTime'],
              # commit '4' commit time, plus git replication lag.
              '2020-01-01T00:23:00+00:00')
          return rval

        find_builds.side_effect = se
        self.assertEqual(
            self._run(['4', '2', '100']),
            (
                # Only builds with revision '2' and '3' should be cancelled. '4'
                # has the good commit, so it's fine to run.
                0,
                set(['22', '33'])))

  def test_print_builds(self, get_pool):
    get_pool.return_value = Pool()
    with self._setup_git(['1,', '2', '3', '4', '5']):
      with self._setup_buildbucket([
          self._gen_build(11, '1', 29),
          self._gen_build(22, '2', 28),
          self._gen_build(33, '3', 27),
          self._gen_build(44, '4', 26),
          self._gen_build(55, '5', 25),
      ]):
        retcode, lines = self._run(['4', '2', '100', '-s'])
        self.assertEqual(retcode, 0)
        # Sort to get rid of header, then remove.
        lines = sorted(lines)
        self.assertTrue(lines[-1].startswith('Build '))
        self.assertTrue(lines[0].startswith('------'))
        self.assertEqual(
            set(lines[1:-1]),
            set([
                '11                   | False      | 29                    ',
                '22                   | True       | 28                    ',
                '33                   | True       | 27                    ',
                '44                   | False      | 26                    ',
                '55                   | False      | 25                    ',
            ]))

  def test_build_time_filter(self, get_pool):
    get_pool.return_value = Pool()
    with self._setup_git(['1,', '2', '3', '4', '5']):
      with self._setup_buildbucket([
          self._gen_build(11, '1', 31),
          self._gen_build(22, '2', 29),
          self._gen_build(33, '3', 27),
          self._gen_build(34, '3', 26),
          self._gen_build(44, '4', 25),
          self._gen_build(55, '5', 23),
      ]):
        self.assertEqual(self._run(['4', '2', '28']), (0, set(['34', '33'])))

  def test_builder_filter(self, get_pool):
    get_pool.return_value = Pool()
    with self._setup_git(['1,', '2', '3', '4', '5']):
      with self._setup_buildbucket([
          self._gen_build(11, '1', 4),
          self._gen_build(21, '2', 5, builder='linux-rel'),
          self._gen_build(22, '2', 5, builder='mac-rel'),
          self._gen_build(23, '2', 5, builder='win-rel'),
          self._gen_build(33, '3', 3),
          self._gen_build(44, '4', 2),
          self._gen_build(55, '5', 1),
      ]):
        self.assertEqual(
            self._run(['4', '2', '100', '-b', 'linux-rel', '-b', 'mac-rel']),
            (0, set(['21', '22', '33'])))

  def test_git_replication(self, get_pool):
    get_pool.return_value = Pool()
    with self._setup_git(['1,', '2', '3', '4', '5']):
      with self._setup_buildbucket([
          self._gen_build(11, '1', 6),
          self._gen_build(22, '2', 5),
          self._gen_build(33, '3', 4),
          self._gen_build(44, '4', 3),
          self._gen_build(55, '5', 2),
          # Sometimes git has replication lag, which means that builds can get
          # old git data. In this case, a build which ran after '5', which was
          # committed after '4', gets '4' as the HEAD revision. This can happen
          # in prod.
          self._gen_build(66, '3', 1),
      ]):
        self.assertEqual(self._run(['4', '2', '100']),
                         (0, set(['22', '33', '66'])))


if __name__ == '__main__':
  unittest.main()
