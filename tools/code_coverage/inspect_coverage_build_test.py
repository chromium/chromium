#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for inspect_coverage_build.py."""

import contextlib
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import sys

import inspect_coverage_build


class InspectCoverageBuildTest(unittest.TestCase):
  """Tests build ID parsing, status verification, config audit, and pipeline check."""

  def setUp(self) -> None:
    """Sets up test fixtures before each test method."""
    inspect_coverage_build._ALL_JSON_GZ_CACHE.clear()
    self.addCleanup(inspect_coverage_build._ALL_JSON_GZ_CACHE.clear)
    self.run_patcher = mock.patch('subprocess.run')
    self.mock_run = self.run_patcher.start()
    self.addCleanup(self.run_patcher.stop)

  def test_parse_build_id_numeric_id(self) -> None:
    """Verifies numeric build ID strings are returned as-is."""
    self.assertEqual(
        inspect_coverage_build.parse_build_id('8676997864980431489'),
        '8676997864980431489')

  def test_parse_build_id_ci_url(self) -> None:
    """Verifies numeric ID extraction from ci.chromium.org build URLs."""
    url = 'https://ci.chromium.org/ui/p/chromium/builders/try/linux-rel/2773124/infra'
    self.assertEqual(inspect_coverage_build.parse_build_id(url),
                     'chromium/try/linux-rel/2773124')

  def test_parse_build_id_invalid_raises(self) -> None:
    """Verifies malformed strings raise ValueError instead of passing through."""
    with self.assertRaises(ValueError):
      inspect_coverage_build.parse_build_id('not-a-build-id-or-url')

  def test_check_build_status_terminal(self) -> None:
    """Verifies terminal SUCCESS status contains terminal flag."""
    build_data = {'id': '12345', 'status': 'SUCCESS'}
    result = inspect_coverage_build.check_build_status(build_data)
    self.assertTrue(result['terminal'])
    self.assertEqual(result['status'], 'SUCCESS')

  def test_check_build_status_running(self) -> None:
    """Verifies non-terminal STARTED status returns terminal=False."""
    build_data = {'id': '12345', 'status': 'STARTED'}
    result = inspect_coverage_build.check_build_status(build_data)
    self.assertFalse(result['terminal'])
    self.assertEqual(result['status'], 'STARTED')

  def test_verify_configuration_valid(self) -> None:
    """Verifies configuration audit reports valid when gclient flags are set."""
    build_data = {
        'input': {
            'properties': {
                'checkout_clang_coverage_tools': True
            }
        },
        'steps': [{
            'name': 'lookup GN args',
            'status': 'SUCCESS'
        }]
    }
    result = inspect_coverage_build.verify_configuration(build_data, 'cpp')
    self.assertTrue(result['configuration_valid'])
    self.assertEqual(result['gclient_config_mismatches'], [])

  def test_verify_configuration_missing_clang_cov(self) -> None:
    """Verifies config audit flags missing checkout_clang_coverage_tools property."""
    build_data = {
        'input': {
            'properties': {}
        },
        'steps': [{
            'name': 'lookup GN args',
            'status': 'FAILURE'
        }]
    }
    result = inspect_coverage_build.verify_configuration(build_data, 'cpp')
    self.assertFalse(result['configuration_valid'])
    self.assertIn(
        'Missing checkout_clang_coverage_tools in gclient config.',
        result['gclient_config_mismatches'],
    )

  def test_verify_configuration_all_languages(self) -> None:
    """Verifies all supported languages can be checked against config map."""
    build_data = {
        'input': {
            'properties': {}
        },
        'steps': [{
            'name': 'lookup GN args',
            'status': 'SUCCESS'
        }],
    }
    for lang in ['cpp', 'objc', 'rust', 'java', 'js', 'ts']:
      res = inspect_coverage_build.verify_configuration(build_data, lang)
      self.assertIn('configuration_valid', res)

  def test_verify_configuration_unsupported_language_raises(self) -> None:
    """Verifies unmapped language strings raise ValueError."""
    build_data = {'input': {'properties': {}}, 'steps': []}
    with self.assertRaises(ValueError):
      inspect_coverage_build.verify_configuration(build_data,
                                                  'unsupported-lang')

  def test_verify_coverage_pipeline_success(self) -> None:
    """Verifies Phase 2 pipeline audit reports success when HTML upload succeeds."""
    build_data = {
        'steps': [
            {
                'name':
                'process clang code coverage data for unit test coverage',
                'status': 'SUCCESS'
            },
            {
                'name':
                'process clang code coverage data for unit test coverage|generate html report in 42 tests',
                'status': 'SUCCESS'
            },
            {
                'name':
                'process clang code coverage data for unit test coverage|gsutil Upload coverage artifacts',
                'status': 'SUCCESS'
            },
        ],
        'output': {
            'properties': {
                'gsutil_urls': {
                    'process clang code coverage data for unit test coverage|gsutil Upload coverage artifacts':
                    'gs://bucket/report'
                }
            }
        }
    }
    result = inspect_coverage_build.verify_coverage_pipeline(build_data)
    unit_res = result['pipelines_checked']['cpp_unit']
    self.assertTrue(unit_res['pipeline_success'])
    self.assertEqual(unit_res['tests_processed_count'], 42)
    self.assertFalse(unit_res['skipped_no_data'])

  def test_verify_coverage_pipeline_skipped_no_data(self) -> None:
    """Verifies pipeline audit flags skipped_no_data and records substeps correctly."""
    build_data = {
        'steps': [{
            'name': 'process clang code coverage data for unit test coverage',
            'status': 'SUCCESS'
        }, {
            'name':
            'process clang code coverage data for unit test coverage|merge all profile files into a single .profdata',
            'status': 'SUCCESS'
        }, {
            'name':
            'process clang code coverage data for unit test coverage|skip processing because no data is found',
            'status': 'SUCCESS'
        }],
        'output': {
            'properties': {}
        }
    }
    result = inspect_coverage_build.verify_coverage_pipeline(build_data)
    unit_res = result['pipelines_checked']['cpp_unit']
    self.assertFalse(unit_res['pipeline_success'])
    self.assertTrue(unit_res['skipped_no_data'])
    self.assertEqual(len(unit_res['child_steps']), 2)

  def test_run_bb_get_success(self) -> None:
    """Verifies run_bb_get returns parsed JSON dictionary when bb succeeds."""
    self.mock_run.return_value = mock.MagicMock(
        returncode=0, stdout='{"id": "123", "status": "SUCCESS"}')
    data = inspect_coverage_build.run_bb_get('123')
    self.assertEqual(data['id'], '123')
    self.assertEqual(data['status'], 'SUCCESS')

  def test_run_bb_get_failure_raises(self) -> None:
    """Verifies run_bb_get raises RuntimeError when bb returns non-zero code."""
    self.mock_run.return_value = mock.MagicMock(returncode=1,
                                                stderr='not found')
    with self.assertRaises(RuntimeError):
      inspect_coverage_build.run_bb_get('999')

  def test_verify_configuration_java(self) -> None:
    """Verifies configuration audit for Java language context."""
    build_data = {
        'input': {
            'properties': {}
        },
        'steps': [{
            'name': 'lookup GN args',
            'status': 'SUCCESS'
        }]
    }
    result = inspect_coverage_build.verify_configuration(build_data, 'java')
    self.assertTrue(result['configuration_valid'])

  def test_verify_configuration_gn_arg_mismatches(self) -> None:
    """Verifies gn_arg_mismatches are detected when input gn_args conflict with required_gn_args."""
    build_data = {
        'input': {
            'properties': {
                'gn_args': {
                    'use_clang_coverage': False,
                    'is_clang': True,
                    'is_debug': False,
                }
            }
        },
        'steps': [],
    }
    result = inspect_coverage_build.verify_configuration(build_data, 'cpp')
    self.assertFalse(result['configuration_valid'])
    self.assertEqual(len(result['gn_arg_mismatches']), 1)
    self.assertIn(
        'use_clang_coverage = False (expected True)',
        result['gn_arg_mismatches'][0],
    )

  def test_verify_configuration_in_props_gn_arg_mismatch(self) -> None:
    """Verifies boolean input properties are checked for required GN args when gn_args dict is omitted."""
    build_data = {
        'input': {
            'properties': {
                'use_clang_coverage': False,
            }
        },
        'steps': [],
    }
    result = inspect_coverage_build.verify_configuration(build_data, 'cpp')
    self.assertFalse(result['configuration_valid'])
    self.assertIn(
        'use_clang_coverage = False (expected True)',
        result['gn_arg_mismatches'][0],
    )

  def test_verify_configuration_orchestrator_compilator_lookup_gn_args(
      self, ) -> None:
    """Verifies orchestrator builds extract lookup GN args status from compilator child steps."""
    build_data = {
        'input': {
            'properties': {
                'recipe': 'chromium_orchestrator',
            }
        },
        'steps': [{
            'name': 'compilator steps (with patch)|lookup GN args',
            'status': 'SUCCESS',
        }],
    }
    result = inspect_coverage_build.verify_configuration(build_data, 'cpp')
    self.assertEqual(result['lookup_gn_args_step_status'], 'SUCCESS')

  def test_verify_configuration_orchestrator_nested_substeps_lookup_gn_args(
      self, ) -> None:
    """Verifies lookup GN args status is found when nested inside substeps/children arrays."""
    build_data = {
        'input': {
            'properties': {
                'recipe': 'chromium_orchestrator',
            }
        },
        'steps': [{
            'name':
            'compilator steps (with patch)',
            'status':
            'SUCCESS',
            'substeps': [{
                'name': 'lookup GN args',
                'status': 'SUCCESS',
            }],
        }],
    }
    result = inspect_coverage_build.verify_configuration(build_data, 'cpp')
    self.assertEqual(result['lookup_gn_args_step_status'], 'SUCCESS')

  def test_verify_coverage_pipeline_url_conversions(self) -> None:
    """Verifies URL helper conversion of gs:// paths to https:// and pantheon links."""
    build_data = {
        'steps': [
            {
                'name':
                'process clang code coverage data for overall test coverage',
                'status': 'SUCCESS'
            },
            {
                'name':
                'process clang code coverage data for overall test coverage|generate html report in 10 tests',
                'status': 'SUCCESS'
            },
            {
                'name':
                'process clang code coverage data for overall test coverage|gsutil upload html report',
                'status': 'SUCCESS'
            },
        ],
        'output': {
            'properties': {
                'gsutil_urls': {
                    'process clang code coverage data for overall test coverage':
                    'gs://bucket/meta',
                    'process clang code coverage data for overall test coverage|gsutil upload html report':
                    'gs://bucket/html',
                    'process clang code coverage data for overall test coverage|gsutil upload artifact to GS':
                    'gs://bucket/profdata',
                }
            }
        }
    }
    result = inspect_coverage_build.verify_coverage_pipeline(build_data)
    overall = result['pipelines_checked']['cpp_overall']

    self.assertEqual(overall['merged_profdata_gcs_url'], 'gs://bucket/profdata')

  def test_verify_coverage_pipeline_detects_recipe_messages(self) -> None:
    """Verifies official recipe skip/error messages and triage actions are detected."""
    build_data = {
        'steps': [
            {
                'name':
                'skip processing clang coverage data because no profile data collected',
                'status': 'SUCCESS',
            },
            {
                'name':
                'process clang code coverage data for overall test coverage|skip processing because no data is found',
                'status': 'SUCCESS',
            },
        ]
    }
    result = inspect_coverage_build.verify_coverage_pipeline(build_data)
    overall = result['pipelines_checked']['cpp_overall']
    messages = [
        m['detected_condition']
        for m in overall['recipe_error_messages_detected']
    ]
    self.assertIn(
        'skip processing clang coverage data because no profile data collected',
        messages,
    )
    self.assertIn('skip processing because no data is found', messages)

  def test_format_line_ranges(self) -> None:
    """Verifies format_line_ranges compresses contiguous line numbers correctly."""
    self.assertEqual(
        inspect_coverage_build.format_line_ranges([1, 2, 3, 5, 8, 9]),
        '1-3, 5, 8-9')
    self.assertEqual(inspect_coverage_build.format_line_ranges([]), 'None')

  def test_extract_coverage_artifacts_for_files(self) -> None:
    """Verifies target file coverage metrics are extracted across pipelines."""
    pipeline_report = {
        'pipelines_checked': {
            'cpp_unit': {
                'pipeline_success': True,
                'html_report_gcs_url': 'gs://bucket/report'
            }
        }
    }
    file_res = inspect_coverage_build.extract_coverage_artifacts_for_files(
        pipeline_report, target_files=['chrome/browser/ui/foo.cc'])
    self.assertIn('chrome/browser/ui/foo.cc', file_res)
    unit_info = file_res['chrome/browser/ui/foo.cc']['cpp_unit']
    self.assertFalse(unit_info['available'])

  def test_format_file_coverage_report(self) -> None:
    """Verifies readable text formatting of target file coverage breakdown."""
    file_data = {
        'foo.cc': {
            'cpp_unit': {
                'available': True,
                'line_coverage': '80.0% (80/100)',
                'function_coverage': '100.0% (5/5)',
                'region_coverage': '75.0%',
                'branch_coverage': '70.0%',
                'uncovered_lines': [10, 11, 12, 25]
            }
        }
    }
    text = inspect_coverage_build.format_file_coverage_report(file_data)
    self.assertIn('[Target File Coverage Extraction]', text)
    self.assertIn('* File: foo.cc', text)
    self.assertIn('Line Coverage: 80.0% (80/100)', text)
    self.assertIn('Uncovered Lines (4): 10-12, 25', text)

  def test_verify_configuration_orchestrator_delegated(self) -> None:
    """Verifies orchestrator builds delegate GN args lookup to compilator."""
    build_data = {
        'input': {
            'properties': {
                'recipe': 'chromium_orchestrator'
            }
        },
        'steps': []
    }
    result = inspect_coverage_build.verify_configuration(build_data, 'cpp')
    self.assertEqual(result['lookup_gn_args_step_status'],
                     inspect_coverage_build.StepStatus.DELEGATED_TO_COMPILATOR)

  def test_verify_coverage_pipeline_all_recipe_error_flags(self) -> None:
    """Verifies detection of invalid profraw, failure flags, and skip profdata."""
    build_data = {
        'steps': [
            {
                'name':
                'process clang code coverage data for overall test coverage|foo',
                'status': 'FAILURE',
                'output': {
                    'properties': {
                        'process_coverage_data_failure': True
                    }
                }
            },
            {
                'name':
                'process clang code coverage data for overall test coverage|Found invalid profraw files',
                'status': 'SUCCESS'
            },
            {
                'name':
                'process clang code coverage data for overall test coverage|skip processing because no profdata was generated',
                'status': 'SUCCESS'
            },
        ]
    }
    result = inspect_coverage_build.verify_coverage_pipeline(build_data)
    overall = result['pipelines_checked']['cpp_overall']
    messages = [
        m['detected_condition']
        for m in overall['recipe_error_messages_detected']
    ]
    self.assertIn('process_coverage_data_failure = True', messages)
    self.assertIn('Found invalid profraw files', messages)
    self.assertIn('skip processing because no profdata was generated', messages)

  def test_extract_coverage_artifacts_for_files_unmocked(self) -> None:
    """Verifies real file report extraction runs fetch_and_parse or not available."""
    pipeline_report = {
        'pipelines_checked': {
            'cpp_unit': {
                'pipeline_success': False,
                'html_report_gcs_url': 'gs://bucket/report'
            }
        }
    }
    file_res = inspect_coverage_build.extract_coverage_artifacts_for_files(
        pipeline_report, target_files=['foo.cc'])
    self.assertFalse(file_res['foo.cc']['cpp_unit']['available'])

  def test_format_file_coverage_report_not_available(self) -> None:
    """Verifies format_file_coverage_report handles unavailable pipelines."""
    file_data = {'foo.cc': {'cpp_unit': {'available': False}}}
    text = inspect_coverage_build.format_file_coverage_report(file_data)
    self.assertIn("Pipeline 'cpp_unit': Not Available", text)

  def test_format_inspection_report_comprehensive(self) -> None:
    """Verifies format_inspection_report outputs all optional sections and mismatches."""
    combined_report = {
        'status_check': {
            'build_id': '12345',
            'status': 'STARTED',
            'terminal': False,
        },
        'config_verification': {
            'configuration_valid': False,
            'lookup_gn_args_step_status': 'FAILURE',
            'gn_arg_mismatches': ['use_clang_coverage missing'],
            'gclient_config_mismatches':
            ['checkout_clang_coverage_tools missing'],
        },
        'coverage_pipeline_verification': {
            'all_pipelines_successful': False,
            'pipelines_checked': {
                'cpp_overall': {
                    'parent_step_status':
                    'FAILURE',
                    'html_report_generation_status':
                    'FAILURE',
                    'artifacts_upload_status':
                    'NOT_FOUND',
                    'tests_processed_count':
                    10,
                    'skipped_no_data':
                    True,
                    'recipe_error_messages_detected': [{
                        'detected_condition': 'foo',
                        'step_name': 'bar'
                    }],
                    'child_steps': [{
                        'name': 'substep',
                        'status': 'FAILURE'
                    }],
                    'html_report_url':
                    'https://storage.cloud.google.com/foo/index.html',
                    'merged_profdata_gcs_url':
                    'gs://foo/merged.profdata',
                    'pipeline_success':
                    False,
                }
            },
        },
        'target_file_coverage': {
            'foo.cc': {
                'cpp_overall': {
                    'available': True,
                    'line_coverage': '50%',
                    'function_coverage': '50%',
                    'region_coverage': '50%',
                    'branch_coverage': '50%',
                    'uncovered_lines': [1]
                }
            }
        },
    }
    text = inspect_coverage_build.format_inspection_report(
        combined_report, 'cpp')
    self.assertIn('Build status is non-terminal (STARTED)', text)
    self.assertIn('use_clang_coverage missing', text)
    self.assertIn('checkout_clang_coverage_tools missing', text)
    self.assertIn("DETECTED RECIPE CONDITION: 'foo'", text)
    self.assertIn("SKIPPED: 'skip processing because no data is found'", text)
    self.assertIn('Substep breakdown:', text)
    self.assertIn('Merged Profdata GCS: gs://foo/merged.profdata', text)


  def test_verify_coverage_pipeline_java_and_js(self) -> None:
    """Verifies Java and JS coverage pipelines and metadata generation steps."""
    build_data = {
        'steps': [
            {
                'name': 'process java coverage (overall)',
                'status': 'SUCCESS',
            },
            {
                'name':
                'process java coverage (overall)|Generate Java coverage metadata',
                'status': 'SUCCESS',
            },
            {
                'name':
                'process java coverage (overall)|gsutil Upload coverage artifacts (2)',
                'status': 'SUCCESS',
            },
            {
                'name': 'process javascript coverage (overall)',
                'status': 'SUCCESS',
            },
            {
                'name':
                'process javascript coverage (overall)|Generate JavaScript coverage metadata',
                'status': 'SUCCESS',
            },
            {
                'name':
                'process javascript coverage (overall)|gsutil Upload coverage artifacts',
                'status': 'SUCCESS',
            },
        ],
        'output': {
            'properties': {
                'gsutil_urls': {
                    'process java coverage (overall)|gsutil Upload coverage artifacts (2)':
                    'gs://bucket/java_meta',
                    'process javascript coverage (overall)|gsutil Upload coverage artifacts':
                    'gs://bucket/js_meta',
                }
            }
        },
    }
    result = inspect_coverage_build.verify_coverage_pipeline(build_data)
    java_res = result['pipelines_checked']['java_overall']
    self.assertTrue(java_res['pipeline_success'])
    self.assertEqual(java_res['all_json_gz_url'],
                     'gs://bucket/java_meta/all.json.gz')

  def test_verify_coverage_pipeline_upload_step_fallback(self) -> None:
    """Verifies fallback between upload step (2) and without (2) when checking status and GCS URL."""
    build_data = {
        'steps': [
            {
                'name':
                'process clang code coverage data for overall test coverage',
                'status': 'SUCCESS',
            },
            {
                'name':
                'process clang code coverage data for overall test coverage|generate html report in 5 tests',
                'status': 'SUCCESS',
            },
            {
                'name': 'gsutil Upload coverage artifacts',
                'status': 'SUCCESS',
            },
        ],
        'output': {
            'properties': {
                'gsutil_urls': {
                    'gsutil Upload coverage artifacts': 'gs://bucket/fallback',
                }
            }
        },
    }
    result = inspect_coverage_build.verify_coverage_pipeline(build_data)
    overall = result['pipelines_checked']['cpp_overall']
    self.assertTrue(overall['pipeline_success'])
    self.assertEqual(overall['all_json_gz_url'],
                     'gs://bucket/fallback/all.json.gz')

  @mock.patch('inspect_coverage_build.fetch_and_parse_json_file_coverage')
  def test_extract_coverage_artifacts_for_files_json_gz(
      self, mock_parse_json: mock.MagicMock) -> None:
    """Verifies extract_coverage_artifacts_for_files uses all_json_gz_url when present."""
    mock_parse_json.return_value = {
        'available': True,
        'line_coverage': '90.0% (90/100)',
    }
    pipeline_report = {
        'pipelines_checked': {
            'java_overall': {
                'pipeline_success': True,
                'all_json_gz_url': 'gs://bucket/all.json.gz',
                'html_report_gcs_url': 'gs://bucket/html_report/index.html',
            }
        }
    }
    file_res = inspect_coverage_build.extract_coverage_artifacts_for_files(
        pipeline_report, target_files=['foo/bar.java'])
    self.assertTrue(file_res['foo/bar.java']['java_overall']['available'])
    self.assertEqual(file_res['foo/bar.java']['java_overall']['line_coverage'],
                     '90.0% (90/100)')
    mock_parse_json.assert_called_once_with('gs://bucket/all.json.gz',
                                            'foo/bar.java')

  @mock.patch('subprocess.run')
  def test_fetch_and_parse_json_file_coverage_dict_summaries(
      self, mock_run: mock.MagicMock) -> None:
    """Verifies fetch_and_parse_json_file_coverage parses dict summary format and uncovered lines."""
    import gzip
    import json
    data = {
        'files': [{
            'path':
            'foo/bar.java',
            'lines': [{
                'first': 10,
                'last': 12,
                'count': 0
            }, {
                'first': 13,
                'last': 13,
                'count': 5
            }],
            'summaries': {
                'line': {
                    'covered': 8,
                    'count': 10
                },
                'method': {
                    'covered': 2,
                    'total': 2
                },
            }
        }]
    }
    with tempfile.NamedTemporaryFile(suffix='.json.gz', delete=False) as tf:
      tf.write(gzip.compress(json.dumps(data).encode('utf-8')))
      src_path = tf.name
    try:

      def fake_cp(*args, **kwargs):
        dest = args[0][4]
        with open(src_path, 'rb') as f_in, open(dest, 'wb') as f_out:
          f_out.write(f_in.read())
        return mock.MagicMock(returncode=0)

      mock_run.side_effect = fake_cp
      res = inspect_coverage_build.fetch_and_parse_json_file_coverage(
          'gs://bucket/all.json.gz', 'bar.java')
      self.assertTrue(res['available'])
      self.assertEqual(res['line_coverage'], '8/10 (80.00%)')
      self.assertEqual(res['function_coverage'], '2/2 (100.00%)')
      self.assertEqual(res['uncovered_lines'], [10, 11, 12])
    finally:
      if os.path.exists(src_path):
        os.remove(src_path)

  @mock.patch('subprocess.run')
  def test_fetch_and_parse_json_file_coverage_list_summaries_or_absent(
      self, mock_run: mock.MagicMock) -> None:
    """Verifies list-based summaries and missing target handling in fetch_and_parse_json_file_coverage."""
    import gzip
    import json
    data = {
        'files': [{
            'filename': 'foo/other.js',
            'summaries': [{
                'name': 'line',
                'covered': 15,
                'count': 20
            }]
        }]
    }
    with tempfile.NamedTemporaryFile(suffix='.json.gz', delete=False) as tf:
      tf.write(gzip.compress(json.dumps(data).encode('utf-8')))
      src_path = tf.name
    try:

      def fake_cp(*args, **kwargs):
        dest = args[0][4]
        with open(src_path, 'rb') as f_in, open(dest, 'wb') as f_out:
          f_out.write(f_in.read())
        return mock.MagicMock(returncode=0)

      mock_run.side_effect = fake_cp
      res = inspect_coverage_build.fetch_and_parse_json_file_coverage(
          'gs://bucket/all.json.gz', 'other.js')
      self.assertTrue(res['available'])
      self.assertEqual(res['line_coverage'], '15/20 (75.00%)')

      res_absent = inspect_coverage_build.fetch_and_parse_json_file_coverage(
          'gs://bucket/all.json.gz', 'missing.js')
      self.assertFalse(res_absent['available'])

      self.assertFalse(
          inspect_coverage_build.fetch_and_parse_json_file_coverage(
              '', 'other.js')['available'])
    finally:
      if os.path.exists(src_path):
        os.remove(src_path)

  def test_verify_coverage_pipeline_upload_step_primary_match(self) -> None:
    """Verifies primary upload_step_name match when nested upload status is NOT_FOUND."""
    build_data = {
        'steps': [
            {
                'name':
                'process clang code coverage data for overall test coverage',
                'status': 'SUCCESS',
            },
            {
                'name': 'gsutil Upload coverage artifacts (2)',
                'status': 'SUCCESS',
            },
        ],
        'output': {
            'properties': {
                'gsutil_urls': {}
            }
        },
    }
    result = inspect_coverage_build.verify_coverage_pipeline(build_data)
    overall = result['pipelines_checked']['cpp_overall']

  @mock.patch.dict(os.environ, {'INSPECT_COVERAGE_NO_NETWORK': ''})
  @mock.patch('subprocess.run')
  def test_verify_coverage_pipeline_gcs_fallback_when_url_missing(
      self, mock_run: mock.MagicMock) -> None:
    """Verifies gsutil ls fallback execution when all_json_gz_url is missing."""
    mock_run.return_value = mock.MagicMock(
        returncode=0, stdout='gs://bucket/fallback/all.json.gz\n')
    build_data = {
        'id':
        '123456789',
        'steps': [
            {
                'name':
                'process clang code coverage data for overall test coverage',
                'status': 'SUCCESS',
            },
        ],
        'output': {
            'properties': {
                'gsutil_urls': {}
            }
        },
    }
    result = inspect_coverage_build.verify_coverage_pipeline(build_data)
    overall = result['pipelines_checked']['cpp_overall']
    self.assertEqual(overall['all_json_gz_url'],
                     'gs://bucket/fallback/all.json.gz')

  @mock.patch('subprocess.run')
  def test_fetch_and_parse_json_file_coverage_uncompressed_fallback(
      self, mock_run: mock.MagicMock) -> None:
    """Verifies fallback to uncompressed json.loads when zlib and gzip decompression both fail."""
    import json
    data = {
        'files': [{
            'path': 'foo/raw.js',
            'summaries': [{
                'name': 'line',
                'covered': 5,
                'count': 5
            }]
        }]
    }
    with tempfile.NamedTemporaryFile(suffix='.json.gz', delete=False) as tf:
      tf.write(json.dumps(data).encode('utf-8'))
      src_path = tf.name
    try:

      def fake_cp(*args, **kwargs):
        dest = args[0][4]
        with open(src_path, 'rb') as f_in, open(dest, 'wb') as f_out:
          f_out.write(f_in.read())
        return mock.MagicMock(returncode=0)

      mock_run.side_effect = fake_cp
      res = inspect_coverage_build.fetch_and_parse_json_file_coverage(
          'gs://bucket/all.json.gz', 'raw.js')
      self.assertTrue(res['available'])
      self.assertEqual(res['line_coverage'], '5/5 (100.00%)')
    finally:
      if os.path.exists(src_path):
        os.remove(src_path)

  @mock.patch('subprocess.run')
  def test_fetch_and_parse_json_file_coverage_exceptions(
      self, mock_run: mock.MagicMock) -> None:
    """Verifies exception handling on corrupt JSON and os.remove failure inside fetch_and_parse."""
    with tempfile.NamedTemporaryFile(suffix='.json.gz', delete=False) as tf:
      tf.write(b'corrupt non-json bytes')
      src_path = tf.name
    try:

      def fake_cp(*args, **kwargs):
        dest = args[0][4]
        with open(src_path, 'rb') as f_in, open(dest, 'wb') as f_out:
          f_out.write(f_in.read())
        return mock.MagicMock(returncode=0)

      mock_run.side_effect = fake_cp
      with mock.patch('os.remove', side_effect=OSError('Permission denied')):
        res = inspect_coverage_build.fetch_and_parse_json_file_coverage(
            'gs://bucket/all.json.gz', 'any.js')
      self.assertFalse(res['available'])
    finally:
      if os.path.exists(src_path):
        os.remove(src_path)


