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

def CheckVetConfigTomlAndItsTemplateAreEditedTogether(input_api, output_api):
    chromium_crates_io = input_api.os_path.join(
        'third_party', 'rust', 'chromium_crates_io')
    toml_path = input_api.os_path.join(
        chromium_crates_io, 'supply-chain', 'config.toml')
    hbs_path = input_api.os_path.join(chromium_crates_io, 'vet_config.toml.hbs')

    affected_files = input_api.LocalPaths()
    is_toml_edited = toml_path in affected_files
    is_hbs_edited = hbs_path in affected_files

    if is_toml_edited != is_hbs_edited:
        return [output_api.PresubmitError(
            f"ERROR: `{hbs_path}` and `{toml_path}` are not modified together",
            long_text=\
                f"Please edit `{hbs_path}` " + \
                f"and then regenerate `{toml_path}` " + \
                f"by running `tools/crates/run_gnrt.py vendor`" \
        )]

    return []
