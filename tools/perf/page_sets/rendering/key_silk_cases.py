# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry import story

from page_sets.rendering import rendering_story
from page_sets.system_health import platforms
from page_sets.rendering import story_tags


class KeySilkPage(rendering_story.RenderingStory):
  """ Base class for all key silk cases pages."""

  ABSTRACT_STORY = True
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.KEY_SILK]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(KeySilkPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(KeySilkPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)

  def RunPageInteractions(self, action_runner):
    # If a key silk case page wants to customize it actions, it should
    # overrides the PerformPageInteractions method instead of this method.
    self.PerformPageInteractions(action_runner)

  def PerformPageInteractions(self, action_runner):
    """ Perform interactions on page after navigate steps.
    Override this to define custom actions to be run after navigate steps.
    """
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class Page1(KeySilkPage):

  """ Why: Infinite scroll. Brings out all of our perf issues. """

  BASE_NAME = 'list_recycle_transform'
  URL = 'http://groupcloned.com/test/plain/list-recycle-transform.html'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='#scrollable')


class Page2(KeySilkPage):

  """ Why: Brings out layer management bottlenecks. """

  BASE_NAME = 'list_animation_simple'
  URL = 'file://../key_silk_cases/list_animation_simple.html'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('SimpleAnimation'):
      action_runner.Wait(2)


class Page3(KeySilkPage):

  """
  Why: Best-known method for fake sticky. Janks sometimes. Interacts badly with
  compositor scrolls.
  """

  BASE_NAME = 'sticky_using_webkit'
  # pylint: disable=line-too-long
  URL = 'http://groupcloned.com/test/plain/sticky-using-webkit-backface-visibility.html'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='#container')


class Page4(KeySilkPage):

  """
  Why: Card expansion: only the card should repaint, but in reality lots of
  storms happen.
  """

  BASE_NAME = 'card_expansion'
  URL = 'http://jsfiddle.net/3yDKh/15/show/'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CardExpansionAnimation'):
      action_runner.Wait(3)


class Page5(KeySilkPage):

  """
  Why: Card expansion with animated contents, using will-change on the card
  """

  BASE_NAME = 'card_expansion_animated'
  URL = 'http://jsfiddle.net/jx5De/14/show/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(Page5, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

    self.gpu_raster = True

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CardExpansionAnimation'):
      action_runner.Wait(4)


class Page6(KeySilkPage):

  """
  Why: Card fly-in: It should be fast to animate in a bunch of cards using
  margin-top and letting layout do the rest.
  """

  BASE_NAME = 'card_flying'
  URL = 'http://jsfiddle.net/3yDKh/16/show/'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CardFlyingAnimation'):
      action_runner.Wait(3)


class Page7(KeySilkPage):

  """
  Why: Image search expands a spacer div when you click an image to accomplish
  a zoomin effect. Each image has a layer. Even so, this triggers a lot of
  unnecessary repainting.
  """

  BASE_NAME = 'zoom_in_animation'
  URL = 'http://jsfiddle.net/R8DX9/4/show/'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ZoominAnimation'):
      action_runner.Wait(3)


class Page8(KeySilkPage):

  """
  Why: Swipe to dismiss of an element that has a fixed-position child that is
  its pseudo-sticky header. Brings out issues with layer creation and
  repainting.
  """

  BASE_NAME = 'swipe_to_dismiss'
  URL = 'http://jsfiddle.net/rF9Gh/7/show/'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('SwipeToDismissAnimation'):
      action_runner.Wait(3)


class Page9(KeySilkPage):

  """
  Why: Horizontal and vertical expansion of a card that is cheap to layout but
  costly to rasterize.
  """

  BASE_NAME = 'horizontal_vertical_expansion'
  URL = 'http://jsfiddle.net/TLXLu/3/show/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(Page9, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

    self.gpu_raster = True

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CardExpansionAnimation'):
      action_runner.Wait(4)


