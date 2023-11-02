# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pathlib import Path

GM3_SUFFIX = '_gm3.css'


def _CheckGM3Counterpart(input_api, output_api):
    """If a CSS file (say foo.css) is included in the patch, check if there
    is a corresponding GM3 counterpart in the same directory (say, foo_gm3.css).
    If so, output a warning message if the GM3 counterpart is not included in
    the patch.

    NOTE: For GM3 migration, we have duplicated the CSS file if we need to
    modify it, this check acts as a warning to prompt developers that if the
    original CSS is changed, the corresponding GM3 counterpart file might also
    need to be updated.
    """
    css_filter = lambda f: Path(f.LocalPath()).suffix == '.css'
    css_files = input_api.AffectedFiles(file_filter=css_filter)
    if not css_files:
        return []

    css_file_paths = set([f.LocalPath() for f in css_files])
    invalid_files = []
    for path in css_file_paths:
        file_path = Path(path)
        # Skip _gm3.css file itself.
        if file_path.name.endswith(GM3_SUFFIX):
            continue
        gm3_file_path = file_path.parent.joinpath(file_path.stem + GM3_SUFFIX)
        gm3_file_abspath = Path(
            input_api.change.RepositoryRoot()).joinpath(gm3_file_path)
        # Skip if the the file doesn't have _gm3 counterpart.
        if not gm3_file_abspath.is_file():
            continue
        # Skip if the _gm3 counterpart is also in the patch.
        if str(gm3_file_path) in css_file_paths:
            continue
        invalid_files.append(path + ' -> ' + str(gm3_file_path))

    if not invalid_files:
        return []

    warning_message = 'You updated a CSS file which has a corresponding '\
    '"_gm3" counterpart, please double check if you need to apply the '\
    'update to the corresponding "_gm3" file. go/files-gm3-presubmit'

    return [
        output_api.PresubmitPromptWarning(warning_message, items=invalid_files)
    ]
