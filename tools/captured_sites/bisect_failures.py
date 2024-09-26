#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Bisects a failure in the captured sites framework to find culprit CL.

  First, this script will retrieve test results from the given build number and
  the prior build number.

  It will then find sites that changed from passing to failing across those
  builds, and pick a site to use for bisection.

  Finally, it will then launch a local git bisect, using the 'Release' version
  (to match how the bots operate) and running tests in the background.'

  In order to run the bisect, no pending git changes can exist in the local
  checkout, and checkout_chromium_autofill_test_dependencies should be set to
  true in your .gclient file.

  This script requires Read permissions of ResultDB and Buildbucket RPCs of
  internal builders, so it is intended only for Googlers at this time.

  Common tasks:
  Bisect an autofill bot failure:
    1) First, find the first failing build you are interested in for the
       linux-autofill-captured-sites-rel bot.
    2) Note the build number {build_num}. The script will use the first site
       which failed in that run and not the previous to bisect with.
    3) Run:
      `tools/captured_sites/bisect_failures.py autofill {build_num}`

  Bisect an autofill bot failure using a specific site. Note: script does not
  verify that given site overrides are valid recorded site names.
    1) First, find the first failing build you are interested in for the
       linux-autofill-captured-sites-rel bot.
    2) Note the build number {build_num} and site {site_name}.
    3) Run:
      `tools/captured_sites/bisect_failures.py autofill {build_num}
        --site_name {site_name}`

  Bisect an password bot failure:
    1) First, find the first failing build you are interested in for the
       linux-password-manager-captured-sites-rel bot.
    2) Note the build number {build_num}.
    3) Run:
      `tools/captured_sites/bisect_failures.py password {build_num}`
