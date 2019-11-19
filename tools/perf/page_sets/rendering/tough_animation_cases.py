# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry import story

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class ToughAnimationPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  NEED_MEASUREMENT_READY = True
  TAGS = [story_tags.TOUGH_ANIMATION]

  def __init__(self,
               page_set,
               shared_page_state_class,
               name_suffix='',
               extra_browser_args=None):
    super(ToughAnimationPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(ToughAnimationPage, self).RunNavigateSteps(action_runner)
    if self.NEED_MEASUREMENT_READY:
      action_runner.WaitForJavaScriptCondition('window.measurementReady')

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ToughAnimation'):
      action_runner.Wait(10)


class BallsSVGAnimationsPage(ToughAnimationPage):
  """Why: Tests the balls animation implemented with SVG animations."""
  BASE_NAME = 'balls_svg_animations'
  URL = 'file://../tough_animation_cases/balls_svg_animations.html'


class BallsJavascriptCanvasPage(ToughAnimationPage):
  """Why: Tests the balls animation implemented with Javascript and canvas."""
  BASE_NAME = 'balls_javascript_canvas'
  URL = 'file://../tough_animation_cases/balls_javascript_canvas.html'
  TAGS = ToughAnimationPage.TAGS + [
    story_tags.REPRESENTATIVE_MOBILE,
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]


class BallsJavascriptCssPage(ToughAnimationPage):
  """Why: Tests the balls animation implemented with Javascript and CSS."""
  BASE_NAME = 'balls_javascript_css'
  URL = 'file://../tough_animation_cases/balls_javascript_css.html'


class BallsCssKeyFrameAnimationsPage(ToughAnimationPage):
  """Why: Tests the balls animation implemented with CSS keyframe animations."""
  BASE_NAME = 'balls_css_key_frame_animations'
  URL = 'file://../tough_animation_cases/balls_css_keyframe_animations.html'


class BallsCssKeyFrameAnimationsCompositedPage(ToughAnimationPage):
  """Why: Tests the balls animation implemented with transforms and CSS
  keyframe animations to be run on the compositor thread.
  """
  BASE_NAME = 'balls_css_key_frame_animations_composited_transform'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/balls_css_keyframe_animations_composited_transform.html'


class BallsCssTransition2PropertiesPage(ToughAnimationPage):
  """Why: Tests the balls animation implemented with CSS transitions on 2
  properties.
  """
  BASE_NAME = 'balls_css_transition_2_properties'
  URL = 'file://../tough_animation_cases/balls_css_transition_2_properties.html'
  TAGS = ToughAnimationPage.TAGS + [story_tags.REPRESENTATIVE_WIN_DESKTOP]


class BallsCssTransition40PropertiesPage(ToughAnimationPage):
  """Why: Tests the balls animation implemented with CSS transitions on 40
  properties.
  """
  BASE_NAME = 'balls_css_transition_40_properties'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/balls_css_transition_40_properties.html'


class BallsCssTransitionAllPropertiesPage(ToughAnimationPage):
  """Why: Tests the balls animation implemented with CSS transitions on all
  animatable properties.
  """
  BASE_NAME = 'balls_css_transition_all_properties'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/balls_css_transition_all_properties.html'


class OverlayBackgroundColorCssTransitionsPage(ToughAnimationPage):
  BASE_NAME = 'overlay_background_color_css_transitions_page'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/overlay_background_color_css_transitions.html'


class CssTransitionsNewElementPage(ToughAnimationPage):
  """Why: Tests many CSS Transitions all starting at the same time triggered
  by inserting new elements.
  """
  BASE_NAME = 'css_transitions_new_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_simultaneous_by_inserting_new_element.html?N=0316'


class CssTransitionsStyleElementPage(ToughAnimationPage):
  """Why: Tests many CSS Transitions all starting at the same time triggered
  by inserting style sheets.
  """
  BASE_NAME = 'css_transitions_style_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_simultaneous_by_inserting_style_element.html?N=0316'


class CssTransitionsUpdatingClassPage(ToughAnimationPage):
  """Why: Tests many CSS Transitions all starting at the same time triggered
  by updating class.
  """
  BASE_NAME = 'css_transitions_updating_class'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_simultaneous_by_updating_class.html?N=0316'


class CssTransitionsInlineStylePage(ToughAnimationPage):
  """Why: Tests many CSS Transitions all starting at the same time triggered
  by updating inline style.
  """
  BASE_NAME = 'css_transitions_inline_style'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_simultaneous_by_updating_inline_style.html?N=0316'
  TAGS = ToughAnimationPage.TAGS + [story_tags.REPRESENTATIVE_WIN_DESKTOP]


class CssTransitionsStaggeredNewElementPage(ToughAnimationPage):
  """Why: Tests many CSS Transitions chained together using events at
  different times triggered by inserting new elements.
  """
  BASE_NAME = 'css_transitions_staggered_new_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_staggered_chaining_by_inserting_new_element.html?N=0316'


class CssTransitionsStaggeredStyleElementPage(ToughAnimationPage):
  """Why: Tests many CSS Transitions chained together using events at
  different times triggered by inserting style sheets.
  """
  BASE_NAME = 'css_transitions_staggered_style_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_staggered_chaining_by_inserting_style_element.html?N=0316'


class CssTransitionsStaggeredUpdatingClassPage(ToughAnimationPage):
  """Why: Tests many CSS Transitions chained together using events at
  different times triggered by updating class.
  """
  BASE_NAME = 'css_transitions_staggered_updating_class'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_staggered_chaining_by_updating_class.html?N=0316'


class CssTransitionsStaggeredInlineStylePage(ToughAnimationPage):
  """Why: Tests many CSS Transitions chained together using events at
  different times triggered by updating inline style.
  """
  BASE_NAME = 'css_transitions_staggered_inline_style'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_staggered_chaining_by_updating_inline_style.html?N=0316'


class CssTransitionsTriggeredNewElementPage(ToughAnimationPage):
  """Why: Tests many CSS Transitions starting at different times triggered by
  inserting new elements.
  """
  BASE_NAME = 'css_transitions_triggered_new_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_staggered_triggering_by_inserting_new_element.html?N=0316'


class CssTransitionsTriggeredStyleElementPage(ToughAnimationPage):
  """Why: Tests many CSS Transitions starting at different times triggered by
  inserting style sheets.
  """
  BASE_NAME = 'css_transitions_triggered_style_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_staggered_triggering_by_inserting_style_element.html?N=0316'


class CssTransitionsTriggeredUpdatingClassPage(ToughAnimationPage):
  """Why: Tests many CSS Transitions starting at different times triggered by
  updating class.
  """
  BASE_NAME = 'css_transitions_triggered_updating_class'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_staggered_triggering_by_updating_class.html?N=0316'


class CssTransitionsTriggeredInlineStylePage(ToughAnimationPage):
  """Why: Tests many CSS Transitions starting at different times triggered by
  updating inline style.
  """
  BASE_NAME = 'css_transitions_triggered_inline_style'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_transitions_staggered_triggering_by_updating_inline_style.html?N=0316'


class CssAnimationsManyKeyframesPage(ToughAnimationPage):
  """Why: Tests many CSS Animations all starting at the same time with 500
  keyframes each.
  """
  BASE_NAME = 'css_animations_many_keyframes'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_many_keyframes.html?N=0316'


class CssAnimationsSimultaneousNewElementPage(ToughAnimationPage):
  """Why: Tests many CSS Animations all starting at the same time triggered
  by inserting new elements.
  """
  BASE_NAME = 'css_animations_simultaneous_new_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_simultaneous_by_inserting_new_element.html?N=0316'


class CssAnimationsSimultaneousStyleElementPage(ToughAnimationPage):
  """Why: Tests many CSS Animations all starting at the same time triggered
  by inserting style sheets.
  """
  BASE_NAME = 'css_animations_simultaneous_style_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_simultaneous_by_inserting_style_element.html?N=0316'


class CssAnimationsSimultaneousUpdatingClassPage(ToughAnimationPage):
  """Why: Tests many CSS Animations all starting at the same time triggered
  by updating class.
  """
  BASE_NAME = 'css_animations_simultaneous_updating_class'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_simultaneous_by_updating_class.html?N=0316'


class CssAnimationsSimultaneousInlineStylePage(ToughAnimationPage):
  """Why: Tests many CSS Animations all starting at the same time triggered
  by updating inline style.
  """
  BASE_NAME = 'css_animations_simultaneous_inline_style'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_simultaneous_by_updating_inline_style.html?N=0316'
  TAGS = ToughAnimationPage.TAGS + [story_tags.REPRESENTATIVE_WIN_DESKTOP]


class CssAnimationsStaggeredNewElementPage(ToughAnimationPage):
  """Why: Tests many CSS Animations chained together using events at
  different times triggered by inserting new elements.
  """
  BASE_NAME = 'css_animations_staggered_new_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_staggered_chaining_by_inserting_new_element.html?N=0316'


class CssAnimationsStaggeredStyleElementPage(ToughAnimationPage):
  """Why: Tests many CSS Animations chained together using events at
  different times triggered by inserting style sheets.
  """
  BASE_NAME = 'css_animations_staggered_style_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_staggered_chaining_by_inserting_style_element.html?N=0316'


class CssAnimationsStaggeredUpdatingClassPage(ToughAnimationPage):
  """Why: Tests many CSS Animations chained together using events at
  different times triggered by updating class.
  """
  BASE_NAME = 'css_animations_staggered_updating_class'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_staggered_chaining_by_updating_class.html?N=0316'


class CssAnimationsStaggeredInlineStylePage(ToughAnimationPage):
  """Why: Tests many CSS Animations chained together using events at
  different times triggered by updating inline style.
  """
  BASE_NAME = 'css_animations_staggered_inline_style'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_staggered_chaining_by_updating_inline_style.html?N=0316'


class CssAnimationsTriggeredNewElementPage(ToughAnimationPage):
  """Why: Tests many CSS Animations starting at different times triggered by
  inserting new elements.
  """
  BASE_NAME = 'css_animations_triggered_new_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_staggered_triggering_by_inserting_new_element.html?N=0316'


class CssAnimationsStaggeredInfinitePage(ToughAnimationPage):
  """Why: Tests many CSS Animations all starting at the same time with
  staggered animation offsets.
  """
  BASE_NAME = 'css_animations_staggered_infinite_iterations'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_staggered_infinite_iterations.html?N=0316'


class CssAnimationsTriggeredStyleElementPage(ToughAnimationPage):
  """Why: Tests many CSS Animations starting at different times triggered by
  inserting style sheets.
  """
  BASE_NAME = 'css_animations_triggered_style_element'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_staggered_triggering_by_inserting_style_element.html?N=0316'


class CssAnimationsTriggeredUpdatingClassPage(ToughAnimationPage):
  """Why: Tests many CSS Animations starting at different times triggered by
  updating class.
  """
  BASE_NAME = 'css_animations_triggered_updating_class'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_staggered_triggering_by_updating_class.html?N=0316'


class CssAnimationsTriggeredInlineStylePage(ToughAnimationPage):
  """Why: Tests many CSS Animations starting at different times triggered by
  updating inline style.
  """
  BASE_NAME = 'css_animations_triggered_inline_style'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_animations_staggered_triggering_by_updating_inline_style.html?N=0316'


class WebAnimationsManyKeyframesPage(ToughAnimationPage):
  """Why: Tests many Web Animations all starting at the same time with 500
  keyframes each.
  """
  BASE_NAME = 'web_animations_many_keyframes'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/web_animations_many_keyframes.html?N=0316'


class WebAnimationsSetCurrentTimePage(ToughAnimationPage):
  """Why: Tests many paused Web Animations having their currentTimes updated
  in every requestAnimationFrame.
  """
  BASE_NAME = 'web_animations_set_current_time'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/web_animations_set_current_time_in_raf.html?N=0316'


class WebAnimationsSimultaneousPage(ToughAnimationPage):
  """Why: Tests many Web Animations all starting at the same time."""
  BASE_NAME = 'web_animations_simultaneous'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/web_animations_simultaneous.html?N=0316'


class WebAnimationsStaggeredChainingPage(ToughAnimationPage):
  """Why: Tests many Web Animations all starting at different times then
  chained together using events.
  """
  BASE_NAME = 'web_animations_staggered_chaining'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/web_animations_staggered_chaining.html?N=0316'


class WebAnimationStaggeredInfinitePage(ToughAnimationPage):
  """Why: Tests many Web Animations all starting at different times with
  infinite iterations.
  """
  BASE_NAME = 'web_animations_staggered_infinite_iterations'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/web_animations_staggered_infinite_iterations.html?N=0316'
  TAGS = ToughAnimationPage.TAGS + [
    story_tags.REPRESENTATIVE_MOBILE,
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]


class WebAnimationsStaggeredTriggeringPage(ToughAnimationPage):
  """Why: Tests many Web Animations all starting at different times."""
  BASE_NAME = 'web_animations_staggered_triggering_page'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/web_animations_staggered_triggering.html?N=0316'
  TAGS = ToughAnimationPage.TAGS + [story_tags.REPRESENTATIVE_WIN_DESKTOP]


class CssValueTypeColorPage(ToughAnimationPage):
  """Why: Tests color animations using CSS Animations."""
  BASE_NAME = 'css_value_type_color'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_color.html?api=css_animations&N=0316'


class CssValueTypeFilterPage(ToughAnimationPage):
  """Why: Tests filter animations using CSS Animations."""
  BASE_NAME = 'css_value_type_filter'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_filter.html?api=css_animations&N=0316'
  TAGS = ToughAnimationPage.TAGS + [
    story_tags.REPRESENTATIVE_MOBILE,
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]


class CssValueTypeLengthPage(ToughAnimationPage):
  """Why: Tests length 3D animations using CSS Animations."""
  BASE_NAME = 'css_value_type_length'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_length_3d.html?api=css_animations&N=0316'


class CssValueTypeLengthComplexPage(ToughAnimationPage):
  """Why: Tests complex length animations using CSS Animations."""
  BASE_NAME = 'css_value_type_length_complex'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_length_complex.html?api=css_animations&N=0316'


class CssValueTypeLengthSimplePage(ToughAnimationPage):
  """Why: Tests simple length animations using CSS Animations."""
  BASE_NAME = 'css_value_type_length_simple'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_length_simple.html?api=css_animations&N=0316'


class CssValueTypePathPage(ToughAnimationPage):
  """Why: Tests path animations using CSS Animations."""
  BASE_NAME = 'css_value_type_path'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_path.html?api=css_animations&N=0316'


class CssValueTypeShadowPage(ToughAnimationPage):
  """Why: Tests shadow animations using CSS Animations."""
  BASE_NAME = 'css_value_type_shadow'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_shadow.html?api=css_animations&N=0316'
  TAGS = ToughAnimationPage.TAGS + [
    story_tags.REPRESENTATIVE_MOBILE,
    story_tags.REPRESENTATIVE_WIN_DESKTOP,
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]


class CssValueTypeTransformComplexPage(ToughAnimationPage):
  """Why: Tests complex transform animations using CSS Animations."""
  BASE_NAME = 'css_value_type_transform_complex'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_transform_complex.html?api=css_animations&N=0316'


class CssValueTypeTransformSimplePage(ToughAnimationPage):
  """Why: Tests simple transform animations using CSS Animations."""
  BASE_NAME = 'css_value_type_transform_simple'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_transform_simple.html?api=css_animations&N=0316'


class WebAnimationValueTypeColorPage(ToughAnimationPage):
  """Why: Tests color animations using Web Animations."""
  BASE_NAME = 'web_animation_value_type_color'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_color.html?api=web_animations&N=0316'


class WebAnimationValueTypeLength3dPage(ToughAnimationPage):
  """Why: Tests length 3D animations using Web Animations."""
  BASE_NAME = 'web_animation_value_type_length_3d'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_length_3d.html?api=web_animations&N=0316'


class WebAnimationValueTypeLengthComplexPage(ToughAnimationPage):
  """Why: Tests complex length animations using Web Animations."""
  BASE_NAME = 'web_animation_value_type_length_complex'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_length_complex.html?api=web_animations&N=0316'


class WebAnimationValueTypeLengthSimplePage(ToughAnimationPage):
  """Why: Tests simple length animations using Web Animations."""
  BASE_NAME = 'web_animation_value_type_length_simple'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_length_simple.html?api=web_animations&N=0316'


class WebAnimationValueTypePathPage(ToughAnimationPage):
  """Why: Tests path animations using Web Animations."""
  BASE_NAME = 'web_animation_value_type_path'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_path.html?api=web_animations&N=0316'


class WebAnimationValueTypeShadowPage(ToughAnimationPage):
  """Why: Tests shadow animations using Web Animations."""
  BASE_NAME = 'web_animation_value_type_shadow'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_shadow.html?api=web_animations&N=0316'


class WebAnimationValueTypeTransformComplexPage(ToughAnimationPage):
  """Why: Tests complex transform animations using Web Animations."""
  BASE_NAME = 'web_animation_value_type_transform_complex'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_transform_complex.html?api=web_animations&N=0316'
  TAGS = ToughAnimationPage.TAGS + [story_tags.REPRESENTATIVE_WIN_DESKTOP]


class WebAnimationValueTypeTransformSimplePage(ToughAnimationPage):
  """Why: Tests simple transform animations using Web Animations."""
  BASE_NAME = 'web_animation_value_type_transform_simple'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/css_value_type_transform_simple.html?api=web_animations&N=0316'
  TAGS = ToughAnimationPage.TAGS + [
    story_tags.REPRESENTATIVE_MOBILE,
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]


class CompositorHeavyAnimationPage(ToughAnimationPage):
  """Why: Test to update and then draw many times a large set of textures
  to compare one-copy and zero-copy.
  """
  BASE_NAME = 'compositor_heavy_animation'
  URL = 'file://../tough_animation_cases/compositor_heavy_animation.html?N=0200'


class KeyframedAnimationsPage(ToughAnimationPage):
  """Why: Tests various keyframed animations."""
  BASE_NAME = 'keyframed_animations'
  URL = 'file://../tough_animation_cases/keyframed_animations.html'
  NEED_MEASUREMENT_READY = False


class TransformTransitionsPage(ToughAnimationPage):
  """Why: Tests various transitions."""
  BASE_NAME = 'transform_transitions'
  URL = 'file://../tough_animation_cases/transform_transitions.html'
  NEED_MEASUREMENT_READY = False


class TransformTransitionsJSBlockPage(ToughAnimationPage):
  """Why: JS execution blocks CSS transition unless initial transform is set."""
  BASE_NAME = 'transform_transitions_js_block'
  URL = 'file://../tough_animation_cases/transform_transition_js_block.html'
  NEED_MEASUREMENT_READY = False
  TAGS = ToughAnimationPage.TAGS + [
    story_tags.REPRESENTATIVE_MOBILE,
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]


class MixBlendModeAnimationDifferencePage(ToughAnimationPage):
  """Why: Tests animating elements having mix-blend-mode: difference (a
  separable popular blend mode).
  """
  BASE_NAME = 'mix_blend_mode_animation_difference'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/mix_blend_mode_animation_difference.html'
  NEED_MEASUREMENT_READY = False


class MixBlendModeAnimationHuePage(ToughAnimationPage):
  """Why: Tests animating elements having mix-blend-mode: hue (a
  non-separable blend mode).
  """
  BASE_NAME = 'mix_blend_mode_animation_hue'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/mix_blend_mode_animation_hue.html'
  NEED_MEASUREMENT_READY = False


class MixBlendModeAnimationScreenPage(ToughAnimationPage):
  """Why: Tests animating elements having mix-blend-mode: screen."""
  BASE_NAME = 'mix_blend_mode_animation_screen'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/mix_blend_mode_animation_screen.html'
  NEED_MEASUREMENT_READY = False
  TAGS = ToughAnimationPage.TAGS + [
    story_tags.REPRESENTATIVE_MOBILE,
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]


class MixAnimationPropagatingIsolationPage(ToughAnimationPage):
  """Why: Tests software-animating a deep DOM subtree having one blending
  leaf.
  """
  BASE_NAME = 'mix_blend_mode_animation_propagating_isolation'
  # pylint: disable=line-too-long
  URL = 'file://../tough_animation_cases/mix_blend_mode_propagating_isolation.html'
  NEED_MEASUREMENT_READY = False


class MicrosoftPerformancePage(ToughAnimationPage):
  """Why: Login page is slow because of ineffecient transform operations."""
  BASE_NAME = 'microsoft_performance'
  URL = 'http://ie.microsoft.com/testdrive/performance/robohornetpro/'
  NEED_MEASUREMENT_READY = False


# TODO(crbug.com/760553):remove this class after
# smoothness.tough_animation_cases benchmark is completely
# replaced by rendering benchmarks
class ToughAnimationCasesPageSet(story.StorySet):

  """
  Description: A collection of animation performance tests
  """

  def __init__(self):
    super(ToughAnimationCasesPageSet, self).__init__(
      archive_data_file='../data/tough_animation_cases.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    page_classes = [
      BallsSVGAnimationsPage,
      BallsJavascriptCanvasPage,
      BallsJavascriptCssPage,
      BallsCssKeyFrameAnimationsPage,
      BallsCssKeyFrameAnimationsCompositedPage,
      BallsCssTransition2PropertiesPage,
      BallsCssTransition40PropertiesPage,
      BallsCssTransitionAllPropertiesPage,
      OverlayBackgroundColorCssTransitionsPage,
      CssTransitionsNewElementPage,
      CssTransitionsStyleElementPage,
      CssTransitionsUpdatingClassPage,
      CssTransitionsInlineStylePage,
      CssTransitionsStaggeredNewElementPage,
      CssTransitionsStaggeredStyleElementPage,
      CssTransitionsStaggeredUpdatingClassPage,
      CssTransitionsStaggeredInlineStylePage,
      CssTransitionsTriggeredNewElementPage,
      CssTransitionsTriggeredStyleElementPage,
      CssTransitionsTriggeredUpdatingClassPage,
      CssTransitionsTriggeredInlineStylePage,
      CssAnimationsManyKeyframesPage,
      CssAnimationsSimultaneousNewElementPage,
      CssAnimationsSimultaneousStyleElementPage,
      CssAnimationsSimultaneousInlineStylePage,
      CssAnimationsSimultaneousUpdatingClassPage,
      CssAnimationsStaggeredNewElementPage,
      CssAnimationsStaggeredStyleElementPage,
      CssAnimationsStaggeredUpdatingClassPage,
      CssAnimationsStaggeredInlineStylePage,
      CssAnimationsTriggeredNewElementPage,
      CssAnimationsStaggeredInfinitePage,
      CssAnimationsTriggeredStyleElementPage,
      CssAnimationsTriggeredUpdatingClassPage,
      CssAnimationsTriggeredInlineStylePage,
      WebAnimationsManyKeyframesPage,
      WebAnimationsSetCurrentTimePage,
      WebAnimationsSimultaneousPage,
      WebAnimationsStaggeredChainingPage,
      WebAnimationStaggeredInfinitePage,
      WebAnimationsStaggeredTriggeringPage,
      CssValueTypeColorPage,
      CssValueTypeFilterPage,
      CssValueTypeLengthPage,
      CssValueTypeLengthComplexPage,
      CssValueTypeLengthSimplePage,
      CssValueTypePathPage,
      CssValueTypeShadowPage,
      CssValueTypeTransformComplexPage,
      CssValueTypeTransformSimplePage,
      WebAnimationValueTypeColorPage,
      WebAnimationValueTypeLength3dPage,
      WebAnimationValueTypeLengthComplexPage,
      WebAnimationValueTypeLengthSimplePage,
      WebAnimationValueTypePathPage,
      WebAnimationValueTypeShadowPage,
      WebAnimationValueTypeTransformComplexPage,
      WebAnimationValueTypeTransformSimplePage,
      KeyframedAnimationsPage,
      TransformTransitionsPage,
      TransformTransitionsJSBlockPage,
      MixBlendModeAnimationDifferencePage,
      MixBlendModeAnimationHuePage,
      MixBlendModeAnimationScreenPage,
      MixAnimationPropagatingIsolationPage,
      MicrosoftPerformancePage
    ]

    for page_class in page_classes:
      self.AddStory(page_class(
          page_set=self,
          shared_page_state_class=shared_page_state.SharedPageState))
