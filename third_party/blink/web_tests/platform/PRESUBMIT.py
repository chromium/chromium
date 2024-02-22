# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for `web_tests/platform/`.

See https://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os


def _CheckForExtraPlatformBaselines(input_api, output_api):
    """Checks that expectations are not added/modified for platforms that do not exist
    """
    # This test does not work on Windows because of the dependencies of
    # the imported blinkpy code below.
    if os.name == 'nt':
        return []

    os_path = input_api.os_path

    local_dir = os_path.relpath(
        os_path.normpath('{0}/'.format(input_api.PresubmitLocalPath().replace(
            os_path.sep, '/'))), input_api.change.RepositoryRoot())

    check_files = []
    for f in input_api.AffectedFiles(include_deletes=False):
        local_path = f.LocalPath()
        assert local_path.startswith(local_dir)
        local_path = os_path.relpath(local_path, local_dir)
        path_components = local_path.split(os_path.sep)
        if len(path_components) > 1:
            check_files.append((local_path, path_components[0]))

    if len(check_files) == 0:
        return []

    from blinkpy.common.host import Host
    port_factory = Host().port_factory
    all_ports_with_builders = [
        port_factory.get(port_name)
        for port_name in port_factory.all_port_names() +
        ['android', 'ios', 'webview']
    ]
    # get any additional supported versions (that might not currently have
    # builders)
    all_ports = [
        port_factory.get(port_name) for port_name in set([
            "{}-{}".format(port.port_name, supported_version)
            for port in all_ports_with_builders
            for supported_version in port.SUPPORTED_VERSIONS
        ])
    ]
    known_platforms = set([
        fallback_path for port in all_ports
        for fallback_path in port.FALLBACK_PATHS[port.version()]
    ])

    results = []
    for (f, platform) in check_files:
        if not platform in known_platforms:
            path = os_path.relpath(
                os_path.join(input_api.PresubmitLocalPath(), f),
                input_api.change.RepositoryRoot())
            results.append(
                output_api.PresubmitError(
                    "This CL adds a new baseline %s, but %s is not a known platform."
                    % (path, platform)))
    return results


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CheckForExtraPlatformBaselines(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CheckForExtraPlatformBaselines(input_api, output_api))
    return results
