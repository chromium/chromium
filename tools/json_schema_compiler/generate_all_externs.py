#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper for quickly generating all known JS externs."""

import argparse
import os
import re
import sys

from compiler import GenerateSchema

# APIs with generated externs.
API_SOURCES = (
    ('chrome', 'common', 'apps', 'platform_apps', 'api'),
    ('chrome', 'common', 'extensions', 'api'),
    ('extensions', 'common', 'api'),
)

_EXTERNS_UPDATE_MESSAGE = """Please run one of:
 src/ $ tools/json_schema_compiler/generate_all_externs.py
OR
 src/ $ tools/json_schema_compiler/compiler.py\
 %(source)s --root=. --generator=externs > %(externs)s"""

DIR = os.path.dirname(os.path.realpath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(DIR))

# Import the helper module.
sys.path.insert(0, os.path.join(REPO_ROOT, 'extensions', 'common', 'api'))
from externs_checker import ExternsChecker

sys.path.pop(0)


class FakeChange:
  """Stand-in for PRESUBMIT input_api.change.

  Enough to make ExternsChecker happy.
  """

  @staticmethod
  def RepositoryRoot():
    return REPO_ROOT


class FakeInputApi:
  """Stand in for PRESUBMIT input_api.

  Enough to make ExternsChecker happy.
  """

  change = FakeChange()
  os_path = os.path
  re = re

  @staticmethod
  def PresubmitLocalPath():
    return DIR

  @staticmethod
  def ReadFile(path):
    with open(path) as fp:
      return fp.read()


class FakeOutputApi:
  """Stand in for PRESUBMIT input_api.

  Enough to make CheckExterns happy.
  """

  class PresubmitResult:

    def __init__(self, msg, long_text=None):
      self.msg = msg
      self.long_text = long_text


def Generate(input_api, output_api, force=False, dryrun=False):
  """(Re)generate all the externs."""
  src_root = input_api.change.RepositoryRoot()
  join = input_api.os_path.join

  # Load the list of all generated externs.
  api_pairs = {}
  for api_source in API_SOURCES:
    api_root = join(src_root, *api_source)
    api_pairs.update(
        ExternsChecker.ParseApiFileList(input_api, api_root=api_root))

  # Unfortunately, our generator is still a bit buggy, so ignore externs that
  # are known to be hand edited after the fact.  We require people to add an
  # explicit TODO marker bound to a known bug.
  # TODO(vapier): Improve the toolchain enough to not require this.
  re_disabled = input_api.re.compile(
      r'^// TODO\(crbug\.com/[0-9]+\): '
      r'Disable automatic extern generation until fixed\.$',
      flags=input_api.re.M)

  # Make sure each one is up-to-date with our toolchain.
  ret = []
  msg_len = 0
  for source, externs in sorted(api_pairs.items()):
    try:
      old_data = input_api.ReadFile(externs)
    except OSError:
      old_data = ''
    if not force and re_disabled.search(old_data):
      continue
    source_relpath = input_api.os_path.relpath(source, src_root)
    externs_relpath = input_api.os_path.relpath(externs, src_root)

    print('\r' + ' ' * msg_len, end='\r')
    msg = 'Checking %s ...' % (source_relpath, )
    msg_len = len(msg)
    print(msg, end='')
    sys.stdout.flush()
    try:
      new_data = GenerateSchema('externs', [source], src_root, None, '', '',
                                None, []) + '\n'
    except Exception as e:
      if not dryrun:
        print('\n%s: %s' % (source_relpath, e))
      ret.append(
          output_api.PresubmitResult('%s: unable to generate' %
                                     (source_relpath, ),
                                     long_text=str(e)))
      continue

    # Ignore the first line (copyright) to avoid yearly thrashing.
    if '\n' in old_data:
      copyright, old_data = old_data.split('\n', 1)
      assert 'Copyright' in copyright
    copyright, new_data = new_data.split('\n', 1)
    assert 'Copyright' in copyright

    if old_data != new_data:
      settings = {
          'source': source_relpath,
          'externs': externs_relpath,
      }
      ret.append(
          output_api.PresubmitResult(
              '%(source)s: file needs to be regenerated' % settings,
              long_text=_EXTERNS_UPDATE_MESSAGE % settings))

      if not dryrun:
        print('\r' + ' ' * msg_len, end='\r')
        msg_len = 0
        print('Updating %s' % (externs_relpath, ))
        with open(externs, 'w', encoding='utf-8') as fp:
          fp.write(copyright + '\n')
          fp.write(new_data)

  print('\r' + ' ' * msg_len, end='\r')

  return ret


def get_parser():
  """Get CLI parser."""
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-n',
                      '--dry-run',
                      dest='dryrun',
                      action='store_true',
                      help="Don't make changes; only show changed files")
  parser.add_argument('-f',
                      '--force',
                      action='store_true',
                      help='Regenerate files even if they have a TODO '
                      'disabling generation')
  return parser


def main(argv):
  """The main entry point for scripts."""
  parser = get_parser()
  opts = parser.parse_args(argv)

  results = Generate(FakeInputApi(),
                     FakeOutputApi(),
                     force=opts.force,
                     dryrun=opts.dryrun)
  if opts.dryrun and results:
    for result in results:
      print(result.msg + '\n' + result.long_text)
      print()
  else:
    print('Done')


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
