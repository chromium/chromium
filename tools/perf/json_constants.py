# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Constants for json results file."""
from types import MappingProxyType

BENCHMARK = 'benchmark'
BENCHMARKS = 'benchmarks'
BOT = 'bot'
BOT_ID = 'botId'
BOT_IDS = 'Bot Id'
BUILD_PAGE = 'Build Page'
CHROMIUM_COMMIT_POSITION = 'Chromium Commit Position'
COUNT = 'count'
DIAGNOSTICS = 'diagnostics'
ERROR = 'error'
GENERIC_SET = 'GenericSet'
GIT_HASH = 'git_hash'
GUID = 'guid'
IMPROVEMENT_DIRECTION = 'improvement_direction'
KEY = 'key'
LINKS = 'links'
MASTER = 'master'
MAX = 'max'
MEASUREMENTS = 'measurements'
MEASUREMENT = 'measurement'
MIN = 'min'
NAME = 'name'
OS_DETAILED_VERSIONS = 'osDetailedVersions'
OS_VERSION = 'OS Version'
RESULTS = 'results'
SAMPLE_VALUES = 'sampleValues'
STD_DEV = 'error'
STAT = 'stat'
STORIES = 'stories'
STORY_TAGS = 'storyTags'
SUBTEST_1 = 'subtest_1'
SUBTEST_2 = 'subtest_2'
SUM = 'sum'
SUMMARY_OPTIONS = 'summaryOptions'
TEST = 'test'
TRACE_URLS = 'traceUrls'
TYPE = 'type'
UNIT = 'unit'
V8_GIT_HASH = 'V8 Git Hash'
VALUE = 'value'
VALUES = 'values'
VERSION = 'version'
WEBRTC_GIT_HASH = 'WebRTC Git Hash'
UNIT_TO_DIRECTION = MappingProxyType({
    'ms_smallerIsBetter': 'down',
    'unitless_biggerIsBetter': 'up',
})
