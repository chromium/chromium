# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit for //third_party/rust/chromium_crates_io

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckGnrtConfig(input_api, output_api):
    # `input_files` lists files that are used as input by
    # `third_party/rust/chromium_crates_io/check_gnrt_config.py`.
    CARGO_LOCK_PATH = "third_party/rust/chromium_crates_io/Cargo.lock"
    GNRT_CONFIG_PATH = "third_party/rust/chromium_crates_io/gnrt_config.toml"
    input_files = set([CARGO_LOCK_PATH, GNRT_CONFIG_PATH])

    # Exit early if no `input_files` are affected by the current CL.
    affected_paths = set(
        map(lambda f: f.UnixLocalPath(), input_api.change.AffectedFiles()))
    if not input_files.intersection(affected_paths):
        return []

    # Delegate actual checks to `check_gnrt_config.py` (one reason is that we
    # can't apparently `import toml` in `PRESUBMIT.py`).
    test_script_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                              "check_gnrt_config.py")
    cmd_name = '//third_party/rust/chromium_crates_io/check_gnrt_config.py'
    test_cmd = input_api.Command(
        name=cmd_name,
        cmd=[input_api.python_executable, test_script_path],
        kwargs={},
        message=output_api.PresubmitError)
    if input_api.verbose:
        print('Running ' + cmd_name)
    return input_api.RunTests([test_cmd])


def CheckPythonUnittestsPass(input_api, output_api):
    results = []
    this_dir = input_api.PresubmitLocalPath()

    results += input_api.RunTests(
        input_api.canned_checks.GetUnitTestsInDirectory(
            input_api,
            output_api,
            this_dir,
            files_to_check=['.*unittest.*\.py$'],
            env=None))

    return results
