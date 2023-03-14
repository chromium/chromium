# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class MotionMarkPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.MOTIONMARK]
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(MotionMarkPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        make_javascript_deterministic=False,
        extra_browser_args=['--report-silk-details', '--disable-top-sites'])
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

    return (
        'http://browserbench.org/MotionMark/developer.html'
        '?suite-name=%s'
        '&test-name=%s'
        '&complexity=%d'
        '&test-interval=20'
        '&display=minimal'
        '&tiles=big'
        '&controller=fixed'
        '&frame-rate=50'
        '&kalman-process-error=1'
        '&kalman-measurement-error=4'
        '&time-measurement=raf'
        ) % (suite_name, test_name, complexity)


# Why: MotionMark Animometer case """
class MotionmarkAnimMultiply175(MotionMarkPage):
  BASE_NAME = 'motionmark_anim_multiply_175'
  URL = MotionMarkPage.GetUrl('Animometer', 'Multiply', 175)


# Why: MotionMark Animometer case """
class MotionmarkAnimLeaves250(MotionMarkPage):
  BASE_NAME = 'motionmark_anim_leaves_250'
  URL = MotionMarkPage.GetUrl('Animometer', 'Leaves', 250)


# Why: MotionMark Animometer case """
class MotionmarkAnimFocus25(MotionMarkPage):
  BASE_NAME = 'motionmark_anim_focus_25'
  URL = MotionMarkPage.GetUrl('Animometer', 'Focus', 25)


# Why: MotionMark Animometer case """
class MotionmarkAnimImages50(MotionMarkPage):
  BASE_NAME = 'motionmark_anim_images_50'
  URL = MotionMarkPage.GetUrl('Animometer', 'Images', 50)


# Why: MotionMark Animometer case """
class MotionmarkAnimDesign15(MotionMarkPage):
  BASE_NAME = 'motionmark_anim_design_15'
  URL = MotionMarkPage.GetUrl('Animometer', 'Design', 15)


# Why: MotionMark Animometer case """
class MotionmarkAnimSuits125(MotionMarkPage):
  BASE_NAME = 'motionmark_anim_suits_125'
  URL = MotionMarkPage.GetUrl('Animometer', 'Suits', 125)


# Why: MotionMark HTML case """
class MotionmarkHTMLCSSBouncingCircles250(MotionMarkPage):
  BASE_NAME = 'motionmark_html_css_bouncing_circles_250'
  URL = MotionMarkPage.GetUrl('HTML suite', 'CSS bouncing circles', 250)


# Why: MotionMark HTML case """
class MotionmarkHTMLCSSBouncingClippedRects100(MotionMarkPage):
  BASE_NAME = 'motionmark_html_css_bouncing_clipped_rects_100'
  URL = MotionMarkPage.GetUrl('HTML suite', 'CSS bouncing clipped rects', 100)


# Why: MotionMark HTML case """
class MotionmarkHTMLCSSBouncingGradientCircles250(MotionMarkPage):
  BASE_NAME = 'motionmark_html_css_bouncing_gradient_circles_250'
  URL = MotionMarkPage.GetUrl('HTML suite', 'CSS bouncing gradient circles',
                              250)


# Why: MotionMark HTML case """
class MotionmarkHTMLCSSBouncingBlendCircles25(MotionMarkPage):
  BASE_NAME = 'motionmark_html_css_bouncing_blend_circles_25'
  URL = MotionMarkPage.GetUrl('HTML suite', 'CSS bouncing blend circles', 25)


# Why: MotionMark HTML case """
class MotionmarkHTMLCSSBouncingFilterCircles15(MotionMarkPage):
  BASE_NAME = 'motionmark_html_css_bouncing_filter_circles_15'
  URL = MotionMarkPage.GetUrl('HTML suite', 'CSS bouncing filter circles', 15)


# Why: MotionMark HTML case """
class MotionmarkHTMLCSSBouncingSVGImages50(MotionMarkPage):
  BASE_NAME = 'motionmark_html_css_bouncing_svg_images_50'
  URL = MotionMarkPage.GetUrl('HTML suite', 'CSS bouncing SVG images', 50)


# Why: MotionMark HTML case """
class MotionmarkHTMLCSSBouncingTaggedImages225(MotionMarkPage):
  BASE_NAME = 'motionmark_html_css_bouncing_tagged_images_225'
  URL = MotionMarkPage.GetUrl('HTML suite', 'CSS bouncing tagged images', 225)


# Why: MotionMark HTML case """
class MotionmarkHTMLLeaves20_50(MotionMarkPage):
  BASE_NAME = 'motionmark_html_leaves_20_50'
  URL = MotionMarkPage.GetUrl('HTML suite', 'Leaves 2.0', 50)


