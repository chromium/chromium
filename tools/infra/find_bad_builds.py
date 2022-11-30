#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
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

# Provided by root level .vpython3 file
import pytz
from dateutil.tz import tzlocal

import git_utils


def _find_builds(predicate):
  """Finds buildbucket builds which satisfy the given predicate."""
  logging.debug('Query buildbucket with predicate: %s',
                json.dumps(predicate, indent=2))
  pred_json = json.dumps(predicate)

  bb_args = ['bb', 'ls', '-json', '-predicate', pred_json]
  return subprocess.check_output(bb_args).strip().splitlines()


def _get_build_running_time(build):
  """Gets the build's current runtime.

  A build's current runtimes which is the difference between the current actual
  time and the build's start time.

  Args:
    build: A dict containing information about a build.

  Returns:
    The build's current runtime as a datetime.timedelta.
  """
  # The timestamp strings buildbucket gives us have a '.123123' bit at the end,
  # which is the microseconds. For some reason, python3 doesn't allow that in
  # datetime timestamps. We don't need this anyways, so just get rid of it and
  # parse the rest of the string.
  t = build['startTime']
  t = t.split('.')[0]
  date = datetime.datetime.strptime(t, '%Y-%m-%dT%H:%M:%S')
  return datetime.datetime.now(tzlocal()) - pytz.timezone('UTC').localize(date)


def _assess_build(build, max_running_time, revisions_in_scope, builders):
  """Assesses a build, to determine if it's 'bad'.

  Multiple criteria are used here. The build must be considered 'bad' by every
  stage of analysis for it to be fully assessed as bad.

  This argument is usually called via functools.partial, with everything but
  the first argument in the partial.

  Args:
    build: The build json of a buildbucket build.
    max_running_time: The maximum amount of time a build could be running. Any
      build which has been running for longer than this is considered 'good'.
      This is a datetime.timedelta object.
    revisions_in_scope: The list of revisions that are considered 'bad'. Any
      build which has one of these revisions is considered 'bad'.
    builders: A list of builders which are considered 'bad'.

  Returns:
    If the build is 'bad'.
  """
  bid = build['id']
  if builders and build['builder']['builder'] not in builders:
    logging.debug('builder of build %s not in %s', bid, builders)
    return False
  if _get_build_running_time(build) >= max_running_time:
    logging.debug('build %s has duration >= %s', bid, max_running_time)
    return False
  return _fetch_build_revision(bid) in revisions_in_scope


# FIXME: Could cache this on disk to make repeated calls to the script fast.
def _fetch_build_revision(bid):
  """Fetches the chromium/src revision checked out in the build.

  Args:
    bid: A buildbucket id.

  Returns:
    The revision the build checked out, or None if either:
      * The build didn't run bot_update
      * The build didn't check out chromium/src at all. Usually this happens due
        to a patch error
  """
  bid = bid.strip()
  try:
    output = subprocess.check_output(
        ['bb', 'log', bid, 'bot_update', 'json.output'],
        # If the build is missing, it dumps to stderr. We handle that, so don't
        # print any errors.
        stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError:
    logging.warning('build %s is missing bot_update. Ignoring...' % bid)
    return None

  contents = json.loads(output)
  # (usually a) patch failure. These builds didn't end up checking anything out,
  # so just ignore them.
  if 'manifest' not in contents:
    return None

  src_manifest = contents['manifest']['src']
  assert src_manifest['repository'] == (
      'https://chromium.googlesource.com/chromium/src.git')
  rev = src_manifest['revision']
  logging.debug('build %s has revision %s', bid, rev)
  return rev


def _get_pool():
  # Returns a multiprocessing pool. Exists for mocking in tests.
  return multiprocessing.Pool()


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
  parser.add_argument(
      'max_running_time',
      help='Only output builds which have been running for less time than this'
      ' (in minutes).',
      type=float)
  parser.add_argument(
      '--show_all_builds',
      '-s',
      action='store_true',
      help='Show all builds, along with information for each build. Useful when'
      ' manually inspecting the output of this tool.')
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
  args = parser.parse_args(raw_args)
  args.max_running_time = datetime.timedelta(minutes=args.max_running_time)
  return args

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

  revisions_in_scope = set(
      git_utils.get_revisions_between(bad_commit, good_commit) + [bad_commit])
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
  logging.info('%d total builds to process' % len(build_jsons))

  p = _get_pool()
  logging.debug('List of known bad revisions:')
  for rev in sorted(revisions_in_scope):
    logging.debug('  * %s', rev)
  results = p.map(
      functools.partial(_assess_build,
                        max_running_time=args.max_running_time,
                        revisions_in_scope=revisions_in_scope,
                        builders=args.builder), build_jsons)
  if args.show_all_builds:
    rows = []
    header = ('Build ID', 'is_bad', 'running time (minutes)')
    # Build IDS are 19 characters.
    column_lens = [20, 10, len(header[-1])]
    for build, is_bad_build in zip(build_jsons, results):
      bid = build['id']
      running_time = _get_build_running_time(build).total_seconds() / 60.0
      rows.append((bid, is_bad_build, int(running_time)))
    for row in [header] + sorted(rows, key=lambda r: r[0]):
      print_fn("%s | %s | %s" % tuple(
          (str(itm).ljust(column_lens[i]) for i, itm in enumerate(row))))
      if row == header:
        print_fn('-' * sum(column_lens))
  else:
    ids = [build['id'] for build in build_jsons]
    for bid, is_bad_build in zip(ids, results):
      if is_bad_build:
        print_fn(bid)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:], print))
