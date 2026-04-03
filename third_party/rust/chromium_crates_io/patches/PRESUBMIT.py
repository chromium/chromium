# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit for //third_party/rust/chromium_crates_io/patches

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckPatchPaths(input_api, output_api):
    """Checks that all modified patches in `chromium_crates_io/patches/`
       use paths relative to the Chromium root.
    """
    import os

    # patches_dir is .../src/third_party/rust/chromium_crates_io/patches
    patches_dir = input_api.PresubmitLocalPath()

    # Calculate chromium_dir relative to the patches_dir
    chromium_dir = input_api.os_path.normpath(
        input_api.os_path.join(patches_dir, '..', '..', '..', '..'))

    errors = []

    # Iterate over ONLY the files that have been modified/added in this CL.
    # This ensures we don't spam errors about unrelated patches,
    # and satisfies the requirement to only run when patches change.
    affected_patches = [
        f.AbsoluteLocalPath()
        for f in input_api.AffectedFiles(include_deletes=False)
        if f.AbsoluteLocalPath().endswith('.patch')
    ]

    for patch_path in affected_patches:
        # The crate dir is the parent directory of the patch file
        # e.g., for `patches/image-v0_25/0001.patch`, crate_dir is `image-v0_25`
        crate_dir = input_api.os_path.basename(
            input_api.os_path.dirname(patch_path))

        # The expected path prefix is the path from the Chromium root to the
        # crate's vendor directory.
        expected_path_prefix = f"third_party/rust/chromium_crates_io/vendor/{crate_dir}/"

        with open(patch_path, 'r', encoding='utf-8') as f:
            line_num = 0
            for line in f:
                line_num += 1
                if line.startswith('--- a/') or line.startswith('+++ b/'):
                    path = line[6:].strip()
                    if path == '/dev/null':
                        continue
                    if not path.startswith(expected_path_prefix):
                        rel_patch_path = input_api.os_path.relpath(
                            patch_path, chromium_dir)
                        errors.append(
                            f"{rel_patch_path}:{line_num}: "
                            f"Patch uses an incorrect path: {line.strip()}. "
                            f"Paths should start with {expected_path_prefix}")
                        break  # Only one error per patch to avoid flooding

    if errors:
        return [
            output_api.PresubmitError(
                "Some patches use incorrect paths:\n\n    " +
                "\n    ".join(errors))
        ]

    return []
