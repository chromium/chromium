# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

_NON_SEMANTIC_CSS_COLOR_PATTERNS = [
  "var(--google-",
  "var(--paper-",
  ": #",
  "rgb",
  "hsl",
]

class SemanticCssChecker(object):
  """Checks that only semantic CSS colors are used.

  Checks that only values defined in ui/chromeos/styles/cros_colors.json5
  are used. A motivation for this is to support Dark Mode, as each color
  has a separate light and dark mode value that is substituted at runtime
  whenever the device changes from light to dark mode or vice versa. This
  check doesn't cover all scenarios as it is difficult to cover everything
  without creating false positives.
  """

  @staticmethod
  def RunChecks(input_api, output_api):
    """Runs check for any non-semantic CSS colors used.

    Checks if the affected lines of code in input_api use non-semantic CSS
    colors, and if so, returns a list containing the warnings that should
    be displayed for each violation.

    Args:
        input_api: presubmit.InputApi containing information of the files
          in the change.
        output_api: presubmit.OutputApi used to display the warnings.

    Returns:
        A list of presubmit warnings, each containing the line the violation
        occurred and the warning message.
    """
    results = []
    for f in input_api.AffectedFiles():
        exts = ['html', 'css']
        if not any(f.LocalPath().endswith(ext) for ext in exts):
            continue

        for line_num, line in f.ChangedContents():
          for pattern in _NON_SEMANTIC_CSS_COLOR_PATTERNS:
            if pattern in line:
              results.append(output_api.PresubmitPromptWarning(
                "%s:%d:\n\n    %s\n\n"
                "Use of non-semantic color in CSS. Please make sure that all "
                "colors in\nCSS use semantic values defined in "
                "ui/chromeos/styles/cros_colors.json5\ninstead of constant "
                "color values." %
                (f.LocalPath(), line_num, line.strip())))
              break
    return results
