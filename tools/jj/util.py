# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import subprocess

# CSV-file like separators. The templating language doesn't support escaping,
# so we use
# https://en.wikipedia.org/wiki/C0_and_C1_control_codes#Field_separators
_NEWLINE = '\x1e'
_COMMA = '\x1f'


def run_command(args: list[str],
                check=True,
                **kwargs) -> subprocess.CompletedProcess:
  logging.debug('Running command %s', ' '.join(map(str, args)))
  ps = subprocess.run(args, **kwargs, check=False)
  if check and ps.returncode:
    # Don't create a stack trace.
    exit(ps.returncode)
  return ps


def _log(args: list[str], templates: dict[str, str],
         ignore_working_copy: bool) -> list[dict[str, str]]:
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
  # Start by assigning indexes based on the field name.
  fields, templates = zip(*sorted(templates.items()))
  # We're just creating a jj template that outputs CSV files.
  template = f' ++ "{_COMMA}" ++ '.join(templates)
  template += f' ++ "{_NEWLINE}"'
  if ignore_working_copy:
    args.append('--ignore-working-copy')

  stdout = run_command(
      ['jj', *args, '--no-pager', '--no-graph', '-T', template],
      stdout=subprocess.PIPE,
      text=True,
  ).stdout

  # Now we parse our CSV file.
  return [{
      field: value
      for field, value in zip(fields, change.split(_COMMA))
  } for change in stdout.rstrip(_NEWLINE).split(_NEWLINE)]


def jj_log(*,
           templates: dict[str, str],
           revisions='@',
           ignore_working_copy=False) -> list[dict[str, str]]:
  """Retrieves information about jj revisions.

  See _log for details."""
  return _log(['log', '-r', revisions],
              templates,
              ignore_working_copy=ignore_working_copy)
