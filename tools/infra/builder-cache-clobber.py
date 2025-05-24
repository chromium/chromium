#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Clobbers all builder caches for a specific builder.

Note that this currently does not support windows.
"""

import argparse
import hashlib
import json
import sys

import clobber_cache_utils


def main(raw_args):
  parser = argparse.ArgumentParser()
  clobber_cache_utils.add_common_args(parser)
  parser.add_argument('--builder', required=True)
  parser.add_argument('--bucket', required=True)
  parser.add_argument('--project', default='chromium')
  parser.add_argument('--pool', default=None)
  parser.add_argument('--bot-id', default=None)
  parser.add_argument('--xcode-action',
                      choices=['warn', 'error', 'clobber'],
                      default='error',
                      help='Action to take if Xcode caches are detected. '
                      'warn: Xcode caches are removed from the bots. '
                      'error: Xcode caches are NOT removed from the bots. '
                      'clobber: Automatically clobber Xcode caches.')
  args = parser.parse_args(raw_args)

  # Always clobber the builder cache.
  # Matches http://bit.ly/2WZO33P
  string_to_hash = '%s/%s/%s' % (args.project, args.bucket, args.builder)
  h = hashlib.sha256(string_to_hash.encode('utf-8'))
  builder_cache = 'builder_%s_v2' % (h.hexdigest())
  pool = args.pool or 'luci.%s.%s' % (args.project, args.bucket)
  realm = '%s:%s' % (args.project, args.bucket)
  mount_rel_path = 'cache/builder'

  bots_state = clobber_cache_utils.confirm_and_trigger_clobber_bots(
      args.swarming_server, pool, realm, builder_cache, mount_rel_path,
      args.dry_run, args.bot_id)

  if not bots_state:
    return 1  # Exit when no bot can be found.

  xcode_caches_dict = clobber_cache_utils.get_xcode_caches_for_all_bots(
      bots_state)
  if xcode_caches_dict.values():
    message = (f'Some bots have Xcode caches:\n'
               f'{json.dumps(xcode_caches_dict, sort_keys=True, indent=2)}')
    if args.xcode_action == 'error':
      print(f'ERROR: {message}.\nUse --xcode-action to change.')
      return 1  # Exit
    if args.xcode_action == 'warn':
      print(f'WARNING: {message}')
    elif args.xcode_action == 'clobber':
      print(message)
    for bot_id, xcode_caches in xcode_caches_dict.items():
      for xcode_cache in xcode_caches:
        mount_rel_path = 'cache/%s' % xcode_cache
        clobber_cache_utils.trigger_clobber_cache(args.swarming_server, pool,
                                                  realm, xcode_cache, bot_id,
                                                  mount_rel_path, args.dry_run)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
