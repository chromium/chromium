# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run all system health stories used by system health benchmarks.

Only memory benchmarks are used when running these stories to make the total
cycle time manageable. Other system health benchmarks should be using the same
stories as memory ones, only with fewer actions (no memory dumping).
"""

import collections
import unittest

from chrome_telemetry_build import chromium_config

from core import perf_benchmark
from core import results_processor
from core import testing

from telemetry import decorators
from telemetry.testing import progress_reporter

from py_utils import discover
from py_utils import tempfile_ext

from benchmarks import system_health


def GetSystemHealthBenchmarksToSmokeTest():
  sh_benchmark_classes = discover.DiscoverClassesInModule(
      system_health, perf_benchmark.PerfBenchmark,
      index_by_class_name=True).values()
  return list(b for b in sh_benchmark_classes if
              b.Name().startswith('system_health.memory'))


_DISABLED_TESTS = frozenset({
  # crbug.com/983326 - flaky.
  'system_health.memory_desktop/browse_accessibility:media:youtube',

  # crbug.com/878390 - These stories are already covered by their 2018 or
  # 2019 versions and will later be removed.
  'system_health.memory_mobile/browse:tech:discourse_infinite_scroll',
  'system_health.memory_mobile/browse:shopping:amazon',
  'system_health.memory_mobile/browse:social:facebook_infinite_scroll',
  'system_health.memory_mobile/browse:social:instagram',
  'system_health.memory_mobile/browse:news:reddit',
  'system_health.memory_mobile/browse:social:tumblr_infinite_scroll',
  'system_health.memory_mobile/browse:social:twitter',
  'system_health.memory_mobile/browse:tools:maps',
  'system_health.memory_mobile/browse:news:cnn',
  'system_health.memory_mobile/browse:news:washingtonpost',
  'system_health.memory_mobile/browse:media:youtube',
  'system_health.memory_mobile/load:media:facebook_photos',
  'system_health.memory_mobile/load:news:cnn',
  'system_health.memory_mobile/load:news:nytimes',
  'system_health.memory_mobile/load:news:qq',
  'system_health.memory_mobile/load:news:reddit',
  'system_health.memory_mobile/load:news:washingtonpost',
  'system_health.memory_mobile/load:tools:stackoverflow',
  'system_health.memory_desktop/load_accessibility:shopping:amazon',
  'system_health.memory_desktop/browse_accessibility:tech:codesearch',
  'system_health.memory_desktop/load_accessibility:media:wikipedia',
  'system_health.memory_desktop/browse:tech:discourse_infinite_scroll',
  'system_health.memory_desktop/browse:tools:maps',
  'system_health.memory_desktop/browse:social:facebook_infinite_scroll',
  'system_health.memory_desktop/browse:social:tumblr_infinite_scroll',
  'system_health.memory_desktop/browse:news:flipboard',
  'system_health.memory_desktop/browse:search:google',
  'system_health.memory_desktop/browse:news:hackernews',
  'system_health.memory_desktop/load:search:amazon',
  'system_health.memory_desktop/load:news:bbc',
  'system_health.memory_desktop/load:news:hackernews',
  'system_health.memory_desktop/load:social:instagram',
  'system_health.memory_desktop/load:news:reddit',
  'system_health.memory_desktop/load:search:taobao',
  'system_health.memory_desktop/multitab:misc:typical24',
  'system_health.memory_desktop/browse:news:reddit',
  'system_health.memory_desktop/browse:media:tumblr',
  'system_health.memory_desktop/browse:social:twitter_infinite_scroll',
  'system_health.memory_mobile/load:social:twitter',
  'system_health.memory_desktop/load:social:vk',

  # crbug.com/637230
  'system_health.memory_desktop/browse:news:cnn',
  # Permenently disabled from smoke test for being long-running.
  'system_health.memory_mobile/long_running:tools:gmail-foreground',
  'system_health.memory_mobile/long_running:tools:gmail-background',
  'system_health.memory_desktop/long_running:tools:gmail-foreground',
  'system_health.memory_desktop/long_running:tools:gmail-background',

  # crbug.com/769263
  'system_health.memory_desktop/play:media:soundcloud',

  # crbug.com/
  'system_health.memory_desktop/browse:news:nytimes',

  # crbug.com/688190
  'system_health.memory_mobile/browse:news:washingtonpost',

  # crbug.com/696824
  'system_health.memory_desktop/load:news:qq',
  # crbug.com/893615
  # DESKTOP:
  'system_health.memory_desktop/browse:media:pinterest',
  'system_health.memory_desktop/browse:media:youtube',
  'system_health.memory_desktop/browse:search:google_india',
  'system_health.memory_desktop/load:games:alphabetty',
  'system_health.memory_desktop/load:games:bubbles',
  'system_health.memory_desktop/load:games:miniclip',
  'system_health.memory_desktop/load:games:spychase',
  'system_health.memory_desktop/load:media:flickr',
  'system_health.memory_desktop/load:media:google_images',
  'system_health.memory_desktop/load:media:imgur',
  'system_health.memory_desktop/load:media:soundcloud',
  'system_health.memory_desktop/load:media:youtube',
  'system_health.memory_desktop/load:news:cnn',
  'system_health.memory_desktop/load:news:wikipedia',
  'system_health.memory_desktop/load:search:baidu',
  'system_health.memory_desktop/load:search:ebay',
  'system_health.memory_desktop/load:search:google',
  'system_health.memory_desktop/load:search:yahoo',
  'system_health.memory_desktop/load:search:yandex',
  'system_health.memory_desktop/load:tools:stackoverflow',
  'system_health.memory_mobile/load:media:soundcloud',
  # MOBILE:
  'system_health.memory_mobile/load:games:bubbles',
  'system_health.memory_mobile/load:games:spychase',
  'system_health.memory_mobile/load:media:flickr',
  'system_health.memory_mobile/load:media:google_images',
  'system_health.memory_mobile/load:media:imgur',
  'system_health.memory_mobile/load:media:youtube',
  'system_health.memory_mobile/load:news:wikipedia',
  'system_health.memory_mobile/load:search:baidu',
  'system_health.memory_mobile/load:search:ebay',
  'system_health.memory_mobile/load:search:google',
  'system_health.memory_mobile/load:search:yahoo',
  'system_health.memory_mobile/load:search:yandex',

  # crbug.com/698006
  'system_health.memory_desktop/load:tools:drive',
  'system_health.memory_desktop/load:tools:gmail',

  # crbug.com/725386
  'system_health.memory_desktop/browse:social:twitter',

  # crbug.com/816482
  'system_health.memory_desktop/load:news:nytimes',

  # crbug.com/885320
  'system_health.memory_desktop/browse:search:google:2018',

  # crbug.com/893615
  'system_health.memory_desktop/multitab:misc:typical24:2018',

  # crbug.com/903849
  'system_health.memory_mobile/browse:news:cnn:2018',

  # crbug.com/937006
  'system_health.memory_mobile/browse:news:toi',

  # crbug.com/978358
  'system_health.memory_desktop/browse:news:flipboard:2018',

  # crbug.com/1008001
  'system_health.memory_desktop/browse:tools:sheets:2019',
  'system_health.memory_desktop/browse:tools:maps:2019',

  # crbug.com/1014661
  'system_health.memory_desktop/browse:social:tumblr_infinite_scroll:2018'
  'system_health.memory_desktop/browse:search:google_india:2018'

  # The following tests are disabled because they are disabled on the perf
  # waterfall (using tools/perf/expectations.config) on one platform or another.
  # They may run fine on the CQ, but it isn't worth the bot time to run them.
  # [
  # crbug.com/799106
  'system_health.memory_desktop/browse:media:flickr_infinite_scroll'
  # crbug.com/836407
  'system_health.memory_desktop/browse:tools:maps'
  # crbug.com/924330
  'system_health.memory_desktop/browse:media:pinterest:2018'
  # crbug.com/899887
  'system_health.memory_desktop/browse:social:facebook_infinite_scroll:2018'
  # crbug.com/649392
  'system_health.memory_desktop/play:media:google_play_music'
  # crbug.com/934885
  'system_health.memory_desktop/load_accessibility:media:wikipedia:2018'
  # crbug.com/942952
  'system_health.memory_desktop/browse:news:hackernews:2018',
  # crbug.com/992436
  'system_health.memory_desktop/browse:social:twitter:2018'
  # ]
})


# We want to prevent benchmarks from accidentally trying to upload too much
# data to the chrome perf dashboard. So the smoke tests below cap the max
# number of values that each story tested would produce when running on the
# waterfall.
MAX_VALUES_PER_TEST_CASE = 1000


def _GenerateSmokeTestCase(benchmark_class, story_to_smoke_test):

  # NOTE TO SHERIFFS: DO NOT DISABLE THIS TEST.
  #
  # This smoke test dynamically tests all system health user stories. So
  # disabling it for one failing or flaky benchmark would disable a much
  # wider swath of coverage  than is usally intended. Instead, if a test is
  # failing, disable it by putting it into the _DISABLED_TESTS list above.
  @decorators.Disabled('chromeos')  # crbug.com/351114
  def RunTest(self):
    class SinglePageBenchmark(benchmark_class):  # pylint: disable=no-init
      def CreateStorySet(self, options):
        # pylint: disable=super-on-old-class
        story_set = super(SinglePageBenchmark, self).CreateStorySet(options)
        stories_to_remove = [s for s in story_set.stories if s !=
                             story_to_smoke_test]
        for s in stories_to_remove:
          story_set.RemoveStory(s)
        assert story_set.stories
        return story_set

    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      # Set the benchmark's default arguments.
      options = GenerateBenchmarkOptions(
          output_dir=temp_dir,
          benchmark_cls=SinglePageBenchmark)
      simplified_test_name = self.id().replace(
          'benchmarks.system_health_smoke_test.SystemHealthBenchmarkSmokeTest.',
          '')
      # Sanity check to ensure that that substring removal was effective.
      assert len(simplified_test_name) < len(self.id())

      if (simplified_test_name in _DISABLED_TESTS and
          not options.run_disabled_tests):
        self.skipTest('Test is explicitly disabled')
      single_page_benchmark = SinglePageBenchmark()
      return_code = single_page_benchmark.Run(options)
      # TODO(crbug.com/1019139): Make 111 be the exit code that means
      # "no stories were run.".
      if return_code in (-1, 111):
        self.skipTest('The benchmark was not run.')
      self.assertEqual(
          return_code, 0,
          msg='Benchmark run failed: %s' % benchmark_class.Name())
      return_code = results_processor.ProcessResults(options)
      self.assertEqual(
          return_code, 0,
          msg='Result processing failed: %s' % benchmark_class.Name())

  # We attach the test method to SystemHealthBenchmarkSmokeTest dynamically
  # so that we can set the test method name to include
  # '<benchmark class name>/<story name>'.
  test_method_name = '%s/%s' % (
      benchmark_class.Name(), story_to_smoke_test.name)

  class SystemHealthBenchmarkSmokeTest(unittest.TestCase):
    pass

  setattr(SystemHealthBenchmarkSmokeTest, test_method_name, RunTest)

  return SystemHealthBenchmarkSmokeTest(methodName=test_method_name)


def GenerateBenchmarkOptions(output_dir, benchmark_cls):
  options = testing.GetRunOptions(
      output_dir=output_dir, benchmark_cls=benchmark_cls,
      environment=chromium_config.GetDefaultChromiumConfig())
  options.pageset_repeat = 1  # For smoke testing only run each page once.
  options.output_formats = ['histograms']
  options.max_values_per_test_case = MAX_VALUES_PER_TEST_CASE

  # Enable browser logging in the smoke test only. Hopefully, this will detect
  # all crashes and hence remove the need to enable logging in actual perf
  # benchmarks.
  options.browser_options.logging_verbosity = 'non-verbose'
  options.target_platforms = benchmark_cls.GetSupportedPlatformNames(
      benchmark_cls.SUPPORTED_PLATFORMS)
  return options


def load_tests(loader, standard_tests, pattern):
  del loader, standard_tests, pattern  # unused
  suite = progress_reporter.TestSuite()
  benchmark_classes = GetSystemHealthBenchmarksToSmokeTest()
  assert benchmark_classes, 'This list should never be empty'
  names_stories_to_smoke_tests = []
  for benchmark_class in benchmark_classes:

    # HACK: these options should be derived from GetRunOptions which are
    # the resolved options from run_tests' arguments. However, options is only
    # parsed during test time which happens after load_tests are called.
    # Since none of our system health benchmarks creates stories based on
    # command line options, it should be ok to pass options=None to
    # CreateStorySet.
    stories_set = benchmark_class().CreateStorySet(options=None)

    # Prefetch WPR archive needed by the stories set to avoid race condition
    # when feching them when tests are run in parallel.
    # See crbug.com/700426 for more details.
    story_names = [s.name for s in stories_set if not s.is_local]
    stories_set.wpr_archive_info.DownloadArchivesIfNeeded(
        story_names=story_names)

    for story_to_smoke_test in stories_set.stories:
      # Per crbug.com/1019383 we don't have many device cycles to work with on
      # Android, so let's just run the most important stories.
      if (benchmark_class.Name() == 'system_health.memory_mobile' and
          'health_check' not in story_to_smoke_test.tags):
        continue
      suite.addTest(
          _GenerateSmokeTestCase(benchmark_class, story_to_smoke_test))

      names_stories_to_smoke_tests.append(
          benchmark_class.Name() + '/' + story_to_smoke_test.name)

  # The full story name should follow this convention: story_name[:version],
  # where version is a year. Please refer to the link below for details:
  # https://docs.google.com/document/d/134u_j_Lk2hLiDHYxK3NVdZM_sOtExrsExU-hiNFa1uw
  # Raise exception for stories which have more than one version enabled.
  multi_version_stories = find_multi_version_stories(
      names_stories_to_smoke_tests, _DISABLED_TESTS)
  if len(multi_version_stories):
    msg = ''
    for prefix, stories in multi_version_stories.items():
      msg += prefix + ' : ' + ','.join(stories) + '\n'
    raise ValueError(
        'The stories below has multiple versions.'
        'In order to save CQ capacity, we should only run the latest '
        'version on CQ. Please put the legacy stories in _DISABLED_TESTS '
        'list or remove them to save CQ capacity (see crbug.com/893615)). '
        'You can use crbug.com/878390 for the disabling reference.'
        '[StoryName] : [StoryVersion1],[StoryVersion2]...\n%s' % (msg))

  return suite


def find_multi_version_stories(stories, disabled):
  """Looks for stories with multiple versions enabled.

  Args:
    stories: list of strings, which are names of all the candidate stories.
    disabled: frozenset of strings, which are names of stories which are
      disabled.

  Returns:
    A dict mapping from a prefix string to a list of stories each of which
    has the name with that prefix and has multiple versions enabled.
  """
  prefixes = collections.defaultdict(list)
  for name in stories:
    if name in disabled:
      continue
    lastColon = name.rfind(':')
    if lastColon == -1:
      prefix = name
    else:
      version = name[lastColon+1:]
      if version.isdigit():
        prefix = name[:lastColon]
      else:
        prefix = name
    prefixes[prefix].append(name)
  for prefix, stories in prefixes.items():
    if len(stories) == 1:
      prefixes.pop(prefix)
  return prefixes
