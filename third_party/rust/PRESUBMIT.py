# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit for //third_party/rust

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

def CheckCargoVet(input_api, output_api):
    vet_args = ['check']

    # Hermetic and idempotent.
    vet_args += ['--locked', '--frozen', '--no-minimize-exemptions']

    run_cargo_vet_path = input_api.os_path.join(
        input_api.PresubmitLocalPath(),
        '..', '..', 'tools', 'crates', 'run_cargo_vet.py')
    cmd_name = '//tools/crates/run_cargo_vet.py check'

    test_cmd = input_api.Command(
        name=cmd_name,
        cmd=[input_api.python3_executable, run_cargo_vet_path] + vet_args,
        kwargs={},
        message=output_api.PresubmitError)
    if input_api.verbose:
        print('Running ' + cmd_name)
    return input_api.RunTests([test_cmd])
