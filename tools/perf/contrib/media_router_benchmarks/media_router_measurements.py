# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contrib.media_router_benchmarks import media_router_cpu_memory_metric
from telemetry.page import legacy_page_test


class MediaRouterCPUMemoryTest(legacy_page_test.LegacyPageTest):
  """Performs a measurement of Media Route CPU/memory usage."""

  def __init__(self):
    super(MediaRouterCPUMemoryTest, self).__init__()
    self._metric = media_router_cpu_memory_metric.MediaRouterCPUMemoryMetric()

  def ValidateAndMeasurePage(self, page, tab, results):
    self._metric.AddResults(tab, results)
