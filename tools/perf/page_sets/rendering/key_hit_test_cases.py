# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import polymer
from page_sets.rendering import story_tags


class PaperCalculatorHitTest(polymer.PolymerPage):
  # Generated from https://github.com/zqureshi/paper-calculator
  # vulcanize --inline --strip paper-calculator/demo.html
  URL = 'file://../key_hit_test_cases/paper-calculator-no-rendering.html'
  BASE_NAME = 'paper_calculator_hit_test'
  TAGS = [story_tags.KEY_HIT_TEST]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(PaperCalculatorHitTest, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=['--disable-top-sites', '--report-silk-details'])

  def RunPageInteractions(self, action_runner):
    return

  def PerformPageInteractions(self, action_runner):
    # pay cost of selecting tap target only once
    action_runner.ExecuteJavaScript('''
        window.__tapTarget = document.querySelector(
            'body /deep/ #outerPanels'
        ).querySelector(
            '#standard'
        ).shadowRoot.querySelector(
            'paper-calculator-key[label="5"]'
        )''')
    action_runner.WaitForJavaScriptCondition(
        'window.__tapTarget != null')

    for _ in range(100):
      self.TapButton(action_runner)

  def TapButton(self, action_runner):
    with action_runner.CreateInteraction('Action_TapAction'):
      action_runner.TapElement(element_function='''window.__tapTarget''')
