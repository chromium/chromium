# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import enum
import os
import print_histogram_names


class WellKnownAllowlistPath(enum.Enum):
  """Enum representing the known production allowlist paths to validate.

  This is used to collect the paths to production allowlist and make them
  accessible to the presubmit checks between this directory and the external
  presubmit checks. Plus a common methods to access relevant information.

  Currently there is only one allowlist, but it seems to make sense to oraganize
  it like this to expose those access methods rather then having multiple
  constants or duplicated code among users of this check.
  """

  ANDROID_WEBVIEW = os.path.join('android_webview', 'java', 'src', 'org',
                                 'chromium', 'android_webview', 'metrics',
                                 'HistogramsAllowlist.java')

  def relative_path(self):
    """Returns the path of the allowlist file relative to src/."""
    return self.value

  def filename(self):
    return os.path.basename(self.value)


def get_histograms_allowlist_content(allowlist_path):
  histogramNames = []
  with open(allowlist_path) as file:
    shouldParse = False
    for line in file:
      if line.strip() == '// histograms_allowlist_check START_PARSING':
        shouldParse = True
        continue
      if line.strip() == '// histograms_allowlist_check END_PARSING':
        break
      if shouldParse:
        # Remove white space, quotes and commas from the entries.
        histogramNames.append(line.strip().replace('"', '').replace(',', ''))
  return histogramNames


def check_histograms_allowlist(output_api, allowlist_path, histograms_files):
  """Checks that all histograms in the allowlist are defined within histograms.

  Args:
    output_api: The output api type, generally provided by the PRESUBMIT system.
    allowlist_path: The path to the allowlist file to validate histograms
      against.
    histograms_files: The list of histograms files to look for allowlisted
      histograms in.
  """

  all_histograms = print_histogram_names.get_names(histograms_files)

  histograms_allowlist = get_histograms_allowlist_content(allowlist_path)

  errors = []
  for histogram in histograms_allowlist:
    if histogram not in all_histograms:
      errors.append(f'{allowlist_path} contains unknown histogram '
                    f'<{histogram}>')

  if not errors:
    return []

  results = [
      output_api.PresubmitError(
          f'All histograms in {allowlist_path} must be valid.',
          errors,
      )
  ]

  return results
