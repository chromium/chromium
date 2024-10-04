#!/usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=too-many-lines
# pylint: disable=line-too-long

"""Generates chromium.perf{,.fyi}.json from a set of condensed configs.

This file contains condensed configurations for the perf bots along with
logic to inflate those into the full (unwieldy) configurations in
//testing/buildbot that are consumed by the chromium recipe code.
"""

from __future__ import print_function

import argparse
import collections
import copy
import csv
import filecmp
import json
import os
import re
import shutil
import sys
import tempfile
import textwrap

from chrome_telemetry_build import android_browser_types
from core import benchmark_finders
from core import benchmark_utils
from core import bot_platforms
from core import path_util
from core import undocumented_benchmarks as ub_module

path_util.AddTelemetryToPath()

from telemetry import decorators


# The condensed configurations below get inflated into the perf builder
# configurations in //testing/buildbot. The expected format of these is:
#
#   {
#     'builder_name1': {
#       # Targets that the builder should compile in addition to those
#       # required for tests, as a list of strings.
#       'additional_compile_targets': ['target1', 'target2', ...],
#
#       'tests': [
#         {
#           # Arguments to pass to the test suite as a list of strings.
#           'extra_args': ['--arg1', '--arg2', ...],
#
#           # Arguments to be removed from the test suite (as a list of strings).
#           'remove_args': ['--arg1', '--arg2', ...],
#
#           # Name of the isolate to run as a string.
#           'isolate': 'isolate_name',
#
#           # Name of the test suite as a string.
#           # If not present, will default to `isolate`.
#           'name': 'presentation_name',
#
#           # The number of shards for this test as an int.
#           # This is only required for GTEST tests since this is defined
#           # in bot_platforms.py for Telemetry tests.
#           'num_shards': 2,
#
#           # What kind of test this is; for options, see TEST_TYPES
#           # below. Defaults to TELEMETRY.
#           'type': TEST_TYPES.TELEMETRY,
#         },
#         ...
#       ],
#
#       # Testing platform, as a string. Used in determining the browser
#       # argument to pass to telemetry.
#       'platform': 'platform_name',
#
#       # Dimensions to pass to swarming, as a dict of string keys & values.
#       'dimension': {
#         'dimension1_name': 'dimension1_value',
#         ...
#       },
#     },
#     ...
#   }

class TEST_TYPES(object):
  GENERIC = 0
  GTEST = 1
  TELEMETRY = 2

  ALL = (GENERIC, GTEST, TELEMETRY)


# This is an opt-in list for tester which will skip the perf data handling.
# The perf data will be handled on a separated 'processor' VM.
# This list will be removed or replace by an opt-out list.
LIGHTWEIGHT_TESTERS = [
    'linux-perf',
    'win-10-perf',
    'win-10_laptop_low_end-perf',
    'win-11-perf',
    'mac-laptop_high_end-perf',
    'mac-laptop_low_end-perf',
]

# This is an opt-in list for builders which uses dynamic sharding.
DYNAMIC_SHARDING_TESTERS = ['linux-perf-calibration']

CALIBRATION_BUILDERS = {
    'linux-perf-calibration': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'linux',
        'dimension': {
            'gpu': '10de:1cb3-440.100',
            'os': 'Ubuntu-18.04',
            'pool': 'chrome.tests.perf',
            'synthetic_product_name': 'PowerEdge R230 (Dell Inc.)'
        },
    },
}

FYI_BUILDERS = {
    'android-cfi-builder-perf-fyi': {
        'additional_compile_targets': [
            'android_tools',
            'chrome_public_apk',
            'chromium_builder_perf',
            'push_apps_to_background_apk',
            'system_webview_apk',
            'system_webview_shell_apk',
        ],
    },
    'android_arm64-cfi-builder-perf-fyi': {
        'additional_compile_targets': [
            'android_tools',
            'chrome_public_apk',
            'chromium_builder_perf',
            'push_apps_to_background_apk',
            'system_webview_apk',
            'system_webview_shell_apk',
        ],
    },
    'linux-perf-fyi': {
        'tests': [{
            'isolate':
            'performance_test_suite',
            'extra_args': [
                '--output-format=histograms',
                '--experimental-tbmv3-metrics',
            ],
        }],
        'platform':
        'linux',
        'dimension': {
            'gpu': '10de',
            'os': 'Ubuntu',
            'pool': 'chrome.tests.perf-fyi',
        },
    },
    'fuchsia-perf-nsn': {
        'tests': [{
            'isolate':
            'performance_web_engine_test_suite',
            'extra_args': [
                '--output-format=histograms', '--experimental-tbmv3-metrics',
                '--extra-path=/b/s/w/ir/bin/'
            ] + bot_platforms.FUCHSIA_EXEC_ARGS['nelson'],
            'type':
            TEST_TYPES.TELEMETRY,
        }],
        'platform':
        'fuchsia-wes',
        # TODO(crbug.com/40272046): Replace with long-term solution for ssh in Fuchsia img,
        # or codify as long-term solution.
        'cipd': {
            "cipd_package": "fuchsia/third_party/openssh-portable/${platform}",
            "location": ".",
            "revision": "build_id:8787350426829126785"
        },
        'dimension': {
            'cpu': None,
            'device_type': 'Nelson',
            'os': 'Fuchsia',
            'pool': 'chrome.tests',
        },
    },
    'fuchsia-perf-shk': {
        'tests': [{
            'isolate':
            'performance_web_engine_test_suite',
            'extra_args': [
                '--output-format=histograms', '--experimental-tbmv3-metrics',
                '--extra-path=/b/s/w/ir/bin/'
            ] + bot_platforms.FUCHSIA_EXEC_ARGS['sherlock'],
            'type':
            TEST_TYPES.TELEMETRY,
        }],
        'platform':
        'fuchsia-wes',
        # TODO(crbug.com/40272046): Replace with long-term solution for ssh in Fuchsia img,
        # or codify as long-term solution.
        'cipd': {
            "cipd_package": "fuchsia/third_party/openssh-portable/${platform}",
            "location": ".",
            "revision": "build_id:8787350426829126785"
        },
        'dimension': {
            'cpu': None,
            'device_type': 'Sherlock',
            'os': 'Fuchsia',
            'pool': 'chrome.tests',
        },
    },
    'win-10_laptop_low_end-perf_HP-Candidate': {
        'tests': [
            {
                'isolate':
                'performance_test_suite',
                'extra_args': [
                    '--output-format=histograms',
                    '--experimental-tbmv3-metrics',
                ],
            },
        ],
        'platform':
        'win',
        'target_bits':
        64,
        'dimension': {
            'pool':
            'chrome.tests.perf-fyi',
            # TODO(crbug.com/41463380): Explicitly set the gpu to None to make
            # chromium_swarming recipe_module ignore this dimension.
            'gpu':
            None,
            'os':
            'Windows-10',
            'synthetic_product_name':
            'HP Laptop 15-bs1xx [Type1ProductConfigId] (HP)'
        },
    },
    'chromeos-kevin-builder-perf-fyi': {
        'additional_compile_targets': ['chromium_builder_perf'],
    },
    'chromeos-kevin-perf-fyi': {
        'tests': [
            {
                'isolate':
                'performance_test_suite',
                'extra_args': [
                    # The magic hostname that resolves to a CrOS device in the test lab
                    '--remote=variable_chromeos_device_hostname',
                ],
            },
        ],
        'platform':
        'chromeos',
        'target_bits':
        32,
        'dimension': {
            'pool': 'chrome.tests',
            # TODO(crbug.com/41463380): Explicitly set the gpu to None to make
            # chromium_swarming recipe_module ignore this dimension.
            'gpu': None,
            'os': 'ChromeOS',
            'device_type': 'kevin',
        },
    },
    'fuchsia-builder-perf-arm64': {
        'additional_compile_targets': [
            'web_engine_shell_pkg', 'cast_runner_pkg', 'chromium_builder_perf',
            'base_perftests'
        ],
    },
}

