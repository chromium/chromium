# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

import os
from pathlib import Path
import sys


def GetPrettyPrintErrors(input_api, output_api, cwd, rel_path, results):
  """Runs pretty-print command for specified file."""
  args = [
      input_api.python3_executable,
      os.path.join(input_api.PresubmitLocalPath(), 'pretty_print.py'), rel_path,
      '--presubmit', '--non-interactive'
  ]
  exit_code = input_api.subprocess.call(args, cwd=cwd)

  if exit_code != 0:
    error_msg = ('%s is not formatted correctly; run `git cl format` to fix.' %
                 rel_path)
    results.append(output_api.PresubmitError(error_msg))


def GetTokenErrors(input_api, output_api, cwd, rel_path, results):
  """Validates histogram tokens in specified file."""
  exit_code = input_api.subprocess.call([
      input_api.python3_executable,
      os.path.join(input_api.PresubmitLocalPath(), 'validate_token.py'),
      rel_path
  ],
                                        cwd=cwd)

  if exit_code != 0:
    error_msg = (
        '%s contains histogram(s) using <variants> not defined in the file, '
        'please run validate_token.py %s to fix.' % (rel_path, rel_path))
    results.append(output_api.PresubmitError(error_msg))


def GetValidateHistogramsError(input_api, output_api, cwd, results):
  """Validates histograms format and index file."""
  exit_code = input_api.subprocess.call([
      input_api.python3_executable,
      os.path.join(input_api.PresubmitLocalPath(), 'validate_format.py')
  ],
                                        cwd=cwd)

  if exit_code != 0:
    error_msg = (
        'Histograms are not well-formatted; please run %s/validate_format.py '
        'and fix the reported errors.' % cwd)
    results.append(output_api.PresubmitError(error_msg))

  exit_code = input_api.subprocess.call([
      input_api.python3_executable,
      os.path.join(input_api.PresubmitLocalPath(),
                   'validate_histograms_index.py')
  ],
                                        cwd=cwd)

  if exit_code != 0:
    error_msg = ('Histograms index file is not up-to-date. Please run '
                 '%s/histogram_paths.py to update it' % cwd)
    results.append(output_api.PresubmitError(error_msg))


def ValidateSingleFile(input_api, output_api, file_obj, cwd, results,
                       allow_test_paths):
  """Does corresponding validations if histograms.xml or enums.xml is changed.

  Args:
    input_api: An input_api instance that contains information about changes.
    output_api: An output_api instance to create results of the PRESUBMIT check.
    file_obj: A file object of one of the changed files.
    cwd: Path to current working directory.
    results: The returned variable which is a list of output_api results.
    allow_testing_paths: A boolean that determines if the test_data directory
      changes should be validated. If it's False, all the files under
      `test_data` directory will be ignored. This is needed as the `test_data`
      xmls contains intentional errors to trip the presubmit checks and we want
      to trip those presubmits in checks, but a the same time due to the nature
      of how the input_api gives all changed files in the directory, we don't
      want "production" checks to trip over those mistakes.

  Returns:
    A boolean that True if a histograms.xml or enums.xml file is changed.
  """
  p = file_obj.AbsoluteLocalPath()
  # Only do PRESUBMIT checks when |p| is under |cwd|.
  if input_api.os_path.commonprefix([p, cwd]) != cwd:
    return False
  filepath = input_api.os_path.relpath(p, cwd)

  if not allow_test_paths and 'test_data' in filepath:
    return False

  # If the changed file is histograms.xml or histogram_suffixes_list.xml,
  # pretty-print it.
  elif ('histograms.xml' in filepath
        or 'histogram_suffixes_list.xml' in filepath):
    GetPrettyPrintErrors(input_api, output_api, cwd, filepath, results)
    GetTokenErrors(input_api, output_api, cwd, filepath, results)
    return True

  # If the changed file is enums.xml, pretty-print it.
  elif 'enums.xml' in filepath:
    GetPrettyPrintErrors(input_api, output_api, cwd, filepath, results)
    return True

  return False


def CheckHistogramFormatting(input_api, output_api, allow_test_paths=False):
  """Checks that histograms.xml is pretty-printed and well-formatted.

  This is a method that is called by the PRESUBMIT system and those it
  represents a production check rather then a test one. This is why we
  set allow_test_paths to False by default.
  """
  results = []
  cwd = input_api.PresubmitLocalPath()
  xml_changed = False

  # Only for changed files, do corresponding checks if the file is
  # histograms.xml or enums.xml.
  for file_obj in input_api.AffectedFiles():
    is_changed = ValidateSingleFile(input_api, output_api, file_obj, cwd,
                                    results, allow_test_paths)
    xml_changed = xml_changed or is_changed

  # Run validate_format.py and validate_histograms_index.py, if changed files
  # contain histograms.xml or enums.xml.
  if xml_changed:
    GetValidateHistogramsError(input_api, output_api, cwd, results)

  return results


def CheckWebViewHistogramsAllowlistOnUpload(
    input_api,
    output_api,
    xml_paths_override=None,
):
  """Checks that histograms_allowlist.txt contains valid histograms."""
  xml_filter = lambda f: Path(f.LocalPath()).suffix == '.xml'
  xml_files = input_api.AffectedFiles(file_filter=xml_filter)
  if not xml_files:
    return []

  sys.path.append(input_api.PresubmitLocalPath())
  from histogram_paths import ALL_XMLS
  if xml_paths_override is not None:
    xml_files_paths = xml_paths_override
  else:
    xml_files_paths = ALL_XMLS

  xml_files = [open(f, encoding='utf-8') for f in xml_files_paths]

  # src_path should point to chromium/src
  src_path = os.path.join(input_api.PresubmitLocalPath(), '..', '..', '..')
  histograms_allowlist_check_path = os.path.join(src_path, 'android_webview',
                                                 'java', 'res', 'raw')
  sys.path.append(histograms_allowlist_check_path)
  from histograms_allowlist_check import CheckWebViewHistogramsAllowlist
  result = CheckWebViewHistogramsAllowlist(src_path, output_api, xml_files)
  for f in xml_files:
    f.close()
  return result


def CheckBooleansAreEnums(input_api, output_api):
  """Checks that histograms that use Booleans do not use units."""
  results = []
  cwd = input_api.PresubmitLocalPath()
  inclusion_pattern = input_api.re.compile(r'units="[Bb]oolean')
  units_warning = """
  You are using 'units' for a boolean histogram, but you should be using
  'enum' instead."""

  # Only for changed files, do corresponding checks if the file is
  # histograms.xml or enums.xml.
  for affected_file in input_api.AffectedFiles():
    filepath = input_api.os_path.relpath(affected_file.AbsoluteLocalPath(), cwd)
    if 'histograms.xml' in filepath:
      for line_number, line in affected_file.ChangedContents():
        if inclusion_pattern.search(line):
          results.append('%s:%s\n\t%s' % (filepath, line_number, line.strip()))

  # If a histograms.xml file was changed, check for units="[Bb]oolean".
  if results:
    return [output_api.PresubmitPromptOrNotify(units_warning, results)]
  return results
