# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Dictionary for the repeat config.
# E.g.:
# {
#   'builder-1':
#   {
#     'benchmark-1':
#     {
#       'story-1': 4,
#     }
#   'builder-2':
#     'benchmark-2':
#     {
#       'story-1': 10,
#       'story-2': 10,
#     }
# }

TARGET_DEVICES = {
    'android-pixel2-perf-fyi': {
        'speedometer2': {
            'Speedometer2': 3
        }
    }
}
