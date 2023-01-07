# Lint as: python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script that is used by PRESUBMIT.py to check Android new infobar files.

This file checks for the following:
  - whether new infobar identifier targeting Android is appended
  - whether new infobar related files are introduced in Android related folder.
"""

import re
import os
import pathlib

INFOBAR_DELEGATE_H = 'components/infobars/core/infobar_delegate.h'
INFOBAR_ANDROID_FOLDERS = (
    'chrome/browser/ui/android/infobars',
    'chrome/android/java/src/org/chromium/chrome/browser/infobar')


def CheckDeprecationOnUpload(input_api, output_api):
  """
  Returns result for all the presubmit upload checks for newly added infobars.
  """
  result = []
  result.extend(_CheckNewInfobar(input_api, output_api))
  # Add more checks here
  return result


def _IncludeFiles(f):
  return pathlib.Path(f.LocalPath()).suffix in ('.h', '.cc', '.java')


def _CheckNewInfobar(input_api, output_api):
  warnings = []
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=_IncludeFiles):

    # Consider only newly added files.
    if f.Action() == 'A' and (os.path.dirname(
        f.LocalPath()) in INFOBAR_ANDROID_FOLDERS) and (
            'infobar' in os.path.basename(f.LocalPath()).lower()):
      warnings.append('  %s' % f.LocalPath())

    if (f.LocalPath() == INFOBAR_DELEGATE_H):
      contents = input_api.ReadFile(f)

      # Capture current identifiers.
      p = re.compile('enum InfoBarIdentifier {(.*?)}',
                     re.IGNORECASE | re.DOTALL | re.MULTILINE)
      ids = p.search(contents).group(1)

      find_id = re.compile('(.*) =')

      for line_number, line in f.ChangedContents():
        # Ignore comments and unchanged identifiers.
        if line not in ids:
          continue
        if line.strip().startswith('//'):
          continue
        match = find_id.search(line)
        if not match:
          continue
        infobar_id = match.group(1)
        if infobar_id.endswith("_ANDROID") or infobar_id.endswith("_MOBILE"):
          warnings.append('  %s:%d\n    \t%s' %
                          (f.LocalPath(), line_number, line.strip()))

  if warnings:
    return [
        output_api.PresubmitPromptWarning(
            '''
Android InfoBar Deprecation Check failed:
  Your new code appears to add a new infobar on Android.

  On Android, InfoBar UI will be deprecated in favor of the new Message UI.
  All Android infobars are being migrated or have been migrated to Message UI.

  See components/messages/README.md or reach out to components/messages/OWNERS
  for more information.
''', warnings)
    ]
  return []
