#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittest for json_util.py."""

import copy
import datetime
import unittest
from unittest import mock
import json_util
from parameterized import parameterized  # pylint: disable=import-error

# pylint: disable=too-many-lines

# Mocked constants to avoid dependency on the actual json_constants file.
MOCK_JSON_CONSTANTS = {
    'AVERAGE': 'average',
    'BENCHMARK': 'benchmark',
    'BENCHMARKS': 'benchmarks',
    'BOT': 'bot',
    'BOT_ID': 'botId',
    'BOT_IDS': 'Bot Id',
    'BUILD_PAGE': 'Build Page',
    'CHROMIUM_COMMIT_POSITION': 'Chromium Commit Position',
    'COUNT': 'count',
    'DIAGNOSTICS': 'diagnostics',
    'EXPERIMENT_GCS_BUCKET': 'chrome-perf-experiment-non-public',
    'ERROR': 'error',
    'GENERIC_SET': 'GenericSet',
    'GIT_HASH': 'git_hash',
    'GUID': 'guid',
    'IMPROVEMENT_DIRECTION': 'improvement_direction',
    'KEY': 'key',
    'LINKS': 'links',
    'MASTER': 'master',
    'MAX': 'max',
    'MEASUREMENTS': 'measurements',
    'MEASUREMENT': 'measurement',
    'MIN': 'min',
    'NAME': 'name',
    'OS_DETAILED_VERSIONS': 'osDetailedVersions',
    'OS_VERSION': 'OS Version',
    'RESULTS': 'results',
    'SAMPLE_VALUES': 'sampleValues',
    'STD_DEV': 'error',
    'STAT': 'stat',
    'STORIES': 'stories',
    'STORY_TAGS': 'storyTags',
    'SUBTEST_1': 'subtest_1',
    'SUBTEST_2': 'subtest_2',
    'SUM': 'sum',
    'SUMMARY_OPTIONS': 'summaryOptions',
    'TEST': 'test',
    'TRACE_URLS': 'traceUrls',
    'TRACING_URI': 'Tracing uri',
    'TYPE': 'type',
    'UNIT': 'unit',
    'V8_GIT_HASH': 'V8',
    'VALUE': 'value',
    'VALUES': 'values',
    'VERSION': 'version',
    'WEBRTC_GIT_HASH': 'WebRTC',
    'STATS_BLOCKLIST': {'std', 'count', 'max', 'min', 'sum'},
}


def mock_json_constants(mock_obj):
  for key, value in MOCK_JSON_CONSTANTS.items():
    setattr(mock_obj, key, value)


