# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for structured.xml, stored in the sync directory.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

STRUCTURED_XML = 'sync/structured.xml'
STRUCTURED_OLD_XML = 'sync/structured.old.xml'


def CheckChange(input_api, output_api):
  """ Checks that structured.xml is pretty-printed and well-formatted. """
  errors = []

  for file in input_api.AffectedTextFiles():
    path = file.AbsoluteLocalPath()
    basename = input_api.basename(path)
    if input_api.os_path.dirname(path) != input_api.PresubmitLocalPath():
      continue

    if basename == STRUCTURED_XML:
      cwd = input_api.os_path.dirname(path)
      exit_code = input_api.subprocess.call(
          [input_api.python3_executable, 'pretty_print.py', '--presubmit'],
          cwd=cwd)
      if exit_code != 0:
        errors.append(
            output_api.PresubmitError(
                STRUCTURED_XML +
                ' is not prettified; run `git cl format` to fix.'))
    elif basename == STRUCTURED_OLD_XML:
      errors.append(
          output_api.PresubmitError(
              STRUCTURED_OLD_XML +
              ' exists after formatting; please remove before upload.'))

  errors.extend(input_api.canned_checks.RunPylint(input_api, output_api))

  return errors


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
