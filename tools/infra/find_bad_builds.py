#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Find builds which contain a bad CL.

A 'bad CL' is (usually) a CL which was later reverted. The simplest usage of
this script is simply to call it with a revert CL. The script will find the
associated CL in your git checkout, and then search for buildbucket try builds
which executed with the original CL, but not the reverted CL.

This script also has filter parameters for various attributes of builds, like
build duration or project/bucket/builder.

This script uses your chromium/src checkout, so you must keep it updated if you
want this to be able to cancel recent builds.
"""

from __future__ import print_function

import argparse
import datetime
import functools
import json
import logging
import multiprocessing
import subprocess
import sys

import git_utils


def _find_builds(predicate):
  """Finds buildbucket builds which satisfy the given predicate."""
  logging.debug('Query buildbucket with predicate: %s',
                json.dumps(predicate, indent=2))
  pred_json = json.dumps(predicate)

  bb_args = ['bb', 'ls', '-json', '-predicate', pred_json]
  return subprocess.check_output(bb_args).strip().splitlines()


def _parse_args(raw_args):
  """Parses command line arguments."""
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      'good_revision',
      help=
      "A known good revision. Builds which start at revisions after this will"
      " not be canceled. If this revision is the revert of an earlier revision,"
      " that revision will be set to bad_revision. If this isn't a revert,"
      " bad_revision is required.")
  parser.add_argument(
      'bad_revision',
      help=
      "A known bad revision. This is usually automatically calculated from the"
      " good revision (assuming it's a revert).")
  # FIXME: This is imperfect in some scenarios. For example, if we want to
  # cancel all linux builds, we'd have to manually specify ~15 different
  # builders (at least). We should potentially allow for filtering based on
  # the swarming task dimensions.
  parser.add_argument(
      '--builder',
      '-b',
      action='append',
      help='Which builder should we find builds for. If not set, finds all'
      ' builds in the given project/bucket. May be used multiple times. If'
      ' multiple builders are specified, this script has to fetch all builds'
      ' in the bucket, which is a bit slow. Specifying one builder is fast,'
      ' however.')
  parser.add_argument('--project',
                      default='chromium',
                      help='The buildbucket project to search for builds in.')
  parser.add_argument('--bucket',
                      default='try',
                      help='The buildbucket bucket to search for builds in')
  parser.add_argument(
      '--verbose',
      '-v',
      action='count',
      default=0,
      help=
      'Use for more logging. Can use multiple times to increase logging level.')
  return parser.parse_args(raw_args)


# FIXME: Add support for time based cancellations. This could be used for
# issues which don't show up via chromium/src commits.
def main(raw_args, print_fn):
  """Runs the script.

  Args:
    raw_args: The raw command line arguments.
    print_fn: Function to print a line to the screen. Overridden for tests.

  Returns:
    The exit code of the program.
  """
  args = _parse_args(raw_args)

  # With 0 verbose, uses ERROR level. Min level is DEBUG. See logging module for
  # the constants.
  level = max(40 - args.verbose * 10, 10)
  logging.basicConfig(level=level)

  good_commit = args.good_revision
  bad_commit = args.bad_revision
  # FIXME: Handle only bad revision? Not sure if a reasonable scenario where
  # we'd want to do that exists.

  revert_date = git_utils.get_commit_date(good_commit)
  orig_date = git_utils.get_commit_date(bad_commit)
  # Add 20 minutes to account for git replication delay. Sometimes gerrit
  # doesn't realize a commit has landed for a few minutes, so builds which start
  # after the 'good' commit landed might still not contain it. We filter by
  # commit below, but want to make sure we have a buffer of builds when we
  # search in buildbucket.
  revert_date += datetime.timedelta(minutes=20)
  logging.debug('Good commit: %s\t%s', good_commit, revert_date)
  logging.debug('Bad Commit: %s\t%s', bad_commit, orig_date)

  predicate = {
      'builder': {
          'bucket': args.bucket,
          'project': args.project,
      },
      'createTime': {
          # We already assumed UTC, so add it in the format buildbucket expects.
          'startTime': orig_date.strftime('%Y-%m-%dT%H:%M:%S') + '+00:00',
          'endTime': revert_date.strftime('%Y-%m-%dT%H:%M:%S') + '+00:00',
      },
      'status': 'STARTED',
  }

  # If we have one builder, buildbucket can filter when we do the RPC. If we
  # have more than one, we have to fetch builds for all builders, then filter
  # after. Theoretically we could run multiple `bb` invocations and merge them
  # together, but that doesn't seem super necessary.
  if args.builder and len(args.builder) == 1:
    predicate['builder']['builder'] = args.builder[0]
  resp = _find_builds(predicate)
  build_jsons = [json.loads(x) for x in resp]

  # TODO: Filter builds found by buildbucket.

  ids = [build['id'] for build in build_jsons]
  for bid in ids:
    print_fn(bid)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:], print))