# These configurations are taken from chromium_perf.py in
# build/scripts/slave/recipe_modules/chromium_tests and must be kept in sync
# to generate the correct json for each tester
#
# The dimensions in pinpoint configs, excluding the dimension "pool",
# must be kept in sync with the dimensions here.
# This is to make sure the same type of machines are used between waterfall
# tests and pinpoint jobs
#
# On desktop builders, chromedriver is added as an additional compile target.
# The perf waterfall builds this target for each commit, and the resulting
# ChromeDriver is archived together with Chrome for use in bisecting.
# This can be used by Chrome test team, as well as by google3 teams for
# bisecting Chrome builds with their web tests. For questions or to report
# issues, please contact johnchen@chromium.org.
BUILDERS = {
    'android-builder-perf': {
        'tests': [
            {
                'name': 'resource_sizes_monochrome_minimal_apks',
                'isolate': 'resource_sizes_monochrome_minimal_apks',
                'type': TEST_TYPES.GENERIC,
                'resultdb': {
                    'has_native_resultdb_integration': True,
                },
            },
            {
                'name': 'resource_sizes_trichrome_google',
                'isolate': 'resource_sizes_trichrome_google',
                'type': TEST_TYPES.GENERIC,
                'resultdb': {
                    'has_native_resultdb_integration': True,
                },
            },
            {
                'name': 'resource_sizes_system_webview_google_bundle',
                'isolate': 'resource_sizes_system_webview_google_bundle',
                'type': TEST_TYPES.GENERIC,
                'resultdb': {
                    'has_native_resultdb_integration': True,
                },
            },
        ],
        'dimension': {
            'cpu': 'x86',
            'os': 'Ubuntu-22.04',
            'pool': 'chrome.tests',
        },
        'perf_trigger':
        False,
    },
    'android-builder-perf-pgo': {
        'dimension': {
            'cpu': 'x86',
            'os': 'Ubuntu-22.04',
            'pool': 'chrome.tests',
        },
        'perf_trigger': False,
    },
    'android_arm64-builder-perf': {
        'tests': [
            {
                'name': 'resource_sizes_monochrome_minimal_apks',
                'isolate': 'resource_sizes_monochrome_minimal_apks',
                'type': TEST_TYPES.GENERIC,
                'resultdb': {
                    'has_native_resultdb_integration': True,
                },
            },
            {
                'name': 'resource_sizes_trichrome_google',
                'isolate': 'resource_sizes_trichrome_google',
                'type': TEST_TYPES.GENERIC,
                'resultdb': {
                    'has_native_resultdb_integration': True,
                },
            },
            {
                'name': 'resource_sizes_system_webview_google_bundle',
                'isolate': 'resource_sizes_system_webview_google_bundle',
                'type': TEST_TYPES.GENERIC,
                'resultdb': {
                    'has_native_resultdb_integration': True,
                },
            },
        ],
        'dimension': {
            'cpu': 'x86',
            'os': 'Ubuntu-22.04',
            'pool': 'chrome.tests',
        },
        'perf_trigger':
        False,
    },
    'android_arm64-builder-perf-pgo': {
        'dimension': {
            'cpu': 'x86',
            'os': 'Ubuntu-22.04',
            'pool': 'chrome.tests',
        },
        'perf_trigger': False,
    },
    'android_arm64_high_end-builder-perf': {
        'additional_compile_targets': ['trichrome_google_64_32_minimal_apks'],
        'pinpoint_additional_compile_targets': [],
    },
    'linux-builder-perf': {
        'additional_compile_targets': [
            'chromedriver_group',
            'chrome/installer/linux',
        ],
        'pinpoint_additional_compile_targets': [],
        'tests': [{
            'name': 'chrome_sizes',
            'isolate': 'chrome_sizes',
            'type': TEST_TYPES.GENERIC,
            'resultdb': {
                'has_native_resultdb_integration': True,
            },
        }],
        'dimension': {
            'cpu': 'x86-64',
            'os': 'Ubuntu-22.04',
            'pool': 'chrome.tests',
        },
        'perf_trigger':
        False,
    },
    'linux-builder-perf-pgo': {
        'dimension': {
            'cpu': 'x86-64',
            'os': 'Ubuntu-22.04',
            'pool': 'chrome.tests',
        },
        'perf_trigger': False,
    },
    'linux-builder-perf-rel': {},
    'mac-builder-perf': {
        'additional_compile_targets': ['chromedriver'],
        'pinpoint_additional_compile_targets': [],
        'tests': [{
            'name': 'chrome_sizes',
            'isolate': 'chrome_sizes',
            'type': TEST_TYPES.GENERIC,
            'resultdb': {
                'has_native_resultdb_integration': True,
            },
        }],
        'dimension': {
            'cpu': 'x86-64',
            'os': 'Mac',
            'pool': 'chrome.tests',
        },
        'perf_trigger':
        False,
    },
    'mac-builder-perf-pgo': {
        'dimension': {
            'cpu': 'x86-64',
            'os': 'Mac',
            'pool': 'chrome.tests',
        },
        'perf_trigger': False,
    },
    'mac-arm-builder-perf': {
        'additional_compile_targets': ['chromedriver'],
        'pinpoint_additional_compile_targets': [],
        'tests': [{
            'name': 'chrome_sizes',
            'isolate': 'chrome_sizes',
            'type': TEST_TYPES.GENERIC,
            'resultdb': {
                'has_native_resultdb_integration': True,
            },
        }],
        'dimension': {
            'cpu': 'x86',
            'os': 'Mac',
            'pool': 'chrome.tests',
        },
        'perf_trigger':
        False,
    },
    'mac-arm-builder-perf-pgo': {
        'dimension': {
            'cpu': 'x86',
            'os': 'Mac',
            'pool': 'chrome.tests',
        },
        'perf_trigger': False,
    },
    'win64-builder-perf': {
        'additional_compile_targets': ['chromedriver'],
        'pinpoint_additional_compile_targets': [],
        'tests': [{
            'name': 'chrome_sizes',
            'isolate': 'chrome_sizes',
            'type': TEST_TYPES.GENERIC,
            'resultdb': {
                'has_native_resultdb_integration': True,
            },
        }],
        'dimension': {
            'cpu': 'x86-64',
            'os': 'Windows-10',
            'pool': 'chrome.tests',
        },
        'perf_trigger':
        False,
    },
    'win64-builder-perf-pgo': {
        'dimension': {
            'cpu': 'x86-64',
            'os': 'Windows-10',
            'pool': 'chrome.tests',
        },
        'perf_trigger': False,
    },
    'android-pixel4_webview-perf': {
        'tests': [{
            'isolate': 'performance_webview_test_suite',
        }],
        'platform': 'android-webview-trichrome-google-bundle',
        'dimension': {
            'pool': 'chrome.tests.perf-webview',
            'os': 'Android',
            'device_type': 'flame',
            'device_os': 'RP1A.201105.002',
            'device_os_flavor': 'google',
        },
    },
    'android-pixel4_webview-perf-pgo': {
        'tests': [{
            'isolate': 'performance_webview_test_suite',
        }],
        'platform': 'android-webview-trichrome-google-bundle',
        'dimension': {
            'pool': 'chrome.tests.perf-webview-pgo',
            'os': 'Android',
            'device_type': 'flame',
            'device_os': 'RP1A.201105.002',
            'device_os_flavor': 'google',
        },
    },
    'android-pixel4-perf': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {
            'pool': 'chrome.tests.perf',
            'os': 'Android',
            'device_type': 'flame',
            'device_os': 'RP1A.201105.002',
            'device_os_flavor': 'google',
        },
    },
    'android-pixel4-perf-pgo': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {
            'pool': 'chrome.tests.perf',
            'os': 'Android',
            'device_type': 'flame',
            'device_os': 'RP1A.201105.002',
            'device_os_flavor': 'google',
        },
    },
    'android-pixel6-perf': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {
            'pool': 'chrome.tests.perf',
            'os': 'Android',
            'device_type': 'oriole',
            'device_os': 'AP1A.240405.002',
            'device_os_flavor': 'google',
        },
    },
    'android-pixel6-perf-pgo': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
            'extra_args': ['--benchmark-max-runs=3'],
            'remove_args': ['--ignore-benchmark-exit-code'],
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {
            'pool': 'chrome.tests.perf-pgo',
            'os': 'Android',
            'device_type': 'oriole',
            'device_os': 'AP1A.240405.002',
            'device_os_flavor': 'google',
        },
    },
    'android-pixel-fold-perf': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {
            'pool': 'chrome.tests.perf',
            'os': 'Android',
            'device_type': 'felix',
            # 'device_os': 'UQ1A.240205.002', # relax before all pixel folds are reimaged
            'device_os_flavor': 'google',
        },
    },
    'android-pixel-tangor-perf': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {
            'pool': 'chrome.tests.perf',
            'os': 'Android',
            'device_type': 'tangorpro',
            'device_os_flavor': 'google',
        },
    },
    'android-pixel6-pro-perf': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {
            'pool': 'chrome.tests.perf',
            'os': 'Android',
            'device_type': 'raven',
            'device_os': 'AP1A.240405.002',
            'device_os_flavor': 'google',
        },
    },
    'android-pixel6-pro-perf-pgo': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {
            'pool': 'chrome.tests.perf',
            'os': 'Android',
            'device_type': 'raven',
            'device_os': 'AP1A.240405.002',
            'device_os_flavor': 'google',
        },
    },
    'android-go-wembley-perf': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_bundle',
        }],
        'platform':
        'android-trichrome-bundle',
        'dimension': {
            'pool': 'chrome.tests.perf',
            'os': 'Android',
            'device_type': 'wembley_2GB',
            'device_os_flavor': 'google',
        },
    },
    'android-go-wembley_webview-perf': {
        'tests': [{
            'isolate': 'performance_webview_test_suite',
        }],
        'platform': 'android-webview-google',
        'dimension': {
            'pool': 'chrome.tests.perf-webview',
            'os': 'Android',
            'device_type': 'wembley_2GB',
            'device_os_flavor': 'google',
        },
    },
    'android-new-pixel-perf': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {},
    },
    'android-new-pixel-perf-pgo': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {},
    },
    'android-new-pixel-pro-perf': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {},
    },
    'android-new-pixel-pro-perf-pgo': {
        'tests': [{
            'isolate':
            'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle',
        }],
        'platform':
        'android-trichrome-chrome-google-64-32-bundle',
        'dimension': {},
    },
    'win-10_laptop_low_end-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'win',
        'target_bits':
        64,
        'dimension': {
            'pool':
            'chrome.tests.perf',
            # Explicitly set GPU driver version and Windows OS version such
            # that we can be informed if this
            # version ever changes or becomes inconsistent. It is important
            # that bots are homogeneous. See crbug.com/988045 for history.
            'os':
            'Windows-10-19045',
            'gpu':
            '8086:1616-20.19.15.5171',
            'synthetic_product_name':
            'HP Laptop 15-bs1xx [Type1ProductConfigId] (HP)'
        },
    },
    'win-10_laptop_low_end-perf-pgo': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'win',
        'target_bits':
        64,
        'dimension': {
            'pool': 'chrome.tests.perf',
            'os': 'Windows-10',
            'gpu': '8086:1616',
        },
    },
    'win-10-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'win',
        'target_bits':
        64,
        'dimension': {
            'pool': 'chrome.tests.perf',
            'os': 'Windows-10',
            'synthetic_product_name': 'OptiPlex 7050 (Dell Inc.)'
        },
    },
    'win-10-perf-pgo': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'win',
        'target_bits':
        64,
        'dimension': {
            'pool': 'chrome.tests.perf',
            'os': 'Windows-10',
            'synthetic_product_name': 'OptiPlex 7050 (Dell Inc.)'
        },
    },
    'win-10_amd_laptop-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'win',
        'target_bits':
        64,
        'dimension': {
            'pool': 'chrome.tests.perf',
            # Explicitly set GPU driver version and Windows OS version such
            # that we can be informed if this
            # version ever changes or becomes inconsistent. It is important
            # that bots are homogeneous. See crbug.com/988045 for history.
            'os': 'Windows-10',
            'gpu': '1002:1638',
            'synthetic_product_name': 'OMEN by HP Laptop 16-c0xxx [ ] (HP)',
        },
    },
    'win-10_amd_laptop-perf-pgo': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'win',
        'target_bits':
        64,
        'dimension': {
            'pool': 'chrome.tests.perf',
            # Explicitly set GPU driver version and Windows OS version such
            # that we can be informed if this
            # version ever changes or becomes inconsistent. It is important
            # that bots are homogeneous. See crbug.com/988045 for history.
            'os': 'Windows-10',
            'gpu': '1002:1638',
            'synthetic_product_name': 'OMEN by HP Laptop 16-c0xxx [ ] (HP)',
        },
    },
    'win-11-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'win',
        'target_bits':
        64,
        'dimension': {
            'pool': 'chrome.tests.perf',
            # Explicitly set GPU driver version and Windows OS version such
            # that we can be informed if this
            # version ever changes or becomes inconsistent. It is important
            # that bots are homogeneous. See crbug.com/988045 for history.
            'os': 'Windows-11-22631.2428',
            'gpu': '102b:0536-4.5.0.5',
            'synthetic_product_name': 'PowerEdge R350 (Dell Inc.)'
        },
    },
    'win-11-perf-pgo': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'win',
        'target_bits':
        64,
        'dimension': {
            'pool': 'chrome.tests.perf',
            # Explicitly set GPU driver version and Windows OS version such
            # that we can be informed if this
            # version ever changes or becomes inconsistent. It is important
            # that bots are homogeneous. See crbug.com/988045 for history.
            'os': 'Windows-11-22631.2428',
            'gpu': '102b:0536-4.5.0.5',
            'synthetic_product_name': 'PowerEdge R350 (Dell Inc.)'
        },
    },
    'mac-laptop_low_end-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'mac',
        'dimension': {
            'cpu':
            'x86-64',
            'gpu':
            '8086:1626',
            'os':
            'Mac-12',
            'pool':
            'chrome.tests.perf',
            'synthetic_product_name':
            'MacBookAir7,2_x86-64-i5-5350U_Intel Broadwell HD Graphics 6000_8192_APPLE SSD SM0128G'
        },
    },
    'mac-intel-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'mac',
        'dimension': {
            'cpu':
            'x86-64',
            'gpu':
            '8086:3e9b',
            'os':
            'Mac-15',
            'pool':
            'chrome.tests.perf',
            'synthetic_product_name':
            'Macmini8,1_x86-64-i7-8700B_Intel UHD Graphics 630_65536_APPLE SSD AP1024M'
        },
    },
    'mac-laptop_low_end-perf-pgo': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'mac',
        'dimension': {
            'cpu':
            'x86-64',
            'gpu':
            '8086:1626',
            'os':
            'Mac-12',
            'pool':
            'chrome.tests.perf',
            'synthetic_product_name':
            'MacBookAir7,2_x86-64-i5-5350U_Intel Broadwell HD Graphics 6000_8192_APPLE SSD SM0128G'
        },
    },
    'mac-m1_mini_2020-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'mac',
        'dimension': {
            'cpu': 'arm',
            'mac_model': 'Macmini9,1',
            'os': 'Mac',
            'pool': 'chrome.tests.perf',
        },
    },
    'mac-m1_mini_2020-perf-pgo': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'mac',
        'dimension': {
            'cpu': 'arm',
            'mac_model': 'Macmini9,1',
            'os': 'Mac',
            'pool': 'chrome.tests.perf-pgo',
        },
    },
    'mac-m1_mini_2020-no-brp-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'mac',
        'dimension': {
            'cpu': 'arm',
            'mac_model': 'Macmini9,1',
            'os': 'Mac',
            'pool': 'chrome.tests.perf',
        },
    },
    'mac-m1-pro-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'mac',
        'dimension': {
            'cpu': 'arm',
            'mac_model': 'MacBookPro18,3',
            'os': 'Mac',
            'pool': 'chrome.tests.perf',
        },
    },
    'mac-m2-pro-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'mac',
        'dimension': {
            'cpu':
            'arm',
            'mac_model':
            'Mac14,7',
            'os':
            'Mac',
            'pool':
            'chrome.tests.perf',
            'synthetic_product_name':
            'Mac14,7_arm64-64-Apple_M2_apple m2_8192_APPLE SSD AP0256Z',
        },
    },
    'linux-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'linux',
        'dimension': {
            'gpu': '10de:1cb3-440.100',
            'os': 'Ubuntu-18.04',
            'pool': 'chrome.tests.perf',
            'synthetic_product_name': 'PowerEdge R230 (Dell Inc.)'
        },
    },
    'linux-perf-pgo': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'linux',
        'dimension': {
            'gpu': '10de:1cb3-440.100',
            'os': 'Ubuntu-18.04',
            'pool': 'chrome.tests.perf',
            'synthetic_product_name': 'PowerEdge R230 (Dell Inc.)'
        },
    },
    'linux-perf-rel': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'linux',
        'dimension': {
            'gpu': '10de:1cb3-440.100',
            'os': 'Ubuntu-18.04',
            'pool': 'chrome.tests.perf',
            'synthetic_product_name': 'PowerEdge R230 (Dell Inc.)'
        },
    },
    'linux-r350-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'linux',
        'dimension': {
            'os': 'Ubuntu-22',
            'pool': 'chrome.tests.perf',
            'synthetic_product_name': 'PowerEdge R350 (Dell Inc.)'
        },
    },
    'mac-laptop_high_end-perf': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'mac',
        'dimension': {
            'cpu':
            'x86-64',
            'gpu':
            '1002:6821-4.0.20-3.2.8',
            'os':
            'Mac-12',
            'pool':
            'chrome.tests.perf',
            'synthetic_product_name':
            'MacBookPro11,5_x86-64-i7-4870HQ_AMD Radeon R8 M370X 4.0.20 [3.2.8]_Intel Haswell Iris Pro Graphics 5200 4.0.20 [3.2.8]_16384_APPLE SSD SM0512G',
        },
    },
    'mac-laptop_high_end-perf-pgo': {
        'tests': [
            {
                'isolate': 'performance_test_suite',
                'extra_args': [
                    '--assert-gpu-compositing',
                ],
            },
        ],
        'platform':
        'mac',
        'dimension': {
            'cpu':
            'x86-64',
            'gpu':
            '1002:6821-4.0.20-3.2.8',
            'os':
            'Mac-12',
            'pool':
            'chrome.tests.perf',
            'synthetic_product_name':
            'MacBookPro11,5_x86-64-i7-4870HQ_AMD Radeon R8 M370X 4.0.20 [3.2.8]_Intel Haswell Iris Pro Graphics 5200 4.0.20 [3.2.8]_16384_APPLE SSD SM0512G',
        },
    },
    'linux-processor-perf': {
        'platform': 'linux',
        'perf_processor': True,
    },
    'android-go-processor-perf': {
        'platform': 'linux',
        'perf_processor': True,
    },
    'win-10-processor-perf': {
        'platform': 'linux',
        'perf_processor': True,
    },
    'win-10_laptop_low_end-processor-perf': {
        'platform': 'linux',
        'perf_processor': True,
    },
    'win-11-processor-perf': {
        'platform': 'linux',
        'perf_processor': True,
    },
    'mac-laptop_low_end-processor-perf': {
        'platform': 'linux',
        'perf_processor': True,
    },
    'mac-laptop_high_end-processor-perf': {
        'platform': 'linux',
        'perf_processor': True,
    }
}

