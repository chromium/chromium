# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

from telemetry import decorators
from telemetry.testing import options_for_unittests

RUNNER_SCRIPTS_DIR = os.path.join(os.path.dirname(__file__),
                                  '..', '..', 'testing', 'scripts')
sys.path.append(RUNNER_SCRIPTS_DIR)
import run_performance_tests  # pylint: disable=wrong-import-position,import-error


class ScriptsSmokeTest(unittest.TestCase):

  perf_dir = os.path.dirname(__file__)

  def setUp(self):
    self.options = options_for_unittests.GetCopy()

  def RunPerfScript(self, args, env=None):
    # TODO(crbug.com/40636798): Switch all clients to pass a list of args rather
    # than a string which we may not be parsing correctly.
    if not isinstance(args, list):
      args = args.split(' ')
    proc = subprocess.Popen([sys.executable] + args, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, cwd=self.perf_dir,
                            env=env)
    stdout = proc.communicate()[0]
    return_code = proc.returncode
    return return_code, stdout.decode('utf-8')

  def testRunBenchmarkHelp(self):
    return_code, stdout = self.RunPerfScript('run_benchmark --help')
    self.assertEqual(return_code, 0, stdout)
    self.assertIn('usage: run_benchmark', stdout)

  @decorators.Disabled('chromeos')  # crbug.com/754913
  def testRunBenchmarkListBenchmarks(self):
    cmdline = [
        'run_benchmark', 'list', '--browser', self.options.browser_type,
        '--chrome-root',
        os.path.abspath(self.options.chrome_root)
    ]
    if self.options.browser_type == 'exact':
      # If we're running with an exact browser and it was not specified with
      # an absolute path, then there's no guarantee that we can actually find it
      # now, so make the test a no-op.
      if not os.path.isabs(self.options.browser_executable):
        return
      cmdline.extend(['--browser-executable', self.options.browser_executable])
    return_code, stdout = self.RunPerfScript(cmdline)
    if sys.version_info.major == 3:
      self.assertRegex(stdout, r'Available benchmarks .*? are:')
    else:
      # TODO: (crbug/1342770) clean up after python migration is done.
      self.assertRegexpMatches(stdout, r'Available benchmarks .*? are:')  # pylint: disable=deprecated-method
    self.assertEqual(return_code, 0)

  def testRunBenchmarkRunListsOutBenchmarks(self):
    return_code, stdout = self.RunPerfScript('run_benchmark run')
    self.assertIn('Pass --browser to list benchmarks', stdout)
    self.assertNotEqual(return_code, 0)

  def testRunBenchmarkRunNonExistingBenchmark(self):
    return_code, stdout = self.RunPerfScript('run_benchmark foo')
    self.assertIn('no such benchmark: foo', stdout)
    self.assertNotEqual(return_code, 0)

  def testRunRecordWprHelp(self):
    return_code, stdout = self.RunPerfScript('record_wpr')
    self.assertEqual(return_code, 0, stdout)
    self.assertIn('optional arguments:', stdout)

  @decorators.Disabled('chromeos')  # crbug.com/814068
  def testRunRecordWprList(self):
    return_code, stdout = self.RunPerfScript('record_wpr --list-benchmarks')
    # TODO(nednguyen): Remove this once we figure out why importing
    # small_profile_extender fails on Android dbg.
    # crbug.com/561668
    if 'ImportError: cannot import name small_profile_extender' in stdout:
      self.skipTest('small_profile_extender is missing')
    self.assertEqual(return_code, 0, stdout)

  @decorators.Disabled('chromeos')  # crbug.com/754913
  def testRunPerformanceTestsTelemetry_end2end(self):
    tempdir = tempfile.mkdtemp()
    benchmarks = ['dummy_benchmark.stable_benchmark_1',
                  'dummy_benchmark.noisy_benchmark_1']
    cmdline = ('../../testing/scripts/run_performance_tests.py '
               '../../tools/perf/run_benchmark '
               '--benchmarks=%s '
               '--browser=%s '
               '--chrome-root=%s '
               '--isolated-script-test-also-run-disabled-tests '
               '--isolated-script-test-output=%s' %
               (','.join(benchmarks), self.options.browser_type,
                os.path.abspath(self.options.chrome_root),
                os.path.join(tempdir, 'output.json')))
    if self.options.browser_type == 'exact':
      # If the path to the browser executable is not absolute, there is no
      # guarantee that we can actually find it at this point, so no-op the
      # test.
      if not os.path.isabs(self.options.browser_executable):
        return
      cmdline += ' --browser-executable=%s' % self.options.browser_executable
    return_code, stdout = self.RunPerfScript(cmdline)
    self.assertEqual(return_code, 0, stdout)
    try:
      with open(os.path.join(tempdir, 'output.json')) as f:
        test_results = json.load(f)
        self.assertIsNotNone(
            test_results, 'json_test_results should be populated: ' + stdout)
        benchmarks_run = [str(b) for b in test_results['tests'].keys()]
        self.assertEqual(sorted(benchmarks_run), sorted(benchmarks))
        story_runs = test_results['num_failures_by_type']['PASS']
        self.assertEqual(
            story_runs, 2,
            'Total runs should be 2 since each benchmark has one story.')
      for benchmark in benchmarks:
        with open(os.path.join(tempdir, benchmark, 'test_results.json')) as f:
          test_results = json.load(f)
          self.assertIsNotNone(
              test_results, 'json_test_results should be populated: ' + stdout)
        with open(os.path.join(tempdir, benchmark, 'perf_results.json')) as f:
          perf_results = json.load(f)
          self.assertIsNotNone(
              perf_results, 'json perf results should be populated: ' + stdout)
    except IOError as e:
      self.fail('json_test_results should be populated: ' + stdout + str(e))
    except AssertionError as e:
      self.fail('Caught assertion error: ' + str(e) + 'With stdout: ' + stdout)
    finally:
      shutil.rmtree(tempdir)

  @decorators.Enabled('linux')  # Testing platform-independent code.
  def testRunPerformanceTestsTelemetry_NoTestResults(self):
    """Test that test results output gets returned for complete failures."""
    tempdir = tempfile.mkdtemp()
    benchmarks = ['benchmark1', 'benchmark2']
    return_code, stdout = self.RunPerfScript(
        '../../testing/scripts/run_performance_tests.py '
        '../../tools/perf/testdata/fail_and_do_nothing '
        '--benchmarks=%s '
        '--browser=%s '
        '--isolated-script-test-output=%s' % (
            ','.join(benchmarks),
            self.options.browser_type,
            os.path.join(tempdir, 'output.json')
        ))
    self.assertNotEqual(return_code, 0)
    try:
      with open(os.path.join(tempdir, 'output.json')) as f:
        test_results = json.load(f)
        self.assertIsNotNone(
            test_results, 'json_test_results should be populated: ' + stdout)
        self.assertTrue(
            test_results['interrupted'],
            'if the benchmark does not populate test results, then we should '
            'populate it with a failure.')
      for benchmark in benchmarks:
        with open(os.path.join(tempdir, benchmark, 'test_results.json')) as f:
          test_results = json.load(f)
          self.assertIsNotNone(
              test_results, 'json_test_results should be populated: ' + stdout)
          self.assertTrue(
              test_results['interrupted'],
              'if the benchmark does not populate test results, then we should '
              'populate it with a failure.')
    except IOError as e:
      self.fail('json_test_results should be populated: ' + stdout + str(e))
    finally:
      shutil.rmtree(tempdir)

  # Android: crbug.com/932301
  # ChromeOS: crbug.com/754913
  # Windows: crbug.com/1024767
  # Linux: crbug.com/1024767
  # all: Disabled everywhere because the smoke test shard map
  # needed to be changed to fix crbug.com/1024767.
  @decorators.Disabled('all')
  def testRunPerformanceTestsTelemetrySharded_end2end(self):
    tempdir = tempfile.mkdtemp()
    env = os.environ.copy()
    env['GTEST_SHARD_INDEX'] = '0'
    env['GTEST_TOTAL_SHARDS'] = '2'
    return_code, stdout = self.RunPerfScript(
        '../../testing/scripts/run_performance_tests.py '
        '../../tools/perf/run_benchmark '
        '--test-shard-map-filename=smoke_test_benchmark_shard_map.json '
        '--browser=%s '
        '--run-ref-build '
        '--isolated-script-test-filter=dummy_benchmark.noisy_benchmark_1/'
        'dummy_page.html::dummy_benchmark.stable_benchmark_1/dummy_page.html '
        '--isolated-script-test-repeat=2 '
        '--isolated-script-test-also-run-disabled-tests '
        '--isolated-script-test-output=%s' % (
            self.options.browser_type,
            os.path.join(tempdir, 'output.json')
        ), env=env)
    test_results = None
    try:
      self.assertEqual(return_code, 0)
      expected_benchmark_folders = (
          'dummy_benchmark.stable_benchmark_1',
          'dummy_benchmark.stable_benchmark_1.reference',
          'dummy_gtest')
      with open(os.path.join(tempdir, 'output.json')) as f:
        test_results = json.load(f)
      self.assertIsNotNone(
          test_results, 'json_test_results should be populated.')
      test_runs = test_results['num_failures_by_type']['PASS']
      # 1 gtest runs (since --isolated-script-test-repeat doesn't work for gtest
      # yet) plus 2 dummy_benchmark runs = 3 runs.
      self.assertEqual(
          test_runs, 3, '--isolated-script-test-repeat=2 should work.')
      for folder in expected_benchmark_folders:
        with open(os.path.join(tempdir, folder, 'test_results.json')) as f:
          test_results = json.load(f)
        self.assertIsNotNone(
            test_results, 'json test results should be populated.')
        test_repeats = test_results['num_failures_by_type']['PASS']
        if 'dummy_gtest' not in folder:  # Repeats don't work for gtest yet.
          self.assertEqual(
              test_repeats, 2, '--isolated-script-test-repeat=2 should work.')
        with open(os.path.join(tempdir, folder, 'perf_results.json')) as f:
          perf_results = json.load(f)
        self.assertIsNotNone(
            perf_results, 'json perf results should be populated.')
    except Exception as exc:
      logging.error(
          'Failed with error: %s\nOutput from run_performance_tests.py:\n\n%s',
          exc, stdout)
      if test_results is not None:
        logging.error(
            'Got test_results: %s\n', json.dumps(test_results, indent=2))
      raise
    finally:
      shutil.rmtree(tempdir)

  def RunGtest(self, generate_trace):
    tempdir = tempfile.mkdtemp()
    benchmark = 'dummy_gtest'
    return_code, stdout = self.RunPerfScript(
        '../../testing/scripts/run_performance_tests.py ' +
        ('../../tools/perf/run_gtest_benchmark.py ' if generate_trace else '') +
        os.path.join('..', '..', 'tools', 'perf', 'testdata',
                     'dummy_gtest') +
        (' --use-gtest-benchmark-script --output-format=histograms'
             if generate_trace else '') +
        ' --non-telemetry=true '
        '--this-arg=passthrough '
        '--argument-to-check-that-arguments-work '
        '--gtest-benchmark-name dummy_gtest '
        '--isolated-script-test-output=%s' % (
            os.path.join(tempdir, 'output.json')
        ))
    try:
      self.assertEqual(return_code, 0, stdout)
    except AssertionError:
      try:
        with open(os.path.join(tempdir, benchmark, 'benchmark_log.txt')) as fh:
          print(fh.read())
      # pylint: disable=bare-except
      except:
        # pylint: enable=bare-except
        pass
      raise
    try:
      with open(os.path.join(tempdir, 'output.json')) as f:
        test_results = json.load(f)
        self.assertIsNotNone(
            test_results, 'json_test_results should be populated: ' + stdout)
      with open(os.path.join(tempdir, benchmark, 'test_results.json')) as f:
        test_results = json.load(f)
        self.assertIsNotNone(
            test_results, 'json_test_results should be populated: ' + stdout)
      with open(os.path.join(tempdir, benchmark, 'perf_results.json')) as f:
        perf_results = json.load(f)
        self.assertIsNotNone(
            perf_results, 'json perf results should be populated: ' + stdout)
    except IOError as e:
      self.fail('json_test_results should be populated: ' + stdout + str(e))
    finally:
      shutil.rmtree(tempdir)

  # Windows: ".exe" is auto-added which breaks Windows.
  # ChromeOS: crbug.com/754913.
  @decorators.Disabled('win', 'chromeos')
  def testRunPerformanceTestsGtest_end2end(self):
    self.RunGtest(generate_trace=False)

  # Windows: ".exe" is auto-added which breaks Windows.
  # ChromeOS: crbug.com/754913.
  @decorators.Disabled('win', 'chromeos')
  def testRunPerformanceTestsGtestTrace_end2end(self):
    self.RunGtest(generate_trace=True)

  def testRunPerformanceTestsShardedArgsParser(self):
    options = run_performance_tests.parse_arguments([
        '../../tools/perf/run_benchmark', '-v', '--browser=release_x64',
        '--upload-results', '--run-ref-build',
        '--test-shard-map-filename=win-10-perf_map.json',
        '--assert-gpu-compositing',
        r'--isolated-script-test-output=c:\a\b\c\output.json',
        r'--isolated-script-test-perf-output=c:\a\b\c\perftest-output.json',
        '--passthrough-arg=--a=b',
    ])
    self.assertIn('--assert-gpu-compositing', options.passthrough_args)
    self.assertIn('--browser=release_x64', options.passthrough_args)
    self.assertIn('-v', options.passthrough_args)
    self.assertIn('--a=b', options.passthrough_args)
    self.assertEqual(options.executable, '../../tools/perf/run_benchmark')
    self.assertEqual(options.isolated_script_test_output,
                     r'c:\a\b\c\output.json')

  def testRunPerformanceTestsTelemetryCommandGenerator_ReferenceBrowserComeLast(self):
    """This tests for crbug.com/928928."""
    options = run_performance_tests.parse_arguments([
        '../../tools/perf/run_benchmark', '--browser=release_x64',
        '--run-ref-build',
        '--test-shard-map-filename=win-10-perf_map.json',
        r'--isolated-script-test-output=c:\a\b\c\output.json',
    ])
    self.assertIn('--browser=release_x64', options.passthrough_args)
    command = run_performance_tests.TelemetryCommandGenerator(
        'fake_benchmark_name', options, is_reference=True).generate(
            'fake_output_dir')
    original_browser_arg_index = command.index('--browser=release_x64')
    reference_browser_arg_index = command.index('--browser=reference')
    self.assertTrue(reference_browser_arg_index > original_browser_arg_index)

  def testRunPerformanceTestsTelemetryCommandGenerator_StorySelectionConfig_Unabridged(self):
    options = run_performance_tests.parse_arguments([
        '../../tools/perf/run_benchmark', '--browser=release_x64',
        '--run-ref-build',
        r'--isolated-script-test-output=c:\a\b\c\output.json',
    ])
    story_selection_config = {
        'abridged': False,
        'begin': 1,
        'end': 5,
    }
    command = run_performance_tests.TelemetryCommandGenerator(
        'fake_benchmark_name', options, story_selection_config).generate(
            'fake_output_dir')
    self.assertNotIn('--run-abridged-story-set', command)
    self.assertIn('--story-shard-begin-index=1', command)
    self.assertIn('--story-shard-end-index=5', command)

  def testRunPerformanceTestsTelemetryCommandGenerator_StorySelectionConfig_Abridged(self):
    options = run_performance_tests.parse_arguments([
        '../../tools/perf/run_benchmark', '--browser=release_x64',
        '--run-ref-build',
        r'--isolated-script-test-output=c:\a\b\c\output.json',
    ])
    story_selection_config = {
        'abridged': True,
    }
    command = run_performance_tests.TelemetryCommandGenerator(
        'fake_benchmark_name', options, story_selection_config).generate(
            'fake_output_dir')
    self.assertIn('--run-abridged-story-set', command)

  def testRunPerformanceTestsGtestArgsParser(self):
    options = run_performance_tests.parse_arguments([
        'media_perftests',
        '--non-telemetry=true',
        '--single-process-tests',
        '--test-launcher-retry-limit=0',
        '--isolated-script-test-filter=*::-*_unoptimized::*_unaligned::'
        '*unoptimized_aligned',
        '--gtest-benchmark-name',
        'media_perftests',
        '--isolated-script-test-output=/x/y/z/output.json',
    ])
    self.assertIn('--single-process-tests', options.passthrough_args)
    self.assertIn('--test-launcher-retry-limit=0', options.passthrough_args)
    self.assertEqual(options.executable, 'media_perftests')
    self.assertEqual(options.isolated_script_test_output, r'/x/y/z/output.json')

  def testRunPerformanceTestsExecuteGtest_OSError(self):
    class FakeCommandGenerator(object):
      def __init__(self):
        self.executable_name = 'binary_that_doesnt_exist'
        self.ignore_shard_env_vars = False

      def generate(self, unused_path):
        return [self.executable_name]

    tempdir = tempfile.mkdtemp()
    try:
      fake_command_generator = FakeCommandGenerator()
      output_paths = run_performance_tests.OutputFilePaths(
          tempdir, 'fake_gtest')
      output_paths.SetUp()
      return_code = run_performance_tests.execute_gtest_perf_test(
          fake_command_generator, output_paths, is_unittest=True)
      self.assertEqual(return_code, 1)
      with open(output_paths.test_results) as fh:
        json_test_results = json.load(fh)
      self.assertGreater(json_test_results['num_failures_by_type']['FAIL'], 0)
    finally:
      shutil.rmtree(tempdir)
