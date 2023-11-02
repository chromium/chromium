#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Clobbers all builder caches for a specific builder.

Note that this currently does not support windows.
"""

import argparse
import hashlib
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
  args = parser.parse_args(raw_args)

  # Matches http://bit.ly/2WZO33P
  string_to_hash = '%s/%s/%s' % (args.project, args.bucket, args.builder)
  h = hashlib.sha256(string_to_hash.encode('utf-8'))
  cache = 'builder_%s_v2' % (h.hexdigest())
  pool = args.pool or 'luci.%s.%s' % (args.project, args.bucket)

  clobber_cache_utils.clobber_caches(args.swarming_server,
                                     pool,
                                     '%s:%s' % (args.project, args.bucket),
                                     cache,
                                     'cache/builder',
                                     args.dry_run,
                                     bot_id=args.bot_id)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
