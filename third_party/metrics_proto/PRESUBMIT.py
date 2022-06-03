# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for metrics_proto.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

USE_PYTHON3 = True

README = 'README.chromium'
PRESUBMIT = 'PRESUBMIT.py'


def IsMetricsProtoPath(input_api, path):
  return input_api.os_path.dirname(path) == input_api.PresubmitLocalPath()


def IsReadmeFile(input_api, path):
  return (input_api.basename(path) == README and
          IsMetricsProtoPath(input_api, path))


def IsPresubmitFile(input_api, path):
  return (input_api.basename(path) == PRESUBMIT and
          IsMetricsProtoPath(input_api, path))


def CheckChange(input_api, output_api):
  """Checks that all changes include a README update."""
  paths = [af.AbsoluteLocalPath() for af in input_api.AffectedFiles()]
  if (any((IsMetricsProtoPath(input_api, p) for p in paths)) and not any(
      (IsReadmeFile(input_api, p) or IsPresubmitFile(input_api, p)
       for p in paths))):
    return [output_api.PresubmitError(
            'Modifies %s without updating %s. '
            'Changes to these files should originate upstream.' %
            (input_api.PresubmitLocalPath(), README))]
  return []


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
