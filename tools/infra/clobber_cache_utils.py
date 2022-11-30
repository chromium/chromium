# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for cache clobbering scripts."""

from __future__ import print_function

import json
import os
import subprocess
import textwrap

_SRC_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
_SWARMING_CLIENT = os.path.join(_SRC_ROOT, 'tools', 'luci-go', 'swarming')
_SWARMING_SERVER = 'chromium-swarm.appspot.com'


def _get_bots(swarming_server, pool, cache):
  cmd = [
      _SWARMING_CLIENT,
      'bots',
      '-S',
      swarming_server,
      '-dimension',
      'caches=' + cache,
      '-dimension',
      'pool=' + pool,
  ]
  return [bot['bot_id'] for bot in json.loads(subprocess.check_output(cmd))]


def _trigger_clobber(swarming_server, pool, realm, cache, bot, mount_rel_path,
                     dry_run):
  cmd = [
      _SWARMING_CLIENT,
      'trigger',
      '-S',
      swarming_server,
      '-realm',
      realm,
      '-dimension',
      'pool=' + pool,
      '-dimension',
      'id=' + bot,
      '-cipd-package',
      'cpython3:infra/3pp/tools/cpython3/${platform}=latest',
      '-named-cache',
      cache + '=' + mount_rel_path,
      '-priority',
      '10',
      '--',
      'cpython3/bin/python3${EXECUTABLE_SUFFIX}',
      '-c',
      textwrap.dedent('''\
          import os, shutil, stat

          def remove_readonly(func, path, _):
              "Clear the readonly bit and reattempt the removal"
              os.chmod(path, stat.S_IWRITE)
              func(path)

          shutil.rmtree({mount_rel_path!r}, onerror=remove_readonly)
          '''.format(mount_rel_path=mount_rel_path)),
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


def clobber_caches(swarming_server,
                   pool,
                   realm,
                   cache,
                   mount_rel_path,
                   dry_run,
                   bot_id=None):
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
    * realm - The realm to trigger tasks into.
    * cache - The name of the cache to clobber.
    * mount_rel_path - The relative path to mount the cache to when clobbering.
    * dry_run - Whether a dry-run should be performed where the commands that
      would be executed to trigger the clobber task are printed rather than
      actually triggering the clobber task.
    * bot_id - optional, the id of the bot that you wish to clobber.
  """
  if bot_id:
    bots = [bot_id]
  else:
    bots = _get_bots(swarming_server, pool, cache)
    if not bots:
      print(f'There are no bots on swarming server {swarming_server}'
            f' in pool {pool} that have cache {cache}')
      return 0

  print('The following bots will be clobbered:')
  print()
  for bot in bots:
    print('  %s' % bot)
  print()
  val = input('Proceed? [Y/n] ')
  if val and not val[0] in ('Y', 'y'):
    print('Cancelled.')
    return 1

  for bot in bots:
    _trigger_clobber(swarming_server, pool, realm, cache, bot, mount_rel_path,
                     dry_run)
  return 0