# pylint: enable=line-too-long

_TESTER_SERVICE_ACCOUNT = (
    'chrome-tester@chops-service-accounts.iam.gserviceaccount.com')


def _generate_pinpoint_builders_dict(builder):
  result = {}
  for key in builder:
    content = copy.deepcopy(builder[key])
    if 'pinpoint_additional_compile_targets' in content:
      additional_compile_targets = content.pop(
          'pinpoint_additional_compile_targets')
    else:
      additional_compile_targets = content.pop('additional_compile_targets',
                                               None)
    if additional_compile_targets:
      content['additional_compile_targets'] = additional_compile_targets
    elif 'additional_compile_targets' in content:
      del content['additional_compile_targets']
    tests = content.get('tests', [])
    tests = list(
        filter(
            lambda x: not (x.get('name') == 'chrome_sizes' or x.get('name', '').
                           startswith('resource_sizes')), tests))
    if tests:
      content['tests'] = tests
    elif 'tests' in content:
      del content['tests']
    if content:
      result[key] = content
  return result


def update_all_builders(file_path):
  return (_update_builders(BUILDERS, file_path)
          and is_perf_benchmarks_scheduling_valid(file_path, sys.stderr))


def update_all_pinpoint_builders(file_path):
  return (_update_builders(_generate_pinpoint_builders_dict(BUILDERS),
                           file_path) and is_perf_benchmarks_scheduling_valid(
                               file_path, sys.stderr, is_waterfall=False))


