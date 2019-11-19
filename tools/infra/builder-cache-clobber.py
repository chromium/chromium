#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
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
  args = parser.parse_args(raw_args)

  # Matches http://bit.ly/2WZO33P
  h = hashlib.sha256('%s/%s/%s' % (args.project, args.bucket, args.builder))
  cache = 'builder_%s_v2' % (h.hexdigest())
  pool = 'luci.%s.%s' % (args.project, args.bucket)

  clobber_cache_utils.clobber_caches(args.swarming_server, pool, cache,
                                     'cache/builder', args.dry_run)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
