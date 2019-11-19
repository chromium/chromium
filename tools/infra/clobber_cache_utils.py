# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for cache clobbering scripts."""

from __future__ import print_function

import os
import subprocess
import sys

_SRC_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
_SWARMING_CLIENT = os.path.join(_SRC_ROOT, 'tools', 'swarming_client',
                                'swarming.py')
_SWARMING_SERVER = 'chromium-swarm.appspot.com'


def _get_bots(swarming_server, pool, cache):
  cmd = [
      sys.executable,
      _SWARMING_CLIENT,
      'bots',
      '-b',
      '-S',
      swarming_server,
      '-d',
      'caches',
      cache,
      '-d',
      'pool',
      pool,
  ]
  return subprocess.check_output(cmd).splitlines()


def _trigger_clobber(swarming_server, pool, cache, bot, mount_rel_path,
                     dry_run):
  cmd = [
      sys.executable,
      _SWARMING_CLIENT,
      'trigger',
      '-S',
      swarming_server,
      '-d',
      'pool',
      pool,
      '-d',
      'id',
      bot,
      '--named-cache',
      cache,
      mount_rel_path,
      '--priority=10',
      '--raw-cmd',
      '--',
      # TODO(jbudorick): Generalize this for windows.
      '/bin/rm',
      '-rf',
      mount_rel_path,
  ]
  if dry_run:
    print('Would run `%s`' % ' '.join(cmd))
  else:
    subprocess.check_call(cmd)


def add_common_args(argument_parser):
  """Add common arguments to the argument parser used for cache clobber scripts.

  The following arguments will be added to the argument parser:
    * swarming_server (-S/--swarming-server) - The swarming server instance to
      lookup bots to clobber caches on, with a default of the
      chromium-swarm.appspot.com.
    * dry_run (-n/--dry-run) - Whether a dry-run should be performed rather than
      actually clobbering caches, defaults to False.
  """
  argument_parser.add_argument(
      '-S', '--swarming-server', default=_SWARMING_SERVER)
  argument_parser.add_argument('-n', '--dry-run', action='store_true')


def clobber_caches(swarming_server, pool, cache, mount_rel_path, dry_run):
  """Clobber caches on bots.

  The set of bots in `pool` in `swarming_server` with a cache named `cache` will
  be looked up and printed out then the user will be asked to confirm that the
  caches should be clobbered. If the user confirms, tasks that clobber the cache
  will be triggered on each bot or if `dry_run` is true, the command that would
  trigger such a task is printed instead.

  Args:
    * swarming_server - The swarming_server instance to lookup bots to clobber
      caches on.
    * pool - The pool of machines to lookup bots to clobber caches on.
    * cache - The name of the cache to clobber.
    * mount_rel_path - The relative path to mount the cache to when clobbering.
    * dry_run - Whether a dry-run should be performed where the commands that
      would be executed to trigger the clobber task are printed rather than
      actually triggering the clobber task.
  """
  bots = _get_bots(swarming_server, pool, cache)

  print('The following bots will be clobbered:')
  print()
  for bot in bots:
    print('  %s' % bot)
  print()
  val = raw_input('Proceed? [Y/n] ')
  if val and not val[0] in ('Y', 'y'):
    print('Cancelled.')
    return 1

  for bot in bots:
    _trigger_clobber(swarming_server, pool, cache, bot, mount_rel_path, dry_run)
