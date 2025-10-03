#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import sys
import util

_FIND_TRAILER = '''self.trailers()
.filter(|trailer| trailer.key().lower() == "{}")
.map(|trailer| trailer.value())
.join(",")'''


def _calculate_bugs(bugs: str) -> set[str]:
  bugs = set(bugs.split(','))
  # The template produces ,, if some parents don't have bugs.
  bugs.discard('')
  return bugs


def _add_handler(args, revs: str):
  direct = {
      x['change_id']
      for x in util.jj_log(
          revisions=f'{revs}',
          templates={
              'change_id': 'change_id',
          },
          ignore_working_copy=True,
      )
  }

  ancestors = util.jj_log(
      revisions=f'mutable()::({revs})',
      templates={
          'change_id': 'change_id',
          'parents': util.MUTABLE_PARENTS,
          'trailers': 'trailers',
          'desc': 'description',
          'bugs': _FIND_TRAILER.format('bug'),
          'fixed': _FIND_TRAILER.format('fixed'),
      },
      ignore_working_copy=True,
  )

  revs = {rev['change_id']: rev for rev in ancestors}

  for rev in ancestors:
    rev['fixed'] = _calculate_bugs(rev['fixed'])
    rev['bugs'] = _calculate_bugs(rev['bugs'])

  bugs = set(args.bug)
  if not bugs and not args.inherit:
    print('List of bugs is required if --inherit is not used', file=sys.stderr)
    exit(1)

  for rev in reversed(ancestors):
    if rev['change_id'] not in direct:
      # Suppose you have A <- B <- C:
      # When running `jj bug add --inherit -r C`, C should inherit from B
      # When running `jj bug add --inherit -r B|C`, C should inherit from B,
      # which should have already inherited from A.
      continue
    bugs_to_add = set(bugs)
    if args.inherit:
      # This doesn't need to be done recursively because of the toposort
      # guaruntee of `jj log`
      if rev['parents']:
        for change_id in rev['parents'].split(','):
          bugs_to_add.update(revs[change_id]['bugs'])

    bugs_to_add -= rev['bugs']
    bugs_to_add -= rev['fixed']
    (rev['fixed'] if args.fixed else rev['bugs']).update(bugs_to_add)

    # Skip the ones we already have
    tag = 'Fixed' if args.fixed else 'Bug'
    # We may not need to add bugs, especially in the case of inherit.
    if bugs_to_add:
      util.add_trailers(
          rev=rev,
          trailers={tag: [','.join(bugs_to_add)]},
          commit=True,
      )


def main(args):
  logging.basicConfig(level=logging.getLevelNamesMapping()[args.verbosity])

  revs = util.join_revsets(args.revision)
  args.func(args, revs)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--verbosity',
      help='Verbosity of logging',
      default='INFO',
      choices=logging.getLevelNamesMapping().keys(),
      type=lambda x: x.upper(),
  )

  subparsers = parser.add_subparsers()

  parser_add = subparsers.add_parser('add')
  parser_add.set_defaults(func=_add_handler)
  parser_add.add_argument(
      '-r',
      '--revision',
      help='The revisions to add the bug to',
      nargs='+',
  )
  parser_add.add_argument(
      '-f',
      '--fixed',
      help='Mark the revision as fixing the bug',
      action='store_true',
  )
  parser_add.add_argument(
      '-i',
      '--inherit',
      help='Inherit the bug from parent commits. Bugs are not transitively ' +
      'inherited unless the parent is also part of -r',
      action='store_true',
  )
  parser_add.add_argument(
      '-b',
      '--bug',
      help='The bug to add to the revisions',
      nargs='*',
      default=[],
  )

  main(parser.parse_args())
