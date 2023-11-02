# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Git utilities."""

import datetime
import json
import logging
import subprocess


def _run_git(*cmd):
  """Runs a git command and returns the output."""
  cmd = ['git'] + list(cmd)
  return subprocess.check_output(cmd)


def _get_commit_message(rev):
  """Gets the commit message for a revision."""
  return _run_git('log', '--format=%B', '-n', '1', rev)


def get_commit_date(rev):
  """Gets the date a commit was committed."""
  raw_date = _run_git('show', '--no-patch', '--no-notes', '--pretty=%cd',
                      rev).strip().decode('utf-8')
  # The last space separate section is timezone. '%z' doesn't let us parse this
  # because python datetime (in 2.7, at least) doesn't support parsing timezones
  # by default.
  split = raw_date.split(' ')
  raw_date, tz = ' '.join(split[:-1]), split[-1]
  # `git log` seems to always give us dates in UTC. Parsing the UTC timezone
  # itself is hard, so just enforce that we always get UTC for now.
  assert tz == '+0000', 'Expected git timezone %s, got %s.' % ('+0000', tz)
  return datetime.datetime.strptime(raw_date.strip(), '%a %b %d %H:%M:%S %Y')


def get_revisions_between(commit1, commit2):
  """Gets the list of revisions between commit1 and commit2.

  commit1 must have been committed before commit2.

  Args:
    commit1: A git commit hash.
    commit2: A git commit hash.

  Returns:
    A list of git commits between the two commits, not including commit1 or
      commit2.
  """
  lines = _run_git('log', '--format=oneline', '%s..%s' % (commit1, commit2))
  return [l.split()[0].strip().decode('utf-8') for l in lines.splitlines()]
