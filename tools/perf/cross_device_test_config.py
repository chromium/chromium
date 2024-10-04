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
        'speedometer3': {
            'Speedometer3': 3,
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
        'speedometer3': {
            'Speedometer3': 10,
        },
    },
    'android-go-wembley-perf': {
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
        'speedometer3': {
            'Speedometer3': 10,
        },
    },
    'android-pixel4-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 20,
        },
    },
    'android-pixel4_webview-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 20,
        },
    },
    'android-pixel4_webview-perf-pgo': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 20,
        },
    },
    'android-pixel6-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'speedometer2': {
            'Speedometer2': 5,
        },
        'speedometer3': {
            'Speedometer3': 5,
        },
    },
    'android-pixel6-perf-pgo': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'speedometer2': {
            'Speedometer2': 5,
        },
        'speedometer3': {
            'Speedometer3': 8,
        },
    },
    'linux-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2-minorms': {
            'JetStream2': 5,
        },
        'system_health.common_desktop': {
            # cputimeToFirstContentfulPaint
            'browse:social:tumblr_infinite_scroll:2018': 10,
            'long_running:tools:gmail-background': 10,
            'browse:media:youtubetv:2019': 10
        },
        # set speedometer to 20 shards to help warm up speedometer2
        # benchmark runs on linux-perf b/325578543
        'speedometer': {
            'http://browserbench.org/Speedometer/': 20,
        },
        'speedometer2-minorms': {
            'Speedometer2': 20,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer2-predictable': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 20,
        },
        'speedometer3-minorms': {
            'Speedometer3': 20,
        },
        'speedometer3-predictable': {
            'Speedometer3': 20,
        },
        'speedometer3.crossbench': 20,
        'jetstream2.crossbench': 20,
    },
    'linux-perf-fyi': {
        'speedometer2': 4,
        'speedometer2-minorms': 4,
        'speedometer3': 4,
        'speedometer3.crossbench': 4,
        'jetstream2.crossbench': 4,
        'motionmark1.3.crossbench': 4,
    },
    'win-10_laptop_low_end-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 5,
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 20,
        },
        'speedometer3.crossbench': 20,
    },
    'win-10-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 5,
        'system_health.common_desktop': {
            # cputimeToFirstContentfulPaint
            'browse:media:tumblr:2018': 10,
            'browse:social:tumblr_infinite_scroll:2018': 10,
            'load:search:google:2018': 10,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 20,
        },
        'speedometer3.crossbench': 20,
    },
    'win-11-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 5,
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 20,
        },
        'speedometer3.crossbench': 20,
    },
    'linux-perf-calibration': {
        'jetstream2': {
            'JetStream2': 10,
        },
        'speedometer2': {
            'Speedometer2': 28,
        },
        'speedometer3': {
            'Speedometer3': 28,
        },
        'blink_perf.shadow_dom': 31
    },
    'mac-laptop_high_end-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 5,
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 20,
        },
        'speedometer3.crossbench': 20,
    },
    'mac-intel-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 5,
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 20,
        },
        'speedometer3.crossbench': 20,
    },
    'mac-m1_mini_2020-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2-minorms': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 5,
        'speedometer2': {
            'Speedometer2': 28,
        },
        'speedometer2-minorms': {
            'Speedometer2': 28,
        },
        'speedometer3': {
            'Speedometer3': 28,
        },
        'speedometer3-minorms': {
            'Speedometer3': 28,
        },
        'speedometer3.crossbench': 20,
        'rendering.desktop.notracing': 20,
        'motionmark1.3.crossbench': 20,
    },
    'mac-m1_mini_2020-perf-pgo': {
        'jetstream2': {
            'JetStream2': 4,
        },
        'jetstream2.crossbench': 4,
        'speedometer2': {
            'Speedometer2': 7,
        },
        'speedometer3': {
            'Speedometer3': 7,
        },
        'speedometer3.crossbench': 4,
    },
    'mac-m1_mini_2020-no-brp-perf': {
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 20,
        },
        'rendering.desktop.notracing': 20,
    },
    'mac-m1-pro-perf': {
        'speedometer3': 4,
        'speedometer3.crossbench': 4,
    },
}
