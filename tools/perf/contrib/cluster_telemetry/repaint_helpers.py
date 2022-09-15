# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import legacy_page_test


def Repaint(action_runner, mode='viewport', width=None, height=None):
  action_runner.WaitForJavaScriptCondition(
    'document.readyState == "complete"', timeout=90)
  # Rasterize only what's visible.
  action_runner.ExecuteJavaScript(
      'chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent();')

  args = {}
  args['mode'] = mode
  if width:
    args['width'] = width
  if height:
    args['height'] = height

  # Enqueue benchmark
  action_runner.ExecuteJavaScript("""
      window.benchmark_results = {};
      window.benchmark_results.id =
          chrome.gpuBenchmarking.runMicroBenchmark(
              "invalidation_benchmark",
              function(value) {},
              {{ args }}
          );
      """,
      args=args)

  micro_benchmark_id = action_runner.EvaluateJavaScript(
      'window.benchmark_results.id')
  if not micro_benchmark_id:
    raise legacy_page_test.MeasurementFailure(
        'Failed to schedule invalidation_benchmark.')

  with action_runner.CreateInteraction('Repaint'):
    action_runner.RepaintContinuously(seconds=5)

  action_runner.ExecuteJavaScript("""
      window.benchmark_results.message_handled =
          chrome.gpuBenchmarking.sendMessageToMicroBenchmark(
                {{ micro_benchmark_id }}, {
                  "notify_done": true
                });
      """,
      micro_benchmark_id=micro_benchmark_id)


def WaitThenRepaint(action_runner):
  action_runner.Wait(2)
  Repaint(action_runner)
