# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms

class ToughCompositorPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.TOUGH_COMPOSITOR]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(ToughCompositorPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=['--report-silk-details', '--disable-top-sites',
                            '--disable-gpu-rasterization'])

  def RunNavigateSteps(self, action_runner):
    super(ToughCompositorPage, self).RunNavigateSteps(action_runner)
    # TODO(epenner): Remove this wait (http://crbug.com/366933)
    action_runner.Wait(5)


class ToughCompositorScrollPage(ToughCompositorPage):
  ABSTRACT_STORY = True

  def RunPageInteractions(self, action_runner):
    # Make the scroll longer to reduce noise.
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(direction='down', speed_in_pixels_per_second=300)


# Why: Baseline CC scrolling page. A long page with only text. """
class CCScrollTextPage(ToughCompositorScrollPage):
  BASE_NAME = 'cc_scroll_text_only'
  URL = 'http://jsbin.com/pixavefe/1/quiet?CC_SCROLL_TEXT_ONLY'


# Why: Baseline JS scrolling page. A long page with only text. """
class JSScrollTextPage(ToughCompositorScrollPage):
  BASE_NAME = 'js_scroll_text_only'
  URL = 'http://jsbin.com/dozirar/quiet?JS_SCROLL_TEXT_ONLY'


class ToughCompositorWaitPage(ToughCompositorPage):
  ABSTRACT_STORY = True

  def RunPageInteractions(self, action_runner):
    # We scroll back and forth a few times to reduce noise in the tests.
    with action_runner.CreateInteraction('Animation'):
      action_runner.Wait(8)


# Why: CC Poster circle animates many layers """
class CCPosterCirclePage(ToughCompositorWaitPage):
  BASE_NAME = 'cc_poster_circle'
  URL = 'http://jsbin.com/falefice/1/quiet?CC_POSTER_CIRCLE'
  TAGS = ToughCompositorWaitPage.TAGS + [story_tags.REPRESENTATIVE_WIN_DESKTOP]


# Why: JS poster circle animates/commits many layers """
class JSPosterCirclePage(ToughCompositorWaitPage):
  BASE_NAME = 'js_poster_circle'
  URL = 'http://jsbin.com/giqafofe/1/quiet?JS_POSTER_CIRCLE'
  TAGS = ToughCompositorWaitPage.TAGS + [story_tags.REPRESENTATIVE_WIN_DESKTOP]


# Why: JS invalidation does lots of uploads """
class JSFullScreenPage(ToughCompositorWaitPage):
  BASE_NAME = 'js_full_screen_invalidation'
  URL = 'http://jsbin.com/beqojupo/1/quiet?JS_FULL_SCREEN_INVALIDATION'


# Why: Creates a large number of new tilings """
class NewTilingsPage(ToughCompositorWaitPage):
  BASE_NAME = 'new_tilings'
  URL = 'http://jsbin.com/covoqi/1/quiet?NEW_TILINGS'
  TAGS = ToughCompositorWaitPage.TAGS + [
    story_tags.REPRESENTATIVE_MOBILE,
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]


# Why: CSS property update baseline """
class CSSOpacityPlusNLayers0(ToughCompositorWaitPage):
  BASE_NAME = 'css_opacity_plus_n_layers_0'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'css_opacity_plus_n_layers.html?layer_count=1&visible_layers=1')


# Why: CSS property change on 1 layer with 50%-ile layer count """
class CSSOpacityPlusNLayers50(ToughCompositorWaitPage):
  BASE_NAME = 'css_opacity_plus_n_layers_50'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'css_opacity_plus_n_layers.html?layer_count=31&visible_layers=10')


# Why: CSS property change on 1 layer with 75%-ile layer count """
class CSSOpacityPlusNLayers75(ToughCompositorWaitPage):
  BASE_NAME = 'css_opacity_plus_n_layers_75'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'css_opacity_plus_n_layers.html?layer_count=53&visible_layers=16')