class Page10(KeySilkPage):

  """
  Why: Vertical Expansion of a card that is cheap to layout but costly to
  rasterize.
  """

  BASE_NAME = 'vertical_expansion'
  URL = 'http://jsfiddle.net/cKB9D/7/show/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(Page10, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

    self.gpu_raster = True

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CardExpansionAnimation'):
      action_runner.Wait(4)


class Page11(KeySilkPage):

  """
  Why: Parallax effect is common on photo-viewer-like applications, overloading
  software rasterization
  """

  BASE_NAME = 'parallax_effect'
  URL = 'http://jsfiddle.net/vBQHH/11/show/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(Page11, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

    self.gpu_raster = True

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ParallaxAnimation'):
      action_runner.Wait(4)


class Page12(KeySilkPage):

  """ Why: Addressing paint storms during coordinated animations. """

  BASE_NAME = 'coordinated_animation'
  URL = 'http://jsfiddle.net/ugkd4/10/show/'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CoordinatedAnimation'):
      action_runner.Wait(5)


class Page13(KeySilkPage):

  """ Why: Mask transitions are common mobile use cases. """

  BASE_NAME = 'mask_transition_animation'
  URL = 'http://jsfiddle.net/xLuvC/1/show/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(Page13, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

    self.gpu_raster = True

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('MaskTransitionAnimation'):
      action_runner.Wait(4)


class Page14(KeySilkPage):

  """ Why: Card expansions with images and text are pretty and common. """

  BASE_NAME = 'card_expansion_images_text'
  URL = 'http://jsfiddle.net/bNp2h/3/show/'

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):
    super(Page14, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args)

    self.gpu_raster = True

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CardExpansionAnimation'):
      action_runner.Wait(4)


class Page15(KeySilkPage):

  """ Why: Coordinated animations for expanding elements. """

  BASE_NAME = 'font_wipe'
  URL = 'file://../key_silk_cases/font_wipe.html'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CoordinatedAnimation'):
      action_runner.Wait(5)


class Page16(KeySilkPage):

  BASE_NAME = 'swipe_action'
  URL = 'file://../key_silk_cases/inbox_app.html?swipe_to_dismiss'

  def SwipeToDismiss(self, action_runner):
    with action_runner.CreateGestureInteraction('SwipeAction'):
      action_runner.SwipeElement(
          left_start_ratio=0.8, top_start_ratio=0.2,
          direction='left', distance=400, speed_in_pixels_per_second=5000,
          element_function='document.getElementsByClassName("message")[2]')

  def PerformPageInteractions(self, action_runner):
    self.SwipeToDismiss(action_runner)


class Page17(KeySilkPage):

  BASE_NAME = 'stress_hidey_bars'
  URL = 'file://../key_silk_cases/inbox_app.html?stress_hidey_bars'

  def PerformPageInteractions(self, action_runner):
    self.StressHideyBars(action_runner)

  def StressHideyBars(self, action_runner):
    with action_runner.CreateGestureInteraction(
        'ScrollAction', repeatable=True):
      action_runner.WaitForElement(selector='#messages')
      action_runner.ScrollElement(
        selector='#messages', direction='down', speed_in_pixels_per_second=200)
    with action_runner.CreateGestureInteraction(
        'ScrollAction', repeatable=True):
      action_runner.WaitForElement(selector='#messages')
      action_runner.ScrollElement(
          selector='#messages', direction='up', speed_in_pixels_per_second=200)
    with action_runner.CreateGestureInteraction(
        'ScrollAction', repeatable=True):
      action_runner.WaitForElement(selector='#messages')
      action_runner.ScrollElement(
          selector='#messages', direction='down',
          speed_in_pixels_per_second=200)


class Page18(KeySilkPage):

  BASE_NAME = 'toggle_drawer'
  URL = 'file://../key_silk_cases/inbox_app.html?toggle_drawer'

  def PerformPageInteractions(self, action_runner):
    for _ in range(6):
      self.ToggleDrawer(action_runner)

  def ToggleDrawer(self, action_runner):
    with action_runner.CreateInteraction('Action_TapAction', repeatable=True):
      action_runner.TapElement('#menu-button')
      action_runner.Wait(1)


