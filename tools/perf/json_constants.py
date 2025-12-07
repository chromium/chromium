# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Constants for json results file."""

AVERAGE = 'average'
BENCHMARK = 'benchmark'
BENCHMARKS = 'benchmarks'
BOT = 'bot'
BOT_ID = 'botId'
BOT_IDS = 'Bot Id'
BUILD_PAGE = 'Build Page'
CHROMIUM_COMMIT_POSITION = 'Chromium Commit Position'
COUNT = 'count'
DIAGNOSTICS = 'diagnostics'
EXPERIMENT_GCS_BUCKET = 'chrome-perf-experiment-non-public'
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
TRACING_URI = 'Tracing uri'
TYPE = 'type'
UNIT = 'unit'
V8_GIT_HASH = 'V8'
VALUE = 'value'
VALUES = 'values'
VERSION = 'version'
WEBRTC_GIT_HASH = 'WebRTC'
REPOSITORY_PROPERTY_MAP = {
  'chromium': {
    'masters': ['ChromeFYIInternal', 'ChromiumAndroid', 'ChromiumChrome',
    'ChromiumChromiumos', 'ChromiumClang', 'ChromiumFuchsia', 'ChromiumGPUFYI',
    'ChromiumPerf', 'ChromiumPerfFyi', 'ChromiumPerfPGO',
    'TryServerChromiumFuchsia', 'TryserverChromiumChromiumOS',
    'ChromiumFuchsiaFyi', 'TryserverChromiumAndroid',
    'ChromiumAndroidFyi', 'ChromiumFYI', 'ChromiumPerfFyi.all'],
    'public_bucket_name': 'chrome-perf-public',
    'internal_bucket_name': 'chrome-perf-non-public',
    'ingest_folder': 'ingest',
    'commit_number': True,
    'revision_param': 'revision',
  },
  'webrtc': {
    'masters': ['WebRTCPerf'],
    'public_bucket_name': 'webrtc-perf-public',
    'internal_bucket_name': None,
    'ingest_folder': 'ingest-cp',
    'commit_number': True,
    'revision_param': 'revision',
  },
  'widevine-cdm': {
    'masters': ['WidevineCdmPerf'],
    'public_bucket_name': None,
    'internal_bucket_name': 'widevine-cdm-perf',
    'ingest_folder': 'ingest',
    'commit_number': False,
    'revision_param': 'r_cdm_git',
  },
  'widevine-whitebox': {
    'masters': ['WidevineWhiteboxPerf_master'],
    'public_bucket_name': None,
    'internal_bucket_name': 'widevine-whitebox-perf',
    'ingest_folder': 'ingest',
    'commit_number': False,
    'revision_param': 'r_cdm_git',
  },
  'v8': {
    'masters': ['internal.client.v8', 'client.v8'],
    'public_bucket_name': None,
    'internal_bucket_name': 'v8-perf-prod',
    'ingest_folder': 'ingest',
    'commit_number': True,
    'revision_param': 'revision',
  },
  'devtools-frontend': {
    'masters': ['client.devtools-frontend.integration'],
    'public_bucket_name': None,
    'internal_bucket_name': 'devtools-frontend-perf',
    'ingest_folder': 'ingest',
    'commit_number': False,
    'revision_param': 'r_devtools_git',
  },
  'fuchsia': {
    'masters': ['fuchsia.global.ci'],
    'public_bucket_name': None,
    'internal_bucket_name': 'fuchsia-perf-public',
    'ingest_folder': 'ingest',
    'commit_number': False,
    'revision_param': 'r_fuchsia_integ_pub_git',
  }
}
