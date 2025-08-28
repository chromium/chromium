#!/usr/bin/env python

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import logging
import pathlib
import tempfile
from util import jj_log
from util import run_command
from util import run_jj

_IMMUTABLE_PARENTS = 'parents.filter(|p| p.immutable()).map(|p| p.commit_id())'
_MUTABLE_PARENTS = 'parents.filter(|p| !p.immutable()).map(|p| p.commit_id())'
_NAME = '''change_id.short() ++
if(current_working_copy, " (@)") ++
" " ++
description.first_line()
'''


def fatal(*args, **kwargs):
  logging.critical(*args, **kwargs)
  exit(1)


def _collect_ids(values):
  ids = set()
  for value in values:
    if value:
      ids.update(value.split(' '))
  return ids


def main(args):
  logging.basicConfig(level=logging.getLevelNamesMapping()[args.verbosity])

  revs = args.revisions + args.revision
  if len(revs) == 0:
    fatal('No revision specified to upload')
  elif len(revs) == 1:
    rev = revs[0]
  else:
    rev = '|'.join(f'({r})' for r in revs)

  snapshot_taken = False

  if args.fix:
    run_jj(['fix', '-s', f'mutable()::({rev})'])
    # After running fix, jj creates another snapshot.
    snapshot_taken = True

  to_upload = jj_log(
      revisions=f'mutable()::({rev})',
      templates={
          'name': _NAME,
          'commit_id': 'commit_id',
          'empty': 'empty',
          'desc': 'description',
          'mutable_parents': _MUTABLE_PARENTS,
      },
      ignore_working_copy=snapshot_taken,
  )
  snapshot_taken = True
  for change in to_upload:
    name = change['name']
    desc = change['desc']
    # Don't trust `git cl presubmit` to pick up on these for stacked changes,
    # since it assumes all the commits will be squashed.
    if change['empty'] == 'true':
      fatal('Attempting to upload an empty change %s', name)
    if not desc:
      fatal('Attempting to upload change with an empty description %s', name)
    if '\nChange-Id: ' not in desc:
      fatal('Attempting to upload change with no Change-Id %s', name)
    if '\nBug: ' not in desc and '\nFixed: ' not in desc:
      logging.warning(
          'Change %s has no associated Bug. If this change has an associated' +
          'bug, add Bug: [bug number] or Fixed: [bug number].', name)

  if args.presubmit:
    # Find the commits that `git cl presubmit` will actually run on
    got_presubmits = jj_log(
        revisions=f'mutable()::@',
        templates={
            'name': _NAME,
            'empty': 'empty',
            'immutable_parents': _IMMUTABLE_PARENTS
        },
        ignore_working_copy=snapshot_taken,
    )

    # We could simplify this with another call to jj_log, but each call to
    # jj_log can take a nontrivial amount of time.
    immutable_parents = _collect_ids(c['immutable_parents']
                                     for c in got_presubmits)
    if len(immutable_parents) != 1:
      fatal(
          '%s has multiple different immutable parents of mutable ancestors. ' +
          'Fix with a rebase or jj simplify-parents.',
          rev,
      )

    want_presubmits = {x['name'] for x in to_upload if x['empty'] == 'false'}
    got_presubmits = {
        x['name']
        for x in got_presubmits if x['empty'] == 'false'
    }

    if want_presubmits.intersection(got_presubmits):
      for change in got_presubmits - want_presubmits:
        logging.warning("Running presubmit on additional non-empty revision %s",
                        change)
      for change in want_presubmits - got_presubmits:
        logging.warning("Presubmits will be skipped for %s", change)

      # Git CL presubmit is very annoying with stacked commits. It:
      # * Always runs with files from @
      #   * This means that if you have an error in a parent commit fixed in a
      #     child commit, it won't pick up on it.
      # * Simply concatenates the CL descriptions from mutable()::@-
      #   * It ignores the CL description for @
      #   * If, for example the first commit in the chain has an associated bug,
      #     it won't warn you other commits in the chain don't have one.
      # This isn't any worse with jj than with git, but it is very annoying.
      # In particular, if you're uploading any commit except @-, expect some
      # weirdness.
      with tempfile.NamedTemporaryFile(suffix='.json') as out:
        run_command([
            'git',
            'cl',
            'presubmit',
            # Allows it to run with a dirty tree and on no branch
            '--force',
            '--parallel',
            # TODO(crbug.com//40253731): Remove --upload once this is fixed
            '--upload',
            f'--json={out}',
            next(iter(immutable_parents))
        ])
        results = json.loads(out.read_text())
        if results.get('errors', []) or results.get('warnings', []):
          if not args.allow_warnings:
            fatal('git cl presubmit had warnings.\n' +
                  'Hint: maybe you want --allow-warnings?')
    else:
      fatal('git cl presubmit only supports running on the revision @. ' +
            'Please either run `jj new/edit` to check out the change before ' +
            'uploading it, or rerun with `--no-presubmit`')

  # This could be simplified by another call to jj_log on heads(...),
  # but this is more performant.
  mutable_parents = _collect_ids(c['mutable_parents'] for c in to_upload)

  if not to_upload:
    fatal('%s resolved to the empty set', rev)

  for change in to_upload:
    # Check if it's a head.
    if change['commit_id'] not in mutable_parents:
      cmd = ['git', 'push', 'origin', f'{change["commit_id"]}:refs/for/main']
      logging.info('Uploading %s', change['name'])
      if args.dry_run:
        logging.info('Dry-run: Would otherwise run `%s`', ' '.join(cmd))
      else:
        run_command(cmd)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--verbosity',
      help='Verbosity of logging',
      default='INFO',
      choices=logging.getLevelNamesMapping().keys(),
      type=lambda x: x.upper(),
  )

  # Alternative form so users can write `upload -r foo` as well as `upload foo``
  parser.add_argument('-r', '--revision', help=None, nargs='*', default=[])

  parser.add_argument('revisions', help='Revisions to upload', nargs='*')

  parser.add_argument(
      '--no-fix',
      help='Skips running `jj fix` before uploading',
      action='store_false',
      dest='fix',
  )

  parser.add_argument(
      '--dry-run',
      help='Skips performing the `git push` step',
      action='store_true',
  )

  parser.add_argument(
      '--no-presubmit',
      help='Skips running presubmits before uploading',
      action='store_false',
      dest='presubmit',
  )

  parser.add_argument(
      '--allow-warnings',
      help='Prevents presubmit warnings from blocking upload',
      action='store_true',
  )

  main(parser.parse_args())
