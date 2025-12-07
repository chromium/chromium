# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry import benchmark
from telemetry import story

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class MotionMarkPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.MOTIONMARK]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  EXTRA_BROWSER_ARGS = None

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    extra_browser_args = ['--report-silk-details', '--disable-top-sites']
    if self.EXTRA_BROWSER_ARGS is not None:
      extra_browser_args.append(self.EXTRA_BROWSER_ARGS)
    super(MotionMarkPage,
          self).__init__(page_set=page_set,
                         shared_page_state_class=shared_page_state_class,
                         name_suffix=name_suffix,
                         make_javascript_deterministic=False,
                         extra_browser_args=extra_browser_args)
    self._score = 0
    self._scoreLowerBound = 0
    self._scoreUpperBound = 0
    self._stories = []
    self._storyScores = []
    self._storyScoreLowerBounds = []
    self._storyScoreUpperBounds = []

  def RunNavigateSteps(self, action_runner):
    action_runner.Navigate(self.url)
    action_runner.WaitForJavaScriptCondition(
        'document.readyState == "complete"')

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(3)
    with action_runner.CreateInteraction('Filter'):
      action_runner.Wait(20)
      action_runner.WaitForJavaScriptCondition(
          'window.benchmarkRunnerClient.results._results')
      [score, lower, upper] = action_runner.EvaluateJavaScript(
          '''[window.benchmarkRunnerClient.results.score,
             window.benchmarkRunnerClient.results.scoreLowerBound,
             window.benchmarkRunnerClient.results.scoreUpperBound]''')
      self._score = score
      self._scoreLowerBound = lower
      self._scoreUpperBound = upper

    # Navigate to about:blank to stop rendering frames and let the device
    # cool down while the trace data for the story is processed.
    action_runner.Navigate('about:blank')

  @property
  def score(self):
    return self._score

  @property
  def scoreLowerBound(self):
    return self._scoreLowerBound

  @property
  def scoreUpperBound(self):
    return self._scoreUpperBound

  @property
  def stories(self):
    return self._stories

  @property
  def storyScores(self):
    return self._storyScores

  @property
  def storyScoreLowerBounds(self):
    return self._storyScoreLowerBounds

  @property
  def storyScoreUpperBounds(self):
    return self._storyScoreUpperBounds

  @classmethod
  def GetUrl(cls, suite_name, test_name, complexity):
    # Strip unwanted characters from names
    for ch in [' ', '.', ',']:
      suite_name = suite_name.replace(ch, '')
      test_name = test_name.replace(ch, '')

    # Using latest MotionMark 1.3.1 as previous patches from 1.3 were pulled
    # into the main repo. Commit at time of recording
    # https://github.com/WebKit/MotionMark/commit/be2a5fea89b6ef411b053ebeb95a6302b3dc0ecb
    return ('https://browserbench.org/MotionMark1.3.1/developer.html'
            '?suite-name=%s'
            '&test-name=%s'
            '&complexity=%d'
            '&test-interval=20'
            '&warmup-length=2000'
            '&warmup-frame-count=30'
            '&first-frame-minimum-length=0'
            '&display=minimal'
            '&tiles=big'
            '&controller=fixed'
            '&system-frame-rate=60'
            '&frame-rate=60'
            '&time-measurement=performance') % (suite_name, test_name,
                                                complexity)

class MotionMarkRampPage(MotionMarkPage):
  ABSTRACT_STORY = True
  TAGS = [story_tags.MOTIONMARK, story_tags.MOTIONMARK_RAMP]
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS

  @classmethod
  def GetRampUrl(cls, suite_name, test_name):
    # Strip unwanted characters from names
    for ch in [' ', '.', ',']:
      suite_name = suite_name.replace(ch, '')
      test_name = test_name.replace(ch, '')

    # Using latest MotionMark 1.3.1 as previous patches from 1.3 were pulled
    # into the main repo. Commit at time of recording
    # https://github.com/WebKit/MotionMark/commit/be2a5fea89b6ef411b053ebeb95a6302b3dc0ecb
    return ('https://browserbench.org/MotionMark1.3.1/developer.html'
            '?suite-name=%s'
            '&test-name=%s'
            '&test-interval=20'
            '&display=minimal'
            '&tiles=big'
            '&controller=ramp'
            '&system-frame-rate=60'
            '&frame-rate=60'
            '&warmup-length=2000'
            '&warmup-frame-count=30'
            '&time-measurement=performance') % (suite_name, test_name)


