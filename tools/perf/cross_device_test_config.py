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
        'loadline_phone.crossbench': 5,
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
    'android-pixel9-perf': {
        'jetstream2': 4,
        'motionmark1.3.crossbench': 4,
        'speedometer3.crossbench': 4,
        'speedometer3.a11y.crossbench': 4,
    },
    'android-pixel9-pro-perf': {
        'jetstream2': 4,
        'motionmark1.3.crossbench': 4,
        'speedometer3.crossbench': 4,
        'speedometer3.a11y.crossbench': 4,
    },
    'android-pixel9-pro-xl-perf': {
        'jetstream2': 4,
        'motionmark1.3.crossbench': 4,
        'speedometer3.crossbench': 4,
        'speedometer3.a11y.crossbench': 4,
    },
    'android-pixel25-ultra-perf': {
        'jetstream2': 4,
        'motionmark1.3.crossbench': 4,
        'speedometer3.crossbench': 4,
        'speedometer3.a11y.crossbench': 4,
    },
    'android-pixel25-ultra-xl-perf': {
        'jetstream2': 3,
        'motionmark1.3.crossbench': 3,
        'speedometer3.crossbench': 3,
        'speedometer3.a11y.crossbench': 3,
    },
    'linux-perf': {
        'jetstream2.crossbench': 10,
        'motionmark1.3.crossbench': 10,
        'speedometer3.crossbench': 10,
    },
    'linux-perf-fyi': {
        'jetstream2.crossbench': 4,
        'jetstream3.crossbench': 4,
        'jetstream3-turbolev_future.crossbench': 4,
        'jetstream_main.crossbench': 4,
        'speedometer2': 4,
        'speedometer3': 4,
        'speedometer3.crossbench': 4,
        'motionmark1.3.crossbench': 4,
    },
    'linux-r350-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 20,
        'jetstream3.crossbench': 20,
        'jetstream3-turbolev_future.crossbench': 20,
        'jetstream_main.crossbench': 20,
        # set speedometer to 20 shards to help warm up speedometer2
        # benchmark runs b/325578543
        'speedometer': {
            'http://browserbench.org/Speedometer/': 20,
        },
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 20,
        },
        'speedometer3-predictable': {
            'Speedometer3': 20,
        },
        'speedometer3.crossbench': 20,
        'speedometer_main.crossbench': 20,
    },
    'win-10_laptop_low_end-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 5,
        'jetstream3.crossbench': 5,
        'jetstream3-turbolev_future.crossbench': 5,
        'jetstream_main.crossbench': 5,
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3.crossbench': 20,
    },
    'win-10-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 5,
        'jetstream3.crossbench': 5,
        'jetstream3-turbolev_future.crossbench': 5,
        'jetstream_main.crossbench': 5,
        'system_health.common_desktop': {
            # cputimeToFirstContentfulPaint
            'browse:media:tumblr:2018': 10,
            'browse:social:tumblr_infinite_scroll:2018': 10,
            'load:search:google:2018': 10,
        },
        'speedometer3.crossbench': 20,
        'speedometer_main.crossbench': 20,
    },
    'win-11-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 5,
        'jetstream3.crossbench': 5,
        'jetstream3-turbolev_future.crossbench': 5,
        'jetstream_main.crossbench': 5,
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3.crossbench': 20,
        'speedometer3.a11y.crossbench': 20,
        'speedometer_main.crossbench': 20,
    },
    'win-arm64-snapdragon-elite-perf': {
        'jetstream2.crossbench': 5,
        'jetstream3.crossbench': 5,
        'jetstream3-turbolev_future.crossbench': 5,
        'speedometer3.crossbench': 20,
    },
    'mac-laptop_high_end-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 5,
        'jetstream3.crossbench': 5,
        'jetstream3-turbolev_future.crossbench': 5,
        'jetstream_main.crossbench': 5,
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3.crossbench': 20,
    },
    'mac-intel-perf': {
        'jetstream2': {
            'JetStream2': 5,
        },
        'jetstream2.crossbench': 5,
        'jetstream3.crossbench': 5,
        'jetstream3-turbolev_future.crossbench': 5,
        'jetstream_main.crossbench': 5,
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3.crossbench': 20,
        'speedometer_main.crossbench': 20,
    },
    'mac-m1_mini_2020-perf': {
        'jetstream2': {
            'JetStream2': 6,
        },
        'jetstream2-no-field-trials': 6,
        'jetstream2.crossbench': 5,
        'jetstream3.crossbench': 5,
        'jetstream3-turbolev_future.crossbench': 5,
        'jetstream_main.crossbench': 5,
        'speedometer2': {
            'Speedometer2': 20,
        },
        'speedometer3': {
            'Speedometer3': 28,
        },
        'speedometer3-no-field-trials': 28,
        'speedometer3.crossbench': 20,
        'speedometer_main.crossbench': 10,
        'rendering.desktop.notracing': 20,
        'motionmark1.3.crossbench': 20,
    },
    'mac-m1_mini_2020-perf-pgo': {
        'jetstream2': {
            'JetStream2': 6,
        },
        'jetstream2.crossbench': 4,
        'jetstream3.crossbench': 4,
        'jetstream3-turbolev_future.crossbench': 4,
        'jetstream_main.crossbench': 4,
        'speedometer2': {
            'Speedometer2': 5,
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
    'mac-m3-pro-perf': {
        'speedometer3.crossbench': 4,
    },
    'mac-m4-mini-perf': {
        'jetstream2': 6,
        'jetstream2.crossbench': 6,
        'jetstream3.crossbench': 6,
        'jetstream3-turbolev_future.crossbench': 6,
        'jetstream_main.crossbench': 6,
        'speedometer2': 20,
        'speedometer3.crossbench': 20,
        'speedometer_main.crossbench': 10,
        'rendering.desktop.notracing': 20,
        'motionmark1.3.crossbench': 20,
    },
}
