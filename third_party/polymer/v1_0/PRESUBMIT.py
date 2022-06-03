# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for third_party/polymer/v1_0.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import os
import json

USE_PYTHON3 = True

def _CheckBowerDependencies(input_api, output_api):
  os_path = input_api.os_path
  cwd = input_api.PresubmitLocalPath()
  components_dir = os_path.join(cwd, 'components-chromium')
  bower_json_path = os_path.join(cwd, 'bower.json')

  for f in input_api.AffectedFiles():
    p = f.AbsoluteLocalPath()
    if p == bower_json_path or p.startswith(components_dir):
      break
  else:
      return []

  bower_dependencies = \
      set(json.load(open(bower_json_path))['dependencies'].keys())
  installed_components = set(p for p in os.listdir(components_dir))
  # Add web-animations-js because we keep it in a separate directory
  # '../third_party/web-animations-js'.
  installed_components.add('web-animations-js')
  # Add shadycss because it ends up bundled withing
  # components-chromium/polymer2 (see minify_polymer.py).
  installed_components.add('shadycss')

  if bower_dependencies == installed_components:
    return []

  problems = []

  if installed_components - bower_dependencies:
    problems.append(output_api.PresubmitError(
        'Found components that are not listed in bower.json.',
        items = list(installed_components - bower_dependencies)))

  if bower_dependencies - installed_components:
    problems.append(output_api.PresubmitError(
        'Some of the Bower dependencies are not installed.',
        items = list(bower_dependencies - installed_components)))

  return problems


def _CommonChecks(input_api, output_api):
  return _CheckBowerDependencies(input_api, output_api) + \
         _RunUnitTestsIfNeeded(input_api, output_api)


def _RunUnitTestsIfNeeded(input_api, output_api):
  cwd, path = input_api.PresubmitLocalPath(), input_api.os_path
  files = [path.basename(f.LocalPath()) for f in input_api.AffectedFiles()]
  tests = []

  if any(f for f in files if f.startswith('css_strip_prefixes')):
    tests.append(path.join(cwd, 'css_strip_prefixes_test.py'))

  if any(f for f in files if f.startswith('rgbify_hex_vars')):
    tests.append(path.join(cwd, 'rgbify_hex_vars_test.py'))

  return input_api.canned_checks.RunUnitTests(input_api, output_api, tests)


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
