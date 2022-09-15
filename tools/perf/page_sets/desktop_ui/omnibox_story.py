# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.desktop_ui.browser_element_identifiers import \
    kOmniboxElementId
from page_sets.desktop_ui.multitab_story import MultiTabStory
from page_sets.desktop_ui.ui_devtools_utils import \
    PLATFORM_ACCELERATOR, InputText, PressKey

OMNIBOX_BENCHMARK_UMA = [
    'Omnibox.CharTypedToRepaintLatency',
    'Omnibox.CharTypedToRepaintLatency.ToPaint',
    'Omnibox.QueryTime2.0',
    'Omnibox.QueryTime2.1',
    'Omnibox.QueryTime2.2',
    'Omnibox.PaintTime',
]


class OmniboxStory(MultiTabStory):
  """Base class for omnibox stories"""
  URL_LIST = ['about:blank']
  # URL must be set to an external link in order to trigger a WPR download.
  URL = 'https://google.com'
  WAIT_FOR_NETWORK_QUIESCENCE = False

  def GetOmniboxNodeID(self):
    return self._devtools.QueryNodes('id:%s' % kOmniboxElementId)[0]

  def WillStartTracing(self, chrome_trace_config):
    super(OmniboxStory, self).WillStartTracing(chrome_trace_config)
    chrome_trace_config.EnableUMAHistograms(*OMNIBOX_BENCHMARK_UMA)


class OmniboxStorySearch(OmniboxStory):
  NAME = 'omnibox:search'

  def RunPageInteractions(self, action_runner):
    node_id = self.GetOmniboxNodeID()
    PressKey(self._devtools, node_id, 'A', PLATFORM_ACCELERATOR)
    # |InputText| will enter each character in the string one at a time, so
    # this will test multiple cycles of suggestion generation and drawing.
    InputText(self._devtools, node_id, 'food near me')
    PressKey(self._devtools, node_id, 'ArrowDown')
    PressKey(self._devtools, node_id, 'ArrowDown')
    PressKey(self._devtools, node_id, 'Return')
    action_runner.Wait(3)


class OmniboxStoryPedal(OmniboxStory):
  NAME = 'omnibox:pedal'

  def RunPageInteractions(self, action_runner):
    node_id = self.GetOmniboxNodeID()
    PressKey(self._devtools, node_id, 'A', PLATFORM_ACCELERATOR)
    InputText(self._devtools, node_id, 'open incognito window')
    PressKey(self._devtools, node_id, 'Tab')
    PressKey(self._devtools, node_id, 'Return')
    action_runner.Wait(3)


class OmniboxStoryScopedSearch(OmniboxStory):
  NAME = 'omnibox:scoped_search'

  def RunPageInteractions(self, action_runner):
    node_id = self.GetOmniboxNodeID()
    PressKey(self._devtools, node_id, 'A', PLATFORM_ACCELERATOR)
    InputText(self._devtools, node_id, 'google.com')
    PressKey(self._devtools, node_id, 'Tab')
    InputText(self._devtools, node_id, 'food near me')
    PressKey(self._devtools, node_id, 'Return')
    action_runner.Wait(3)
