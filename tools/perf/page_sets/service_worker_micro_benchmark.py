# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page
from telemetry import story


class ServiceWorkerBenchmarkPage(page.Page):
  """Page for workload to measure some specific functions in JS"""

  def RunNavigateSteps(self, action_runner):
    super(ServiceWorkerBenchmarkPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition('window.done')


class ServiceWorkerMicroBenchmarkPageSet(story.StorySet):
  """Page set for micro benchmarking of each functions with ServiceWorker"""

  def __init__(self):
    super(ServiceWorkerMicroBenchmarkPageSet, self).__init__(
        archive_data_file='data/service_worker_micro_benchmark.json',
        cloud_storage_bucket=story.PUBLIC_BUCKET)

    # pylint: disable=line-too-long
    # The latest code of localhost:8091 is from:
    # https://github.com/amiq11/Service-Worker-Performance/tree/fix-flakyness
    # (rev: e6b3f604674209a30e4cf416a18cb8be3b991abd)
    # TODO(falken): House the code in GoogleChrome's GitHub repository.
    # pylint: enable=C0301
    # Why: to measure performance of many concurrent fetches
    self.AddStory(ServiceWorkerBenchmarkPage(
        'http://localhost:8091/index.html', self,
        make_javascript_deterministic=False,
        name='http://localhost:8091/index.html'))