"""

import os
import sys
import argparse

import captured_sites_commands

_TOOLS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def _JoinPath(*path_parts):
  return os.path.abspath(os.path.join(*path_parts))


def _InsertPath(path):
  assert os.path.isdir(path), 'Not a valid path: %s' % path
  if path not in sys.path:
    # Some call sites that use Telemetry assume that sys.path[0] is the
    # directory containing the script, so we add these extra paths to right
    # after sys.path[0].
    sys.path.insert(1, path)


def _AddDirToPythonPath(*path_parts):
  path = _JoinPath(*path_parts)
  _InsertPath(path)


_AddDirToPythonPath(_TOOLS_DIR, 'bisect')
import bisect_gtests

_AddDirToPythonPath(_TOOLS_DIR, 'perf')
from core.services import buildbucket_service
from core.services import resultdb_service


def _FindSiteRegressions(bad_buildbucket_id, good_buildbucket_id):
  """Retrieve failed sites from 2 builds, and return the new failures.

    Args:
        bad_buildbucket_id: A build with new failures.
        good_buildbucket_id: A baseline build.

    Returns:
        The sites which fail in the bad build but not in the good build.
    """
  bad_failed_sites = _GetTerminalSiteFailures(bad_buildbucket_id)
  good_failed_sites = _GetTerminalSiteFailures(good_buildbucket_id)
  site_regressions = [k for k in bad_failed_sites if k not in good_failed_sites]
  return site_regressions


# These are convenience shorthand names for command line usage.
_BOT_SHORT_LONG_MAPPING = {
    'autofill': 'linux-autofill-captured-sites-rel',
    'password': 'linux-password-manager-captured-sites-rel',
}


def _GetBotName(short_name):
  """Maps convenient short names to full bot names.

    Args:
        short_name: Either "autofill" or "password"

    Returns:
        The full bot name to be used in build retrievals.

    Raises:
        ValueError: If an invalid short_name is given.
    """
  if short_name not in _BOT_SHORT_LONG_MAPPING:
    raise ValueError(f'Unrecognized short bot name: "{short_name}".'
                     ' Only "autofill" or "password" is known.')
  return _BOT_SHORT_LONG_MAPPING[short_name]


def _GetTerminalSiteFailures(buildbucket_id):
  """Retrieves non-passing (CRASH/TIMEOUT/FAIL) test failures from a build.

    Args:
        buildbucket_id: A build with which to interrogate failures.

    Returns:
        The sites which do not pass in the build.
    """
  all_results = resultdb_service.GetQueryTestResult(buildbucket_id)
  if 'testResults' not in all_results:
    return set()

  site_statuses = {}
  for result in all_results['testResults']:
    test_id = result['testId']
    siteName = test_id[test_id.rfind('All.') + 4:]
    site_statuses[siteName] = result['status']
  site_final_failures = [k for k, v in site_statuses.items() if v != 'PASS']
  return set(site_final_failures)


def _ParseCommandLine(args):
  """Parses command line options from given args.

    Args:
        args: argument which to parse.

    Returns:
        An object containing parsed argument values.

    Raises:
        SystemExit: if invalid or unrecognized arguments were given.
    """
  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawTextHelpFormatter)
  parser.usage = __doc__
  parser.add_argument(
      'bot_name',
      choices=list(_BOT_SHORT_LONG_MAPPING.keys()),
      help='Choose which linux captured sites builder flavor to bisect.')
  parser.add_argument('build_number',
                      type=int,
                      help='The failing build number to bisect')
  parser.add_argument(
      '-s',
      '--site_name',
      type=str,
      help=(
          'An explicit site to use for bisecting (versus picking a failing one'
          ' from build result).'))
  parser.add_argument(
      '-p',
      '--print_only',
      action='store_true',
      help='Only print the commands that would be used to bisect.')
  options = parser.parse_args(args)
  return options


def _RetrieveBuildbucketInfo(builder_name, number):
  """From a given builder name and number, retrieve relevant hash and id.

    Args:
        builder_name: The full builder name to query.
        number: The number of the builder to gather infro from.

    Returns:
        A tuple of the builds (hash, id).

    Raises:
        BuildRequestError: if improper permissions or query format is given.
  """
  build = buildbucket_service.GetBuild('chrome', 'ci', builder_name, number)
  build_bucket_hash = build['input']['gitilesCommit']['id']
  build_bucket_id = build['id']
  return build_bucket_hash, build_bucket_id


def _TranslateSiteName(site_name):
  """Translates complex site_name into array of necessary parts depending on
       if it is a password or autofill type name.

    Args:
        site_name: For autofill, basic site_name, but for Password, a site_name
                   with password scenario prefix.
    Returns:
        A list containing the parsed pieces describing the scenario & site_name.
  """
  password_prefixes = [
      'sign_up_fill', 'sign_up_pass', 'sign_in_pass', 'capture_update_pass'
  ]
  for password_prefix in password_prefixes:
    if site_name.startswith(password_prefix + '_'):
      return [password_prefix, site_name[len(password_prefix + '_'):]]
  return [site_name]


def DoBisect(bad_hash, good_hash, site_name, print_only=False):
  """Takes the comparison hashes and a site_name and peforms a local bisect
     to find the culprit CL which first causes the test for this site to fail.

    Args:
        bad_hash: The hash of a failing build.
        good_hash: The hash of the last known prior good build.
        site_name: A list of distinguishing site parts. For autofill, simply
                   the [site_name], but for password, this would be
                   [scenario_dir, site_name]. Consider _TranslateSiteName to
                   build this properly.
        print_only: A boolean that if True, only print the pieces that would go
                    into a potential bisect process, instead of actually
                     initiating one.
  """
  site_name = _TranslateSiteName(site_name)
  print('Translated to:', site_name)

  build_command = captured_sites_commands.initiate_command('build')
  build_command.build(['-r'])
  build_command_text = build_command.print()

  run_command = captured_sites_commands.initiate_command('run')
  # -r:Use Release version, -b:Run In Background', -u:Use Bot Timeout (3 min).
  run_command.build(['-r', '-b', '-u'] + site_name)
  run_command_text = run_command.print()

  print(f'Will bisect from {good_hash} to {bad_hash}.')
  print(f'Will build using:\n{build_command_text}')
  print(f'Will run using:\n{run_command_text}')
  if print_only:
    print('print_only is set, exiting before bisect begins.')
    return
  bisect_gtests.StartBisect(good_hash, bad_hash, build_command_text,
                            run_command_text)


def GetBuildInfo(bot_name, build_number, site_name=None):
  """Given a bot_name and build_number, returns the info necessary to perform a
     bisect of any novel changes between that build and the previous one.

    Args:
        bot_name: An acceptable short bot_name. See _GetBotName for more detail.
        build_number: The 'bad' build number from which to find novel failures.
        site_name: An optional site_name to force usage of. This likely will
                   want to be from new site failures in the given build, but is
                   not enforced so.
    Returns:
        bad_hash: Buildbucket hash from the given 'bad' build number.
        good_hash: Buildbucket hash from the prior 'good' build.
        site_name: Either the given optional override, or a single novel failure
                   that occurred between the given build_number and the prior.
  """
  full_bot_name = _GetBotName(bot_name)
  first_bad_number = build_number
  last_good_number = build_number - 1

  try:
    bad_hash, bad_id = _RetrieveBuildbucketInfo(full_bot_name, first_bad_number)
    good_hash, good_id = _RetrieveBuildbucketInfo(full_bot_name,
                                                  last_good_number)
  except Exception as e:
    print('Unable to retrieve build bucket info for builds to compare:', e)
    return None, None, None

  try:
    possible_site_regressions = _FindSiteRegressions(bad_id, good_id)
  except Exception as e:
    print('Unable to retrieve site failures from given buildbucket ids:', e)
    return None, None, None

  if not possible_site_regressions:
    print(
        f'Compared build numbers {first_bad_number} and {last_good_number}, but'
        ' found no clear site regressions. Make sure to add the first builder'
        ' that failed as the input. Also note this script cannot handle'
        ' bisecting infra failures.')
  else:
    print('All Site Regressions (%d):%s' %
          (len(possible_site_regressions), ' '.join(possible_site_regressions)))

  if site_name:
    if site_name not in possible_site_regressions:
      print(f'WARNING: given site "{site_name}" did not show as possbile site'
            ' regression but was given as override choice.')
  elif possible_site_regressions:
    site_name = possible_site_regressions[0]

  print(f'Choosing Site:"{site_name}".')
  return bad_hash, good_hash, site_name


def main():
  options = _ParseCommandLine(sys.argv[1:])
  bad_hash, good_hash, site_name = GetBuildInfo(options.bot_name,
                                                options.build_number,
                                                options.site_name)
  if not bad_hash or not good_hash or not site_name:
    print(f'Unable to gather enough info for a bisect, as do not have a'
          f' good hash:"{good_hash}",'
          f' bad hash:"{bad_hash}",'
          f' and/or site name:"{site_name}"".')
    return 1
  DoBisect(bad_hash, good_hash, site_name, options.print_only)
  return 0


if __name__ == '__main__':
  sys.exit(main())
