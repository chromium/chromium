# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark
from telemetry import benchmark
from telemetry import story
from telemetry.internal.backends.chrome_inspector import inspector_backend
from telemetry.internal.browser import tab as tab_module
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement

from page_sets.desktop_ui import omnibox_story
from page_sets.desktop_ui.ui_devtools_utils import (ClickOn, PressKey,
                                                    PLATFORM_ACCELERATOR,
                                                    InputText)
from page_sets.desktop_ui.browser_element_identifiers import (
    kAiModePageActionIconElementId, kOmniboxElementId)

import py_utils

AI_MODE_INPUT_ELEMENT_FUNCTION = '''
(function() {
  return document.querySelector('omnibox-aim-app').shadowRoot
        .querySelector('cr-composebox').shadowRoot
        .querySelector('#input');
})()
'''

WEB_CONTENTS_UMA = [
    'EventLatency.FirstGestureScrollUpdate.TotalLatency2',
    'EventLatency.GestureScrollUpdate.TotalLatency2',
    'Memory.GPU.PeakMemoryUsage2.Scroll',
    'Memory.GPU.PeakMemoryUsage2.PageLoad',
    'Graphics.Smoothness.PercentDroppedFrames3.AllAnimations',
    'Graphics.Smoothness.PercentDroppedFrames3.AllInteractions',
    'Graphics.Smoothness.PercentDroppedFrames3.AllSequences',
    'Graphics.Smoothness.PercentDroppedFrames4.AllAnimations',
    'Graphics.Smoothness.PercentDroppedFrames4.AllInteractions',
    'Graphics.Smoothness.PercentDroppedFrames4.AllSequences',
    'Omnibox.Popup.WebUI.ConstructionToFirstShownDuration',
    'Omnibox.Popup.WebUI.CrashRecovery',
    'Omnibox.Popup.WebUI.PageRemoteIsBoundOnFirstCall',
    'Omnibox.Popup.WebUI.PresenterShowLatency.ToPaint',
    'Omnibox.Popup.WebUI.RendererProcessGoneStatus',
    'Omnibox.Popup.WebUI.ResultChangedToRepaintLatency.ToPaint',
    'Omnibox.Popup.Aim.ConstructionToFirstShownDuration',
    'Omnibox.Popup.Aim.CrashRecovery',
    'Omnibox.Popup.Aim.PresenterShowLatency.ToPaint',
    'Omnibox.Popup.Aim.RendererProcessGoneStatus',
    'Omnibox.WebUI.CharTypedToRepaintLatency.ToPaint',
]


class OmniboxAIModePopupStory(omnibox_story.OmniboxStory):
  NAME = 'omnibox:aim_popup'
  URL = 'chrome://newtab'

  def WillStartTracing(self, chrome_trace_config):
    super().WillStartTracing(chrome_trace_config)
    chrome_trace_config.EnableUMAHistograms(*WEB_CONTENTS_UMA)

  def RunNavigateSteps(self, action_runner):
    super().RunNavigateSteps(action_runner)

    # Explicitly focus the omnibox.
    ClickOn(self._devtools, element_id=kOmniboxElementId)

    # Enter text into the omnibox.
    node_id = self.GetOmniboxNodeID()
    PressKey(self._devtools, node_id, 'Home')
    PressKey(self._devtools, node_id, 'A', PLATFORM_ACCELERATOR)
    InputText(self._devtools, node_id,
              'Entering some text to exercise the metrics')

    # Clear the omnibox by selecting all and backspacing.
    PressKey(self._devtools, node_id, 'Home')
    PressKey(self._devtools, node_id, 'A', PLATFORM_ACCELERATOR)
    PressKey(self._devtools, node_id, 'Backspace')

  def RunPageInteractions(self, action_runner):
    # Click the AI Mode button to open the popup.
    ClickOn(self._devtools, element_id=kAiModePageActionIconElementId)
    action_runner.Wait(2)

    # The popup is not a "tab", so we can't use Inspect().
    # We have to find its devtools context manually.
    browser = action_runner.tab.browser
    browser_backend = browser.browser_backend

    def _FindPopupContext():
      contexts = browser_backend.devtools_client.GetListOfInspectableContexts()
      for c in contexts:
        if 'omnibox_popup_aim.html' in c['url']:
          return c
      return None

    popup_context = py_utils.WaitFor(_FindPopupContext, timeout=10)
    if not popup_context:
      raise Exception('Could not find AIMode popup context')

    popup_inspector_backend = inspector_backend.InspectorBackend(
        browser_backend.devtools_client, popup_context)
    popup_tab = tab_module.Tab(popup_inspector_backend,
                               browser_backend.tab_list_backend, browser)
    popup_action_runner = popup_tab.action_runner

    # Tap the input field in the popup to focus it.
    popup_action_runner.TapElement(
        element_function=AI_MODE_INPUT_ELEMENT_FUNCTION)

    # Enter text into the input field in the popup.
    popup_action_runner.EnterText(text='why is there air?')


@benchmark.Info(
    emails=[
        'kylixrd@chromium.org', 'mahmadi@chromium.org', "avivkiss@chromium.org"
    ],
    component='UI>Browser>Omnibox',
    documentation_url=
    'https://chromium.googlesource.com/chromium/src/+/main/docs/speed/benchmark/harnesses/desktop_ui.md'
)
class OmniboxPerf(perf_benchmark.PerfBenchmark):
  """Measures the performance of omnibox interactions."""
  PLATFORM = 'desktop'
  SUPPORTED_PLATFORMS = [story.expectations.ALL_DESKTOP]
  # Do not schedule benchmark at this time.
  SCHEDULED = False

  def CreateStorySet(self, options):
    story_set = story.story_set.StorySet(
        archive_data_file='../data/desktop_ui.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)
    story_set.AddStory(OmniboxAIModePopupStory(story_set))
    return story_set

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.ChromeTraceCategoryFilter(
        filter_string='uma,disabled-by-default-histogram_samples')
    options = timeline_based_measurement.Options(category_filter)
    options.SetTimelineBasedMetrics(['umaMetric', 'tbmv3:uma_metrics'])
    return options

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-ui-devtools=0')
    options.AppendExtraBrowserArgs('--disable-field-trial-config')
    options.RemoveExtraBrowserArg('--enable-field-trial-config')
    options.AppendExtraBrowserArgs(
        '--enable-features=ui-debug-tools-enable-synthetic-events,'
        'WebUIOmniboxPopup,'
        'WebUIOmniboxAimPopup:AddContextButtonVariant/below_results,'
        'AiModeOmniboxEntryPoint')
    options.AppendExtraBrowserArgs(
        '--disable-features=AimServerEligibilityEnabled')

  @classmethod
  def Name(cls):
    return 'omnibox.aim.perf'
