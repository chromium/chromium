# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import re
import subprocess

# CSV-file like separators. The templating language doesn't support escaping,
# so we use
# https://en.wikipedia.org/wiki/C0_and_C1_control_codes#Field_separators
_NEWLINE = '\x1e'
_COMMA = '\x1f'

_TRAILER = re.compile(r'([a-zA-Z0-9\-_]+): (.*)')
_CHANGE_PRETTY = '''change_id.short() ++
if(current_working_copy, " (@)") ++
" " ++
description.first_line()
'''

MUTABLE_PARENTS = '''parents
.filter(|c| !c.immutable())
.map(|p| p.change_id())
.join(",")'''


def run_command(args: list[str],
                check=True,
                **kwargs) -> subprocess.CompletedProcess:
  logging.debug('Running command %s', ' '.join(map(str, args)))
  ps = subprocess.run(args, **kwargs, check=False)
  if check and ps.returncode:
    # Don't create a stack trace.
    exit(ps.returncode)
  return ps


def run_jj(args: list[str],
           ignore_working_copy=False,
           **kwargs) -> subprocess.CompletedProcess:
  prefix = ['jj', '--no-pager']
  if ignore_working_copy:
    prefix.append('--ignore-working-copy')
  return run_command(prefix + args, **kwargs)


def _log(args: list[str], templates: dict[str, str],
         **kwargs) -> list[dict[str, str]]:
  """Log acts akin to a database query on a table.

  The user will provide templates such as {
    'change_id': 'change_id',
    'parents': 'parents.map(|c| c.change_id())',
  }

  And a set of revisions to lookup (eg. 'a|b').

  And it would then return [
     {'change_id': 'a', 'parents': '<parent of a's id>'}
     {'change_id': 'b', 'parents': '<parent of b's id>'}
  ]
  """
  templates.setdefault('name', _CHANGE_PRETTY)
  # Start by assigning indexes based on the field name.
  fields, templates = zip(*sorted(templates.items()))
  # We're just creating a jj template that outputs CSV files.
  template = f' ++ "{_COMMA}" ++ '.join(templates)
  template += f' ++ "{_NEWLINE}"'

  stdout = run_jj(
      [*args, '--no-graph', '-T', template],
      stdout=subprocess.PIPE,
      text=True,
      **kwargs,
  ).stdout

  # Now we parse our CSV file.
  return [{
      field: value
      for field, value in zip(fields, change.split(_COMMA))
  } for change in stdout.rstrip(_NEWLINE).split(_NEWLINE)]


def jj_log(*,
           templates: dict[str, str],
           revisions='@',
           **kwargs) -> list[dict[str, str]]:
  """Retrieves information about jj revisions.

  See _log for details."""
  return _log(['log', '-r', revisions], templates, **kwargs)


def split_description(description: str) -> tuple[str, dict[str, list[str]]]:
  """Splits a description into the description and git trailers."""
  trailers = collections.defaultdict(list)
  user_desc, sep, trailer_paragraph = description.rstrip().rpartition('\n\n')
  # If the description has only a single paragraph, we should interpret it as
  # a user description (eg. "WIP: blah" is not a trailer).
  if not user_desc and not sep:
    return trailer_paragraph, {}

  trailer_lines = trailer_paragraph.lstrip().split('\n')
  # Note: for multiline values, we only retrieve the first line here.
  for line in trailer_lines:
    match = _TRAILER.match(line)
    if match is not None:
      trailers[match.group(1)].append(match.group(2))
  if not trailers:
    return description, {}
  return user_desc, trailers


def join_revsets(revs: list[str]):
  if len(revs) == 1:
    return revs[0]
  else:
    return ' | '.join(f'({r})' for r in revs)


def add_trailers(rev: dict[str, str], trailers: dict[str, list[str]],
                 commit: bool) -> str:
  old_desc = rev['desc'].rstrip()
  old_trailers = rev['trailers'].rstrip()
  assert old_desc.endswith(old_trailers)
  old_desc = old_desc.removesuffix(old_trailers).rstrip()
  if not old_desc:
    # We need a non-empty first line in order for it to be treated as a trailer.
    old_desc = 'TODO: add description'

  if old_trailers:
    lines = [old_desc, '', old_trailers]
  else:
    lines = [old_desc, '']
  for k, vs in trailers.items():
    for v in vs:
      lines.append(f'{k}: {v}')
  new_desc = '\n'.join(lines)

  if commit:
    run_jj(
        ['describe', '-m', new_desc, '-r', rev['change_id']],
        ignore_working_copy=True,
    )
  return new_desc
