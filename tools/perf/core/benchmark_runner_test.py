# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import tempfile
import unittest
from unittest import mock

from telemetry import decorators
from telemetry.testing import test_stories
from telemetry.web_perf import timeline_based_measurement
from tracing.value.diagnostics import all_diagnostics
from tracing.value.diagnostics import reserved_infos
from tracing.value import histogram_set

from core import benchmark_runner
from core import perf_benchmark
from core import results_processor
from core import testing


def _FakeParseArgs(environment, args, results_arg_parser):
  del environment  # Unused.
  options, _ = results_arg_parser.parse_known_args(args)
  return options


class BenchmarkRunnerUnittest(unittest.TestCase):
  """Tests for benchmark_runner.main which mock out benchmark running."""

  def testMain_ReturnCode(self):
    """Test that benchmark_runner.main() respects return code from Telemetry."""
    config = mock.Mock()
    with mock.patch('core.benchmark_runner.command_line') as telemetry_cli:
      telemetry_cli.ParseArgs.side_effect = _FakeParseArgs
      telemetry_cli.RunCommand.return_value = 42

      # Note: We pass `--output-format none` and a non-existent output
      # dir to prevent the results processor from processing any results.
      return_code = benchmark_runner.main(config, [
          'run', 'some.benchmark', '--browser', 'stable',
          '--output-dir', '/does/not/exist', '--output-format', 'none'])
      self.assertEqual(return_code, 42)


class BenchmarkRunnerIntegrationTest(unittest.TestCase):
  """Integration tests for benchmark running and results processing.

  Note, however, no command line processing is tested.
  """

  def setUp(self):
    self.options = testing.GetRunOptions(
        output_dir=tempfile.mkdtemp())
    self.options.output_formats = ['histograms']
    results_processor.ProcessOptions(self.options)

  def tearDown(self):
    shutil.rmtree(self.options.output_dir)

  def assertHasDiagnostic(self, hist, diag_info, value=None):
    """Assert that a histogram is associated with the given diagnostic."""
    self.assertIn(diag_info.name, hist.diagnostics)
    diag = hist.diagnostics[diag_info.name]
    self.assertIsInstance(
        diag, all_diagnostics.GetDiagnosticClassForName(diag_info.type))
    if value is not None:
      # Assume we expect singleton GenericSet with the given value.
      self.assertEqual(len(diag), 1)
      self.assertEqual(next(iter(diag)), value)

  def RunBenchmark(self, benchmark_class):
    """Run a benchmark, process results, and return generated histograms."""
    # TODO(crbug.com/40636798): Ideally we should be able to just call
    # telemetry.command_line.RunCommand(self.options) with the right set
    # of options chosen. However, argument parsing and command running are
    # currently tangled in Telemetry. In particular the class property
    # Run._benchmark is not set when we skip argument parsing, and the Run.Run
    # method call fails. Simplify this when argument parsing and command
    # running are no longer intertwined like this.
    run_return_code = benchmark_class().Run(self.options)
    self.assertEqual(run_return_code, 0)

    process_return_code = results_processor.ProcessResults(self.options,
                                                           is_unittest=True)
    self.assertEqual(process_return_code, 0)

    histograms_file = os.path.join(self.options.output_dir, 'histograms.json')
    self.assertTrue(os.path.exists(histograms_file))

    with open(histograms_file) as f:
      dicts = json.load(f)
    histograms = histogram_set.HistogramSet()
    histograms.ImportDicts(dicts)
    return histograms

  @decorators.Disabled(
      'chromeos',  # TODO(crbug.com/40137013): Fix the test.
      'android-nougat',  # Flaky: https://crbug.com/1342706
      'mac')  # Failing: https://crbug.com/1370958
  def testTimelineBasedEndToEnd(self):
    class TestTimelineBasedBenchmark(perf_benchmark.PerfBenchmark):
      """A dummy benchmark that records a trace and runs sampleMetric on it."""
      def CreateCoreTimelineBasedMeasurementOptions(self):
        options = timeline_based_measurement.Options()
        options.config.enable_chrome_trace = True
        options.SetTimelineBasedMetrics(['consoleErrorMetric'])
        return options

      def CreateStorySet(self, _):
        def log_error(action_runner):
          action_runner.EvaluateJavaScript('console.error("foo!")')

        return test_stories.SinglePageStorySet(
            name='log_error_story', story_run_side_effect=log_error)

      @classmethod
      def Name(cls):
        return 'test_benchmark.timeline_based'

    histograms = self.RunBenchmark(TestTimelineBasedBenchmark)

    # Verify that the injected console.log error was counted by the metric.
    hist = histograms.GetHistogramNamed('console:error:js')
    self.assertEqual(hist.average, 1)
    self.assertHasDiagnostic(hist, reserved_infos.BENCHMARKS,
                             'test_benchmark.timeline_based')
    self.assertHasDiagnostic(hist, reserved_infos.STORIES, 'log_error_story')
    self.assertHasDiagnostic(hist, reserved_infos.STORYSET_REPEATS, 0)
    self.assertHasDiagnostic(hist, reserved_infos.TRACE_START)
