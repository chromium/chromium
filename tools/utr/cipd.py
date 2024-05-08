# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Util for fetching recipe bundles from CIPD."""

import logging
import pathlib
import subprocess
import sys

_THIS_DIR = pathlib.Path(__file__).resolve().parent

# Bundles will be placed under //tools/utr/.bundles/.
_CIPD_ROOT_BASE_DIR = _THIS_DIR.joinpath('.bundles')

# TODO(crbug.com/40712760): Remove non-inclusive terms when repo changes name.
_CHROME_RECIPE_BUNDLE = (
    'infra_internal/recipe_bundles/chrome-internal.googlesource.com/chrome/'
    'tools/build_limited/scripts/slave')  # nocheck
_CHROMIUM_RECIPE_BUNDLE = (
    'infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build')
_RECIPE_BUNDLE_VERSION = 'refs/heads/main'


def fetch_recipe_bundle(project, is_verbose):
  cipd_root_dir = _CIPD_ROOT_BASE_DIR.joinpath(project)
  # Assume cipd client is on PATH. Since we're in a standard Chromium checkout,
  # it should be a safe bet.
  exe = 'cipd.bat' if sys.platform == 'win32' else 'cipd'
  if not cipd_root_dir.exists():
    # Re-using the same cipd "root" that `gclient sync` uses in a checkout leads
    # to interference. (ie: `gclient sync` wiping out the bundle.) So just use
    # our own root for the bundle.
    cmd = [exe, 'init', '-force', str(cipd_root_dir)]
    logging.info('Initializing cipd root for bundle:')
    # Use the "basic_logger" here (and below) to avoid rich from coloring random
    # bits of the printed command.
    logging.getLogger('basic_logger').info(' '.join(cmd))
    subprocess.check_call(cmd)

  recipe_bundle_package = _CHROME_RECIPE_BUNDLE
  if project == 'chromium':
    recipe_bundle_package = _CHROMIUM_RECIPE_BUNDLE
  cmd = [
      exe,
      'install',
      recipe_bundle_package,
      _RECIPE_BUNDLE_VERSION,
      '-root',
      str(cipd_root_dir),
      '-log-level',
      'debug' if is_verbose else 'warning',
  ]
  logging.info('[cyan]Running bundle install command:[/]')
  logging.getLogger('basic_logger').info(' '.join(cmd))
  # The stdout of `cipd install` seems noisy, and all useful logs appear to go
  # to stderr anyway.
  subprocess.check_call(cmd, stdout=subprocess.DEVNULL)
  return cipd_root_dir
