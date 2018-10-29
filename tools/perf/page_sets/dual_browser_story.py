# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import re
import urllib

from telemetry import story as story_module
# TODO(perezju): Remove references to telementry.internal when
# https://github.com/catapult-project/catapult/issues/2102 is resolved.
from telemetry.internal.browser import browser_finder
from telemetry.internal.browser import browser_finder_exceptions
from telemetry.util import wpr_modes

from page_sets.top_10_mobile import URL_LIST


GOOGLE_SEARCH = 'https://www.google.co.uk/search?'

SEARCH_QUERIES = [
  'science',
  'cat pictures',
  '1600 Amphitheatre Pkwy, Mountain View, CA',
  'tom hanks',
  'weather 94110',
  'goog',
  'population of california',
  'sfo jfk flights',
  'movies 94110',
  'tip on 100 bill'
]


def _OptionsForBrowser(browser_type, finder_options):
  """Return options used to get a browser of the given type.

  TODO(perezju): Currently this clones the finder_options passed via the
  command line to telemetry. When browser_options are split appart from
  finder_options (crbug.com/570348) we will be able to construct our own
  browser_options as needed.
  """
  finder_options = finder_options.Copy()
  finder_options.browser_type = browser_type
  finder_options.browser_executable = None
  finder_options.browser_options.browser_type = browser_type

  # TODO(crbug.com/881469): remove this once Webview support surface
  # synchronization and viz.
  if browser_type and 'android-webview' in browser_type:
    finder_options.browser_options.AppendExtraBrowserArgs(
        '--disable-features=SurfaceSynchronization,VizDisplayCompositor')

  return finder_options


class MultiBrowserSharedState(story_module.SharedState):
  def __init__(self, test, finder_options, story_set):
    """A shared state to run a test involving multiple browsers.

    The story_set is expected to include SinglePage instances (class defined
    below) mapping each page to a browser on which to run. The state
    requires at least one page to run on the 'default' browser, i.e. the
    browser selected from the command line by the user.
    """
    super(MultiBrowserSharedState, self).__init__(
        test, finder_options, story_set)
    self._platform = None
    self._story_set = story_set
    self._possible_browsers = {}
    # We use an ordered dict to record the order in which browsers appear on
    # the story set. However, browsers are not created yet.
    self._browsers_created = False
    self._browsers = collections.OrderedDict(
        (s.browser_type, None) for s in story_set)
    self._current_story = None
    self._current_browser = None
    self._current_tab = None

    possible_browser = self._PrepareBrowser('default', finder_options)
    if not possible_browser:
      raise browser_finder_exceptions.BrowserFinderException(
          'No browser found.\n\nAvailable browsers:\n%s\n' %
          '\n'.join(browser_finder.GetAllAvailableBrowserTypes(finder_options)))

    extra_browser_types = set(story.browser_type for story in story_set)
    extra_browser_types.remove('default')  # Must include 'default' browser.
    for browser_type in extra_browser_types:
      finder_options_copy = _OptionsForBrowser(browser_type, finder_options)
      if not self._PrepareBrowser(browser_type, finder_options_copy):
        logging.warning(
          'Cannot run %s (%s) because %s browser is not available',
          test.__name__, str(test), browser_type)
        logging.warning('Install %s to be able to run the test.', browser_type)
        raise Exception("Browser not available, unable to run benchmark.")

    # TODO(crbug/404771): Move network controller options out of
    # browser_options and into finder_options.
    browser_options = finder_options.browser_options
    if finder_options.use_live_sites:
      wpr_mode = wpr_modes.WPR_OFF
    elif browser_options.wpr_mode == wpr_modes.WPR_RECORD:
      wpr_mode = wpr_modes.WPR_RECORD
    else:
      wpr_mode = wpr_modes.WPR_REPLAY
    self._extra_wpr_args = browser_options.extra_wpr_args

    self.platform.network_controller.Open(wpr_mode)

  @property
  def current_tab(self):
    return self._current_tab

  @property
  def platform(self):
    return self._platform

  def _PrepareBrowser(self, browser_type, finder_options):
    """Add a browser to the dict of possible browsers.

    TODO(perezju): When available, use the GetBrowserForPlatform API instead.
    See: crbug.com/570348

    Returns:
      The possible browser if found, or None otherwise.
    """
    possible_browser = browser_finder.FindBrowser(finder_options)
    if possible_browser is None:
      return None

    if self._platform is None:
      self._platform = possible_browser.platform
    else:
      assert self._platform is possible_browser.platform
    self._possible_browsers[browser_type] = (
        possible_browser, finder_options.browser_options)
    return possible_browser

  def _CreateAllBrowsersIfNeeeded(self):
    """Launch all browsers needed for the story set, if not already done.

    This ensures that all browsers are alive during the whole duration of the
    benchmark and, therefore, e.g. memory dumps are always provided for all
    of them.
    """
    if self._browsers_created:
      return
    for browser_type in self._browsers:
      possible_browser, browser_options = self._possible_browsers[browser_type]
      possible_browser.SetUpEnvironment(browser_options)
      self._browsers[browser_type] = possible_browser.Create()
    self._browsers_created = True

  def _CloseAllBrowsers(self):
    """Close all of the browsers that were launched for this benchmark."""
    for browser_type, browser in list(self._browsers.iteritems()):
      if browser is not None:
        try:
          browser.Close()
        except Exception:
          logging.exception('Error while closing %s browser', browser_type)
        self._browsers[browser_type] = None
      possible_browser, _ = self._possible_browsers[browser_type]
      try:
        possible_browser.CleanUpEnvironment()
      except Exception:
        logging.exception(
            'Error while cleaning up environment for %s', browser_type)
    self._browsers_created = False

  def CanRunStory(self, _):
    return True

  def WillRunStory(self, story):
    self._current_story = story

    self.platform.network_controller.StartReplay(
        self._story_set.WprFilePathForStory(story),
        story.make_javascript_deterministic,
        self._extra_wpr_args)

    # Note: browsers need to be created after replay has been started.
    self._CreateAllBrowsersIfNeeeded()
    self._current_browser = self._browsers[story.browser_type]
    self._current_browser.Foreground()
    self._current_tab = self._current_browser.foreground_tab

  def RunStory(self, _):
    self._current_story.Run(self)

  def DidRunStory(self, _):
    if (not self._story_set.long_running and
        self._story_set[-1] == self._current_story):
      # In long_running mode we never close the browsers; otherwise we close
      # them only after the last story in the set runs.
      self._CloseAllBrowsers()
    self._current_story = None

  def TakeMemoryMeasurement(self):
    self.current_tab.action_runner.ForceGarbageCollection()
    self.platform.FlushEntireSystemCache()
    if not self.platform.tracing_controller.is_tracing_running:
      return  # Tracing is not running, e.g., when recording a WPR archive.
    for browser_type, browser in self._browsers.iteritems():
      if not browser.DumpMemory():
        logging.error('Unable to dump memory for %s', browser_type)

  def TearDownState(self):
    self.platform.network_controller.Close()
    self._CloseAllBrowsers()

  def DumpStateUponFailure(self, unused_story, unused_results):
    if self._browsers:
      for browser_type, browser in self._browsers.iteritems():
        if browser is not None:
          logging.info("vvvvv BROWSER STATE BELOW FOR '%s' vvvvv", browser_type)
          browser.DumpStateUponFailure()
        else:
          logging.info("browser '%s' not yet created", browser_type)
    else:
      logging.warning('Cannot dump browser states: No browsers.')


