#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys
import tempfile
import json
import unittest

from core import path_util
sys.path.insert(1, path_util.GetTelemetryDir())
sys.path.insert(
    1, os.path.join(path_util.GetTelemetryDir(), 'third_party', 'mock'))

from telemetry import decorators

import mock

import process_perf_results as ppr_module


class _FakeLogdogStream(object):

  def write(self, data):
    del data  # unused

  def close(self):
    pass

  def get_viewer_url(self):
    return 'http://foobar.not.exit'


class ProcessPerfResultsIntegrationTest(unittest.TestCase):
  def setUp(self):
    self.test_dir = tempfile.mkdtemp()
    self.output_json = os.path.join(self.test_dir, 'output.json')
    self.service_account_file = os.path.join(
        self.test_dir, 'fake_service_account.json')
    with open(self.service_account_file, 'w') as f:
      json.dump([1,2,3,4], f)
    self.task_output_dir = os.path.join(
        os.path.dirname(__file__), 'testdata', 'task_output_dir')

    m1 = mock.patch(
        'process_perf_results.logdog_helper.text',
        return_value = 'http://foo.link')
    m1.start()
    self.addCleanup(m1.stop)

    m2 = mock.patch(
        'process_perf_results.logdog_helper.open_text',
        return_value=_FakeLogdogStream())
    m2.start()
    self.addCleanup(m2.stop)

    m3 = mock.patch('core.results_dashboard.SendResults')
    m3.start()
    self.addCleanup(m3.stop)


  def tearDown(self):
    shutil.rmtree(self.test_dir)

  @decorators.Disabled('chromeos')  # crbug.com/865800
  @decorators.Disabled('win')  # crbug.com/860677, mock doesn't integrate well
                               # with multiprocessing on Windows.
  def testIntegration(self):
    build_properties = json.dumps({
        'perf_dashboard_machine_group': 'test-builder',
        'buildername': 'test-builder',
        'buildnumber': 777,
        'got_v8_revision': 'beef1234',
        'got_revision_cp': 'refs/heads/master@{#1234}',
        'got_webrtc_revision': 'fee123',
        'git_revision': 'deadbeef',
        'buildbucket': r"""{"build":
            {"bucket": "master.tryserver.chromium.perf",
             "created_by": "user:foo",
             "created_ts": "1535490272757820",
             "id": "8936915467712010816",
             "project": "chrome",
             "lease_key": "461228535",
             "tags": ["builder:android-go-perf", "buildset:patch/1194825/3",
                      "cq_experimental:False",
                      "master:master.tryserver.chromium.perf",
                      "user_agent:cq"]}}"""
        })
    return_code, benchmark_upload_result_map = ppr_module.process_perf_results(
        self.output_json, configuration_name='test-builder',
        service_account_file = self.service_account_file,
        build_properties=build_properties,
        task_output_dir=self.task_output_dir,
        smoke_test_mode=False)

    self.assertEquals(return_code, 1)
    self.assertEquals(benchmark_upload_result_map,
        {
          "power.desktop.reference": True,
          "blink_perf.image_decoder": True,
          "octane.reference": True,
          "power.desktop": True,
          "speedometer-future": True,
          "blink_perf.owp_storage": True,
          "memory.desktop": True,
          "wasm": True,
          "dummy_benchmark.histogram_benchmark_1": True,
          "dummy_benchmark.histogram_benchmark_1.reference": True,
          "wasm.reference": True,
          "speedometer": True,
          "memory.long_running_idle_gmail_tbmv2": True,
          "v8.runtime_stats.top_25": True,
          "dummy_benchmark.noisy_benchmark_1": True,
          "blink_perf.svg": True,
          "v8.runtime_stats.top_25.reference": True,
          "jetstream.reference": True,
          "jetstream": True,
          "speedometer2-future.reference": True,
          "speedometer2-future": False,  # Only this fails due to malformed data
          "blink_perf.svg.reference": True,
          "blink_perf.image_decoder.reference": True,
          "power.idle_platform.reference": True,
          "power.idle_platform": True,
          "dummy_benchmark.noisy_benchmark_1.reference": True,
          "speedometer-future.reference": True,
          "memory.long_running_idle_gmail_tbmv2.reference": True,
          "memory.desktop.reference": True,
          "blink_perf.owp_storage.reference": True,
          "octane": True,
          "speedometer.reference": True
        })

if __name__ == '__main__':
  unittest.main()