# Why: CSS property change on 1 layer with 95%-ile layer count """
class CSSOpacityPlusNLayers95(ToughCompositorWaitPage):
  BASE_NAME = 'css_opacity_plus_n_layers_95'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'css_opacity_plus_n_layers.html?layer_count=144&visible_layers=29')

# Why: CSS property change on 1 layer with 99%-ile layer count """
class CSSOpacityPlusNLayers99(ToughCompositorWaitPage):
  BASE_NAME = 'css_opacity_plus_n_layers_99'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'css_opacity_plus_n_layers.html?layer_count=306&visible_layers=46')


# Why: JS driven CSS property change on 1 layer baseline """
class JSOpacityPlusNLayers0(ToughCompositorWaitPage):
  BASE_NAME = 'js_opacity_plus_n_layers_0'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'js_opacity_plus_n_layers.html?layer_count=1&visible_layers=1')


# Why: JS driven property change on 1 layer with 50%-ile layer count """
class JSOpacityPlusNLayers50(ToughCompositorWaitPage):
  BASE_NAME = 'js_opacity_plus_n_layers_50'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'js_opacity_plus_n_layers.html?layer_count=31&visible_layers=10')


# Why: JS driven property change on 1 layer with 75%-ile layer count """
class JSOpacityPlusNLayers75(ToughCompositorWaitPage):
  BASE_NAME = 'js_opacity_plus_n_layers_75'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'js_opacity_plus_n_layers.html?layer_count=53&visible_layers=16')


# Why: JS driven property change on 1 layer with 95%-ile layer count """
class JSOpacityPlusNLayers95(ToughCompositorWaitPage):
  BASE_NAME = 'js_opacity_plus_n_layers_95'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'js_opacity_plus_n_layers.html?layer_count=144&visible_layers=29')


# Why: JS driven property update with 99%-ile layer count """
class JSOpacityPlusNLayers99(ToughCompositorWaitPage):
  BASE_NAME = 'js_opacity_plus_n_layers_99'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'js_opacity_plus_n_layers.html?layer_count=306&visible_layers=46')


# Why: Painting 1 layer baseline """
class JSPaintPlusNLayers0(ToughCompositorWaitPage):
  BASE_NAME = 'js_paint_plus_n_layers_0'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'js_paint_plus_n_layers.html?layer_count=1&visible_layers=1')


# Why: Painting 1 layer with 50%-ile layer count"""
class JSPaintPlusNLayers50(ToughCompositorWaitPage):
  BASE_NAME = 'js_paint_plus_n_layers_50'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'js_paint_plus_n_layers.html?layer_count=31&visible_layers=10')


# Why: Painting 1 layer with 75%-ile layer count"""
class JSPaintPlusNLayers75(ToughCompositorWaitPage):
  BASE_NAME = 'js_paint_plus_n_layers_75'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'js_paint_plus_n_layers.html?layer_count=53&visible_layers=16')


# Why: Painting 1 layer with 95%-ile layer count"""
class JSPaintPlusNLayers95(ToughCompositorWaitPage):
  BASE_NAME = 'js_paint_plus_n_layers_95'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'js_paint_plus_n_layers.html?layer_count=144&visible_layers=29')


# Why: Painting 1 layer with 99%-ile layer count"""
class JSPaintPlusNLayers99(ToughCompositorWaitPage):
  BASE_NAME = 'js_paint_plus_n_layers_99'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'js_paint_plus_n_layers.html?layer_count=306&visible_layers=46')


class InfiniteScrollElementNLayersPage(ToughCompositorPage):
  ABSTRACT_STORY = True
  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(
          selector='#container', speed_in_pixels_per_second=1000)


# Why: Infinite non-root scroller with 1 layer baseline"""
class InfiniteScrollElementNLayers0(InfiniteScrollElementNLayersPage):
  BASE_NAME = 'infinite_scroll_element_n_layers_0'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_element_n_layers.html?layer_count=1')


# Why: Infinite non-root scroller with 50%-ile layer count"""
class InfiniteScrollElementNLayers50(InfiniteScrollElementNLayersPage):
  BASE_NAME = 'infinite_scroll_element_n_layers_50'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_element_n_layers.html?layer_count=31')


