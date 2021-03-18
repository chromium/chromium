# Copyright 2021 The Chromium Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PRESUBMIT_VERSION = '2.0.0'

import sys


def CheckChangeOnUpload(input_api, output_api):
    old_sys_path = sys.path
    try:
        sys.path = [
            input_api.os_path.join(input_api.PresubmitLocalPath(), 'scripts')
        ] + sys.path
        from gen_ts_dep_lists import get_ts_sources
        expected_ts_sources = get_ts_sources()
    finally:
        sys.path = old_sys_path

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