def update_all_fyi_builders(file_path):
  return _update_builders(FYI_BUILDERS, file_path)


def update_all_calibration_builders(file_path):
  return _update_builders(CALIBRATION_BUILDERS, file_path)


def _update_builders(builders_dict, file_path):
  tests = {}
  tests['AAAAA1 AUTOGENERATED FILE DO NOT EDIT'] = {}
  tests['AAAAA2 See //tools/perf/generate_perf_data to make changes'] = {}

  for name, config in builders_dict.items():
    tests[name] = generate_builder_config(config, name)

  with open(file_path, 'w',
            newline='') if sys.version_info.major == 3 else open(
                file_path, 'wb') as fp:
    json.dump(tests, fp, indent=2, separators=(',', ': '), sort_keys=True)
    fp.write('\n')
  return True


def merge_dicts(*dict_args):
  result = {}
  for dictionary in dict_args:
    result.update(dictionary)
  return result


class BenchmarkMetadata(object):
  def __init__(self, emails, component='', documentation_url='', stories=None):
    """An object to hold information about a benchmark.

    Args:
      emails: A string with a comma separated list of owner emails.
      component: An optional string with a component for filing bugs about this
        benchmark.
      documentation_url: An optional string with a URL where documentation
        about the benchmark can be found.
      stories: An optional list of benchmark_utils.StoryInfo tuples with
        information about stories contained in this benchmark.
    """
    self.emails = emails
    self.component = component
    self.documentation_url = documentation_url
    if stories is not None:
      assert isinstance(stories, list)
      self.stories = stories
    else:
      self.stories = []

  @property
  def tags(self):
    """Return a comma separated list of all tags used by benchmark stories."""
    return ','.join(sorted(set().union(*(s.tags for s in self.stories))))


