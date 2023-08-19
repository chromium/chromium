#!/usr/bin/env python
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Downloads pgo profiles for optimizing official Chrome.

This script has the following responsibilities:
1. Download a requested profile if necessary.
2. Return a path to the current profile to feed to the build system.
3. Removed stale profiles (2 days) to save disk spaces because profiles are
   large (~1GB) and updated frequently (~4 times a day).
"""

from __future__ import print_function

import argparse
import os
import sys
import time

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.path.pardir))
sys.path.append(os.path.join(_SRC_ROOT, 'third_party', 'depot_tools'))
import download_from_google_storage

sys.path.append(os.path.join(_SRC_ROOT, 'build'))
import gn_helpers

# Absolute path to the directory that stores pgo related state files, which
# specifcies which profile to update and use.
_PGO_DIR = os.path.join(_SRC_ROOT, 'chrome', 'build')

# Absolute path to the directory that stores pgo profiles.
_PGO_PROFILE_DIR = os.path.join(_PGO_DIR, 'pgo_profiles')


def _read_profile_name(target):
  """Read profile name given a target.

  Args:
    target(str): The target name, such as win32, mac.

  Returns:
    Name of the profile to update and use, such as:
    chrome-win32-master-67ad3c89d2017131cc9ce664a1580315517550d1.profdata.
  """
  state_file = os.path.join(_PGO_DIR, '%s.pgo.txt' % target)
  with open(state_file, 'r') as f:
    profile_name = f.read().strip()

  return profile_name


def _remove_unused_profiles(current_profile_name):
  """Removes unused profiles, except the current one, to save disk space."""
  days = 2
  expiration_duration = 60 * 60 * 24 * days
  for f in os.listdir(_PGO_PROFILE_DIR):
    if f == current_profile_name:
      continue

    p = os.path.join(_PGO_PROFILE_DIR, f)
    age = time.time() - os.path.getmtime(p)
    if age > expiration_duration:
      print('Removing profile %s as it hasn\'t been used in the past %d days' %
            (p, days))
      os.remove(p)


def _update(args):
  """Update profile if necessary according to the state file.

  Args:
    args(dict): A dict of cmd arguments, such as target and gs_url_base.

  Raises:
    RuntimeError: If failed to download profiles from gcs.
  """
  profile_name = _read_profile_name(args.target)
  profile_path = os.path.join(_PGO_PROFILE_DIR, profile_name)
  if os.path.isfile(profile_path):
    os.utime(profile_path, None)
    return

  gsutil = download_from_google_storage.Gsutil(
      download_from_google_storage.GSUTIL_DEFAULT_PATH)
  gs_path = 'gs://' + args.gs_url_base.strip('/') + '/' + profile_name
  code = gsutil.call('cp', gs_path, profile_path)
  if code != 0:
    raise RuntimeError('gsutil failed to download "%s"' % gs_path)

  _remove_unused_profiles(profile_name)


def _get_profile_path(args):
  """Returns an absolute path to the current profile.

  Args:
    args(dict): A dict of cmd arguments, such as target and gs_url_base.

  Raises:
    RuntimeError: If the current profile is missing.
  """
  profile_path = os.path.join(_PGO_PROFILE_DIR, _read_profile_name(args.target))
  if not os.path.isfile(profile_path):
    raise RuntimeError(
        'requested profile "%s" doesn\'t exist, please make sure '
        '"checkout_pgo_profiles" is set to True in the "custom_vars" section '
        'of your .gclient file, e.g.: \n'
        'solutions = [ \n'
        '  { \n'
        '    "name": "src", \n'
        '    # ...  \n'
        '    "custom_vars": { \n'
        '      "checkout_pgo_profiles": True, \n'
        '    }, \n'
        '  }, \n'
        '], \n'
        'and then run "gclient runhooks" to download it. You can also simply '
        'disable the PGO optimizations by setting |chrome_pgo_phase = 0| in '
        'your GN arguments.'%
        profile_path)

  os.utime(profile_path, None)
  profile_path.rstrip(os.sep)
  print(gn_helpers.ToGNString(profile_path))


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      '--target',
      required=True,
      choices=[
          'win-arm64',
          'win32',
          'win64',
          'mac',
          'mac-arm',
          'linux',
          'lacros64',
          'lacros-arm',
          'lacros-arm64',
          'android-arm32',
          'android-arm64',
      ],
      help='Identifier of a specific target platform + architecture.')
  subparsers = parser.add_subparsers()

  parser_update = subparsers.add_parser('update')
  parser_update.add_argument(
      '--gs-url-base',
      required=True,
      help='The base GS URL to search for the profile.')
  parser_update.set_defaults(func=_update)

  parser_get_profile_path = subparsers.add_parser('get_profile_path')
  parser_get_profile_path.set_defaults(func=_get_profile_path)

  args = parser.parse_args()
  return args.func(args)


if __name__ == '__main__':
  sys.exit(main())