class MotionMarkRampMultiply(MotionMarkRampPage):
  BASE_NAME = 'motionmark_ramp_multiply'
  URL = MotionMarkRampPage.GetRampUrl('MotionMark', 'Multiply')


class MotionMarkRampCanvasArcs(MotionMarkRampPage):
  BASE_NAME = 'motionmark_ramp_canvas_arcs'
  URL = MotionMarkRampPage.GetRampUrl('MotionMark', 'Canvas Arcs')


class MotionMarkRampLeaves(MotionMarkRampPage):
  BASE_NAME = 'motionmark_ramp_leaves'
  URL = MotionMarkRampPage.GetRampUrl('MotionMark', 'Leaves')


class MotionMarkRampPaths(MotionMarkRampPage):
  BASE_NAME = 'motionmark_ramp_paths'
  URL = MotionMarkRampPage.GetRampUrl('MotionMark', 'Paths')


class MotionMarkRampCanvasLines(MotionMarkRampPage):
  BASE_NAME = 'motionmark_ramp_canvas_lines'
  URL = MotionMarkRampPage.GetRampUrl('MotionMark', 'Canvas Lines')


class MotionMarkRampImages(MotionMarkRampPage):
  BASE_NAME = 'motionmark_ramp_images'
  URL = MotionMarkRampPage.GetRampUrl('MotionMark', 'Images')


class MotionMarkRampDesign(MotionMarkRampPage):
  BASE_NAME = 'motionmark_ramp_design'
  URL = MotionMarkRampPage.GetRampUrl('MotionMark', 'Design')


class MotionMarkRampSuits(MotionMarkRampPage):
  BASE_NAME = 'motionmark_ramp_suits'
  URL = MotionMarkRampPage.GetRampUrl('MotionMark', 'Suits')


class MotionMarkRampComposite(MotionMarkPage):
  DISABLE_TRACING = True
  TAGS = [story_tags.MOTIONMARK, story_tags.MOTIONMARK_RAMP]
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  BASE_NAME = 'motionmark_ramp_composite'
  # Using latest MotionMark 1.3.1 as previous patches from 1.3 were pulled
  # into the main repo. Commit at time of recording
  # https://github.com/WebKit/MotionMark/commit/be2a5fea89b6ef411b053ebeb95a6302b3dc0ecb
  URL = 'https://browserbench.org/MotionMark1.3.1/developer.html'

  def RunNavigateSteps(self, action_runner):
    action_runner.Navigate(self.url)
    action_runner.Wait(3)
    action_runner.ExecuteJavaScript('''
    const list = document.querySelectorAll('.tree > li');
    const row = list[0];
    const labels = row.querySelectorAll('input[type=checkbox]');
    for (const label of labels) {
          label.checked = true;
        }
    ''')
    action_runner.ExecuteJavaScript(
        'window.benchmarkController.startBenchmark()')
    action_runner.WaitForJavaScriptCondition(
        'document.readyState == "complete"')

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(3)
    with action_runner.CreateInteraction('Filter'):
      action_runner.Wait(300)  # Determined experimentally
      action_runner.WaitForJavaScriptCondition(
          'window.benchmarkRunnerClient.results._results')
      [score, lower, upper] = action_runner.EvaluateJavaScript(
          '''[window.benchmarkRunnerClient.results.score,
             window.benchmarkRunnerClient.results.scoreLowerBound,
             window.benchmarkRunnerClient.results.scoreUpperBound]''')
      self._score = score
      self._scoreLowerBound = lower
      self._scoreUpperBound = upper

      # The MotionMark object is a non-iterable map, so we need to access the
      # components manually. Currently we only run one iteration, this would
      # need to be updated if we add iteration support in the future.
      [stories, scores, lowerBounds,
       upperBounds] = action_runner.EvaluateJavaScript('''const stories =
                 window.benchmarkRunnerClient.results._results.
                     iterationsResults[0].testsResults.MotionMark;
             const scores = [];
             const lowerBounds = [];
             const upperBounds = [];
             for (const val of Object.keys(stories)) {
                  const story = stories[val];
                  scores.push(story.score);
                  lowerBounds.push(story.scoreLowerBound);
                  upperBounds.push(story.scoreUpperBound);
             }
             [stories, scores, lowerBounds, upperBounds]''')

      self._stories = stories
      self._storyScores = scores
      self._storyScoreLowerBounds = lowerBounds
      self._storyScoreUpperBounds = upperBounds

    # Navigate to about:blank to stop rendering frames and let the device
    # cool down while the trace data for the story is processed.
    action_runner.Navigate('about:blank')

  def WillStartTracing(self, chrome_trace_config):
    chrome_trace_config.record_mode = 'record-until-full'