class JsonUtilTest(unittest.TestCase):

  def _create_generic_set(self, guid, values):
    """Helper to create a GenericSet entry for test data."""
    return {'type': 'GenericSet', 'guid': guid, 'values': values}

  def _create_sample_result(self, name, values, stories_guid, tags_guid):
    """Helper to create a non-summary result item with sampleValues."""
    return {
        'name': name,
        'unit': 'ms_smallerIsBetter',
        'diagnostics': {
            'stories': stories_guid,
            'storyTags': tags_guid,
        },
        'sampleValues': values,
    }

  def _create_summary_result(self, name):
    """Helper to create a summary result item with summaryOptions."""
    return {
        'name': name,
        'unit': 'ms_smallerIsBetter',
        'diagnostics': {
            # Diagnostics can exist on summaries but are not used for subtests.
            'stories': 'summary-story-guid',
            'storyTags': 'summary-tag-guid',
        },
        'summaryOptions': {
            'count': 'true'
        },
    }

  @parameterized.expand([
      (
          'empty_stories_and_tags',
          [],
          [],
          ['', ''],
      ),
      (
          'just_stories',
          ['load:news:reddit:2018'],
          [],
          ['load_news_reddit_2018', ''],
      ),
      (
          'just_tags',
          [],
          ['2018', 'case:load', 'group:news'],
          ['load_news', ''],
      ),
      (
          'just_tags_2',
          ['no_colon_stories'],
          ['2018', 'case:load', 'group:games'],
          ['load_games', 'no_colon_stories'],
      ),
      (
          'valid_stories_and_tags',
          ['load:news:reddit:2018'],
          ['2018', 'case:load', 'group:news'],
          ['load_news', 'load_news_reddit_2018'],
      ),
      (
          'valid_stories_and_tags_2',
          ['load:games:lazors'],
          [
              '2016',
              'case:load',
              'group:games',
          ],
          ['load_games', 'load_games_lazors'],
      ),
      (
          'valid_stories_and_tags_3',
          ['browse:tools:photoshop_warm:2021'],
          [
              '2021',
              'wasm',
              'case:browse',
              'group:tools',
          ],
          ['browse_tools', 'browse_tools_photoshop_warm_2021'],
      ),
      (
          'valid_stories_and_tags_4',
          ['speedometer3'],
          [
              '2021',
              'wasm',
              'case',
              'group',
          ],
          ['speedometer3', ''],
      ),
      (
          'story_with_colon_and_slash',
          ['browse:news/story'],
          [],
          ['browse_news_story', ''],
      ),
      (
          'tag_with_equals_and_hash',
          [],
          ['case:run=fast', 'group:test#1'],
          ['run_fast_test_1', ''],
      ),
      (
          'story_and_tag_with_multiple_special_chars',
          ['page|load,story#1'],
          ['ver:1,2&3'],
          ['1_2_3', 'page_load_story_1'],
      ),
      (
          'no_colon_in_story',
          ['speedometer3'],
          ['case:run', 'group:all'],
          ['run_all', 'speedometer3'],
      ),
  ])
  def test_extract_subtest_from_stories_tags(self, _, stories, tags, expected):
    got_1, got_2 = json_util.extract_subtest_from_stories_tags(stories, tags)
    self.assertEqual(
        [got_1, got_2],
        expected,
    )

  def test_calculate_stats(self):
    with self.subTest(name='normal_values'):
      self.assertEqual(
        json_util.calculate_stats([1, 2, 3]),
        (2.0, 1.0, 3, 3, 1, 6),
      )
    with self.subTest(name='mixed_with_None'):
      self.assertEqual(
        json_util.calculate_stats([1, None, 3, None]),
        (2.0, 1.4142135623730951, 2, 3, 1, 4),
      )
    with self.subTest(name='mixed_with_zero'):
      self.assertEqual(
        json_util.calculate_stats([1, 0, 3, 0]),
        (1.0, 1.4142135623730951, 4, 3, 0, 4),
      )
    with self.subTest(name='mixed_with_None_and_zero'):
      self.assertEqual(
        json_util.calculate_stats([1, None, 3, 0]),
        (1.3333333333333333, 1.5275252316519468, 3, 3, 0, 4),
      )
    with self.subTest(name='empty_list'):
      self.assertEqual(
        json_util.calculate_stats([]),
        (0.0, 0.0, 0, 0, 0, 0),
      )

  @mock.patch('json_util.histogram_helpers')
  @mock.patch('json_util.json_constants')
  def test_process(self, mock_constants, mock_hist_helpers):
    mock_json_constants(mock_constants)
    setattr(mock_hist_helpers, '_STATS_BLACKLIST',
            MOCK_JSON_CONSTANTS['STATS_BLOCKLIST'])
    mock_hist_helpers.ShouldGenerateStatistics.return_value = True

    result2_json = [
        {
            'type': 'GenericSet',
            'guid': '35d015dd-887e-4b34-a8d5-0be7599dec78',
            'values': ['speedometer3'],
        },
        {
            'type': 'GenericSet',
            'guid': '9ef9da4a-3b79-4574-9603-bf9e2fe4bbe7',
            'values': ['speedometer3'],
        },
        {
            'type': 'GenericSet',
            'guid': '69d51091-2cd5-4f0d-a87e-9c08254e1a16',
            'values': ['The latest version of the Speedometer3 benchmark.'],
        },
        {
            'type': 'GenericSet',
            'guid': '6214d34c-9200-4271-9cea-895f24e99d5c',
            'values': ['win-222-e504'],
        },
        {
            'type': 'GenericSet',
            'guid': '947e0701-97ef-4e0c-8bde-2b565aa1a087',
            'values': ['cbruni@chromium.org', 'vahl@chromium.org'],
        },
        {
            'type': 'GenericSet',
            'guid': '93bae44b-c29d-48d6-99fa-a7c6bc962751',
            'values': ['Blink>JavaScript'],
        },
        {
            'type':
            'GenericSet',
            'guid':
            'f0a439d6-f0d2-4dcd-8bca-8f3912cd760f',
            'values': [[
                'Benchmark documentation link',
                'https://github.com/WebKit/Speedometer',
            ]],
        },
        {
            'type': 'GenericSet',
            'guid': '454e29c4-57dd-4987-9e3e-792ec84f07ce',
            'values': ['AMD64'],
        },
        {
            'type': 'GenericSet',
            'guid': 'dfb2f28d-fb32-49f2-b011-1d26853a6d6c',
            'values': ['win'],
        },
        {
            'type': 'GenericSet',
            'guid': '84d184f8-cfb0-4929-994b-85f908b91839',
            'values': ['win10'],
        },
        {
            'type': 'GenericSet',
            'guid': '6e8ba79c-9588-426f-92cf-a3ab803bcbba',
            'values': ['10.0.19045'],
        },
        {
            'type': 'DateRange',
            'guid': 'b5c3d0fa-e3dd-4e32-ab41-31c8a3ab0e2a',
            'min': 1736637289209.919,
        },
        {
            # this is the stories
            'type': 'GenericSet',
            'guid': '6903cc09-ca9e-47a4-82a0-d77d214d37fc',
            'values': ['Speedometer3'],
        },
        {
            'type': 'GenericSet',
            'guid': 'a7f54f55-870b-4b76-bb87-d64df4bf3e7b',
            'values': [0],
        },
        {
            # this is the storyTags
            'type': 'GenericSet',
            'guid': 'f0bb92d7-5ab2-42ed-ad7f-d79018aa3b60',
            'values': ['all'],
        },
        {
            # this is the stories that yield subtest_2
            'type': 'GenericSet',
            'guid': '6903cc09-ca9e-47a4-82a0-d77d214d37fd',
            'values': ['browse:news:nytimes:2020'],
        },
        {
            # this is the storyTags that yield subtest_1
            'type': 'GenericSet',
            'guid': 'f0bb92d7-5ab2-42ed-ad7f-d79018aa3b61',
            'values': ['2020', 'case:browse', 'group:news'],
        },
        {
            'type': 'DateRange',
            'guid': 'cf09d1a1-8b3b-4d3c-bee6-b8d341d5e31e',
            'min': 1736637289209.919,
        },
        {
            'type':
            'GenericSet',
            'guid':
            '8563ece0-740a-44c0-ae72-418721ee56d8',
            'values': [('https://storage.cloud.google.com/'
                        'chrome-telemetry-output/20251112T181417_63858/'
                        'rendering.desktop/balls_css_transition_all_properties/'
                        'retry_0/trace.pb')]
        },
        {
            'name':
            'Editor-TipTap',
            'unit':
            'ms_smallerIsBetter',
            'binBoundaries': [0.001, [1, 1000000.0, 100]],
            'diagnostics': {
                'benchmarks': '9ef9da4a-3b79-4574-9603-bf9e2fe4bbe7',
                'benchmarkDescriptions': '69d51091-2cd5-4f0d-a87e-9c08254e1a16',
                'botId': '6214d34c-9200-4271-9cea-895f24e99d5c',
                'owners': '947e0701-97ef-4e0c-8bde-2b565aa1a087',
                'bugComponents': '93bae44b-c29d-48d6-99fa-a7c6bc962751',
                'documentationLinks': 'f0a439d6-f0d2-4dcd-8bca-8f3912cd760f',
                'architectures': '454e29c4-57dd-4987-9e3e-792ec84f07ce',
                'osNames': 'dfb2f28d-fb32-49f2-b011-1d26853a6d6c',
                'osVersions': '84d184f8-cfb0-4929-994b-85f908b91839',
                'osDetailedVersions': '6e8ba79c-9588-426f-92cf-a3ab803bcbba',
                'benchmarkStart': 'b5c3d0fa-e3dd-4e32-ab41-31c8a3ab0e2a',
                'stories': '6903cc09-ca9e-47a4-82a0-d77d214d37fc',
                'storysetRepeats': 'a7f54f55-870b-4b76-bb87-d64df4bf3e7b',
                'storyTags': 'f0bb92d7-5ab2-42ed-ad7f-d79018aa3b60',
                'traceStart': 'cf09d1a1-8b3b-4d3c-bee6-b8d341d5e31e',
                'traceUrls': '8563ece0-740a-44c0-ae72-418721ee56d8',
            },
            'sampleValues': [
                172.90000000130385,
                135.3999999994412,
                137.10000000149012,
                133,
                134.5,
                130.90000000037253,
                137.70000000018626,
                158.1000000005588,
                133.09999999962747,
                134.19999999925494,
            ],
            'running': [
                10,
                172.90000000130385,
                4.942674143981753,
                140.6900000002235,
                130.90000000037253,
                1406.9000000022352,
                1683.429000105582,
            ],
            'allBins': {
                '57': [5],
                '58': [4],
                '59': [1]
            },
        },
    ]
    key = {
        'improvement_direction': 'down',
        'unit': 'ms_smallerIsBetter',
        'test': 'Editor-TipTap',
        'subtest_1': 'Speedometer3',
    }
    measurement = {
        'measurements': {
            'stat': [
                {
                    'value': 'value',
                    'measurement': 140.6900000002235
                },
                {
                    'value': 'error',
                    'measurement': 13.676537086499565
                },
                {
                    'value': 'count',
                    'measurement': 10.0
                },
                {
                    'value': 'max',
                    'measurement': 172.90000000130385
                },
                {
                    'value': 'min',
                    'measurement': 130.90000000037253
                },
                {
                    'value': 'sum',
                    'measurement': 1406.9000000022352
                },
            ]
        },
        'key': key,
    }
    links = {
        'Build Page':
        ('https://ci.chromium.org/ui/p/chrome/builders/ci/win-10-perf/39376'),
        'OS Version':
        '10.0.19045',
        'Bot Id':
        'win-222-e504',
        'Chromium Commit Position':
        'https://crrev.com/1405221',
        'V8': ('https://chromium.googlesource.com/v8/v8/+/'
               '60e67b93909a1c858305b27111d9988f94fff0f8'),
        'WebRTC': ('https://webrtc.googlesource.com/src/+/'
                   '1e19045eaa63d00a3b4017fd43c5b502c6ed73a2'),
        'Tracing uri': ('https://storage.cloud.google.com/'
                        'chrome-telemetry-output/20251112T181417_63858/'
                        'rendering.desktop/balls_css_transition_all_properties/'
                        'retry_0/trace.pb'),
    }
    expected = {
        'version': 1,
        'git_hash': 'CP:1405221',
        'key': {
            'master': 'ChromiumPerf',
            'bot': 'win-10-perf',
            'benchmark': 'speedometer3',
        },
        'results': [measurement],
        'links': links,
    }
    details = json_util.PerfBuilderDetails(
        bot='win-10-perf',
        builder_page=(
            'https://ci.chromium.org/ui/p/chrome/builders/ci/win-10-perf/39376'
        ),
        git_hash='CP:1405221',
        chromium_commit_position='https://crrev.com/1405221',
        master='ChromiumPerf',
        v8_git_hash=('https://chromium.googlesource.com/v8/v8/+/'
                     '60e67b93909a1c858305b27111d9988f94fff0f8'),
        webrtc_git_hash=('https://webrtc.googlesource.com/src/+/'
                         '1e19045eaa63d00a3b4017fd43c5b502c6ed73a2'),
    )
    with self.subTest(name='no_synthetic_measurements'):
      agent = json_util.JsonUtil(generate_synthetic_measurements=False)
      agent.add(result2_json)
      got = agent.process(details)
      self.assertEqual(len(got['results']), 1)
      self.assertDictEqual(got, expected)

    with self.subTest(name='no_synthetic_measurements_with_benchmark_name'):
      expected2 = copy.deepcopy(expected)
      expected2['key']['benchmark'] = 'speedometer3_modified'
      agent = json_util.JsonUtil(generate_synthetic_measurements=False)
      agent.add(result2_json)
      got = agent.process(details, benchmark_name='speedometer3_modified')
      self.assertDictEqual(got, expected2)

    with self.subTest(name='generate_synthetic_measurements'):
      mock_hist_helpers.ShouldGenerateStatistics.return_value = True
      agent = json_util.JsonUtil(generate_synthetic_measurements=True)
      agent.add(result2_json)
      synthetic_measurements_1 = {
          'measurements': {
              'stat': [
                  {
                      'value': 'value',
                      'measurement': 140.6900000002235
                  },
              ]
          },
          'key': {
              'improvement_direction': 'down',
              'unit': 'ms_smallerIsBetter',
              'test': 'Editor-TipTap_avg',
              'subtest_1': 'Speedometer3',
          },
      }
      synthetic_measurements_2 = {
          'measurements': {
              'stat': [
                  {
                      'value': 'value',
                      'measurement': 130.90000000037253
                  },
              ]
          },
          'key': {
              'improvement_direction': 'down',
              'unit': 'ms_smallerIsBetter',
              'test': 'Editor-TipTap_min',
              'subtest_1': 'Speedometer3',
          },
      }
      synthetic_measurements_3 = {
          'measurements': {
              'stat': [
                  {
                      'value': 'value',
                      'measurement': 172.90000000130385
                  },
              ]
          },
          'key': {
              'improvement_direction': 'down',
              'unit': 'ms_smallerIsBetter',
              'test': 'Editor-TipTap_max',
              'subtest_1': 'Speedometer3',
          },
      }
      synthetic_measurements_4 = {
          'measurements': {
              'stat': [
                  {
                      'value': 'value',
                      'measurement': 1406.9000000022352
                  },
              ]
          },
          'key': {
              'improvement_direction': 'down',
              'unit': 'ms_smallerIsBetter',
              'test': 'Editor-TipTap_sum',
              'subtest_1': 'Speedometer3',
          },
      }
      synthetic_measurements_5 = {
          'measurements': {
              'stat': [
                  {
                      'value': 'value',
                      'measurement': 10.0
                  },
              ]
          },
          'key': {
              'improvement_direction': 'up',
              'unit': 'unitless_biggerIsBetter',
              'test': 'Editor-TipTap_count',
              'subtest_1': 'Speedometer3',
          },
      }
      synthetic_measurements_6 = {
          'measurements': {
              'stat': [
                  {
                      'value': 'value',
                      'measurement': 13.676537086499565
                  },
              ]
          },
          'key': {
              'improvement_direction': 'down',
              'unit': 'ms_smallerIsBetter',
              'test': 'Editor-TipTap_std',
              'subtest_1': 'Speedometer3',
          },
      }
      expected['results'].extend([
          synthetic_measurements_1,
          synthetic_measurements_2,
          synthetic_measurements_3,
          synthetic_measurements_4,
          synthetic_measurements_5,
          synthetic_measurements_6])
      got = agent.process(details)
      self.assertDictEqual(got, expected)

    with self.subTest(name='generate_synthetic_measurements_with_subtest'):
      agent = json_util.JsonUtil(generate_synthetic_measurements=True)
      # modifies result2_json_copy to support subtest_1 and subtest_2
      result2_json_copy = copy.deepcopy(result2_json)
      for item in result2_json_copy:
        if 'diagnostics' in item:
          item['diagnostics'][
              'stories'] = '6903cc09-ca9e-47a4-82a0-d77d214d37fd'
          item['diagnostics'][
              'storyTags'] = 'f0bb92d7-5ab2-42ed-ad7f-d79018aa3b61'
      key['subtest_1'] = 'browse_news'
      key['subtest_2'] = 'browse_news_nytimes_2020'
      synthetic_measurements_1['key']['subtest_1'] = 'browse_news'
      synthetic_measurements_1['key']['subtest_2'] = 'browse_news_nytimes_2020'
      synthetic_measurements_2['key']['subtest_1'] = 'browse_news'
      synthetic_measurements_2['key']['subtest_2'] = 'browse_news_nytimes_2020'
      synthetic_measurements_3['key']['subtest_1'] = 'browse_news'
      synthetic_measurements_3['key']['subtest_2'] = 'browse_news_nytimes_2020'
      synthetic_measurements_4['key']['subtest_1'] = 'browse_news'
      synthetic_measurements_4['key']['subtest_2'] = 'browse_news_nytimes_2020'
      synthetic_measurements_5['key']['subtest_1'] = 'browse_news'
      synthetic_measurements_5['key']['subtest_2'] = 'browse_news_nytimes_2020'
      synthetic_measurements_6['key']['subtest_1'] = 'browse_news'
      synthetic_measurements_6['key']['subtest_2'] = 'browse_news_nytimes_2020'
      expected = {
          'version': 1,
          'git_hash': 'CP:1405221',
          'key': {
              'master': 'ChromiumPerf',
              'bot': 'win-10-perf',
              'benchmark': 'speedometer3',
          },
          'results': [
              measurement,
              synthetic_measurements_1,
              synthetic_measurements_2,
              synthetic_measurements_3,
              synthetic_measurements_4,
              synthetic_measurements_5,
              synthetic_measurements_6],
          'links': links,
      }
      agent.add(result2_json_copy)
      got = agent.process(details)
      self.assertDictEqual(got, expected)

  @parameterized.expand([
      (
          'empty_data',
          False,  # synthetic_measurements enabled in JsonUtil
          True,  # should_generate_stats (Mock return value)
          'some_benchmark',
          None,
          [],
      ),
      (
          'without_subtest',
          False,
          True,
          'some_benchmark',
          {
              ('abc', 'ms_smallerIsBetter', 'down'): [1, 2, 3],
          },
          [{
              'measurements': {
                  'stat': [
                      {
                          'value': 'value',
                          'measurement': 2.0
                      },
                      {
                          'value': 'error',
                          'measurement': 1.0
                      },
                      {
                          'value': 'count',
                          'measurement': 3.0
                      },
                      {
                          'value': 'max',
                          'measurement': 3.0
                      },
                      {
                          'value': 'min',
                          'measurement': 1.0
                      },
                      {
                          'value': 'sum',
                          'measurement': 6.0
                      },
                  ],
              },
              'key': {
                  'improvement_direction': 'down',
                  'unit': 'ms_smallerIsBetter',
                  'test': 'abc',
              },
          }],
      ),
      (
          'with_subtest',
          False,
          True,
          'some_benchmark',
          {
              ('abc', 'ms_smallerIsBetter', 'down', 'subtest', 'subtest2'): [
                  1,
                  2,
                  3,
              ],
          },
          [{
              'measurements': {
                  'stat': [
                      {
                          'value': 'value',
                          'measurement': 2.0
                      },
                      {
                          'value': 'error',
                          'measurement': 1.0
                      },
                      {
                          'value': 'count',
                          'measurement': 3.0
                      },
                      {
                          'value': 'max',
                          'measurement': 3.0
                      },
                      {
                          'value': 'min',
                          'measurement': 1.0
                      },
                      {
                          'value': 'sum',
                          'measurement': 6.0
                      },
                  ],
              },
              'key': {
                  'improvement_direction': 'down',
                  'unit': 'ms_smallerIsBetter',
                  'test': 'abc',
                  'subtest_1': 'subtest',
                  'subtest_2': 'subtest2',
              },
          }],
      ),
      (
          'without_subtest_with_synthetic_measurements',
          True,
          True,
          'some_benchmark',
          {
              ('abc', 'ms_smallerIsBetter', 'down'): [1, 2, 3],
          },
          [
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 2.0
                          },
                          {
                              'value': 'error',
                              'measurement': 1.0
                          },
                          {
                              'value': 'count',
                              'measurement': 3.0
                          },
                          {
                              'value': 'max',
                              'measurement': 3.0
                          },
                          {
                              'value': 'min',
                              'measurement': 1.0
                          },
                          {
                              'value': 'sum',
                              'measurement': 6.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 2.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc_avg',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 1.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc_min',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 3.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc_max',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 6.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc_sum',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 3.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'up',
                      'unit': 'unitless_biggerIsBetter',
                      'test': 'abc_count',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 1.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc_std',
                  },
              },
          ],
      ),
      (
          'with_subtest_with_synthetic_measurements',
          True,
          True,
          'some_benchmark',
          {
              ('abc', 'ms_smallerIsBetter', 'down', 'subtest', 'subtest2'): [
                  1,
                  2,
                  3,
              ],
          },
          [
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 2.0
                          },
                          {
                              'value': 'error',
                              'measurement': 1.0
                          },
                          {
                              'value': 'count',
                              'measurement': 3.0
                          },
                          {
                              'value': 'max',
                              'measurement': 3.0
                          },
                          {
                              'value': 'min',
                              'measurement': 1.0
                          },
                          {
                              'value': 'sum',
                              'measurement': 6.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc',
                      'subtest_1': 'subtest',
                      'subtest_2': 'subtest2',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 2.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc_avg',
                      'subtest_1': 'subtest',
                      'subtest_2': 'subtest2',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 1.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc_min',
                      'subtest_1': 'subtest',
                      'subtest_2': 'subtest2',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 3.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc_max',
                      'subtest_1': 'subtest',
                      'subtest_2': 'subtest2',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 6.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc_sum',
                      'subtest_1': 'subtest',
                      'subtest_2': 'subtest2',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 3.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'up',
                      'unit': 'unitless_biggerIsBetter',
                      'test': 'abc_count',
                      'subtest_1': 'subtest',
                      'subtest_2': 'subtest2',
                  },
              },
              {
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 1.0
                          },
                      ],
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'abc_std',
                      'subtest_1': 'subtest',
                      'subtest_2': 'subtest2',
                  },
              },
          ],
      ),
      (
          'synthetics_enabled_but_blocked_by_helper',
          True,  # generate_synthetic_measurements=True
          False,  # should_generate_stats=False (Mock override)
          'any_benchmark_name',
          {
              ('abc', 'ms_smallerIsBetter', 'down'): [1, 2, 3],
          },
          [{
              'measurements': {
                  'stat': [
                      {
                          'value': 'value',
                          'measurement': 2.0
                      },
                      {
                          'value': 'error',
                          'measurement': 1.0
                      },
                      {
                          'value': 'count',
                          'measurement': 3.0
                      },
                      {
                          'value': 'max',
                          'measurement': 3.0
                      },
                      {
                          'value': 'min',
                          'measurement': 1.0
                      },
                      {
                          'value': 'sum',
                          'measurement': 6.0
                      },
                  ],
              },
              'key': {
                  'improvement_direction': 'down',
                  'unit': 'ms_smallerIsBetter',
                  'test': 'abc',
              },
          }],
      ),
  ])
  @mock.patch('json_util.histogram_helpers')
  @mock.patch('json_util.json_constants')
  def test_measurement(self, _, synthetic_measurements, should_generate_stats,
                       benchmark_name, data, expected, mock_constants,
                       mock_hist_helpers):
    mock_json_constants(mock_constants)
    setattr(mock_hist_helpers, '_STATS_BLACKLIST',
            MOCK_JSON_CONSTANTS['STATS_BLOCKLIST'])
    mock_hist_helpers.ShouldGenerateStatistics.return_value = should_generate_stats

    instance = json_util.JsonUtil(
        generate_synthetic_measurements=synthetic_measurements)
    got = instance.measurements_from_results(data=data,
                                             benchmark_name=benchmark_name)
    self.assertEqual(expected, got)

  def test_key_from_builder_details(self):
    builder_details = json_util.PerfBuilderDetails(
        bot='win-10-perf',
        builder_page=(
            'https://ci.chromium.org/ui/p/chrome/builders/ci/win-10-perf/39376'
        ),
        git_hash='CP:1405221',
        chromium_commit_position='https://crrev.com/1405221',
        master='ChromiumPerf',
        v8_git_hash=('https://chromium.googlesource.com/v8/v8/+/'
                     '60e67b93909a1c858305b27111d9988f94fff0f8'),
        webrtc_git_hash=('https://webrtc.googlesource.com/src/+/'
                         '1e19045eaa63d00a3b4017fd43c5b502c6ed73a2'),
    )
    got = json_util.key_from_builder_details(builder_details=builder_details,
                                             benchmark_key='speedometer3')
    expected = {
        'master': 'ChromiumPerf',
        'bot': 'win-10-perf',
        'benchmark': 'speedometer3',
    }
    self.assertDictEqual(got, expected)

  def test_links_from_builder_details_no_benchmark_key(self):
    builder_details = json_util.PerfBuilderDetails(
        bot='win-10-perf',
        builder_page=(
            'https://ci.chromium.org/ui/p/chrome/builders/ci/win-10-perf/39376'
        ),
        git_hash='CP:1405221',
        chromium_commit_position='https://crrev.com/1405221',
        master='ChromiumPerf',
        v8_git_hash=('https://chromium.googlesource.com/v8/v8/+/'
                     '60e67b93909a1c858305b27111d9988f94fff0f8'),
        webrtc_git_hash=('https://webrtc.googlesource.com/src/+/'
                         '1e19045eaa63d00a3b4017fd43c5b502c6ed73a2'),
    )
    got = json_util.links_from_builder_details(
        builder_details=builder_details,
        bot_ids={'win-222-e504', 'win-223-e504', 'win-224-e504'},
        os_versions={'10.0.19045'},
        trace_urls=[])
    expected = {
        'Build Page':
        ('https://ci.chromium.org/ui/p/chrome/builders/ci/win-10-perf/39376'),
        'OS Version':
        '10.0.19045',
        'Bot Id':
        'win-222-e504, win-223-e504, win-224-e504',
        'Chromium Commit Position':
        'https://crrev.com/1405221',
        'V8': ('https://chromium.googlesource.com/v8/v8/+/'
               '60e67b93909a1c858305b27111d9988f94fff0f8'),
        'WebRTC': ('https://webrtc.googlesource.com/src/+/'
                   '1e19045eaa63d00a3b4017fd43c5b502c6ed73a2'),
    }
    self.assertDictEqual(got, expected)

  def test_perf_builder_details_from_build_properties(self):
    build_properties = {
        'builder_group': 'chromium.perf',
        'buildername': 'win-11-perf',
        'buildnumber': 9719,
        'git_cache_epoch': '1738403861',
        'got_angle_revision': '56d796a9f12a4bc2388f0fb37ba473eceefe4a8e',
        'got_dawn_revision': '2a3e80d33d4855c8ff8d01f1ba3b076b3088ffd1',
        'got_revision': 'bae4f05ce6002349a5b2ac01decb4ce27c47a30b',
        'got_revision_cp': 'refs/heads/main@{#1415171}',
        'got_src_internal_revision': 'f517862818abc1fed7ed8b3eeed9419f2ce17a71',
        'got_swiftshader_revision': '86cf34f50cbe5a9f35da7eedad0f4d4127fb8342',
        'got_v8_revision': '0f87a54dade4353b6ece1d7591ca8c66f90c1c93',
        'got_v8_revision_cp': 'refs/heads/13.4.114@{#1}',
        'got_webrtc_revision': '0533b5eafe69b744f10fa178f5a6f9657eaeeb25',
        'got_webrtc_revision_cp': 'refs/heads/main@{#43844}',
        'perf_dashboard_machine_group': 'ChromiumPerf'
    }
    got = json_util.perf_builder_details_from_build_properties(
        build_properties, 'win64-builder-perf', 'ChromiumPerf')
    expected = json_util.PerfBuilderDetails(
        bot='win64-builder-perf',
        builder_page=
        'https://ci.chromium.org/ui/p/chrome/builders/ci/win-11-perf/9719',
        git_hash='CP:1415171',
        chromium_commit_position='refs/heads/main@{#1415171}',
        master='ChromiumPerf',
        v8_git_hash='0f87a54dade4353b6ece1d7591ca8c66f90c1c93',
        webrtc_git_hash='0533b5eafe69b744f10fa178f5a6f9657eaeeb25',
    )
    self.assertEqual(got, expected)

  @parameterized.expand([
    (
        'empty_filename',
        {
            'buildername': 'win-11-perf',
            'buildnumber': 9719,
        },
        json_util.PerfBuilderDetails(
            bot='win64-builder-perf',
            master='ChromiumPerf',
            builder_page='http://foo.link',
            git_hash='CP:deadbeef',
            chromium_commit_position='refs/heads/master@{#1234}',
            v8_git_hash='beef1234',
            webrtc_git_hash='fee123',
        ),
        'benchmark.example',
        datetime.datetime(2024, 8, 29),
        '',
        ('ingest/2024/08/29/ChromiumPerf/'
         'win-11-perf/9719/benchmark.example'),
    ),
    (
        'non_empty_filename',
        {
            'buildername': 'win-11-perf',
            'buildnumber': 9719,
        },
        json_util.PerfBuilderDetails(
            bot='win64-builder-perf',
            master='ChromiumPerf',
            builder_page='http://foo.link',
            git_hash='CP:deadbeef',
            chromium_commit_position='refs/heads/master@{#1234}',
            v8_git_hash='beef1234',
            webrtc_git_hash='fee123',
        ),
        'benchmark.example',
        datetime.datetime(2024, 8, 29),
        'skia_results.json',
        ('ingest/2024/08/29/ChromiumPerf/'
         'win-11-perf/9719/benchmark.example/skia_results.json'),
    ),
  ])
  def test_get_gcs_prefix_path(
          self, _, build_properties, builder_details,
          benchmark_name, given_datetime, filename, expected):
    got = json_util.get_gcs_prefix_path(
        build_properties=build_properties,
        builder_details=builder_details,
        benchmark_name=benchmark_name,
        given_datetime=given_datetime,
        filename=filename)
    self.assertEqual(got, expected)

  @mock.patch('builtins.open', new_callable=mock.mock_open)
  def test_is_public_builder(self, mock_open):
    mock_open.return_value.__enter__.return_value.read.return_value = (
        '{"public_perf_builders": ["win-10-perf"]}')
    self.assertTrue(json_util.is_public_builder(''))
    self.assertTrue(json_util.is_public_builder('win-10-perf'))
    self.assertFalse(json_util.is_public_builder('win-11-perf'))

  @parameterized.expand([
      (
          'empty_master_name',
          'win-11-perf',
          '',
          False,
          False,
          [],
      ),
      (
          'empty_builder_name',
          '',
          'ChromiumPerf',
          False,
          False,
          [],
      ),
      (
          'public_builder',
          'win-10-perf',
          'ChromiumPerf',
          False,
          False,
          ['chrome-perf-public', 'chrome-perf-non-public'],
      ),
      (
          'public_builder_with_copy_to_experiment',
          'win-10-perf',
          'ChromiumPerf',
          False,
          True,
          ['chrome-perf-public', 'chrome-perf-non-public',
           'chrome-perf-experiment-non-public'],
      ),
      (
          'internal_builder',
          'win-11-perf',
          'ChromiumPerf',
          False,
          False,
          ['chrome-perf-non-public'],
      ),
      (
          'internal_builder_with_copy_to_experiment',
          'win-11-perf',
          'ChromiumPerf',
          False,
          True,
          ['chrome-perf-non-public'],
      ),
      (
          'another_internal_builder',
          'android-pixel6-perf',
          'ChromiumPerf',
          False,
          False,
          ['chrome-perf-non-public'],
      ),
      (
          'experiment_only',
          'win-11-perf',
          'ChromiumPerf',
          True,
          False,
          ['chrome-perf-experiment-non-public'],
      ),
  ])
  @mock.patch('json_util.json_constants')
  def test_gcs_buckets_from_builder_name(self, _, builder_name, master_name,
                                         experiment_only, copy_to_experiment,
                                         expected, mock_constants):
    mock_constants.REPOSITORY_PROPERTY_MAP = {
        'chromium': {
            'masters': ['ChromiumPerf'],
            'public_bucket_name': 'chrome-perf-public',
            'internal_bucket_name': 'chrome-perf-non-public',
        },
    }
    mock_constants.EXPERIMENT_GCS_BUCKET = 'chrome-perf-experiment-non-public'
    with mock.patch('builtins.open', new_callable=mock.mock_open) as mock_open:
      mock_open.return_value.__enter__.return_value.read.return_value = (
          '{"public_perf_builders": ["win-10-perf"]}')
      got = json_util.gcs_buckets_from_builder_name(
          builder_name=builder_name,
          master_name=master_name,
          experiment_only=experiment_only,
          public_copy_to_experiment=copy_to_experiment)
      self.assertEqual(got, expected)

  @parameterized.expand([
      (
          'default_up_on_empty_string',
          '',
          'up',
      ),
      (
          'foo_smallerIsBetter',
          'foo_smallerIsBetter',
          'down',
      ),
      (
          'smallerIsBetter',
          '_smallerIsBetter',
          'down',
      ),
      (
          'abc_smallerIsBetter',
          'abc_smallerIsBetter',
          'down',
      ),
      (
          'default_up',
          'foo_bar',
          'up',
      ),
      (
          'bar_biggerIsBetter',
          'bar_biggerIsBetter',
          'up',
      ),
  ])
  def test_get_improvement_direction(
      self, _, unit, expected):
    # pylint: disable=protected-access
    got= json_util._get_improvement_direction(unit)
    self.assertEqual(got, expected)


  @parameterized.expand([
      ('none', None, True),
      (
          'with_empty_results',
          {
              'version': 1,
              'git_hash': 'CP:1405221',
              'key': {
                  'master': 'ChromiumPerf',
                  'bot': 'win-10-perf',
                  'benchmark': 'speedometer3',
              },
              'results': [],
              'links': {
                  'Build Page':
                  ('https://ci.chromium.org/ui/p/chrome/builders/ci/win-10-perf/39376'
                   ),
                  'OS Version':
                  '10.0.19045',
                  'Bot Id':
                  'win-222-e504',
                  'Chromium Commit Position':
                  'https://crrev.com/1405221',
                  'V8': ('https://chromium.googlesource.com/v8/v8/+/'
                         '60e67b93909a1c858305b27111d9988f94fff0f8'),
                  'WebRTC': ('https://webrtc.googlesource.com/src/+/'
                             '1e19045eaa63d00a3b4017fd43c5b502c6ed73a2'),
              }
          },
          True,
      ),
      (
          'with_some_results',
          {
              'version':
              1,
              'git_hash':
              'CP:1405221',
              'key': {
                  'master': 'ChromiumPerf',
                  'bot': 'win-10-perf',
                  'benchmark': 'speedometer3',
              },
              'results': [{
                  'measurements': {
                      'stat': [
                          {
                              'value': 'value',
                              'measurement': 140.6900000002235
                          },
                          {
                              'value': 'error',
                              'measurement': 13.676537086499565
                          },
                          {
                              'value': 'count',
                              'measurement': 10.0
                          },
                          {
                              'value': 'max',
                              'measurement': 172.90000000130385
                          },
                          {
                              'value': 'min',
                              'measurement': 130.90000000037253
                          },
                          {
                              'value': 'sum',
                              'measurement': 1406.9000000022352
                          },
                      ]
                  },
                  'key': {
                      'improvement_direction': 'down',
                      'unit': 'ms_smallerIsBetter',
                      'test': 'Editor-TipTap',
                      'subtest_1': 'Speedometer3',
                  },
              }],
              'links': {
                  'Build Page':
                  ('https://ci.chromium.org/ui/p/chrome/builders/ci/win-10-perf/39376'
                   ),
                  'OS Version':
                  '10.0.19045',
                  'Bot Id':
                  'win-222-e504',
                  'Chromium Commit Position':
                  'https://crrev.com/1405221',
                  'V8': ('https://chromium.googlesource.com/v8/v8/+/'
                         '60e67b93909a1c858305b27111d9988f94fff0f8'),
                  'WebRTC': ('https://webrtc.googlesource.com/src/+/'
                             '1e19045eaa63d00a3b4017fd43c5b502c6ed73a2'),
              }
          },
          False,
      )
  ])
  def test_is_empty(self, _, data, expected):
    self.assertEqual(json_util.is_empty(data), expected)

  @mock.patch('json_util.extract_subtest_from_stories_tags')
  @mock.patch('json_util.histogram_helpers')
  @mock.patch('json_util.json_constants')
  def test_process_applies_logic_conditionally(self, mock_constants,
                                               mock_hist_helpers,
                                               mock_extractor):
    """
    CRITICAL: Verifies that story/tag processing is ONLY applied to
    non-summary results, matching the legacy pipeline's conditional logic.
    """
    mock_json_constants(mock_constants)
    setattr(mock_hist_helpers, '_STATS_BLACKLIST',
            MOCK_JSON_CONSTANTS['STATS_BLOCKLIST'])

    mock_hist_helpers.ShouldGenerateStatistics.return_value = True
    mock_extractor.return_value = ('sub1', 'sub2')

    agent = json_util.JsonUtil()
    # Create one non-summary item and one summary item.
    result2_json = [
        self._create_generic_set('s1', ['story1']),
        self._create_generic_set('t1', ['tag1']),
        self._create_sample_result('NonSummaryTest', [10], 's1', 't1'),
        self._create_summary_result('SummaryTest'),
    ]
    agent.add(result2_json)
    details = json_util.PerfBuilderDetails(bot='test-bot',
                                           master='TestMaster',
                                           git_hash='test-hash',
                                           builder_page='',
                                           chromium_commit_position='',
                                           v8_git_hash='',
                                           webrtc_git_hash='')

    result = agent.process(details)

    # Assert that the extraction logic was called only ONCE,
    # for the non-summary item.
    mock_extractor.assert_called_once()

    # Assert the final output contains ONLY the processed non-summary result.
    self.assertEqual(len(result['results']), 1)
    self.assertEqual(result['results'][0]['key']['test'], 'NonSummaryTest')

if __name__ == '__main__':
  unittest.main()
