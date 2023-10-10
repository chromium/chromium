# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pathlib import Path


def _CheckNoDirectLitImport(input_api, output_api):
    """We want to isolate the dependency of LitElement to only one file
    (ui/file_manager/file_manager/widgets/xf_base.ts), so it's easier to handle
    the potential breaking changes from LitElement easier. This job checks
    the imports from the TS/JS files to make sure only xf_base can import from
    Lit directly.
    """
    def _isLitDisallowed(file_path):
        xf_base_file_path = input_api.os_path.join('ui', 'file_manager',
                                                   'file_manager', 'widgets',
                                                   'xf_base.ts')
        local_path = file_path.LocalPath()
        return Path(local_path).suffix in {
            '.ts', '.js'
        } and local_path != xf_base_file_path

    ts_files = input_api.AffectedFiles(include_deletes=False,
                                       file_filter=_isLitDisallowed)
    if not ts_files:
        return []

    lit_import_pattern = "from 'chrome://resources/mwc/lit/"
    results = []
    for f in ts_files:
        for line_num, line in enumerate(f.NewContents(), 1):
            if lit_import_pattern in line:
                results.append(
                    output_api.PresubmitPromptWarning(
                        "%s:%d:\n\n    %s\n\n"
                        "Direct import from lit is not allowed. All lit "
                        "related dependencies should be limited in file "
                        "ui/file_manager/file_manager/widgets/xf_base.ts and "
                        "all other files should import xf_base instead." %
                        (f.LocalPath(), line_num, line.strip())))
                break

    return results