class Page19(KeySilkPage):

  BASE_NAME = 'slide_drawer'
  URL = 'file://../key_silk_cases/inbox_app.html?slide_drawer'

  def ToggleDrawer(self, action_runner):
    with action_runner.CreateGestureInteraction('TapAction'):
      action_runner.TapElement('#menu-button')

    with action_runner.CreateInteraction('Wait'):
      action_runner.WaitForJavaScriptCondition('''
          document.getElementById("nav-drawer").active &&
          document.getElementById("nav-drawer").children[0]
              .getBoundingClientRect().left == 0''')

  def RunNavigateSteps(self, action_runner):
    super(Page19, self).RunNavigateSteps(action_runner)
    action_runner.Wait(2)
    self.ToggleDrawer(action_runner)

  def PerformPageInteractions(self, action_runner):
    self.SlideDrawer(action_runner)

  def SlideDrawer(self, action_runner):
    with action_runner.CreateInteraction('Action_SwipeAction'):
      action_runner.SwipeElement(
          left_start_ratio=0.8, top_start_ratio=0.2,
          direction='left', distance=200,
          element_function='document.getElementById("nav-drawer").children[0]')
      action_runner.WaitForJavaScriptCondition(
          '!document.getElementById("nav-drawer").active')


class Page20(KeySilkPage):

  """ Why: Shadow DOM infinite scrolling. """

  BASE_NAME = 'infinite_scrolling'
  URL = 'file://../key_silk_cases/infinite_scrolling.html'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(
          selector='#container', speed_in_pixels_per_second=5000)


class GwsExpansionPage(KeySilkPage):

  """Abstract base class for pages that expand Google knowledge panels."""

  ABSTRACT_STORY = True

  def NavigateWait(self, action_runner):
    super(GwsExpansionPage, self).RunNavigateSteps(action_runner)
    action_runner.Wait(3)

  def ExpandKnowledgeCard(self, action_runner):
    # expand card
    with action_runner.CreateInteraction('Action_TapAction'):
      action_runner.TapElement(
          element_function='document.getElementsByClassName("vk_arc")[0]')
      action_runner.Wait(2)

  def ScrollKnowledgeCardToTop(self, action_runner, card_id):
    # scroll until the knowledge card is at the top
    action_runner.ExecuteJavaScript(
        "document.getElementById({{ card_id }}).scrollIntoView()",
        card_id=card_id)

  def PerformPageInteractions(self, action_runner):
    self.ExpandKnowledgeCard(action_runner)


class GwsGoogleExpansion(GwsExpansionPage):

  """ Why: Animating height of a complex content card is common. """

  BASE_NAME = 'gws_google_expansion'
  URL = 'http://www.google.com/#q=google'

  def RunNavigateSteps(self, action_runner):
    self.NavigateWait(action_runner)
    self.ScrollKnowledgeCardToTop(action_runner, 'kno-result')


class GwsBoogieExpansion(GwsExpansionPage):

  """ Why: Same case as Google expansion but text-heavy rather than image. """

  BASE_NAME = 'gws_boogie_expansion'
  URL = 'https://www.google.com/search?hl=en&q=define%3Aboogie'

  def RunNavigateSteps(self, action_runner):
    self.NavigateWait(action_runner)
    self.ScrollKnowledgeCardToTop(action_runner, 'rso')


class Page22(KeySilkPage):

  BASE_NAME = 'basic_stream'
  URL = 'http://plus.google.com/app/basic/stream'

  def RunNavigateSteps(self, action_runner):
    super(Page22, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementsByClassName("fHa").length > 0')
    action_runner.Wait(2)

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(selector='#mainContent')


class Page23(KeySilkPage):

  """
  Why: Physical simulation demo that does a lot of element.style mutation
  triggering JS and recalc slowness
  """

  BASE_NAME = 'physical_simulation'
  URL = 'file://../key_silk_cases/physical_simulation.html'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction',
                                                repeatable=True):
      action_runner.ScrollPage(
          distance_expr='window.innerHeight / 2',
          direction='down',
          use_touch=True)
    with action_runner.CreateGestureInteraction('ScrollAction',
                                                repeatable=True):
      action_runner.Wait(1)


