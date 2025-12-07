# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'

def CheckTastIsRequested(input_api, output_api):
  """Checks that the user did add the tast trybot to the description
  """
  if input_api.no_diffs:
    return []
  keyword = 'CQ_INCLUDE_TRYBOTS=luci.chrome.try:chromeos-betty-chrome'
  if not(keyword in input_api.change.DescriptionText()):
    return [output_api.PresubmitPromptWarning(
        'Changes in this directory are high risk for breaking ChromeOS inputs,'
        + ' please include the tast trybot by adding: '
        + keyword
        + ' to your CL description')]

  return []

def CheckPotentialImpactOnOobe(input_api, output_api):
  """Warns user about potential implications of changes in the
  input_method_util.* in context of OOBE.
  """

  if input_api.no_diffs:
    return []

  target_files = ("input_method_util.h", "input_method_util.cc")

  # Iterate through all files affected by this change, including deleted files.
  # We only care if the content of these specific files might have changed.
  affected_target_file_found = False
  for f in input_api.AffectedFiles(include_deletes=True):
      if f.LocalPath().endswith(target_files):
          affected_target_file_found = True
          break

  # If one of the target files was found in the changeset, return the warning.
  if affected_target_file_found:
      return [output_api.PresubmitPromptWarning(
          'Potential OOBE Impact: Please verify that your changes do not '
          'break input methods in OOBE. Specifically, check if your changes '
          'affect the input method allowlist or modify which input method '
          'data is prebundled into the system image.'
      )]

  # If none of the target files were affected, return an empty list (no warning).
  return []