GTEST_BENCHMARKS = {
    'base_perftests':
    BenchmarkMetadata(
        'skyostil@chromium.org, gab@chromium.org', 'Internals>SequenceManager',
        ('https://chromium.googlesource.com/chromium/src/+/HEAD/base/' +
         'README.md#performance-testing')),
    'tracing_perftests':
    BenchmarkMetadata(
        'eseckler@chromium.org, khokhlov@chromium.org, kraskevich@chromium.org',
        'Speed>Tracing'),
    'load_library_perf_tests':
    BenchmarkMetadata('xhwang@chromium.org, jrummell@chromium.org',
                      'Internals>Media>Encrypted'),
    'views_perftests':
    BenchmarkMetadata('tapted@chromium.org', 'Internals>Views'),
    'components_perftests':
    BenchmarkMetadata('csharrison@chromium.org'),
    'dawn_perf_tests':
    BenchmarkMetadata(
        'enga@chromium.org', 'Dawn',
        'https://dawn.googlesource.com/dawn/+/HEAD/src/tests/perf_tests/README.md'
    ),
    'tint_benchmark':
    BenchmarkMetadata(
        'jrprice@google.com, dsinclair@chromium.org', 'Dawn>Tint',
        'https://dawn.googlesource.com/dawn/+/HEAD/docs/tint/benchmark.md'),
}

RESOURCE_SIZES_METADATA = BenchmarkMetadata(
    'agrieve@chromium.org', 'Build>Android',
    ('https://chromium.googlesource.com/chromium/src/+/HEAD/'
     'tools/binary_size/README.md#resource_sizes_py'))

OTHER_BENCHMARKS = {
    'resource_sizes_monochrome_minimal_apks': RESOURCE_SIZES_METADATA,
    'resource_sizes_trichrome_google': RESOURCE_SIZES_METADATA,
    'resource_sizes_system_webview_google_bundle': RESOURCE_SIZES_METADATA,
}

