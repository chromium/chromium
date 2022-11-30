# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium browser resources.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools, and see
https://chromium.googlesource.com/chromium/src/+/main/styleguide/web/web.md
for the rules we're checking against here.
"""

import os
import sys

class IcoFiles(object):
  """Verifier of ICO files for Chromium resources.
  """

  def __init__(self, input_api, output_api):
    """ Initializes IcoFiles with path."""
    self.input_api = input_api
    self.output_api = output_api

    tool_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
        '../../../tools/resources')
    sys.path.insert(0, tool_path)

  def RunChecks(self):
    """Verifies the correctness of the ICO files.

    Returns:
        An array of presubmit errors if any ICO files were broken in some way.
    """
    results = []
    affected_files = self.input_api.AffectedFiles(include_deletes=False)
    for f in affected_files:
      path = f.LocalPath()
      if os.path.splitext(path)[1].lower() != '.ico':
        continue

      # Import this module from here (to avoid importing it in the highly common
      # case where there are no ICO files being changed).
      import ico_tools

      repository_path = self.input_api.change.RepositoryRoot()

      with open(os.path.join(repository_path, path), 'rb') as ico_file:
        errors = list(ico_tools.LintIcoFile(ico_file))
        if errors:
          error_string = '\n'.join('    * ' + e for e in errors)
          results.append(self.output_api.PresubmitError(
              '%s: This file does not meet the standards for Chromium ICO '
              'files.\n%s\n    Please run '
              'tools/resources/optimize-ico-files.py on this file. See '
              'chrome/app/theme/README for details.' % (path, error_string)))
    return results
