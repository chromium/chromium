#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
from util import jj_log
from util import run_command
from util import run_jj


def _fetch(shallow: bool) -> None:
  args = ['git', 'fetch', 'origin', 'main']
  if shallow:
    # Do something similar to a shallow clone with depth 2
    # For rationale, see:
    # https://stackoverflow.com/questions/66431436/pushing-to-github-after-a-shallow-clone-is-horribly-slow
    history_limit = jj_log(revisions='parents(fork_point(parents(mutable())))',
                           templates={'commit_id': 'commit_id'},
                           ignore_working_copy=True)
    assert len(history_limit) == 1
    history_limit = history_limit[0]['commit_id']
    args.append(f'--shallow-exclude={history_limit}')
  run_command(args)


def main(args):
  logging.basicConfig(level=logging.getLevelNamesMapping()[args.verbosity])

  _fetch(args.shallow)

  logging.info('Rebasing onto main@origin')
  rebase_source = 'mutable()' if args.all else '@'
  run_jj(['rebase', '-b', rebase_source, '-d', 'trunk()', '--skip-emptied'])
  # Skip-emptied with merge commits can produce weird shapes.
  run_jj(['simplify-parents', '-r', 'mutable()'], ignore_working_copy=True)

  while True:
    # This can fail if you've changed third-party repos. Since git fetch can be
    # quite slow, we make this step able to retry on failure.
    logging.info('Running gclient sync')
    ps = run_command(['gclient', 'sync', '-D'], check=False)
    if ps.returncode == 0:
      break
    else:
      input('press control-C to exit, or enter to retry gclient sync')


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--verbosity',
      help='Verbosity of logging',
      default='INFO',
      choices=logging.getLevelNamesMapping().keys(),
      type=lambda x: x.upper(),
  )

  parser.add_argument(
      '-a',
      '--all',
      help='Rebases all local changes onto head',
      action='store_true',
  )

  parser.add_argument(
      '-s',
      '--shallow',
      help=
      'Garbage-collects all commits before the common ancestor of all mutable commits',
      action='store_true',
  )

  main(parser.parse_args())