class Page24(KeySilkPage):

  """
  Why: Google News: this iOS version is slower than accelerated scrolling
  """

  BASE_NAME = 'google_news_ios'
  URL = 'http://mobile-news.sandbox.google.com/news/pt0?scroll'

  def RunNavigateSteps(self, action_runner):
    super(Page24, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById(":h") != null')
    action_runner.Wait(1)

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollElement(
          element_function='document.getElementById(":5")',
          distance=2500,
          use_touch=True)


class Page25(KeySilkPage):

  BASE_NAME = 'mobile_news_sandbox'
  URL = 'http://mobile-news.sandbox.google.com/news/pt0?swipe'

  def RunNavigateSteps(self, action_runner):
    super(Page25, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementById(":h") != null')
    action_runner.Wait(1)

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('SwipeAction', repeatable=True):
      action_runner.SwipeElement(
          direction='left', distance=100,
          element_function='document.getElementById(":f")')
    with action_runner.CreateGestureInteraction('SwipeAction', repeatable=True):
      action_runner.Wait(1)


class Page26(KeySilkPage):

  """ Why: famo.us twitter demo """

  BASE_NAME = 'famo_us_twitter_demo'
  URL = 'http://s.codepen.io/befamous/fullpage/pFsqb?scroll'

  def RunNavigateSteps(self, action_runner):
    super(Page26, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.getElementsByClassName("tweet").length > 0')
    action_runner.Wait(1)

  def PerformPageInteractions(self, action_runner):
    # Add a touch-action: none because this page prevent defaults all
    # touch moves.
    action_runner.ExecuteJavaScript('''
        var style = document.createElement("style");
        document.head.appendChild(style);
        style.sheet.insertRule("body { touch-action: none }", 0);
        ''')
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(distance=5000)


class SVGIconRaster(KeySilkPage):

  """ Why: Mutating SVG icons; these paint storm and paint slowly. """

  BASE_NAME = 'svg_icon_raster'
  URL = 'http://wiltzius.github.io/shape-shifter/'

  def RunNavigateSteps(self, action_runner):
    super(SVGIconRaster, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'loaded = true')
    action_runner.Wait(1)

  def PerformPageInteractions(self, action_runner):
    for i in range(9):
      button_func = ('document.getElementById("demo").$.'
                     'buttons.children[%d]') % i
      with action_runner.CreateInteraction('Action_TapAction', repeatable=True):
        action_runner.TapElement(element_function=button_func)
        action_runner.Wait(1)


class UpdateHistoryState(KeySilkPage):

  """ Why: Modern apps often update history state, which currently is janky."""

  BASE_NAME = 'update_history_state'
  URL = 'file://../key_silk_cases/pushState.html'

  def RunNavigateSteps(self, action_runner):
    super(UpdateHistoryState, self).RunNavigateSteps(action_runner)
    action_runner.ExecuteJavaScript('''
        window.requestAnimationFrame(function() {
            window.__history_state_loaded = true;
          });
        ''')
    action_runner.WaitForJavaScriptCondition(
        'window.__history_state_loaded == true;')

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('animation_interaction'):
      action_runner.Wait(5) # JS runs the animation continuously on the page


class SilkFinance(KeySilkPage):

  """ Why: Some effects repaint the page, possibly including plenty of text. """

  BASE_NAME = 'silk_finance'
  URL = 'file://../key_silk_cases/silk_finance.html'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('animation_interaction'):
      action_runner.Wait(10) # animation runs automatically


class PolymerTopeka(KeySilkPage):

  """ Why: Sample Polymer app. """

  BASE_NAME = 'polymer_topeka'
  URL = 'https://polymer-topeka.appspot.com/'

  def PerformPageInteractions(self, action_runner):
    profile = 'html /deep/ topeka-profile /deep/ '
    first_name = profile + 'paper-input#first /deep/ input'
    action_runner.WaitForElement(selector=first_name)
    # Input First Name:
    action_runner.ExecuteJavaScript('''
        var fn = document.querySelector({{ first_name }});
        fn.value = 'Chrome';
        fn.fire('input');''',
        first_name=first_name)
    # Input Last Initial:
    action_runner.ExecuteJavaScript('''
        var li = document.querySelector({{ selector }});
        li.value = 'E';
        li.fire('input');''',
        selector='%s paper-input#last /deep/ input' % profile)
    with action_runner.CreateInteraction('animation_interaction'):
      # Click the check-mark to login:
      action_runner.ExecuteJavaScript('''
          window.topeka_page_transitions = 0;
          [].forEach.call(document.querySelectorAll(
              'html /deep/ core-animated-pages'), function(p){
                  p.addEventListener(
                      'core-animated-pages-transition-end', function(e) {
                          window.topeka_page_transitions++;
                      });
              });
          document.querySelector({{ selector }}).fire('tap')''',
          selector='%s paper-fab' % profile)
      # Wait for category list to animate in:
      action_runner.WaitForJavaScriptCondition('''
          window.topeka_page_transitions === 1''')
      # Click a category to start a quiz:
      action_runner.ExecuteJavaScript('''
          document.querySelector('\
              html /deep/ core-selector.category-list').fire(
              'tap',1,document.querySelector('html /deep/ \
                      div.category-item.red-theme'));''')
      # Wait for the category splash to animate in:
      action_runner.WaitForJavaScriptCondition('''
          window.topeka_page_transitions === 2''')
      # Click to start the quiz:
      action_runner.ExecuteJavaScript('''
          document.querySelector('html /deep/ topeka-category-front-page /deep/\
              paper-fab').fire('tap');''')
      action_runner.WaitForJavaScriptCondition('''
          window.topeka_page_transitions === 4''')
      # Input a mostly correct answer:
      action_runner.ExecuteJavaScript('''
          document.querySelector('html /deep/ topeka-quiz-fill-blank /deep/\
              input').value = 'arkinsaw';
          document.querySelector('html /deep/ topeka-quiz-fill-blank /deep/\
              input').fire('input');
          document.querySelector('html /deep/ topeka-quizzes /deep/ \
              paper-fab').fire('tap');''')
      action_runner.WaitForJavaScriptCondition('''
          window.topeka_page_transitions === 6''')

class Masonry(KeySilkPage):

  """ Why: Popular layout hack. """

  BASE_NAME = 'masonry'
  URL = 'file://../key_silk_cases/masonry.html'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('animation_interaction'):
      action_runner.ExecuteJavaScript('window.brick()')
      action_runner.WaitForJavaScriptCondition('window.done')


# TODO(crbug.com/760553):remove this class after smoothness.key_silk_cases
# benchmark is completely replaced by rendering benchmarks
class KeySilkCasesPageSet(story.StorySet):

  """ Pages hand-picked for project Silk. """

  def __init__(self):
    super(KeySilkCasesPageSet, self).__init__(
      archive_data_file='../data/key_silk_cases.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    page_classes = [
      Page1,
      Page2,
      Page3,
      Page4,
      Page5,
      Page6,
      Page7,
      Page8,
      Page9,
      Page10,
      Page11,
      Page12,
      Page13,
      Page14,
      Page15,
      Page16,
      Page17,
      # Missing frames during tap interaction; crbug.com/446332
      Page18,
      Page19,
      Page20,
      GwsGoogleExpansion,
      GwsBoogieExpansion,
      # Times out on Windows; crbug.com/338838,
      Page22,
      Page23,
      Page24,
      Page25,
      Page26,
      SVGIconRaster,
      UpdateHistoryState,
      SilkFinance,
      # Flaky interaction steps on Android; crbug.com/507865,
      PolymerTopeka,
      Masonry
    ]

    for page_class in page_classes:
      self.AddStory(page_class(self))

    for page in self:
      assert (page.__class__.RunPageInteractions ==
              KeySilkPage.RunPageInteractions), (
              'Pages in this page set must not override KeySilkPage\' '
              'RunPageInteractions method.')
