# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for `wpt_internal/`.

See https://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


def _LintWPT(input_api, output_api):
    tools_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                        input_api.os_path.pardir,
                                        input_api.os_path.pardir, 'tools')
    if tools_path not in input_api.sys.path:
        input_api.sys.path.insert(0, tools_path)
    from blinkpy.presubmit.common_checks import lint_wpt_root
    return lint_wpt_root(input_api, output_api)


def CheckChangeOnUpload(input_api, output_api):
    return _LintWPT(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _LintWPT(input_api, output_api)
