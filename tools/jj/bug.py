#!/usr/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import re
import shutil
import subprocess
import sys
import util

_FIND_TRAILER = '''self.trailers()
.filter(|trailer| trailer.key().lower() == "{}")
.map(|trailer| trailer.value())
.join(",")'''

_BUGGED_MISSING_ERR = '''\
Searching for bugs requires the CLI tool "bugged" to be installed.
This tool is only available to Googlers (see go/bugged for install instructions)
'''


def _calculate_bugs(bugs: str) -> set[str]:
  bugs = set(bugs.split(','))
  # The template produces ,, if some parents don't have bugs.
  bugs.discard('')
  return bugs


def _search_bugs(query: str | None):
  args = [
      'bugged', 'search', '--error-if-not-found=true',
      '--columns=issue,reporter,assignee,status,summary'
  ]
  if query:
    args.append(query)
  ps = util.run_command(
      args,
      stdout=subprocess.PIPE,
      check=False,
      text=True,
  )
  if ps.returncode == 5:  # No results found
    print('No results found. Try another query?')
    return _search_bugs(input('>>> '))
  elif ps.returncode != 0:
    print('Issue tracker search failed', file=sys.stderr)
    exit(1)

  # The first column is the bug number. We need this internally, but don't want
  # to show it to the user.
  lines = [
      re.split(r'\s+', line, maxsplit=1)
      for line in ps.stdout.rstrip().split('\n')
  ]
  print(f'#  {lines[0][1]}')
  issues = {}
  # Reverse it because we want the important information to be at the bottom.
  # We do this since the prompt is at the bottom.
  for i, (issue, line) in reversed(list(enumerate(lines))[1:]):
    issues[str(i)] = issue
    fmt = f'{i:>2} {line}'
    # Print every second line in cyan so you can easily line up the numbers
    # with the summary
    print(f'\033[36m{fmt}\033[0m' if i % 2 else fmt)

  # Maybe someone with experience with TUIs could improve on the UX, but this
  # works pretty well and is much easier to code.
  print('Type the number in the left column to select an issue')
  print('Type "q [query]" to search for something different')
  while True:
    answer = input('>>> ')
    if answer.startswith('q '):
      return _search_bugs(answer.removeprefix('q '))
    elif answer == '':
      # Run the default query
      return _search_bugs(None)
    elif answer in issues:
      return issues[answer]
    print('Invalid response')


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

  if args.query or (not bugs and not args.inherit):
    if args.query:
      print('Searching for bugs on issue tracker...')
    else:
      print('No bugs provided. Searching for bugs on issue tracker...')
    bugged = shutil.which('bugged')
    if bugged is None:
      print(_BUGGED_MISSING_ERR, file=sys.stderr)
      exit(1)
    bugs.add(_search_bugs(args.query))

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
      action='append',
      required=True,
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
      action='append',
      default=[],
  )
  parser_add.add_argument(
      '-q',
      '--query',
      help='Query to use when searching for a bug in issue tracker',
      default=None,
  )

  main(parser.parse_args())
