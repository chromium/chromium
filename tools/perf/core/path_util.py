# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import sys


@contextlib.contextmanager
def SysPath(path, position=None):
  if position is None:
    sys.path.append(path)
  else:
    sys.path.insert(position, path)
  try:
    yield
  finally:
    if sys.path[-1] == path:
      sys.path.pop()
    else:
      sys.path.remove(path)


def GetChromiumSrcDir():
  return os.path.abspath(os.path.join(
      os.path.dirname(__file__), '..', '..', '..'))


def GetAndroidDeviceInteractionToPath():
  return os.path.join(GetChromiumSrcDir(), 'third_party', 'catapult', 'devil')


def GetTelemetryDir():
  return os.path.join(
      GetChromiumSrcDir(), 'third_party', 'catapult', 'telemetry')


def GetTracingDir():
  return os.path.join(
      GetChromiumSrcDir(), 'third_party', 'catapult', 'tracing')


def GetPyUtilsDir():
  return os.path.join(
      GetChromiumSrcDir(), 'third_party', 'catapult', 'common', 'py_utils')


def GetCrossBenchDir():
  return os.path.join(GetChromiumSrcDir(), 'third_party', 'crossbench')


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


def GetVariationsDir():
  return os.path.join(GetChromiumSrcDir(), 'tools', 'variations')


def AddAndroidDeviceInteractionToPath():
  device_interaction_path = GetAndroidDeviceInteractionToPath()
  if device_interaction_path not in sys.path:
    sys.path.insert(1, device_interaction_path)


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


def AddCrossBenchToPath():
  crossbench_path = GetCrossBenchDir()
  if crossbench_path not in sys.path:
    sys.path.insert(1, crossbench_path)


def GetExpectationsPath():
  return os.path.join(GetPerfDir(), 'expectations.config')
