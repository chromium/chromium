# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for //tools/crates/icu4x_datagen."""

import tomllib

PRESUBMIT_VERSION = '2.0.0'


def CheckIcu4xVersionsMatchChromiumCratesIo(input_api, output_api):
    """Checks that ICU4X crate versions in //tools/crates/icu4x_datagen match
    //third_party/rust/chromium_crates_io/Cargo.lock.

    Emits a warning rather than an error so versions are allowed to deviate
    temporarily during two-step crate rolls.
    """
    repo_root = input_api.change.RepositoryRoot()
    datagen_toml_path = input_api.os_path.join(repo_root, 'tools', 'crates',
                                               'icu4x_datagen', 'Cargo.toml')
    datagen_lock_path = input_api.os_path.join(repo_root, 'tools', 'crates',
                                               'icu4x_datagen', 'Cargo.lock')
    chromium_lock_path = input_api.os_path.join(repo_root, 'third_party',
                                                'rust', 'chromium_crates_io',
                                                'Cargo.lock')

    with open(datagen_toml_path, 'rb') as f:
        datagen_toml = tomllib.load(f)
    with open(datagen_lock_path, 'rb') as f:
        datagen_lock = tomllib.load(f)
    with open(chromium_lock_path, 'rb') as f:
        chromium_lock = tomllib.load(f)

    datagen_versions = {
        pkg['name']: pkg['version']
        for pkg in datagen_lock.get('package', [])
    }
    chromium_versions = {
        pkg['name']: pkg['version']
        for pkg in chromium_lock.get('package', [])
    }

    mismatches = []

    # 1. Check that Cargo.toml dependency versions match Cargo.lock.
    for name, spec in sorted(datagen_toml.get('dependencies', {}).items()):
        if not (name == 'icu' or name.startswith('icu_')):
            continue
        version_req = spec if isinstance(spec, str) else spec.get(
            'version', '')
        expected_version = version_req.lstrip('=')
        locked_version = datagen_versions.get(name)
        if locked_version != expected_version:
            mismatches.append(
                f'  {name}: Cargo.toml specifies {version_req}, '
                f'but icu4x_datagen/Cargo.lock has {locked_version}')

    # 2. Check that icu_* crates in both Cargo.lock files have identical
    # versions.
    shared_icu_crates = sorted(
        name for name in (set(datagen_versions) & set(chromium_versions))
        if name.startswith('icu_'))
    for name in shared_icu_crates:
        if datagen_versions[name] != chromium_versions[name]:
            mismatches.append(
                f'  {name}: icu4x_datagen/Cargo.lock has '
                f'{datagen_versions[name]}, but '
                f'chromium_crates_io/Cargo.lock has {chromium_versions[name]}')

    # 3. Check the meta-crate `icu` against `icu_provider` in
    # chromium_crates_io.
    if 'icu' in datagen_versions and 'icu_provider' in chromium_versions:
        if datagen_versions['icu'] != chromium_versions['icu_provider']:
            mismatches.append(
                f'  icu: icu4x_datagen/Cargo.lock has '
                f'{datagen_versions["icu"]}, but '
                f'chromium_crates_io/Cargo.lock has icu_provider '
                f'{chromium_versions["icu_provider"]}')

    if mismatches:
        return [
            output_api.PresubmitPromptWarning(
                'ICU4X crate versions in //tools/crates/icu4x_datagen do not '
                'match //third_party/rust/chromium_crates_io/Cargo.lock:\n' +
                '\n'.join(mismatches))
        ]
    return []