# Why: MotionMark HTML case """
class MotionmarkHTMLFocus20_15(MotionMarkPage):
  BASE_NAME = 'motionmark_html_focus_20_15'
  URL = MotionMarkPage.GetUrl('HTML suite', 'Focus 2.0', 15)


# Why: MotionMark HTML case """
class MotionmarkHTMLDomParticlesSvgMasks25(MotionMarkPage):
  BASE_NAME = 'motionmark_html_dom_particles_svg_masks_25'
  URL = MotionMarkPage.GetUrl('HTML suite', 'DOM particles, SVG masks', 25)


# Why: MotionMark HTML case """
class MotionmarkHTMLCompositedTransforms125(MotionMarkPage):
  BASE_NAME = 'motionmark_html_composited_transforms_125'
  URL = MotionMarkPage.GetUrl('HTML suite', 'Composited Transforms', 125)


# Why: MotionMark SVG case """
class MotionmarkSVGBouncingCircles250(MotionMarkPage):
  BASE_NAME = 'motionmark_svg_bouncing_circles_250'
  URL = MotionMarkPage.GetUrl('SVG suite', 'SVG bouncing circles', 250)


# Why: MotionMark SVG case """
class MotionmarkSVGBouncingClippedRects100(MotionMarkPage):
  BASE_NAME = 'motionmark_svg_bouncing_clipped_rects_100'
  URL = MotionMarkPage.GetUrl('SVG suite', 'SVG bouncing clipped rects', 100)


# Why: MotionMark SVG case """
class MotionmarkSVGBouncingGradientCircles200(MotionMarkPage):
  BASE_NAME = 'motionmark_svg_bouncing_gradient_circles_200'
  URL = MotionMarkPage.GetUrl('SVG suite', 'SVG bouncing gradient circles', 200)


# Why: MotionMark SVG case """
class MotionmarkSVGBouncingSVGImages50(MotionMarkPage):
  BASE_NAME = 'motionmark_svg_bouncing_svg_images_50'
  URL = MotionMarkPage.GetUrl('SVG suite', 'SVG bouncing SVG images', 50)


# Why: MotionMark SVG case """
class MotionmarkSVGBouncingPNGImages200(MotionMarkPage):
  BASE_NAME = 'motionmark_svg_bouncing_png_images_200'
  URL = MotionMarkPage.GetUrl('SVG suite', 'SVG bouncing png images', 200)


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

    return ('https://browserbench.org/MotionMark1.2/developer.html'
            '?suite-name=%s'
            '&test-name=%s'
            '&test-interval=20'
            '&display=minimal'
            '&tiles=big'
            '&controller=ramp'
            '&kalman-process-error=1'
            '&kalman-measurement-error=4'
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
  URL = 'https://browserbench.org/MotionMark1.2/developer.html'

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
      action_runner.Wait(2)

    # Navigate to about:blank to stop rendering frames and let the device
    # cool down while the trace data for the story is processed.
    action_runner.Navigate('about:blank')

  @classmethod
  def GetFixed2SecondsUrl(cls, suite_name, test_name, complexity):
    # Strip unwanted characters from names
    for ch in [' ', '.', ',']:
      suite_name = suite_name.replace(ch, '')
      test_name = test_name.replace(ch, '')

    return ('https://browserbench.org/MotionMark1.2/developer.html'
            '?suite-name=%s'
            '&test-name=%s'
            '&complexity=%d'
            '&test-interval=1'
            '&display=minimal'
            '&tiles=big'
            '&controller=fixed'
            '&kalman-process-error=1'
            '&kalman-measurement-error=4'
            '&time-measurement=raf') % (suite_name, test_name, complexity)


#Numbers for complexity based on MotionMark score for chrome build without PGO
class MotionMarkFixed2SecondsMultiply(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_multiply'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark',
                                                        'Multiply', 1396)


class MotionMarkFixed2SecondsCanvasArcs(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_canvas_arcs'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark',
                                                        'Canvas Arcs', 6194)


class MotionMarkFixed2SecondsLeaves(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_leaves'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark', 'Leaves',
                                                        1377)


class MotionMarkFixed2SecondsPaths(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_paths'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark', 'Paths',
                                                        29172)


class MotionMarkFixed2SecondsCanvasLines(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_canvas_lines'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark',
                                                        'Canvas Lines', 16520)


class MotionMarkFixed2SecondsImages(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_images'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark', 'Images',
                                                        102)


class MotionMarkFixed2SecondsDesign(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_design'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark', 'Design',
                                                        213)


class MotionMarkFixed2SecondsSuits(MotionMarkFixed2SecondsPage):
  BASE_NAME = 'motionmark_fixed_2_seconds_suits'
  URL = MotionMarkFixed2SecondsPage.GetFixed2SecondsUrl('MotionMark', 'Suits',
                                                        1299)