class SinglePage(story_module.Story):
  def __init__(self, name, url, browser_type, phase):
    """A story that associates a particular page with a browser to view it.

    Args:
      name: A string with the name of the page as it will appear reported,
        e.g., on results and dashboards.
      url: A string with the url of the page to load.
      browser_type: A string identifying the browser where this page should be
        displayed. Accepts the same strings as the command line --browser
        option (e.g. 'android-webview'), and the special value 'default' to
        select the browser chosen by the user on the command line.
    """
    super(SinglePage, self).__init__(MultiBrowserSharedState, name=name)
    self._url = url
    self._browser_type = browser_type
    self.grouping_keys['phase'] = phase

  @property
  def url(self):
    return self._url

  @property
  def browser_type(self):
    return self._browser_type

  def Run(self, shared_state):
    shared_state.current_tab.Navigate(self._url)
    shared_state.current_tab.WaitForDocumentReadyStateToBeComplete()
    shared_state.TakeMemoryMeasurement()


class DualBrowserStorySet(story_module.StorySet):
  """A story set that switches back and forth between two browsers."""

  def __init__(self, long_running=False):
    super(DualBrowserStorySet, self).__init__(
        archive_data_file='data/dual_browser_story.json',
        cloud_storage_bucket=story_module.PARTNER_BUCKET)
    self.long_running = long_running

    for query, url in zip(SEARCH_QUERIES, URL_LIST):
      # Stories that run on the android-webview browser.
      self.AddStory(SinglePage(
          name='google_%s' % re.sub(r'\W+', '_', query.lower()),
          url=GOOGLE_SEARCH + urllib.urlencode({'q': query}),
          browser_type='android-webview',
          phase='on_webview'))

      # Stories that run on the browser selected by command line options.
      self.AddStory(SinglePage(
          name=re.sub(r'\W+', '_', url),
          url=url,
          browser_type='default',
          phase='on_chrome'))
