#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Refreshes existing WPR archive files from live Autofill Server

  $ tools/captured_sites/refresh.py [site_name]

This script attempts to capture the process of refreshing a site's Autofill
Server Predictions.

It will loop through the given sites and run the refresh process which hits
the Autofill Server to receive fresh Autofill Server Predictions. It then
removes the existing WPR file's predictions, and merges in the update ones.

With no arguments or just an '*', the script will run through all non-disabled
sites in the testcases.json file.

An optional argument of [site_name] can be provided to refresh a single site.
"""

from __future__ import print_function

import argparse
import json
import os
import signal
import sys
import subprocess

_BASE_FOLDER = 'chrome/test/data/autofill/captured_sites/artifacts'
_TELEMETRY_BIN_FOLDER = ('third_party/catapult/telemetry/telemetry/bin/'
                         'linux/x86_64/')
_TRIMMED_FOLDER = os.path.join(_BASE_FOLDER, 'trimmed')
_REFRESH_FOLDER = os.path.join(_BASE_FOLDER, 'refresh')
_MERGED_FOLDER = os.path.join(_BASE_FOLDER, 'merged')
_PRINT_ONLY = False


class Refresh():
  def collect_sites(self, testcases_file):
    with open(testcases_file, 'r') as file:
      content = json.load(file)
      self.sites = content["tests"]
    filtered = list(filter(lambda a: 'disabled' not in a, self.sites))
    return filtered

  def refresh_site(self, site_name):
    """Run the Refresh test for the given site_name. This process will create
    a new .wpr archive in the captured_sites/refresh folder. Runs the process
    with flags:
       --store-log to keep text log
       --release to run against release build
       --background to run with xvfb.py."""
    command_args = [
        'tools/captured_sites/control.py', 'refresh', '--store-log',
        '--release', '--background', site_name
    ]
    _make_process_call(command_args, _PRINT_ONLY)

  def delete_existing_predictions(self, site_name):
    """Use httparchive go tool to remove any existing Server Predictions stored
    in the current .wpr archive and create a trimmed version in the
    captured_sites/trimmed folder."""
    host_domains = ['clients1.google.com', 'content-autofill.googleapis.com']
    existing_wpr_archive = os.path.join(_BASE_FOLDER, '%s.wpr' % site_name)
    trimmed_wpr_archive = os.path.join(_TRIMMED_FOLDER, '%s.wpr' % site_name)
    first_trim = True

    for host_domain in host_domains:
      to_trim_wpr_archive = trimmed_wpr_archive
      if first_trim:
        to_trim_wpr_archive = existing_wpr_archive
        first_trim = False

      command_args = [
          _TELEMETRY_BIN_FOLDER + 'httparchive', 'trim', '--host', host_domain,
          to_trim_wpr_archive, trimmed_wpr_archive
      ]
      _make_process_call(command_args, _PRINT_ONLY)

  def merge_new_predictions(self, site_name):
    """Use httparchive go tool to merge the .wpr file in refresh/ folder with
    the .wpr file in trimmed/ folder and create a new .wpr file in the
    merged/ folder."""
    trimmed_wpr_archive = os.path.join(_TRIMMED_FOLDER, '%s.wpr' % site_name)
    fresh_wpr_archive = os.path.join(_REFRESH_FOLDER, '%s.wpr' % site_name)
    merged_wpr_archive = os.path.join(_MERGED_FOLDER, '%s.wpr' % site_name)

    command_args = [
        _TELEMETRY_BIN_FOLDER + 'httparchive', 'merge', trimmed_wpr_archive,
        fresh_wpr_archive, merged_wpr_archive
    ]
    _make_process_call(command_args, _PRINT_ONLY)

  def update_expectations(self, site_name):
    """Update .test file expectations to reflect the changes in the newly merged
    Server Predictions"""
    cmd = '...'
    #TODO(crbug.com/40216356)
    print('Not Implemented')


def _parse_args():
  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawTextHelpFormatter)
  parser.usage = __doc__
  parser.add_argument('site_name',
                      nargs='?',
                      default='*',
                      help=('The site name which should have a match in '
                            'testcases.json. Use * to indicate all enumerated '
                            'sites in that file.'))
  return parser.parse_args()


def _make_process_call(command_args, print_only):
  command_text = ' '.join(command_args)
  print(command_text)
  if print_only:
    return

  if not os.path.exists(command_args[0]):
    raise EnvironmentError('Cannot locate binary to execute. '
                           'Ensure that working directory is chromium/src')
  subprocess.call(command_text, shell=True)


def _create_subfolders():
  assert os.path.isdir(_BASE_FOLDER), ('Expecting path "%s" to exist in your '
                                       'chromium checkout' % _BASE_FOLDER)
  if not os.path.isdir(_MERGED_FOLDER):
    os.mkdir(_MERGED_FOLDER)
  if not os.path.isdir(_REFRESH_FOLDER):
    os.mkdir(_REFRESH_FOLDER)
  if not os.path.isdir(_TRIMMED_FOLDER):
    os.mkdir(_TRIMMED_FOLDER)


def _handle_signal(sig, _):
  """Handles received signals to make sure spawned test process are killed.

  sig (int): An integer representing the received signal, for example SIGTERM.
  """

  # Don't do any cleanup here, instead, leave it to the finally blocks.
  # Assumption is based on https://docs.python.org/3/library/sys.html#sys.exit:
  # cleanup actions specified by finally clauses of try statements are honored.

  # https://tldp.org/LDP/abs/html/exitcodes.html:
  # Exit code 128+n -> Fatal error signal "n".
  print('Signal to quit received')
  sys.exit(128 + sig)


def main():
  for sig in (signal.SIGTERM, signal.SIGINT):
    signal.signal(sig, _handle_signal)

  _create_subfolders()

  options = _parse_args()

  r = Refresh()

  if options.site_name == '*':
    sites = r.collect_sites(os.path.join(_BASE_FOLDER, 'testcases.json'))
    print('Refreshing %d sites from the testcases file' % len(sites))
  else:
    sites = [{'site_name': options.site_name}]
    print('Refreshing single site "%s"' % options.site_name)

  for site in sites:
    site_name = site['site_name']
    print('Refreshing Server Predictions for "%s"' % site_name)
    r.refresh_site(site_name)
    r.delete_existing_predictions(site_name)
    r.merge_new_predictions(site_name)
  print('Merged WPR archives have been written to "%s"' % _MERGED_FOLDER)


if __name__ == '__main__':
  sys.exit(main())
