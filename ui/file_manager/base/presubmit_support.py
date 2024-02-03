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
        selector_path = input_api.os_path.join('ui', 'file_manager',
                                               'file_manager', 'lib',
                                               'selector.ts')

        allowed_paths = {xf_base_file_path, selector_path}
        local_path = file_path.LocalPath()
        return Path(local_path).suffix in {
            '.ts', '.js'
        } and local_path not in allowed_paths

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


def _IsComment(line):
    l = line.lstrip()
    return l.startswith(('//', '/*', '* '))


def _CheckBannedTsTags(input_api, output_api):
    # It allow @ts-ignore in test files
    is_test = lambda fname: '_unittest' in fname or 'mock_' in fname
    ts_only = lambda f: (f.LocalPath().endswith('.ts') and
                         not is_test(f.LocalPath()))
    results = []
    offending_files = []
    for f in input_api.AffectedFiles(file_filter=ts_only):
        for line_num, line in f.ChangedContents():
            if not _IsComment(line):
                continue
            if '@ts-ignore' in line:
                offending_files.append(f'{f}: {line_num}: {line[:100]}')

    if offending_files:
        results.append(
            output_api.PresubmitError('@ts-ignore is banned in TS files.',
                                      offending_files))

    return results
