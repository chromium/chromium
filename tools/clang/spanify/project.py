# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains the metadata for all projects supported by the spanify tool.

# yapf: disable
PROJECTS = {
    # http://go/autospan-tracker
    'chrome': {
        'spreadsheet_id': '14YCQY2eBlLDr2wd8XaCfbLacz0t94YRuyt5CjkohBK4',
        'compile_dirs': '.',
    },
    # http://go/autospan-partition-alloc-tracker
    'partition_alloc': {
        'spreadsheet_id': '15AuAyRmxG95L5G8ejTlJ7C2p6zgvKTLt8lV9lg6xmDk',
        'compile_dirs': 'base/allocator/partition_allocator/src',
    },
    # http://go/autospan-dawn-tracker
    'dawn': {
        'spreadsheet_id': '11I41N369S7tcbMrWhsn6gStLKbOxseGSOMSTE3dsQok',
        'submodule': 'third_party/dawn',
        'compile_dirs': 'src',
    },
    # http://go/autospan-skia-tracker
    'skia': {
        'spreadsheet_id': '1dJ5PIQMsQ4IBYTcZUrshOJUIOmoa0MOwedK-jGzHYxI',
        'submodule': 'third_party/skia',
        'compile_dirs': '.',
        'run_gn_check': False,
    },
    # http://go/autospan-angle-tracker
    'angle': {
        'spreadsheet_id': '10g9-rrhGRQM1bGfHyZyK4Yris6X1ZfPmq-iRxw-9n38',
        'submodule': 'third_party/angle',
        'compile_dirs': '.',
    },
    # http://go/autospan-webrtc-tracker
    'webrtc': {
        'spreadsheet_id': '1gDu0ZCAONoIm242lRscCoYKmrfWLbwflxtwgWi-NUVk',
        'submodule': 'third_party/webrtc/src',
        'compile_dirs': '.',
    },
}
# yapf: enable
