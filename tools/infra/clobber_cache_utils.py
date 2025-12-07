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


def get_xcode_caches_for_all_bots(bots_state):
  """Extracts Xcode cache names for each bot from a list of bot states.

  Args:
    bots_state: A list of bot state dictionaries.

  Returns:
    A dict where keys are bot IDs and values are lists of Xcode cache names.
  """
  xcode_caches_dict = {}
  for bot_state in bots_state:
    xcode_caches = []

    for dimension in bot_state['dimensions']:
      if dimension['key'] == 'caches':
        for cache in dimension['value']:
          if cache.startswith('xcode'):
            xcode_caches.append(cache)
    if xcode_caches:
      xcode_caches_dict[bot_state['bot_id']] = xcode_caches
  return xcode_caches_dict


def get_bots_state_by_dimensions(swarming_server, dimensions):
  """Gets the states of bots matching given dimensions."""
  cmd = [
      _SWARMING_CLIENT,
      'bots',
      '-S',
      swarming_server,
  ]
  for d in dimensions:
    cmd.extend(['-dimension', d])
  try:
    return json.loads(subprocess.check_output(cmd))
  except (subprocess.CalledProcessError, json.JSONDecodeError) as e:
    print(f"Error getting bot states: {e}")
    return []


def trigger_clobber_cache(swarming_server, pool, realm, cache, bot_id,
                          mount_rel_path, dry_run):
  """Clobber a specific cache on a given bot.

  Args:
    swarming_server: The swarming_server instance to lookup bots to clobber
        caches on.
    pool: The pool of machines to lookup bots to clobber caches on.
    realm: The realm to trigger tasks into.
    cache: The name of the cache to clobber.
    bot_id: the id of the bot that you wish to clobber.
    mount_rel_path: The relative path to mount the cache to when clobbering.
    dry_run: Whether a dry-run should be performed where the commands that
        would be executed to trigger the clobber task are printed rather than
        actually triggering the clobber task.
  """
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
      'id=' + bot_id,
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


def confirm_and_trigger_clobber_bots(swarming_server,
                                     pool,
                                     realm,
                                     cache,
                                     mount_rel_path,
                                     dry_run,
                                     bot_id=None):
  """Gets bot IDs, confirms with the user, handles dry-run and removes caches.

  Args:
      swarming_server: The Swarming server URL.
      pool: The Swarming pool.
      realm: The Swarming realm.
      cache: The name of the cache to clobber.
      mount_rel_path - The relative path to mount the cache to when clobbering.
      dry_run:  If True, don't actually clobber or prompt.
      bot_id: Optional, the id of a specific bot that you wish to clobber.

  Returns:
      A list of botsto clobber, or an empty list if no bots
      should be clobbered (either none were found, or the user cancelled).
  """

  if bot_id:
    bot_ids = [bot_id]  # Use explicitly provided bot IDs.
    bots_state = get_bots_state_by_dimensions(swarming_server, ['id=' + bot_id])
  else:
    bots_state = get_bots_state_by_dimensions(
        swarming_server, ['pool=' + pool, 'caches=' + cache])
    bot_ids = [bot['bot_id'] for bot in bots_state]

  if not bot_ids:
    print(f"No bots found in pool {pool} with cache {cache}.")
    return []

  print(f"The following bots with {cache} will be clobbered:")
  for b_id in bot_ids:
    print(f"  {b_id}")
  print()

  if not dry_run:
    val = input('Proceed? [Y/n] ')
    if val and not val.lower().startswith('y'):
      print('Cancelled.')
      return []

  for b_id in bot_ids:
    trigger_clobber_cache(swarming_server, pool, realm, cache, b_id,
                          mount_rel_path, dry_run)

  return bots_state