OTHER_BENCHMARKS.update({
    'chrome_sizes':
    BenchmarkMetadata(
        emails='chonggu@chromium.org',
        component='Build',
        documentation_url=(
            'https://chromium.googlesource.com/chromium/'
            'src/+/HEAD/tools/binary_size/README.md#resource_sizes_py'),
    ),
})

SYSTEM_HEALTH_BENCHMARKS = set([
    'system_health.common_desktop',
    'system_health.common_mobile',
    'system_health.memory_desktop',
    'system_health.memory_mobile',
])

# Valid test suite (benchmark) names should match this regex.
RE_VALID_TEST_SUITE_NAME = r'^[\w._-]+$'


def _get_telemetry_perf_benchmarks_metadata():
  metadata = {}
  for benchmark in benchmark_finders.GetOfficialBenchmarks():
    benchmark_name = benchmark.Name()
    emails = decorators.GetEmails(benchmark)
    if emails:
      emails = ', '.join(emails)
    metadata[benchmark_name] = BenchmarkMetadata(
        emails=emails,
        component=decorators.GetComponent(benchmark),
        documentation_url=decorators.GetDocumentationLink(benchmark),
        stories=benchmark_utils.GetBenchmarkStoryInfo(benchmark()))
  return metadata


TELEMETRY_PERF_BENCHMARKS = _get_telemetry_perf_benchmarks_metadata()

PERFORMANCE_TEST_SUITES = [
    'performance_test_suite',
    'performance_test_suite_eve',
    'performance_test_suite_octopus',
    'performance_webview_test_suite',
    'performance_web_engine_test_suite',
]
for suffix in android_browser_types.TELEMETRY_ANDROID_BROWSER_TARGET_SUFFIXES:
  PERFORMANCE_TEST_SUITES.append('performance_test_suite' + suffix)


def get_scheduled_non_telemetry_benchmarks(perf_waterfall_file):
  test_names = set()

  with open(perf_waterfall_file) as f:
    tests_by_builder = json.load(f)

  script_tests = []
  for tests in tests_by_builder.values():
    if 'isolated_scripts' in tests:
      script_tests += tests['isolated_scripts']
    if 'scripts' in tests:
      script_tests += tests['scripts']

  for s in script_tests:
    name = s['name']
    # TODO(eyaich): Determine new way to generate ownership based
    # on the benchmark bot map instead of on the generated tests
    # for new perf recipe.
    if not name in PERFORMANCE_TEST_SUITES:
      test_names.add(name)

  for platform in bot_platforms.ALL_PLATFORMS:
    for executable in platform.executables:
      test_names.add(executable.name)

  return test_names


def is_perf_benchmarks_scheduling_valid(perf_waterfall_file,
                                        outstream,
                                        is_waterfall=True):
  """Validates that all existing benchmarks are properly scheduled.

  Return: True if all benchmarks are properly scheduled, False otherwise.
  """
  scheduled_non_telemetry_tests = get_scheduled_non_telemetry_benchmarks(
      perf_waterfall_file)
  all_perf_gtests = set(GTEST_BENCHMARKS)
  all_perf_other_tests = set(OTHER_BENCHMARKS)

  error_messages = []

  for test_name in all_perf_gtests - scheduled_non_telemetry_tests:
    error_messages.append(
        'Benchmark %s is tracked but not scheduled on any perf waterfall '
        'builders. Either schedule or remove it from GTEST_BENCHMARKS.' %
        test_name)

  if is_waterfall:
    for test_name in all_perf_other_tests - scheduled_non_telemetry_tests:
      error_messages.append(
          'Benchmark %s is tracked but not scheduled on any perf waterfall '
          'builders. Either schedule or remove it from OTHER_BENCHMARKS.' %
          test_name)

  for test_name in scheduled_non_telemetry_tests.difference(
      all_perf_gtests, all_perf_other_tests):
    error_messages.append(
        'Benchmark %s is scheduled on perf waterfall but not tracked. Please '
        'add an entry for it in GTEST_BENCHMARKS or OTHER_BENCHMARKS in'
        '//tools/perf/core/perf_data_generator.py.' % test_name)

  for message in error_messages:
    print('*', textwrap.fill(message, 70), '\n', file=outstream)

  return not error_messages


# Verify that all benchmarks have owners except those on the whitelist.
def _verify_benchmark_owners(benchmark_metadatas):
  unowned_benchmarks = set()
  for benchmark_name in benchmark_metadatas:
    if benchmark_metadatas[benchmark_name].emails is None:
      unowned_benchmarks.add(benchmark_name)

  assert not unowned_benchmarks, (
      'All benchmarks must have owners. Please add owners for the following '
      'benchmarks:\n%s' % '\n'.join(unowned_benchmarks))


# Open a CSV file for writing, handling the differences between Python 2 and 3.
def _create_csv(file_path):
  if sys.version_info.major == 2:
    return open(file_path, 'wb')
  return open(file_path, 'w', newline='')


def update_benchmark_csv(file_path):
  """Updates go/chrome-benchmarks.

  Updates telemetry/perf/benchmark.csv containing the current benchmark names,
  owners, and components. Requires that all benchmarks have owners.
  """
  header_data = [
      ['AUTOGENERATED FILE DO NOT EDIT'],
      [
          'See the following link for directions for making changes ' +
          'to this data:', 'https://bit.ly/update-benchmarks-info'
      ],
      [
          'Googlers can view additional information about internal perf ' +
          'infrastructure at',
          'https://goto.google.com/chrome-benchmarking-sheet'
      ],
      [
          'Benchmark name', 'Individual owners', 'Component', 'Documentation',
          'Tags'
      ]
  ]

  csv_data = []
  benchmark_metadatas = merge_dicts(
      GTEST_BENCHMARKS, OTHER_BENCHMARKS, TELEMETRY_PERF_BENCHMARKS)
  _verify_benchmark_owners(benchmark_metadatas)

  undocumented_benchmarks = set()
  for benchmark_name in benchmark_metadatas:
    if not re.match(RE_VALID_TEST_SUITE_NAME, benchmark_name):
      raise ValueError('Invalid benchmark name: %s' % benchmark_name)
    if not benchmark_metadatas[benchmark_name].documentation_url:
      undocumented_benchmarks.add(benchmark_name)
    csv_data.append([
        benchmark_name,
        benchmark_metadatas[benchmark_name].emails,
        benchmark_metadatas[benchmark_name].component,
        benchmark_metadatas[benchmark_name].documentation_url,
        benchmark_metadatas[benchmark_name].tags,
    ])
  if undocumented_benchmarks != ub_module.UNDOCUMENTED_BENCHMARKS:
    error_message = (
        'The list of known undocumented benchmarks does not reflect the actual '
        'ones.\n')
    if undocumented_benchmarks - ub_module.UNDOCUMENTED_BENCHMARKS:
      error_message += (
          'New undocumented benchmarks found. Please document them before '
          'enabling on perf waterfall: %s' %
          (','.join(b for b in undocumented_benchmarks -
                    ub_module.UNDOCUMENTED_BENCHMARKS)))
    if ub_module.UNDOCUMENTED_BENCHMARKS - undocumented_benchmarks:
      error_message += (
          'These benchmarks are already documented. Please remove them from '
          'the UNDOCUMENTED_BENCHMARKS list in undocumented_benchmarks.py: %s' %
          (','.join(b for b in ub_module.UNDOCUMENTED_BENCHMARKS -
                    undocumented_benchmarks)))

    raise ValueError(error_message)

  csv_data = sorted(csv_data, key=lambda b: b[0])
  csv_data = header_data + csv_data

  with _create_csv(file_path) as f:
    writer = csv.writer(f, lineterminator='\n')
    writer.writerows(csv_data)
  return True