# Why: Infinite non-root scroller with 75%-ile layer count"""
class InfiniteScrollElementNLayers75(InfiniteScrollElementNLayersPage):
  BASE_NAME = 'infinite_scroll_element_n_layers_75'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_element_n_layers.html?layer_count=53')


# Why: Infinite non-root scroller with 95%-ile layer count"""
class InfiniteScrollElementNLayers95(InfiniteScrollElementNLayersPage):
  BASE_NAME = 'infinite_scroll_element_n_layers_95'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_element_n_layers.html?layer_count=144')


# Why: Infinite non-root scroller with 99%-ile layer count"""
class InfiniteScrollElementNLayers99(InfiniteScrollElementNLayersPage):
  BASE_NAME = 'infinite_scroll_element_n_layers_99'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_element_n_layers.html?layer_count=306')


class InfiniteScrollRootNLayersPage(ToughCompositorPage):
  ABSTRACT_STORY = True
  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(direction='down',
                               speed_in_pixels_per_second=1000)


# Why: Infinite root scroller with 1 layer baseline """
class InfiniteScrollRootNLayers0(InfiniteScrollRootNLayersPage):
  BASE_NAME = 'infinite_scroll_root_n_layers_0'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_root_n_layers.html?layer_count=1')


# Why: Infinite root scroller with 50%-ile layer count"""
class InfiniteScrollRootNLayers50(InfiniteScrollRootNLayersPage):
  BASE_NAME = 'infinite_scroll_root_n_layers_50'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_root_n_layers.html?layer_count=31')


# Why: Infinite root scroller with 75%-ile layer count"""
class InfiniteScrollRootNLayers75(InfiniteScrollRootNLayersPage):
  BASE_NAME = 'infinite_scroll_root_n_layers_75'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_root_n_layers.html?layer_count=53')


# Why: Infinite root scroller with 95%-ile layer count"""
class InfiniteScrollRootNLayers95(InfiniteScrollRootNLayersPage):
  BASE_NAME = 'infinite_scroll_root_n_layers_95'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_root_n_layers.html?layer_count=144')


# Why: Infinite root scroller with 99%-ile layer count"""
class InfiniteScrollRootNLayers99(InfiniteScrollRootNLayersPage):
  BASE_NAME = 'infinite_scroll_root_n_layers_99'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_root_n_layers.html?layer_count=306')


# Why: Infinite root scroller + fixed element, with 1 layer baseline """
class InfiniteScrollRootFixedNLayers0(InfiniteScrollRootNLayersPage):
  BASE_NAME = 'infinite_scroll_root_fixed_n_layers_0'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_root_fixed_n_layers.html?layer_count=1')


# Why: Infinite root scroller + fixed element, with 50%-ile layer count"""
class InfiniteScrollRootFixedNLayers50(InfiniteScrollRootNLayersPage):
  BASE_NAME = 'infinite_scroll_root_fixed_n_layers_50'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_root_fixed_n_layers.html?layer_count=31')


# Why: Infinite root scroller + fixed element, with 75%-ile layer count"""
class InfiniteScrollRootFixedNLayers75(InfiniteScrollRootNLayersPage):
  BASE_NAME = 'infinite_scroll_root_fixed_n_layers_75'
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_root_fixed_n_layers.html?layer_count=53')


# Why: Infinite root scroller + fixed element, with 95%-ile layer count"""
class InfiniteScrollRootFixedNLayers95(InfiniteScrollRootNLayersPage):
  BASE_NAME = 'infinite_scroll_root_fixed_n_layers_95'
  SUPPORTED_PLATFORMS = platforms.NO_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_root_fixed_n_layers.html?layer_count=144')


# Why: Infinite root scroller + fixed element, with 99%-ile layer count"""
class InfiniteScrollRootFixedNLayers99(InfiniteScrollRootNLayersPage):
  BASE_NAME = 'infinite_scroll_root_fixed_n_layers_99'
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS
  URL = ('file://../../../../chrome/test/data/perf/tough_compositor_cases/'
         'infinite_scroll_root_fixed_n_layers.html?layer_count=306')
