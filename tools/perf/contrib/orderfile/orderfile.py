# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Orderfile Generation and Testing

This provides a suite of benchmarks for orderfile generation and testing based
on the mobile system health benchmarks.

The orderfile_generation.training benchmark is meant to be run with an
orderfile instrumented build to produce function usage profiles. It can be
invoked as follows:
    (CHROMIUM_OUTPUT_DIR=out/Instrumented; \
     ./tools/perf/run_benchmark --device=${YOUR_DEVICE_SN} --browser=exact \
        --browser-executable=${CHROMIUM_OUTPUT_DIR}/apks/Monochrome.apk \
        orderfile_generation.training)

The orderfile_generation.testing benchmark has a smaller test set whose
benchmarks are distinct from those in orderfile_generation.training. Run it as
follows, note the --orderfile-memory-optimization flag is necessary only with
legacy orderfile builds of chrome.
    (CHROMIUM_OUTPUT_DIR=out/Official; \
     ./tools/perf/run_benchmark --device=${YOUR_DEVICE_SN} --browser=exact \
        --browser-executable=${CHROMIUM_OUTPUT_DIR}/apks/Monochrome.apk \
        --extra-browser-args=--orderfile-memory-optimization \
        --results-label=${LABEL_CORRESPONDING_TO_YOUR_BUILD} \
        orderfile_generation.training)

The orderfile_generation.variation.* benchmarks use a smaller training set so
that several test set variations can be produced. They are run as above but
using the following benchmark names.
    orderfile_generation.variation.training
    orderfile_generation.variation.testing0
    orderfile_generation.variation.testing1
    orderfile_generation.variation.testing2

