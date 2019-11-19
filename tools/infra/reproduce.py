#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Debug chromium builds locally.

Takes as input a build you want to reproduce locally.

Will do the following:
  * (Optionally) sync to the revision the build ran at
  * Build using the GN args used in the build.
  * Run a subset of the tests that failed in the build.

Each command which is executed in the list of steps above will be printed as it
is executed. The script will prompt before running tests and syncing your
checkout.

Bugs and feature requests should be given as bugs filed via
https://bit.ly/cci-generic-bug.
"""

from __future__ import print_function

import argparse
import base64
import collections
import json
import os
import platform
import sys
import subprocess
import tempfile
import traceback
import urllib


# From vpython
import colorama
import requests


CHROMIUM_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))

LOGDOG_BASE = 'https://logs.chromium.org/'
# TODO(martiniss): Allow this to be configured, to support internal builds.
TEST_RESULTS_BASE = 'https://test-results.appspot.com/'
BB_API_BASE = 'https://cr-buildbucket.appspot.com/'
CI_PREFIX = 'https://ci.chromium.org/p/chromium/builders/'


# Small named tuple for test suite entries in the
# //testing/buildbot/chromium.*.json files.
TestSuiteEntry = collections.namedtuple('TestSuiteEntry', [
    'name', 'args', 'is_isolated', 'swarming'])


# Small named tuple to describe what builders a trybot uses to decide which
# tests to run.
BuilderDep = collections.namedtuple('BuilderDep', [
    'builder', 'master', 'tester_builder', 'tester_master'])


def fetch_step_log(build, step, log):
  """Fetches a log from a step in a build."""
  step = step.replace(' ', '_').replace('(', '_').replace(')', '_')
  log_path = (
      'buildbucket/cr-buildbucket.appspot.com/%s'
      '/+/steps/%s/0/logs/%s/0' % (build, step, log))
  data = {
      'path': log_path.replace(' ', '_'),
      # TODO(martiniss): Get actual project.
      'project': 'chromium',
  }
  headers = {
      'accept': 'application/json',
  }
  resp = requests.post(
      LOGDOG_BASE + 'prpc/logdog.Logs/Get', json=data, headers=headers)
  if resp.status_code != 200:
    print('Unexpected status code %d'
          ' when requesting build information from logdog' % resp.status_code)
    return None
  logdog_data = json.loads(resp.text[4:])
  text = ""
  for packet in logdog_data['logs']:
    for line in packet['text']['lines']:
      text += base64.b64decode(line['value']) + line['delimiter']
  return text


def run_command(cmd):
  """Small utility function to run a command.

  Prints out the command before running.
  """
  print(colorama.Fore.RED + '>> RUNNING the following command: <<\n%s%s' % (
      colorama.Style.RESET_ALL, ' '.join(cmd)))
  subprocess.check_call(cmd)


def guess_host_dimensions():
  """Guesses the approximate swarming dimensions for a host."""
  # For now, just do basic os. Can add later dimensions like gpus, number of
  # cores, etc...
  host_os = platform.system()
  return {
      'os': {
          # All the swarming bots are on ubuntu, so just assume that most dev
          # machines which run linux would work for bots which run ubuntu, even
          # if dev machines are a different distro.
          'linux': 'Ubuntu',
      }.get(host_os.lower(), host_os)
  }


class Build(object):
  """All relevant information for an already executed build.

  The constructor does several HTTP requests to get needed information from
  various LUCI services.
  """
  def __init__(self, build_address):
    # Build address of the build.
    self.build_address = urllib.unquote_plus(build_address)
    assert len(self.build_address.split('/')) == 3, (
      'Expected build address to look like <bucket>/<builder>/<buildnumber>, '
      'but got %s' % self.build_address
    )


    # Dimensions the user has acknowledged are different from the build they're
    # trying to debug. Once a user acks that a dimension is different, we
    # shouldn't prompt them about it again.
    self.acked_dimensions = set()

    # List of failed test suites.
    self.failed_suites = []

    # Mastername of the bot. Needed to find the appropriate //testing/buildbot
    # json file.
    self._mastername = ''

    # Buildbot id number. Used to fetch data from logdog.
    self._bb_id = ''

    # Chromium revision. Used to run `gclient sync`.
    self.chromium_revision = ''

    # Patch information. Tuple of (repo url, patch ref). Used in `gclient sync`.
    self.patch_info = ('', '')

    # Test results for each failed test.
    self._test_results = []

    # Builders this build is "emulating". Only useful for trybots. Trybots
    # emulate a few builders, which means they run the tests those builders run.
    # List of BuilderDep objects.
    self.emulated_builders = []

  @property
  def builder(self):
    return self.build_address.split('/')[1]

  @property
  def buildnumber(self):
    return self.build_address.split('/')[2]

  @property
  def bucket(self):
    return self.build_address.split('/')[0]

  @property
  def mastername(self):
    return self._mastername

  @property
  def is_tryjob(self):
    return bool(self.patch_info[0])

  def _fetch_build_info(self):
    """Fetch basic build information from buildbucket.

    Returns a tuple of (id, steps in the build, build properties)"""
    assert '.' not in self.bucket, (
      'Expected plain bucket with no \'.\' in it, got %s' % self.bucket)
    bb_url = BB_API_BASE + 'prpc/buildbucket.v2.Builds/GetBuild'
    data = {
        'builder': {
            # TODO(martiniss): Remove this hard coding.
            'project': 'chromium',
            'bucket': self.bucket,
            'builder': self.builder,
        },
        'buildNumber': int(self.buildnumber),
        'fields': 'steps,output.properties,id'
    }
    headers = {
        'accept': 'application/json',
    }

    res = requests.post(bb_url, json=data, headers=headers)
    # Remove XSSI header.
    resp = json.loads(res.text[4:])
    return (
        resp['id'],
        resp['steps'],
        resp['output']['properties'],
    )

  def _parse_failed_steps(self, steps):
    failures = [
        step for step in steps
        if 'tests' in step['name'] and step['status'] != u'SUCCESS'
    ]

    if self.is_tryjob:
      # Check for suites that were retried with patch. Suites which fail on
      # 'with patch' but pass later won't show up this way (because they're
      # flaky and shouldn't be re-run probably).
      failures = [
          step for step in failures
          if '(retry shards with patch)' in step['name']
      ]

    step_names = []
    for f in failures:
      for log in f['logs']:
        # Just want to check that the step has a step_metadata log in it.
        if log['name'] == 'step_metadata':
          metadata = json.loads(fetch_step_log(
              self._bb_id, f['name'], 'step_metadata'))
          if metadata:
            step_names.append(metadata['canonical_step_name'])
          else:
            step_names.append(f['name'])
    return step_names

  def _fetch_test_results(self, failed_suites):
    results = []
    for suite in failed_suites:
      test_results_url = TEST_RESULTS_BASE + 'testfile?%s' % urllib.urlencode({
          'builder': self.builder,
          'name': 'full_results.json',
          'master': self.mastername,
          'testtype': '%s (with patch)' % suite,
          'buildnumber': self.buildnumber,
      })
      data = requests.get(test_results_url).json()
      results.append(data)

    return results

  def _parse_test_results(self, results):
    test_results = {}
    for suite, result in results.iteritems():
      failed = []
      # Need a recursive function to be able to search nested test results
      # (blink web tests does this).
      def helper(data, prefix=None):
        if not prefix:
          prefix = ()
        for test, test_data in data.iteritems():
          if 'expected' in test_data:
            # This is a node containing test result information.
            if test_data['expected'] != test_data['actual']:
              if test_data['actual'].split(' ')[-1] == 'PASS':
                continue
              # Non blink tests use the test results format, but don't set this
              # flag :(.
              if (('webkit_layout' in suite or 'blink_web_tests' in suite)
                  and not test_data.get('is_unexpected')):
                continue
              failed.append('/'.join(prefix + (test,)))
          else:
            # This is just a prefix, doesn't contain actual result data.
            helper(test_data, prefix + (test,))

      helper(result['tests'])
      test_results[suite] = failed

    return test_results

  def _fetch_build_emulation(self):
    """Fetches logdog data about which builders this build emulates."""
    raw_data = fetch_step_log(self._bb_id, 'report_builders', 'bots.json')
    if not raw_data:
      # Try to guess, this should hopefully work. If it doesn't, something will
      # throw an exception, and hopefully the user will file a bug.
      return [
          BuilderDep(
            self.builder, self.mastername,
            self.builder, self.mastername)
      ]
    data = json.loads(raw_data)
    return [
        BuilderDep(
            entry['buildername'], entry['mastername'],
            entry['tester_buildername'], entry['tester_mastername'])
        for entry in data]

  def fetch_all_info(self):
    """Fetches info from various LUCI services."""
    # Get basic build info.
    bb_id, steps, properties = self._fetch_build_info()
    self._bb_id = bb_id
    self._mastername = properties['mastername']
    self.chromium_revision = properties.get('got_revision')
    self.patch_info = (
        properties.get('patch_repository_url'),
        properties.get('patch_ref'),
    )

    self.failed_suites = self._parse_failed_steps(steps)

    self._test_results = self._parse_test_results(
        dict(zip(self.failed_suites,
                 self._fetch_test_results(self.failed_suites))))

    self.emulated_builders = self._fetch_build_emulation()

  def guess_swarming_dimensions(self, suite):
    explicit_dims = self.lookup_suite(suite).swarming.get('dimensions_sets', {})
    if 'os' not in explicit_dims:
      # Copied from
      # https://cs.chromium.org/chromium/build/scripts/slave/recipe_modules/swarming/api.py?l=422&rcl=181d7618c62947cbfd9941f3d48537c92c955acf
      # Also very hacky, as it uses the mastername of the bot as a key.
      explicit_dims['os'] = {
          'linux': 'Ubuntu-14.04',
          # This is wrong often. Should refine this more.
          'mac': 'Mac-10.13',
          'win': 'Windows-10-15063',
      }.get(self.mastername.split('.')[-1], 'Ubuntu-14.04')

    return explicit_dims

  def run_suite(self, suite_name, path):
    suite = self.lookup_suite(suite_name)

    # Check that the build they're trying to reproduce is run on approximately
    # the same hardware.
    swarming_dimensions = self.guess_swarming_dimensions(suite_name)
    host_dimensions = guess_host_dimensions()
    # TODO(https://crbug.com/929332): Very specialized for os only for now,
    # change to be more general.
    for dimension in set(['os']) - self.acked_dimensions:
      swarming_value = swarming_dimensions[dimension]
      host_values = host_dimensions[dimension]
      if not swarming_value.startswith(host_values):
        print (
            'You are attempting to run a test suite which requires different'
            ' swarming dimensions than your current machine. The test suite'
            ' requires a %r value of %r, but we think your host machine has '
            ' values of %r. Do you wish to proceed with running the test? '
            '(y/N)' % (
                dimension, swarming_value, host_values))
        response = raw_input('>> ').lower()
        if response != 'y':
          print('Not running test suite %s...' % suite_name)
          return 1
        self.acked_dimensions.add(dimension)

    cases = self._test_results[suite_name]

    # Blink web tests end with this.
    suite_name = suite.name
    if suite_name.endswith('_exparchive'):
      suite_name = suite_name[:-len('_exparchive')]
    cmd = [
        os.path.join(CHROMIUM_ROOT, 'tools', 'mb', 'mb.py'), 'run', '-m',
        self.mastername, '-b', self.builder, path, suite.name, '--',
        '--%s=%s' % (
            'isolated-script-test-filter' if
              suite.is_isolated else 'gtest_filter',
              ('::' if suite.is_isolated else ':').join(cases)),
    ]
    cmd += suite.args

    temp_filename = None
    if suite.is_isolated:
      f, name = tempfile.mkstemp()
      os.close(f)
      temp_filename = name
      cmd += ['--isolated-script-test-output', temp_filename]

    try:
      run_command(cmd)
    finally:
      if suite.is_isolated:
        os.unlink(temp_filename)

  def checkout_commands(self):
    cmd = [
        'gclient',
        'sync',
        '-r', 'src@%s' % self.chromium_revision,
    ]
    if self.patch_info:
      cmd.extend([
          '--patch-ref',
          '@'.join(self.patch_info)
      ])
    yield cmd

  def ensure_checkout(self):
    for cmd in self.checkout_commands():
      run_command(cmd)

  def lookup_suite(self, suite_name):
    """Looks up information about a suite from json files in chromium src.

    Returns: A list of TestSuiteEntry objects.
    """
    found = []

    for entry in self.emulated_builders:
      with open(os.path.join(
          CHROMIUM_ROOT, 'testing', 'buildbot',
          '%s.json' % entry.tester_master)) as f:
        data = json.load(f)
      for file_buildername, builder_data in data.iteritems():
        if entry.tester_builder == file_buildername:
          for gtest in builder_data.get('gtest_tests', {}):
            name = gtest.get('name', gtest['test'])
            if name == suite_name:
              found.append(TestSuiteEntry(
                  gtest['test'],
                  gtest.get('args', []),
                  False,
                  gtest.get('swarming', {}),
              ))
          for isolated_script in builder_data.get('isolated_scripts', {}):
            if isolated_script['name'] == suite_name:
              found.append(TestSuiteEntry(
                  isolated_script['isolate_name'],
                  isolated_script.get('args', []),
                  True,
                  isolated_script.get('swarming', {}),
              ))
          break

    if not found:
      raise Exception('Suite %s not found in //testing/buildbot files' % (
          suite_name))
    if len(found) > 1:
      raise Exception('Suite %s found in multiple builder definitions.' % (
          suite_name))
    return found[0]


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)

  parser.add_argument('build_url',
                      help='the URL of a build you want to debug locally. '
                      'Usually something like '
                      '%sluci.chromium.try/linux-rel/265964.' % CI_PREFIX)
  parser.add_argument('path', nargs='?', default='//out/Default',
                      help='path to output directory to build. Default is '
                      '%(default)s.')
  parser.add_argument('-s', '--sync-checkout', action='store_true',
                      help='attempts to emulate the chromium checkout in the '
                      'build to reproduce. Does this by calling `gclient sync`')
  parser.add_argument('-n', '--no-run-tests', action='store_false',
                      dest='run_tests', help='don\'t run any tests. Useful if '
                      'you just want to see what failed, or just want to sync '
                      'your checkout to match a failed build.')
  args = parser.parse_args()

  if not args.build_url.startswith(CI_PREFIX):
    raise Exception('Invalid build URL %s. Expected it to start with %s' % (
        args.build_url, CI_PREFIX))
  build_address = args.build_url[len(CI_PREFIX):]
  print('Fetching build information...')
  build = Build(build_address)
  build.fetch_all_info()

  if args.sync_checkout:
    print('The build you are attempting to debug is a %s.' % (
        'tryjob' if build.is_tryjob else 'continuous build'))
    print('The build checked out revision %s%s.' % (
        build.chromium_revision,
        ' and applied the patch https://crrev.com/c/%s' % (
            '/'.join(build.patch_info[1].split('/')[3:])
            if build.is_tryjob else '')))
    # TODO(martiniss): Consider validating the user's gclient config.
    print(
        'This script can attempt to reproduce the chromium checkout the build '
        'executed with. It would do so by running the following commands:')
    print()
    for cmd in build.checkout_commands():
      print(' '.join(cmd))
    print()

    print('Would you like this script to run these commands? (Y/n)')
    response = raw_input('>> ').lower().strip()
    if response in ('y', ''):
      build.ensure_checkout()
    else:
      print('Using your existing checkout.')

  if not args.run_tests:
    return 0

  print('Found the following failed test suites for this build:')
  for i, suite in enumerate(build.failed_suites, start=1):
    print('  %s. %s' % (i, suite))
  print('Commands are:')
  for cmd in (
      '1,2....n to run that specific test suite.',
      'A to run all the test suites',
      'Q to quit',
  ):
    print('  ' + cmd)
  print('What would you like to do?')

  response = raw_input('>> ').lower()

  try:
    num = int(response)

    # Subtract one, since the suites start with an index of 1.
    suite = build.failed_suites[num-1]
    build.run_suite(suite, args.path)
    return 0
  except ValueError:
    # If the response isn't a number, just assume it was another command.
    pass

  if response == 'a':
    for failed_suite in build.failed_suites:
      build.run_suite(failed_suite, args.path)

  if response != 'q':
    print('Unrecognized command "%s"'% response)

  print('Exiting...')
  return 0


if __name__ == '__main__':
  try:
    sys.exit(main())
  except Exception:
    traceback.print_exc()
    print()
    print()
    print('If this exception is unexpected, please file a bug via '
          'https://bit.ly/cci-generic-bug.')
    sys.exit(1)

