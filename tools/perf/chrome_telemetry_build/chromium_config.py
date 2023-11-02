# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

# pylint: disable=wrong-import-position
from core import path_util

CLIENT_CONFIG_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'binary_dependencies.json')

with path_util.SysPath(path_util.GetTelemetryDir()):
  from telemetry import project_config
with path_util.SysPath(path_util.GetVariationsDir()):
  import fieldtrial_util  # pylint: disable=import-error
with path_util.SysPath(path_util.GetAndroidPylibDir()):
  from pylib.constants import host_paths


class ChromiumConfig(project_config.ProjectConfig):

  def __init__(self, top_level_dir=None, benchmark_dirs=None,
               client_configs=None, default_chrome_root=None,
               expectations_files=None):
    if client_configs is None:
      client_configs = [CLIENT_CONFIG_PATH]
    if default_chrome_root is None:
      default_chrome_root = path_util.GetChromiumSrcDir()
    # Some android utilities in Catapult expect a java binary available on PATH,
    # so place chromium's heremetic JDK on PATH if we have it checked out.
    if os.path.exists(host_paths.JAVA_PATH):
      os.environ['PATH'] = (host_paths.JAVA_PATH + os.pathsep +
                            os.environ['PATH'])

    super(ChromiumConfig, self).__init__(
        top_level_dir=top_level_dir, benchmark_dirs=benchmark_dirs,
        client_configs=client_configs, default_chrome_root=default_chrome_root,
        expectations_files=expectations_files)

  def AdjustStartupFlags(self, args):
    return fieldtrial_util.MergeFeaturesAndFieldTrialsArgs(args)


def GetDefaultChromiumConfig():
  return ChromiumConfig(
      benchmark_dirs=[path_util.GetOfficialBenchmarksDir(),
                      path_util.GetContribDir()],
      top_level_dir=path_util.GetPerfDir(),
      expectations_files=[path_util.GetExpectationsPath()])