The orderfile_generation.debugging benchmark is a short benchmark of 3 stories
that is useful for debugging hardware and test setup problems.
"""

import random

from benchmarks import system_health
from page_sets.system_health import platforms
from page_sets.system_health import system_health_stories
from telemetry import benchmark
from telemetry import story
from telemetry.timeline import chrome_trace_category_filter
from telemetry.timeline import chrome_trace_config
from telemetry.web_perf import timeline_based_measurement


class OrderfileStorySet(story.StorySet):
  """User stories for orderfile generation.

  The run set specified in the constructor splits the stories into training and
  testing sets (or debugging, see the code for details).
  """
  # Run set names.
  TRAINING = 'training'
  TESTING = 'testing'
  DEBUGGING = 'debugging'

  _PLATFORM = 'mobile'

  _BLACKLIST = set([
      'background:news:nytimes',
      'background:tools:gmail',
      'browse:chrome:newtab',
      'browse:chrome:omnibox',
      'browse:news:cnn:2018',
      'browse:news:globo',
      'browse:news:toi',
      'browse:shopping:avito',
      'browse:shopping:flipkart',
      'long_running:tools:gmail-foreground',
      'browse:social:facebook',
      'browse:social:facebook_infinite_scroll',
      'browse:social:pinterest_infinite_scroll',
      'browse:social:tumblr_infinite_scroll',
      'browse:tech:discourse_infinite_scroll:2018',
      'load:media:soundcloud',
      'load:news:cnn',
      'load:news:washingtonpost',
      'load:tools:drive',
      'load:tools:gmail',
      'long_running:tools:gmail-background',
  ])

  # The random seed used for reproducible runs.
  SEED = 8675309

  # These defaults are current best practice for production orderfiles.
  DEFAULT_TRAINING = 25
  DEFAULT_TESTING = 8
  DEFAULT_VARIATIONS = 1

  # The number of variations to use with the variation benchmarks. If this is
  # changed, the number of OrderfileVariationTesting* classes declared below
  # should change as well.
  NUM_VARIATION_BENCHMARKS = 3

  def __init__(self, run_set, num_training=DEFAULT_TRAINING,
               num_testing=DEFAULT_TESTING, num_variations=DEFAULT_VARIATIONS,
               test_variation=0):
    """Create an orderfile training or testing benchmark set.

    Args:
      run_set: one of TRAINING, TESTING or DEBUGGING.
      num_training: the number of benchmarks to use for training.
      num_testing: the number of benchmarks to use in each test set.
      num_variations: the number of test set variations.
      test_variation: the test set variation to use.
    """
    super(OrderfileStorySet, self).__init__(
        archive_data_file=('../../page_sets/data/system_health_%s.json' %
                           self._PLATFORM),
        cloud_storage_bucket=story.PARTNER_BUCKET)

    assert self._PLATFORM in platforms.ALL_PLATFORMS, '{} not in {}'.format(
        self._PLATFORM, str(platforms.ALL_PLATFORMS))
    assert run_set in (self.TRAINING, self.TESTING, self.DEBUGGING)
    assert test_variation >= 0 and test_variation < num_variations

    self._run_set = run_set
    self._num_training = num_training
    self._num_testing = num_testing
    self._num_variations = num_variations
    self._test_variation = test_variation

    # We want the story selection to be consistent across runs.
    random.seed(self.SEED)

    for story_class in self.RunSetStories():
      # pylint: disable=E1102
      self.AddStory(story_class(self, take_memory_measurement=True))

  def RunSetStories(self):
    possible_stories = [
        s for s in system_health_stories.IterAllSystemHealthStoryClasses()
        if (s.NAME not in self._BLACKLIST and
            not s.ABSTRACT_STORY and
            self._PLATFORM in s.SUPPORTED_PLATFORMS)]
    assert (self._num_training + self._num_variations * self._num_testing
            <= len(possible_stories)), \
        'We only have {} stories to work with, but want {} + {}*{}'.format(
            len(possible_stories), self._num_training, self._num_variations,
            self._num_testing)

    if self._run_set == self.DEBUGGING:
      return random.sample(possible_stories, 3)

    random.shuffle(possible_stories)
    if self._run_set == self.TRAINING:
      return possible_stories[:self._num_training]
    elif self._run_set == self.TESTING:
      return possible_stories[
          (self._num_training + self._test_variation * self._num_testing):
          (self._num_training + (self._test_variation + 1) * self._num_testing)]
    assert False, 'Bad run set {}'.format(self._run_set)


class _OrderfileBenchmark(system_health.MobileMemorySystemHealth):
  """Base benchmark for orderfile generation."""
  STORY_RUN_SET = None  # Must be set in subclasses.

  def CreateStorySet(self, options):
    return OrderfileStorySet(run_set=self.STORY_RUN_SET)


# pylint: disable=R0901
@benchmark.Owner(emails=['mattcary@chromium.org'])
class OrderfileTraining(_OrderfileBenchmark):
  STORY_RUN_SET = OrderfileStorySet.TRAINING

  options = {'pageset_repeat': 2}

  @classmethod
  def Name(cls):
    return 'orderfile_generation.training'


# pylint: disable=R0901
@benchmark.Owner(emails=['mattcary@chromium.org'])
class OrderfileTesting(_OrderfileBenchmark):
  STORY_RUN_SET = OrderfileStorySet.TESTING

  options = {'pageset_repeat': 7}

  @classmethod
  def Name(cls):
    return 'orderfile_generation.testing'


class _OrderfileVariation(system_health.MobileMemorySystemHealth):
  """Orderfile generation with test set variations."""
  STORY_RUN_SET = None   # Must be set in all subclasses.
  TEST_VARIATION = 0     # Can be overridden testing subclasses.

  options = {'pageset_repeat': 7}

  def CreateStorySet(self, options):
    return OrderfileStorySet(
        run_set=self.STORY_RUN_SET,
        num_training=25, num_testing=8,
        num_variations=OrderfileStorySet.NUM_VARIATION_BENCHMARKS,
        test_variation=self.TEST_VARIATION)

  @classmethod
  def Name(cls):
    if cls.STORY_RUN_SET == OrderfileStorySet.TESTING:
      return 'orderfile_generation.variation.testing{}'.format(
          cls.TEST_VARIATION)
    elif cls.STORY_RUN_SET == OrderfileStorySet.TRAINING:
      return 'orderfile_generation.variation.training'
    assert False, 'Bad STORY_RUN_SET {}'.format(cls.STORY_RUN_SET)


# pylint: disable=R0901
@benchmark.Owner(emails=['mattcary@chromium.org'])
class OrderfileVariationTraining(_OrderfileVariation):
  STORY_RUN_SET = OrderfileStorySet.TRAINING


# pylint: disable=R0901
@benchmark.Owner(emails=['mattcary@chromium.org'])
class OrderfileVariationTesting0(_OrderfileVariation):
  STORY_RUN_SET = OrderfileStorySet.TESTING
  TEST_VARIATION = 0


# pylint: disable=R0901
@benchmark.Owner(emails=['mattcary@chromium.org'])
class OrderfileVariationTesting1(_OrderfileVariation):
  STORY_RUN_SET = OrderfileStorySet.TESTING
  TEST_VARIATION = 1


# pylint: disable=R0901
@benchmark.Owner(emails=['mattcary@chromium.org'])
class OrderfileVariationTesting2(_OrderfileVariation):
  STORY_RUN_SET = OrderfileStorySet.TESTING
  TEST_VARIATION = 2


# pylint: disable=R0901
@benchmark.Owner(emails=['mattcary@chromium.org'])
class OrderfileDebugging(_OrderfileBenchmark):
  """A very short benchmark for debugging metrics collection."""
  STORY_RUN_SET = OrderfileStorySet.DEBUGGING

  options = {'pageset_repeat': 1}

  @classmethod
  def Name(cls):
    return 'orderfile_generation.debugging'

@benchmark.Owner(emails=['mattcary@chromium.org'])
class OrderfileMemory(system_health.MobileMemorySystemHealth):
  """Benchmark for native code memory footprint evaluation."""
  class OrderfileMemoryStorySet(story.StorySet):
    _STORY_SET = set([
      'browse:news:cnn:2018',
      'browse:social:facebook'
    ])

    def __init__(self, platform, take_memory_measurement=True):
      super(OrderfileMemory.OrderfileMemoryStorySet, self).__init__(
          archive_data_file=('../../page_sets/data/system_health_%s.json' %
                             platform),
          cloud_storage_bucket=story.PARTNER_BUCKET)

      assert platform in platforms.ALL_PLATFORMS
      for story_class in system_health_stories.IterAllSystemHealthStoryClasses():
        if (story_class.ABSTRACT_STORY or
            platform not in story_class.SUPPORTED_PLATFORMS or
            story_class.NAME not in self._STORY_SET):
          continue
        self.AddStory(story_class(self, take_memory_measurement))


  def CreateStorySet(self, options):
    return self.OrderfileMemoryStorySet(platform=self.PLATFORM)

  def CreateCoreTimelineBasedMeasurementOptions(self):
    cat_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='-*,disabled-by-default-memory-infra')
    options = timeline_based_measurement.Options(cat_filter)
    # options.config.enable_android_graphics_memtrack = True
    options.SetTimelineBasedMetrics(['nativeCodeResidentMemoryMetric'])
    # Setting an empty memory dump config disables periodic dumps.
    options.config.chrome_trace_config.SetMemoryDumpConfig(
        chrome_trace_config.MemoryDumpConfig())
    return options

  @classmethod
  def Name(cls):
    return 'orderfile.memory_mobile'
