# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _CommonChecks(input_api, output_api):
  """Performs common checks, which includes running pylint."""
  results = []

  results.extend(_CheckContribDir(input_api, output_api))
  return results


def _CheckOwnershipForContribSubDir(sub_dir, input_api, output_api):
  results = []
  owner_file = input_api.os_path.join(sub_dir, 'OWNERS')
  if not input_api.os_path.isfile(owner_file):
    results.append(output_api.PresubmitError(
        '%s must have an OWNERS file' % sub_dir))
  return results


def _CheckContribDir(input_api, output_api):
  """Check that:
  - tools/perf/contrib/ contains only directories (except for
    __init__.py, README.md, and PRESUBMIT.py)
  - Each directory under tools/perf/contrib has an OWNERS file.
  """
  results = []
  contrib_dir = input_api.PresubmitLocalPath()
  init = input_api.os_path.join(contrib_dir, '__init__.py')
  readme = input_api.os_path.join(contrib_dir, 'README.md')
  presubmit = input_api.os_path.join(contrib_dir, 'PRESUBMIT.py')

  invalid_contrib_files = []
  for f in input_api.AffectedFiles(include_deletes=False):
    file_path = f.AbsoluteLocalPath()
    if (input_api.os_path.dirname(file_path) == contrib_dir and
        not file_path in (init, readme, presubmit)):
      invalid_contrib_files.append(file_path)

  for f in input_api.os_listdir(contrib_dir):
    if f == '__pycache__':
      continue
    path = input_api.os_path.join(contrib_dir, f)
    if input_api.os_path.isdir(path):
      results.extend(
          _CheckOwnershipForContribSubDir(path, input_api, output_api))

  if invalid_contrib_files:
    results.append(
        output_api.PresubmitError(
            'You cannot add files to the top level of a contrib directory. '
            'Please move these files to a sub directory:\n %s' %
            '\n'.join(invalid_contrib_files)))
  return results


def CheckChangeOnUpload(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api))
  return report


def CheckChangeOnCommit(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api))
  return report
