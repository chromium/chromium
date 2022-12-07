# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

# This file should be kept in sync with android_browser_types.gni. It provides
# a list of Android Chromium Telemetry targets for use in Python scripts.

_CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
_CLANK_DIR = os.path.join(_CHROMIUM_SRC_DIR, 'clank')
_CLANK_LIST_FILEPATH = os.path.join(_CLANK_DIR, 'telemetry_browser_types.py')

TELEMETRY_ANDROID_BROWSER_TARGET_SUFFIXES = [
    '_android_chrome',
    '_android_monochrome',
    '_android_monochrome_bundle',
    '_android_webview',
]

if os.path.exists(_CLANK_LIST_FILEPATH):
  sys.path.append(_CLANK_DIR)
  import telemetry_browser_types  # pylint: disable=import-error,wrong-import-position
  sys.path.remove(_CLANK_DIR)
  TELEMETRY_ANDROID_BROWSER_TARGET_SUFFIXES +=\
      telemetry_browser_types.TELEMETRY_CLANK_BROWSER_TARGET_SUFFIXES
else:
  logging.warning(
      'No Clank checkout detected - falling back to hard-coded list of '
      'suffixes, which may be out of date')
  TELEMETRY_ANDROID_BROWSER_TARGET_SUFFIXES += [
      '_android_clank_chrome',
      '_android_clank_monochrome',
      '_android_clank_monochrome_64_32_bundle',
      '_android_clank_monochrome_bundle',
      '_android_clank_trichrome_chrome_google_64_32_bundle',
      '_android_clank_trichrome_bundle',
      '_android_clank_trichrome_webview',
      '_android_clank_trichrome_webview_bundle',
      '_android_clank_webview',
      '_android_clank_webview_bundle',
  ]
