# Copyright 2017 The Chromium Authors. All rights reserved.
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
        extra_browser_args=['--report-silk-details', '--disable-top-sites'])

  def RunNavigateSteps(self, action_runner):
    action_runner.Navigate(self.url)
    action_runner.WaitForJavaScriptCondition(
        'document.readyState == "complete"')

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(3)
    with action_runner.CreateInteraction('Filter'):
      action_runner.Wait(5)

    # Navigate to about:blank to stop rendering frames and let the device
    # cool down while the trace data for the story is processed.
    action_runner.Navigate('about:blank')

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