class MotionMarkFixed2SecondsPage(MotionMarkPage):
  ABSTRACT_STORY = True
  TAGS = [story_tags.MOTIONMARK, story_tags.MOTIONMARK_FIXED_2_SECONDS]
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('Filter'):
      action_runner.WaitForJavaScriptCondition(
          'window.benchmarkRunnerClient.results._results')

  @classmethod
  def GetFixed2SecondsUrl(cls, suite_name, test_name, complexity,
                          test_interval):
    # Strip unwanted characters from names
    for ch in [' ', '.', ',']:
      suite_name = suite_name.replace(ch, '')
      test_name = test_name.replace(ch, '')

  # Using latest MotionMark 1.3.1 as previous patches from 1.3 were pulled
  # into the main repo. Commit at time of recording#
  # https://github.com/WebKit/MotionMark/commit/be2a5fea89b6ef411b053ebeb95a6302b3dc0ecb
    return ('https://browserbench.org/MotionMark1.3.1/developer.html'
            '?suite-name=%s'
            '&test-name=%s'
            '&complexity=%d'
            '&test-interval=%d'
            '&warmup-length=0'
            '&warmup-frame-count=0'
            '&first-frame-minimum-length=0'
            '&display=minimal'
            '&tiles=big'
            '&controller=fixed'
            '&system-frame-rate=60'
            '&frame-rate=60'
            '&time-measurement=performance') % (suite_name, test_name,
                                                complexity, test_interval)


# Numbers for complexity come from recent high scores on perf bots with a PGO
# build.
#TODO(vmiura): Update names from fixed_2_seconds to match the new durations.
class MotionMarkFixed2SecondsMultiply(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_multiply'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark',
                                                        'Multiply', 5150, 5)


class MotionMarkFixed2SecondsCanvasArcs(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_canvas_arcs'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark',
                                                        'Canvas Arcs', 17400, 5)


class MotionMarkFixed2SecondsLeaves(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_leaves'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark', 'Leaves',
                                                        4300, 5)


class MotionMarkFixed2SecondsPaths(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_paths'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark', 'Paths',
                                                        64700, 5)


class MotionMarkFixed2SecondsCanvasLines(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_canvas_lines'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark',
                                                        'Canvas Lines', 54200,
                                                        5)


class MotionMarkFixed2SecondsImages(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_images'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark', 'Images',
                                                        440, 5)


class MotionMarkFixed2SecondsDesign(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_design'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark', 'Design',
                                                        705, 5)


class MotionMarkFixed2SecondsSuits(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_suits'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark', 'Suits',
                                                        2800, 5)


@benchmark.Info(emails=['chrome-skia-graphite@google.com'],
                component='Internals>GPU>Internals')
class MotionMarkRampCompositeGraphite(MotionMarkRampComposite):
  BASE_NAME = 'motionmark_ramp_composite_graphite'
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MAC, story.expectations.ALL_WIN]
  EXTRA_BROWSER_ARGS = '--enable-features=SkiaGraphite'


@benchmark.Info(emails=['chrome-skia-graphite@google.com'],
                component='Internals>GPU>Internals')
class MotionMarkRampCompositeGanesh(MotionMarkRampComposite):
  BASE_NAME = 'motionmark_ramp_composite_ganesh'
  SUPPORTED_PLATFORMS = [story.expectations.ALL_MAC, story.expectations.ALL_WIN]
  EXTRA_BROWSER_ARGS = '--disable-features=SkiaGraphite'
