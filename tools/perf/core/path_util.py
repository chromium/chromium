# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def GetChromiumSrcDir():
  return os.path.abspath(os.path.join(
      os.path.dirname(__file__), '..', '..', '..'))


def GetTelemetryDir():
  return os.path.join(
      GetChromiumSrcDir(), 'third_party', 'catapult', 'telemetry')


def GetTracingDir():
  return os.path.join(
      GetChromiumSrcDir(), 'third_party', 'catapult', 'tracing')


def GetPyUtilsDir():
  return os.path.join(
      GetChromiumSrcDir(), 'third_party', 'catapult', 'common', 'py_utils')


def GetPerfDir():
  return os.path.join(GetChromiumSrcDir(), 'tools', 'perf')


def GetPerfStorySetsDir():
  return os.path.join(GetPerfDir(), 'page_sets')


def GetOfficialBenchmarksDir():
  return os.path.join(GetPerfDir(), 'benchmarks')


def GetContribDir():
  return os.path.join(GetPerfDir(), 'contrib')


def GetAndroidPylibDir():
  return os.path.join(GetChromiumSrcDir(), 'build', 'android')


def AddTelemetryToPath():
  telemetry_path = GetTelemetryDir()
  if telemetry_path not in sys.path:
    sys.path.insert(1, telemetry_path)


def AddTracingToPath():
  tracing_path = GetTracingDir()
  if tracing_path not in sys.path:
    sys.path.insert(1, tracing_path)


def AddPyUtilsToPath():
  py_utils_dir = GetPyUtilsDir()
  if py_utils_dir not in sys.path:
    sys.path.insert(1, py_utils_dir)


def AddAndroidPylibToPath():
  android_pylib_path = GetAndroidPylibDir()
  if android_pylib_path not in sys.path:
    sys.path.insert(1, android_pylib_path)


def GetExpectationsPath():
  return os.path.join(GetPerfDir(), 'expectations.config')
