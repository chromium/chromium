# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Util for fetching recipe bundles from CIPD."""

import logging
import pathlib
import subprocess
import sys

_THIS_DIR = pathlib.Path(__file__).resolve().parent

# Bundle will be placed at //tools/utr/.bundle/.
_CIPD_ROOT_DIR = _THIS_DIR.joinpath('.bundle')

# TODO(crbug.com/41492688): Support internal bundles for internal builders.
_RECIPE_BUNDLE = 'infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build'
_RECIPE_BUNDLE_VERSION = 'refs/heads/main'


def fetch_recipe_bundle(is_verbose):
  # Assume cipd client is on PATH. Since we're in a standard Chromium checkout,
  # it should be a safe bet.
  exe = 'cipd.bat' if sys.platform == 'win32' else 'cipd'
  if not _CIPD_ROOT_DIR.exists():
    # Re-using the same cipd "root" that `gclient sync` uses in a checkout leads
    # to interference. (ie: `gclient sync` wiping out the bundle.) So just use
    # our own root for the bundle.
    cmd = [exe, 'init', '-force', str(_CIPD_ROOT_DIR)]
    logging.info('Initializing cipd root for bundle:')
    logging.info(' '.join(cmd))
    subprocess.check_call(cmd)

  cmd = [
      exe,
      'install',
      _RECIPE_BUNDLE,
      _RECIPE_BUNDLE_VERSION,
      '-root',
      str(_CIPD_ROOT_DIR),
      '-log-level',
      'debug' if is_verbose else 'warning',
  ]
  logging.info('Running bundle install command:')
  logging.info(' '.join(cmd))
  # The stdout of `cipd install` seems noisy, and all useful logs appear to go
  # to stderr anyway.
  subprocess.check_call(cmd, stdout=subprocess.DEVNULL)
