# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for metrics_proto.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""


DIR_METADATA = 'DIR_METADATA'
README = 'README.chromium'
PRESUBMIT = 'PRESUBMIT.py'
PRESUBMIT_TEST = 'PRESUBMIT_test.py'
OWNERS = 'OWNERS'


def IsMetricsProtoPath(input_api, path):
  return input_api.os_path.dirname(path) == input_api.PresubmitLocalPath()


def IsReadmeFile(input_api, path):
  return (input_api.os_path.basename(path) == README and
          IsMetricsProtoPath(input_api, path))


def IsImportedFile(input_api, path):
  return (not input_api.os_path.basename(path) in (PRESUBMIT, PRESUBMIT_TEST,
                                                  OWNERS, DIR_METADATA) and
          IsMetricsProtoPath(input_api, path))


def CheckChange(input_api, output_api):
  """Checks that all changes include a README update."""
  paths = [af.AbsoluteLocalPath() for af in input_api.AffectedFiles()]
  if (any(IsImportedFile(input_api, p) for p in paths) and not
      any(IsReadmeFile(input_api, p) for p in paths)):
    return [output_api.PresubmitError(
            'Modifies %s without updating %s. '
            'Changes to these files should originate upstream.' %
            (input_api.PresubmitLocalPath(), README))]
  return []


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
