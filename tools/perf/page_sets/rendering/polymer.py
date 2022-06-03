# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry.util import js_template

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class PolymerPage(rendering_story.RenderingStory):
  """ Base class for all polymer pages.

  Args:
    run_no_page_interactions: whether the page will run any interactions after
      navigate steps.
  """
  ABSTRACT_STORY = True
  SUPPORTED_PLATFORMS = platforms.MOBILE_ONLY
  TAGS = [story_tags.POLYMER]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedMobilePageState,
               name_suffix='',
               extra_browser_args=None):

    super(PolymerPage,
          self).__init__(page_set=page_set,
                         shared_page_state_class=shared_page_state_class,
                         name_suffix=name_suffix,
                         extra_browser_args=extra_browser_args)

    self.script_to_evaluate_on_commit = '''
      document.addEventListener("polymer-ready", function() {
        window.__polymer_ready = true;
      });
    '''

  def RunPageInteractions(self, action_runner):
    # If a polymer page wants to customize its actions, it should
    # override the PerformPageInteractions method instead of this method.
    self.PerformPageInteractions(action_runner)

  def PerformPageInteractions(self, action_runner):
    """ Override this to perform actions after the page has navigated. """
    pass

  def RunNavigateSteps(self, action_runner):
    super(PolymerPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.__polymer_ready')


class PolymerCalculatorPage(PolymerPage):
  BASE_NAME = 'paper_calculator'
  URL = 'http://www.polymer-project.org/components/paper-calculator/demo.html'

  def PerformPageInteractions(self, action_runner):
    self.TapButton(action_runner)
    self.SlidePanel(action_runner)

  def TapButton(self, action_runner):
    with action_runner.CreateInteraction('PolymerAnimation', repeatable=True):
      action_runner.TapElement(element_function='''
          document.querySelector(
              'body /deep/ #outerPanels'
          ).querySelector(
              '#standard'
          ).shadowRoot.querySelector(
              'paper-calculator-key[label="5"]'
          )''')
      action_runner.Wait(2)

  def SlidePanel(self, action_runner):
    # only bother with this interaction if the drawer is hidden
    opened = action_runner.EvaluateJavaScript('''
        (function() {
          var outer = document.querySelector("body /deep/ #outerPanels");
          return outer.opened || outer.wideMode;
          }());''')
    if not opened:
      with action_runner.CreateInteraction('PolymerAnimation', repeatable=True):
        action_runner.SwipeElement(
            left_start_ratio=0.1, top_start_ratio=0.2,
            direction='left', distance=300, speed_in_pixels_per_second=5000,
            element_function='''
                document.querySelector(
                  'body /deep/ #outerPanels'
                ).querySelector(
                  '#advanced'
                ).shadowRoot.querySelector(
                  '.handle-bar'
                )''')
        action_runner.WaitForJavaScriptCondition('''
            var outer = document.querySelector("body /deep/ #outerPanels");
            outer.opened || outer.wideMode;''')


class PolymerShadowPage(PolymerPage):
  BASE_NAME = 'paper_shadow'
  URL = 'http://www.polymer-project.org/components/paper-shadow/demo.html'

  def PerformPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ScrollAndShadowAnimation'):
      action_runner.ExecuteJavaScript(
          "document.getElementById('fab').scrollIntoView()")
      action_runner.Wait(5)
      self.AnimateShadow(action_runner, 'card')
      #FIXME(wiltzius) disabling until this issue is fixed:
      # https://github.com/Polymer/paper-shadow/issues/12
      #self.AnimateShadow(action_runner, 'fab')

  def AnimateShadow(self, action_runner, eid):
    for i in range(1, 6):
      action_runner.ExecuteJavaScript(
          'document.getElementById({{ eid }}).z = {{ i }}', eid=eid, i=i)
      action_runner.Wait(1)


class PolymerSampler(PolymerPage):
  """Page exercising interactions with a single Paper Sampler subpage.

  Args:
    page_set: Page set to inforporate this page into.
    anchor: string indicating which subpage to load (matches the element
        type that page is displaying)
    scrolling_page: Whether scrolling the content pane is relevant to this
        content page or not.
  """

  ABSTRACT_STORY = True
  SCROLLING_PAGE = False
  IFRAME_JS = 'document'

  def RunNavigateSteps(self, action_runner):
    super(PolymerSampler, self).RunNavigateSteps(action_runner)
    action_runner.ExecuteJavaScript("""
        window.Polymer.whenPolymerReady(function() {
          {{ @iframe }}.contentWindow.Polymer.whenPolymerReady(function() {
            window.__polymer_ready = true;
          })
        });
        """, iframe=self.IFRAME_JS)
    action_runner.WaitForJavaScriptCondition(
        'window.__polymer_ready')

  def PerformPageInteractions(self, action_runner):
    #TODO(wiltzius) Add interactions for input elements and shadow pages
    if self.SCROLLING_PAGE:
      # Only bother scrolling the page if its been marked as worthwhile
      self.ScrollContentPane(action_runner)
    self.TouchEverything(action_runner)

  def ScrollContentPane(self, action_runner):
    element_function = (self.IFRAME_JS + '.querySelector('
        '"core-scroll-header-panel").$.mainContainer')
    with action_runner.CreateInteraction('Scroll_Page', repeatable=True):
      action_runner.ScrollElement(use_touch=True,
                                  direction='down',
                                  distance='900',
                                  element_function=element_function)
    with action_runner.CreateInteraction('Scroll_Page', repeatable=True):
      action_runner.ScrollElement(use_touch=True,
                                  direction='up',
                                  distance='900',
                                  element_function=element_function)

  def TouchEverything(self, action_runner):
    tappable_types = [
        'paper-button',
        'paper-checkbox',
        'paper-fab',
        'paper-icon-button',
        # crbug.com/394756
        # 'paper-radio-button',
        'paper-tab',
        'paper-toggle-button',
        'x-shadow',
        ]
    for tappable_type in tappable_types:
      self.DoActionOnWidgetType(action_runner, tappable_type, self.TapWidget)
    swipeable_types = ['paper-slider']
    for swipeable_type in swipeable_types:
      self.DoActionOnWidgetType(action_runner, swipeable_type, self.SwipeWidget)

  def DoActionOnWidgetType(self, action_runner, widget_type, action_function):
    # Find all widgets of this type, but skip any that are disabled or are
    # currently active as they typically don't produce animation frames.
    element_list_query = js_template.Render(
        '{{ @iframe }}.querySelectorAll({{ selector }})',
        iframe=self.IFRAME_JS,
        selector='body %s:not([disabled]):not([active])' % widget_type)

    roles_count_query = element_list_query + '.length'
    for i in range(action_runner.EvaluateJavaScript(roles_count_query)):
      element_query = js_template.Render(
        '{{ @query }}[{{ i }}]', query=element_list_query, i=i)
      if action_runner.EvaluateJavaScript(
          element_query + '.offsetParent != null'):
        # Only try to tap on visible elements (offsetParent != null)
        action_runner.ExecuteJavaScript(element_query + '.scrollIntoView()')
        action_runner.Wait(1) # wait for page to settle after scrolling
        action_function(action_runner, element_query)

  def TapWidget(self, action_runner, element_function):
    with action_runner.CreateInteraction('Tap_Widget', repeatable=True):
      action_runner.TapElement(element_function=element_function)
      action_runner.Wait(1) # wait for e.g. animations on the widget

  def SwipeWidget(self, action_runner, element_function):
    with action_runner.CreateInteraction('Swipe_Widget'):
      action_runner.SwipeElement(element_function=element_function,
                                 left_start_ratio=0.75,
                                 speed_in_pixels_per_second=300)


class PaperButtonPage(PolymerSampler):
  BASE_NAME = 'paper_button'
  URL = 'http://www.polymer-project.org/components/paper-button/demo.html'


class PaperCheckboxPage(PolymerSampler):
  BASE_NAME = 'paper_checkbox'
  URL = 'http://www.polymer-project.org/components/paper-checkbox/demo.html'


class PaperFabPage(PolymerSampler):
  BASE_NAME = 'paper_fab'
  URL = 'http://www.polymer-project.org/components/paper-fab/demo.html'


class PaperIconButtonPage(PolymerSampler):
  BASE_NAME = 'paper_icon_button'
  URL = 'http://www.polymer-project.org/components/paper-icon-button/demo.html'


# crbug.com/394756
# class PaperRadioButtonPage(PolymerSampler):
#   BASE_NAME = 'paper_radio_button'
#   URL = 'http://www.polymer-project.org/components/paper-radio-button/demo.html'


# FIXME(wiltzius) Disabling x-shadow until this issue is fixed:
# https://github.com/Polymer/paper-shadow/issues/12
# class PaperShadowPage(PolymerSampler):
#   BASE_NAME = 'paper_shadow'
#   URL = 'http://www.polymer-project.org/components/paper-shadow/demo.html'


class PaperTabsPage(PolymerSampler):
  BASE_NAME = 'paper_tabs'
  URL = 'http://www.polymer-project.org/components/paper-tabs/demo.html'


class PaperToggleButtonPage(PolymerSampler):
  BASE_NAME = 'paper_toggle_button'
  URL = 'http://www.polymer-project.org/components/paper-toggle-button/demo.html'


class CoreScrollHeaderPanelPage(PolymerSampler):
  BASE_NAME = 'core_scroll_header_panel'
  URL = 'http://www.polymer-project.org/components/core-scroll-header-panel/demo.html'
  SCROLLING_PAGE = True
