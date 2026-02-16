#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Creates a new jj chromium workspace in a separate directory."""

import argparse
import os
import pathlib
import shutil
import sys

import util

_SUCCESS_MSG = \
'''A new jj workspace has been created, but submodules have not been synced.
Run `gclient sync` to sync submodules.'''


def main(args):
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
  parser.add_argument('workspace',
                      help='The directory to create the new workspace in.',
                      type=pathlib.Path)
  parser.add_argument('--name', help='The name of the new workspace.')
  parser.add_argument('-r',
                      '--revision',
                      help='The revision to create the workspace at',
                      default='@')

  opts = parser.parse_args(args)

  ws = opts.workspace
  name = opts.name or ws.name
  if ws.exists():
    print(f'Error: {ws} already exists.', file=sys.stderr)
    return 1

  source = pathlib.Path(os.environ['JJ_WORKSPACE_ROOT']).parent

  try:
    ws.mkdir()
    (ws / '.gclient').symlink_to((source / '.gclient').resolve())
    util.run_jj(
        ['workspace', 'add', ws / 'src', '-r', opts.revision, '--name', name])

  except:
    # Ensure that we can't have a partially created workspace.
    shutil.rmtree(opts.workspace)
    raise

  print(_SUCCESS_MSG)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
