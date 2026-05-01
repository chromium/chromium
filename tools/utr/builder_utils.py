# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utils for inferring builder names from GN args."""

import json
import logging
import os
import pathlib
import platform
import subprocess
import sys

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_DIR = _THIS_DIR.parents[1]

# Add SRC_DIR to path to allow importing from build/
sys.path.append(str(_SRC_DIR))
from build import gn_helpers
from build import detect_host_arch


def read_explicit_gn_args(build_dir):
  """Reads explicit GN args from build dir"""
  return gn_helpers.ReadArgsGN(str(build_dir))


def get_host_fallback_args():
  """Returns host OS and CPU."""
  host_os = platform.system().lower()
  if host_os == 'darwin':
    host_os = 'mac'
  elif host_os == 'windows':
    host_os = 'win'

  host_cpu = detect_host_arch.HostArch()
  return host_os, host_cpu


def guess_builder(build_dir):
  """Infers the builder name based on build_dir."""
  gn_args = read_explicit_gn_args(build_dir)

  if not gn_args:
    logging.info('No GN args found in build dir, '
                 'falling back to host machine dimensions.')
    target_os, target_cpu = get_host_fallback_args()
  else:
    target_os = gn_args.get('target_os')
    target_cpu = gn_args.get('target_cpu')
    if not target_os or not target_cpu:
      # If an args.gn file is present but does not explicitly specify target_os
      # or target_cpu, default to the host machine's dimensions.
      fallback_os, fallback_cpu = get_host_fallback_args()
      target_os = target_os or fallback_os
      target_cpu = target_cpu or fallback_cpu

  mapping = {
      ('linux', 'x64'): ('ci', 'Linux Tests'),
      ('mac', 'x64'): ('ci', 'mac15-x64-rel-tests'),
      ('mac', 'arm64'): ('ci', 'mac15-arm64-rel-tests'),
      ('win', 'x64'): ('ci', 'Win10 Tests x64'),
      ('android', 'arm64'): ('ci', 'android-14-arm64-rel'),
      ('android', 'x64'): ('ci', 'android-16-x64-rel'),
      ('android', 'x86'): ('ci', 'android-x86-rel'),
      ('chromeos', 'x64'): ('ci', 'chromeos-amd64-generic-rel-gtest'),
      ('chromeos', 'arm64'): ('ci', 'chromeos-arm64-generic-rel'),
      ('ios', 'arm64'): ('ci', 'ios-simulator'),
  }

  key = (target_os, target_cpu)
  return mapping.get(key)
