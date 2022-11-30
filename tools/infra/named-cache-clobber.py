#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Clobbers all instances of a named cache.

Note that this currently does not support windows.
"""

import argparse
import sys

import clobber_cache_utils


def main(raw_args):
  parser = argparse.ArgumentParser()
  clobber_cache_utils.add_common_args(parser)
  parser.add_argument('--pool', required=True)
  parser.add_argument('--cache', required=True)
  parser.add_argument('--realm',
                      default='chromium:try',
                      help='The LUCI realm to trigger the task into.')
  parser.add_argument('--mount-rel-path')
  parser.add_argument('--bot-id')
  args = parser.parse_args(raw_args)

  mount_rel_path = args.mount_rel_path
  if mount_rel_path is None:
    mount_rel_path = 'cache/%s' % args.cache

  clobber_cache_utils.clobber_caches(args.swarming_server, args.pool,
                                     args.realm, args.cache, mount_rel_path,
                                     args.dry_run, args.bot_id)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
