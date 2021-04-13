# Copyright 2021 The Chromium Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'

import sys

def CheckChangeOnUpload(input_api, output_api):
    results = []
    old_sys_path = sys.path
    try:
        sys.path = [
            input_api.os_path.join(input_api.PresubmitLocalPath(), 'scripts')
        ] + sys.path

        results.extend(_CheckTsSources(input_api, output_api))
        results.extend(_CheckCTSHtml(input_api, output_api))
    finally:
        sys.path = old_sys_path

    return results

def _CheckTsSources(input_api, output_api):
    from gen_ts_dep_lists import get_ts_sources
    expected_ts_sources = get_ts_sources()

    ts_sources = input_api.ReadFile(
        input_api.os_path.join(input_api.PresubmitLocalPath(),
                               'ts_sources.txt')).split()

    if expected_ts_sources != ts_sources:
        return [
            output_api.PresubmitError(
                'Typescript source list out of date. Please run //third_party/webgpu-cts/scripts/gen_ts_dep_lists.py to regenerate ts_sources.txt.'
            )
        ]

    return []


def _CheckCTSHtml(input_api, output_api):
    from regenerate_internal_cts_html import generate_internal_cts_html
    expected_contents = generate_internal_cts_html()

    cts_html = input_api.os_path.join(input_api.change.RepositoryRoot(),
                                      'third_party', 'blink', 'web_tests',
                                      'wpt_internal', 'webgpu', 'cts.html')
    with open(cts_html, 'rb') as f:
        if f.read() != expected_contents:
            return [
                output_api.PresubmitError(
                    '%s out of date. Please run //third_party/webgpu-cts/scripts/regenerate_internal_cts_html.py.'
                    % cts_html)
            ]
    return []

