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
            'Speedometer2': 3,
        },
        'rendering.mobile': {
            'css_transitions_triggered_style_element': 4,
            'canvas_animation_no_clear': 4
        }
    },
    'android-pixel2_weblayer-perf': {
        'startup.mobile': {
            'intent:coldish:bbc': 4,
            'intent:warm:bbc': 4,
            'cct:coldish:bbc': 4
        }
    },
    'android-pixel2-perf': {
        'startup.mobile': {
            'intent:coldish:bbc': 10,
            'intent:warm:bbc': 10,
            'cct:coldish:bbc': 10
        },
        'system_health.common_mobile': {
            # timeToFirstContentfulPaint
            'browse:media:googleplaystore:2019': 10,
            'load:social:pinterest:2019': 10,
            'browse:media:facebook_photos:2019': 10
        }
    },
    'Android Nexus5 Perf': {
        'startup.mobile': {
            'intent:coldish:bbc': 10,
            'intent:warm:bbc': 10,
            'cct:coldish:bbc': 10
        }
    },
    'android-go-perf': {
        'startup.mobile': {
            'intent:coldish:bbc': 10,
            'intent:warm:bbc': 10,
            'cct:coldish:bbc': 10
        },
        'system_health.common_mobile': {
            # timeToFirstContentfulPaint
            'background:social:facebook:2019': 10,
            # cputimeToFirstContentfulPaint
            'load:search:google:2018': 10
        }
    },
    'android-pixel4-perf': {
        'startup.mobile': {
            'intent:coldish:bbc': 10,
            'intent:warm:bbc': 10,
            'cct:coldish:bbc': 10
        }
    },
    'android-pixel4_weblayer-perf': {
        'startup.mobile': {
            'intent:coldish:bbc': 4,
            'intent:warm:bbc': 4,
            'cct:coldish:bbc': 4
        }
    },
    'linux-perf': {
        'system_health.common_desktop': {
            # cputimeToFirstContentfulPaint
            'browse:social:tumblr_infinite_scroll:2018': 10,
            'long_running:tools:gmail-background': 10,
            'browse:media:youtubetv:2019': 10
        }
    },
    'win-10-perf': {
        'system_health.common_desktop': {
            # cputimeToFirstContentfulPaint
            'browse:media:tumblr:2018': 10,
            'browse:social:tumblr_infinite_scroll:2018': 10,
            'load:search:google:2018': 10,
        }
    },
}