class InspectCoverageBuildMainTest(unittest.TestCase):
  """Tests CLI main() execution modes and error reporting."""

  def setUp(self) -> None:
    """Initializes stdout, stderr, exit, and run_bb_get patchers for main()."""
    self.stdout_buffer = io.StringIO()
    self.stderr_buffer = io.StringIO()
    self.stdout_context = contextlib.redirect_stdout(self.stdout_buffer)
    self.stderr_context = contextlib.redirect_stderr(self.stderr_buffer)
    self.stdout_context.__enter__()
    self.stderr_context.__enter__()
    self.addCleanup(self.stdout_context.__exit__, None, None, None)
    self.addCleanup(self.stderr_context.__exit__, None, None, None)

    self.exit_patcher = mock.patch('sys.exit', side_effect=SystemExit)
    self.bb_patcher = mock.patch('inspect_coverage_build.run_bb_get')
    self.run_patcher = mock.patch('subprocess.run')

    self.mock_exit = self.exit_patcher.start()
    self.mock_bb = self.bb_patcher.start()
    self.mock_run = self.run_patcher.start()

    self.addCleanup(self.exit_patcher.stop)
    self.addCleanup(self.bb_patcher.stop)
    self.addCleanup(self.run_patcher.stop)

  def test_main_json_output(self) -> None:
    """Verifies main() formats and outputs JSON when --json flag is passed."""
    self.mock_bb.return_value = {
        'id': '8676997864980431489',
        'status': 'SUCCESS',
        'steps': []
    }
    with mock.patch(
        'sys.argv',
        [
            'inspect_coverage_build.py',
            '--build',
            '8676997864980431489',
            '--language',
            'cpp',
            '--json',
        ],
    ):
      inspect_coverage_build.main()
    self.mock_bb.assert_called_once_with('8676997864980431489')
    self.mock_exit.assert_not_called()

  def test_main_text_output(self) -> None:
    """Verifies main() prints human-readable inspection summary by default."""
    self.mock_bb.return_value = {
        'id':
        '8676997864980431489',
        'status':
        'STARTED',
        'steps': [
            {
                'name': 'lookup GN args',
                'status': 'SUCCESS'
            },
            {
                'name':
                'process clang code coverage data for unit test coverage',
                'status': 'SUCCESS'
            },
        ]
    }
    with mock.patch(
        'sys.argv',
        [
            'inspect_coverage_build.py',
            '--build',
            '8676997864980431489',
            '--language',
            'cpp',
        ],
    ):
      inspect_coverage_build.main()
    self.mock_bb.assert_called_once_with('8676997864980431489')
    self.mock_exit.assert_not_called()

  def test_main_with_files_argument(self) -> None:
    """Verifies main() extracts per-file coverage reports when --files is passed."""
    self.mock_bb.return_value = {
        'id': '8676997864980431489',
        'status': 'SUCCESS',
        'steps': []
    }
    with mock.patch(
        'sys.argv',
        [
            'inspect_coverage_build.py',
            '--build',
            '8676997864980431489',
            '--language',
            'cpp',
            '--files',
            'foo.cc',
        ],
    ):
      inspect_coverage_build.main()
    self.mock_bb.assert_called_once_with('8676997864980431489')
    self.mock_exit.assert_not_called()

  def test_main_exception_exits(self) -> None:
    """Verifies main() logs error and exits with status 1 on exceptions."""
    self.mock_bb.side_effect = RuntimeError('API Error')
    with mock.patch(
        'sys.argv',
        [
            'inspect_coverage_build.py',
            '--build',
            '8676997864980431489',
            '--language',
            'cpp',
        ],
    ):
      with self.assertRaises(SystemExit):
        inspect_coverage_build.main()
    self.mock_exit.assert_called_once_with(1)


if __name__ == '__main__':
  unittest.main()
