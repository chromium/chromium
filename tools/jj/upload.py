#!/usr/bin/env python3

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
from util import join_revsets
from util import split_description

_IMMUTABLE_PARENTS = 'parents.filter(|p| p.immutable()).map(|p| p.commit_id())'
_MUTABLE_PARENTS = 'parents.filter(|p| !p.immutable()).map(|p| p.commit_id())'


def fatal(*args, **kwargs):
  logging.critical(*args, **kwargs)
  exit(1)


def _collect_ids(values):
  ids = set()
  for value in values:
    if value:
      ids.update(value.split(' '))
  return ids


def get_refspec_opts(args) -> list[str]:
  # Extra options that can be specified at push time. Doc:
  # https://gerrit-review.googlesource.com/Documentation/user-upload.html
  refspec_opts = []
  if args.topic:
    # Documentation on Gerrit topics is here:
    # https://gerrit-review.googlesource.com/Documentation/user-upload.html#topic
    refspec_opts.append(f'topic={args.topic}')

  # Code mostly stolen from `git_cl.py`
  if args.private:
    refspec_opts.append('private')
  if args.send_mail:
    refspec_opts.append('ready')
    refspec_opts.append('notify=ALL')
  if args.enable_auto_submit:
    refspec_opts.append('l=Auto-Submit+1')
  if args.enable_owners_override:
    refspec_opts.append('l=Owners-Override+1')
  if args.use_commit_queue:
    refspec_opts.append('l=Commit-Queue+2')
  elif args.cq_dry_run:
    refspec_opts.append('l=Commit-Queue+1')
  for cc in args.cc:
    refspec_opts.append(f'cc={cc}')
  for reviewer in args.reviewers:
    refspec_opts.append(f'r={reviewer}')
  return refspec_opts


def main(args):
  logging.basicConfig(level=logging.getLevelNamesMapping()[args.verbosity])

  revs = args.revisions + args.revision
  implicit_revs = False
  # If no revisions are provided, we will upload `@` unless it is empty and
  # descriptionless, in which case we upload 'parents(@)'.
  if len(revs) == 0:
    rev = '@'
    implicit_revs = True
  else:
    rev = join_revsets(revs)

  snapshot_taken = False

  if args.fix:
    run_jj(['fix', '-s', f'mutable()::({rev})'])
    # After running fix, jj creates another snapshot.
    snapshot_taken = True

  to_upload = jj_log(
      revisions=f'mutable()::({rev})',
      templates={
          'commit_id': 'commit_id',
          'empty': 'empty',
          'desc': 'description',
          'mutable_parents': _MUTABLE_PARENTS,
      },
      ignore_working_copy=snapshot_taken,
  )
  snapshot_taken = True
  if implicit_revs:
    # It's in reverse topological order, so to_upload[0] is the working copy '@'
    wc = to_upload[0]
    if not split_description(wc['desc'])[0] and wc['empty'] == 'true':
      revs = ['parents(@)']
      logging.info('No revisions provided and working copy is empty and ' +
                   'descriptionless, uploading parents(@)')
      to_upload.remove(wc)
    else:
      revs = '@'
      logging.info('No revisions provided, uploading working copy')

  for change in to_upload:
    name = change['name']
    desc, trailers = split_description(change['desc'])
    # Don't trust `git cl presubmit` to pick up on these for stacked changes,
    # since it assumes all the commits will be squashed.
    if change['empty'] == 'true':
      fatal('Attempting to upload an empty change %s', name)
    if not desc:
      fatal('Attempting to upload change with an empty description %s', name)
    if 'Bug' not in trailers and 'Fixed' not in trailers:
      logging.warning(
          'Change %s has no associated Bug. If this change has an associated ' +
          'bug, run `jj bug add [--inherit]`', name)

  if not args.bypass_hooks:
    # Find the commits that `git cl presubmit` will actually run on
    got_presubmits = jj_log(
        revisions=f'mutable()::@',
        templates={
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
        out = pathlib.Path(out.name)
        run_command([
            'git',
            'cl',
            'presubmit',
            # Allows it to run with a dirty tree and on no branch
            '--force',
            '--parallel',
            # Unfortunately, upload skips certain checks which would be
            # useful. However, it also skips certain checks we really don't
            # want to run. CheckTreeIsOpen(), for example.
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
      # For consistency's sake, we warn if the intersection of commits is small,
      # so we should also warn if the intersection is emmpty.
      logging.warning('git cl presubmit only supports running on the ' +
                      'revision @. `git cl presubmit` will be skipped')

  refspec = get_refspec_opts(args)
  refspec_suffix = '%' + ','.join(refspec) if refspec else ''

  cmd = [
      'gerrit', 'upload', '--remote', 'origin', '--remote-branch',
      args.target_branch + refspec_suffix
  ]
  for rev in revs:
    cmd.extend(['-r', rev])
  if args.upload:
    run_jj(cmd)
  else:
    logging.info('no-upload: Would otherwise run `%s`', ' '.join(cmd))


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
  parser.add_argument('-r',
                      '--revision',
                      help=None,
                      action='append',
                      default=[])
  parser.add_argument('revisions', help='Revisions to upload', nargs='*')
  parser.add_argument(
      '--no-fix',
      help='Skips running `jj fix` before uploading',
      action='store_false',
      dest='fix',
  )
  parser.add_argument('--no-upload',
                      help='Doesn\'t actually upload the change to gerrit',
                      action='store_false',
                      dest='upload')
  parser.add_argument(
      '--allow-warnings',
      help='Prevents presubmit warnings from blocking upload',
      action='store_true',
  )

  # These args are directly copied from git_cl.py
  parser.add_argument('--bypass-hooks',
                      action='store_true',
                      help='bypass upload presubmit hook')
  # We use -R instead of the -r that git cl upload uses because -r in jj means
  # revision.
  parser.add_argument('-R',
                      '--reviewers',
                      action='append',
                      default=[],
                      help='reviewer email addresses')
  parser.add_argument('--cc',
                      action='append',
                      default=[],
                      help='cc email addresses')
  parser.add_argument('-s',
                      '--send-mail',
                      '--send-email',
                      dest='send_mail',
                      action='store_true',
                      help='send email to reviewer(s) and cc(s) immediately')
  parser.add_argument('--target_branch',
                      '--target-branch',
                      metavar='TARGET',
                      help='Apply CL to remote branch TARGET.',
                      default='main')
  parser.add_argument('--topic',
                      default=None,
                      help='Topic to specify when uploading')

  parser.add_argument(
      '-c',
      '--use-commit-queue',
      action='store_true',
      default=False,
      help='tell the CQ to commit this patchset; implies --send-mail',
  )
  parser.add_argument(
      '-d',
      '--dry-run',
      '--cq-dry-run',
      action='store_true',
      dest='cq_dry_run',
      default=False,
      help='Send the patchset to do a CQ dry run right after upload.',
  )
  parser.add_argument(
      '-a',
      '--auto-submit',
      '--enable-auto-submit',
      action='store_true',
      dest='enable_auto_submit',
      help='Sends your change to the CQ after an approval. Only '
      'works on repos that have the Auto-Submit label '
      'enabled')
  parser.add_argument('--enable-owners-override',
                      action='store_true',
                      help='Adds the Owners-Override label to your change.')
  parser.add_argument('--private',
                      action='store_true',
                      help='Set the review private.')

  main(parser.parse_args())
