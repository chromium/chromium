# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for ukm/ukm.xml on changes made to histograms/enums.xml

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""


UKM_XML = 'ukm.xml'
ENUMS_XML = 'enums.xml'


def CheckChange(input_api, output_api):
  """Checks that ukm/ukm.xml is validated on changes to histograms/enums.xml"""
  for f in input_api.AffectedTextFiles():
    p = f.AbsoluteLocalPath()
    # Early return if the ukm file is changed, then the presubmit script in the
    # ukm directory would run and report the errors.
    if (UKM_XML in p):
      return []
  for f in input_api.AffectedTextFiles():
    p = f.AbsoluteLocalPath()
    filepath = input_api.os_path.relpath(p, input_api.PresubmitLocalPath())
    if (ENUMS_XML in filepath):
      cwd = input_api.os_path.dirname(input_api.PresubmitLocalPath())

      exit_code = input_api.subprocess.call([
          input_api.python3_executable, 'metrics/ukm/validate_format.py',
          '--presubmit'
      ],
                                            cwd=cwd)
      if exit_code != 0:
        return [
            output_api.PresubmitError(
                '%s does not pass format validation; run '
                '%s/ukm/validate_format.py and fix the reported error(s) or '
                'warning(s).' % (UKM_XML, input_api.PresubmitLocalPath())),
        ]

  return []


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