def update_system_health_stories(filepath):
  """Updates bit.ly/csh-stories.

  Updates tools/perf/system_health_stories.csv containing the current set
  of system health stories.
  """
  header_data = [[
      'AUTOGENERATED FILE DO NOT EDIT'
  ], ['See //tools/perf/core/perf_data_generator.py to make changes'],
                 ['Story', 'Description', 'Platforms', 'Tags']]

  stories = {}
  for benchmark_name in sorted(SYSTEM_HEALTH_BENCHMARKS):
    platform = benchmark_name.rsplit('_', 1)[-1]
    for story in TELEMETRY_PERF_BENCHMARKS[benchmark_name].stories:
      if story.name not in stories:
        stories[story.name] = {
            'description': story.description,
            'platforms': set([platform]),
            'tags': set(story.tags)
        }
      else:
        stories[story.name]['platforms'].add(platform)
        stories[story.name]['tags'].update(story.tags)

  with _create_csv(filepath) as f:
    writer = csv.writer(f, lineterminator='\n')
    for row in header_data:
      writer.writerow(row)
    for story_name, info in sorted(stories.items()):
      platforms = ','.join(sorted(info['platforms']))
      tags = ','.join(sorted(info['tags']))
      writer.writerow([story_name, info['description'], platforms, tags])
  return True


def update_labs_docs_md(filepath):
  primary_configs = collections.defaultdict(list)
  pinpoint_configs = collections.defaultdict(list)
  fyi_configs = collections.defaultdict(list)
  for tester in bot_platforms.ALL_PLATFORMS:
    if tester.pinpoint_only:
      pinpoint_configs[tester.platform].append(tester)
    elif tester.is_fyi:
      fyi_configs[tester.platform].append(tester)
    else:
      primary_configs[tester.platform].append(tester)

  with open(filepath, 'w', newline='') as f:
    f.write("""
[comment]: # (AUTOGENERATED FILE DO NOT EDIT)
[comment]: # (See //tools/perf/generate_perf_data to make changes)

# Platforms tested in the Performance Lab

""")
    config_groups = (
        ('Primary', primary_configs),
        ('Pinpoint-Only', pinpoint_configs),
        ('FYI', fyi_configs),
    )
    for group, configs in config_groups:
      f.write('## %s Platforms\n\n' % group)
      for platform, testers in sorted(configs.items()):
        f.write('### %s\n\n' % platform.title())
        testers.sort()
        for tester in testers:
          f.write(' * ')

          if tester.builder_url:
            f.write('[{0.name}]({0.builder_url})'.format(tester))
          else:
            f.write(tester.name)

          if tester.description:
            f.write(': {0.description}.\n'.format(tester))
          else:
            f.write('.\n')

        f.write('\n')
  return True


def generate_telemetry_args(tester_config, platform):
  # First determine the browser that you need based on the tester
  browser_name = ''
  # For trybot testing we always use the reference build
  if tester_config.get('testing', False):
    browser_name = 'reference'
  elif 'browser' in tester_config:
    browser_name = 'exact'
  elif tester_config['platform'] == 'android':
    browser_name = 'android-chromium'
  elif tester_config['platform'].startswith('android-'):
    browser_name = tester_config['platform']
  elif tester_config['platform'] == 'chromeos':
    browser_name = 'cros-chrome'
  elif tester_config['platform'] == 'lacros':
    browser_name = 'lacros-chrome'
  elif (tester_config['platform'] == 'win'
        and tester_config['target_bits'] == 64):
    browser_name = 'release_x64'
  elif tester_config['platform'] == 'fuchsia-wes':
    browser_name = 'web-engine-shell'
  elif tester_config['platform'] == 'fuchsia-chrome':
    browser_name = 'fuchsia-chrome'
  else:
    browser_name = 'release'
  test_args = [
      '-v',
      '--browser=%s' % browser_name,
      '--upload-results',
      '--test-shard-map-filename=%s' % platform.shards_map_file_name,
      '--ignore-benchmark-exit-code',
  ]
  if platform.run_reference_build:
    test_args.append('--run-ref-build')
  if 'browser' in tester_config:
    test_args.append('--browser-executable=../../out/Release/%s' %
                     tester_config['browser'])
    if tester_config['platform'].startswith('android'):
      test_args.append('--device=android')
  return test_args


def generate_gtest_args(test_name):
  # --gtest-benchmark-name so the benchmark name is consistent with the test
  # step's name. This is not always the same as the test binary's name (see
  # crbug.com/870692).
  return [
      '--gtest-benchmark-name',
      test_name,
  ]


