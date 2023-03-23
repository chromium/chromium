# Copyright 2021 The Chromium Authors
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
        'jetstream2': {
            'JetStream2': 3,
        },
        'speedometer2': {
            'Speedometer2': 3,
        },
        'rendering.mobile': {
            'css_transitions_triggered_style_element': 4,
            'canvas_animation_no_clear': 4
        },
    },
    'android-pixel2-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'system_health.common_mobile': 3,
        'system_health.memory_mobile': 3,
        'startup.mobile': 10,
        'speedometer2': {
            'Speedometer2': 10,
        },
    },
    'android-go-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'system_health.common_mobile': {
            # timeToFirstContentfulPaint
            'background:social:facebook:2019': 10,
            # cputimeToFirstContentfulPaint
            'load:search:google:2018': 10
        },
        'speedometer2': {
            'Speedometer2': 10,
        },
    },
    'android-pixel4-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
    },
    'android-pixel4_webview-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
    },
    'android-pixel4a_power-perf': {
        'power.mobile': {
            'browse:media:flickr_infinite_scroll:2019': 10,
            'browse:media:tiktok_infinite_scroll:2021': 10,
            'browse:social:pinterest_infinite_scroll:2021': 10,
            'browse:social:tumblr_infinite_scroll:2018': 10,
            'browse:tech:discourse_infinite_scroll:2018': 10,
        }
    },
    'linux-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'system_health.common_desktop': {
            # cputimeToFirstContentfulPaint
            'browse:social:tumblr_infinite_scroll:2018': 10,
            'long_running:tools:gmail-background': 10,
            'browse:media:youtubetv:2019': 10
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer2-minormc': {
            'Speedometer2': 20,
        },
    },
    'win-10_laptop_low_end-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
    },
    'win-10-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'system_health.common_desktop': {
            # cputimeToFirstContentfulPaint
            'browse:media:tumblr:2018': 10,
            'browse:social:tumblr_infinite_scroll:2018': 10,
            'load:search:google:2018': 10,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
    },
    'linux-perf-calibration': {
        'jetstream2': {
            'JetStream2': 10,
        },
        'speedometer2': {
            'Speedometer2': 28,
        },
        'blink_perf.shadow_dom': 31
    },
    'mac-laptop_high_end-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
    },
    'mac-m1_mini_2020-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer2-minormc': {
            'Speedometer2': 20,
        },
    },
    'mac-m1_mini_2020-perf-pgo': {
        'jetstream2': {
            'JetStream2': 4,
        },
        'speedometer2': {
            'Speedometer2': 4,
        },
    },
}