def generate_performance_test(tester_config, test, builder_name):
  isolate_name = test['isolate']

  test_name = test.get('name', isolate_name)
  test_type = test.get('type', TEST_TYPES.TELEMETRY)
  assert test_type in TEST_TYPES.ALL

  shards = test.get('num_shards', None)
  test_args = []
  if test_type == TEST_TYPES.TELEMETRY:
    platform = bot_platforms.PLATFORMS_BY_NAME[builder_name]
    test_args += generate_telemetry_args(tester_config, platform)
    assert shards is None
    shards = platform.num_shards
  elif test_type == TEST_TYPES.GTEST:
    test_args += generate_gtest_args(test_name=test_name)
    assert shards
  # Append any additional args specific to an isolate
  test_args += test.get('extra_args', [])
  test_args = [
      arg for arg in test_args if arg not in test.get('remove_args', [])
  ]

  result = {
      'args': test_args,
      'test': isolate_name,
      'name': test_name,
  }

  if test.get('resultdb'):
    result['resultdb'] = test['resultdb'].copy()
  elif 'builder-perf' not in builder_name:
    # Enable Result DB on all perf test bots. Builders with names including
    # "builder-perf" are used for compiling only, and do not run perf tests.
    # TODO(crbug.com/40151981): Replace the following line by specifying either
    # "result_format" for GTests, or "has_native_resultdb_integration" for all
    # other tests.
    result['resultdb'] = {'enable': True}

  # For now we either get shards from the number of devices specified
  # or a test entry needs to specify the num shards if it supports
  # soft device affinity.

  if tester_config.get('perf_trigger', True):
    result['trigger_script'] = {
        'requires_simultaneous_shard_dispatch': True,
        'script': '//testing/trigger_scripts/perf_device_trigger.py',
        'args': [
            '--multiple-dimension-script-verbose',
            'True'
        ],
    }
    if builder_name in DYNAMIC_SHARDING_TESTERS:
      result['trigger_script']['args'].append('--use-dynamic-shards')

  result['merge'] = {
      'script': '//tools/perf/process_perf_results.py',
  }
  if builder_name in LIGHTWEIGHT_TESTERS:
    result['merge']['args'] = ['--lightweight', '--skip-perf']

  result['swarming'] = {
      # Always say this is true regardless of whether the tester
      # supports swarming. It doesn't hurt.
      'can_use_on_swarming_builders': True,
      'expiration': 2 * 60 * 60,  # 2 hours pending max
      # TODO(crbug.com/40585750): once we have plenty of windows hardwares,
      # to shards perf benchmarks on Win builders, reduce this hard timeout
      # limit to ~2 hrs.
      # Note that the builder seems to time out after 7 hours
      # (crbug.com/1036447), so we must timeout the shards within ~6 hours to
      # allow for other overhead. If the overall builder times out then we
      # don't get data even from the passing shards.
      'hard_timeout': test.get('timeout', 6 * 60 * 60),  # default 6 hours
      # This is effectively the timeout for a
      # benchmarking subprocess to run since we intentionally do not stream
      # subprocess output to the task stdout.
      # TODO(crbug.com/40585750): Reduce this once we can reduce hard_timeout.
      'io_timeout': test.get('timeout', 6 * 60 * 60),
      'dimensions': tester_config['dimension'],
      'service_account': _TESTER_SERVICE_ACCOUNT,
  }
  if shards:
    result['swarming']['shards'] = shards
  if tester_config.get('cipd'):
    result['swarming']['cipd_packages'] = [tester_config['cipd']]
  return result


def generate_builder_config(condensed_config, builder_name):
  config = {}

  if 'additional_compile_targets' in condensed_config:
    config['additional_compile_targets'] = (
        condensed_config['additional_compile_targets'])
  # TODO(crbug.com/40129604): remove this setting
  if 'perf_processor' in condensed_config:
    config['merge'] = {
        'script': '//tools/perf/process_perf_results.py',
    }
    config['merge']['args'] = ['--lightweight']

  condensed_tests = condensed_config.get('tests')
  if condensed_tests:
    gtest_tests = []
    telemetry_tests = []
    other_tests = []
    for test in condensed_tests:
      generated_script = generate_performance_test(
          condensed_config, test, builder_name)
      test_type = test.get('type', TEST_TYPES.TELEMETRY)
      if test_type == TEST_TYPES.GTEST:
        gtest_tests.append(generated_script)
      elif test_type == TEST_TYPES.TELEMETRY:
        telemetry_tests.append(generated_script)
      elif test_type == TEST_TYPES.GENERIC:
        other_tests.append(generated_script)
      else:
        raise ValueError(
            'perf_data_generator.py does not understand test type %s.' %
            test_type)
    gtest_tests.sort(key=lambda x: x['name'])
    telemetry_tests.sort(key=lambda x: x['name'])
    other_tests.sort(key=lambda x: x['name'])

    # Put Telemetry tests as the end since they tend to run longer to avoid
    # starving gtests (see crbug.com/873389).
    config['isolated_scripts'] = gtest_tests + telemetry_tests + other_tests

  return config


# List of all updater functions and the file they generate. The updater
# functions must return True on success and False otherwise. File paths are
# relative to chromium src and should use posix path separators (i.e. '/').
ALL_UPDATERS_AND_FILES = [
    (update_all_builders, 'testing/buildbot/chromium.perf.json'),
    (update_all_pinpoint_builders,
     'testing/buildbot/chromium.perf.pinpoint.json'),
    (update_all_fyi_builders, 'testing/buildbot/chromium.perf.fyi.json'),
    (update_all_calibration_builders,
     'testing/buildbot/chromium.perf.calibration.json'),
    (update_benchmark_csv, 'tools/perf/benchmark.csv'),
    (update_system_health_stories, 'tools/perf/system_health_stories.csv'),
    (update_labs_docs_md, 'docs/speed/perf_lab_platforms.md'),
]


def _source_filepath(posix_path):
  return os.path.join(path_util.GetChromiumSrcDir(), *posix_path.split('/'))


def validate_all_files():
  """Validate all generated files."""
  tempdir = tempfile.mkdtemp()
  try:
    for run_updater, src_file in ALL_UPDATERS_AND_FILES:
      real_filepath = _source_filepath(src_file)
      temp_filepath = os.path.join(tempdir, os.path.basename(real_filepath))
      if not (os.path.exists(real_filepath) and
              run_updater(temp_filepath) and
              filecmp.cmp(temp_filepath, real_filepath)):
        return False
  finally:
    shutil.rmtree(tempdir)
  return True


def update_all_files():
  """Update all generated files."""
  for run_updater, src_file in ALL_UPDATERS_AND_FILES:
    if not run_updater(_source_filepath(src_file)):
      print('Failed updating:', src_file)
      return False
    print('Updated:', src_file)
  return True


def main(args):
  parser = argparse.ArgumentParser(
      description=('Generate perf test\' json config and benchmark.csv. '
                   'This needs to be done anytime you add/remove any existing'
                   'benchmarks in tools/perf/benchmarks.'))
  parser.add_argument(
      '--validate-only', action='store_true', default=False,
      help=('Validate whether the perf json generated will be the same as the '
            'existing configs. This does not change the contain of existing '
            'configs'))
  options = parser.parse_args(args)

  if options.validate_only:
    if validate_all_files():
      print('All the perf config files are up-to-date. \\o/')
      return 0
    print('Not all perf config files are up-to-date. Please run %s '
          'to update them.' % sys.argv[0])
    return 1
  return 0 if update_all_files() else 1
