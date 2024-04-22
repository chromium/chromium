# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import Counter
import json
import logging
import os
import py_utils
import six
from socket import timeout
import time

from telemetry import story
from telemetry.internal.backends.chrome_inspector import websocket
from telemetry.page import page as page_module

from contrib.shared_storage import shared_storage_shared_page_state as state
from contrib.shared_storage import utils


# Timeouts in seconds.
_ACTION_TIMEOUT = 2
_NAVIGATION_TIMEOUT = 90

# Time in seconds to sleep at end of story to let histograms finish recording.
_SLEEP_TIME = 1

# Placeholder substring for index value in the action script template.
_INDEX_PLACEHOLDER = '{{ index }}'

# Note that the true default number of iterations is defined by
# `_DEFAULT_NUM_ITERATIONS` in
# tools/perf/contrib/shared_storage/shared_storage.py.
_PLACEHOLDER_ITERATIONS = 10


# Replaces `_INDEX_PLACEHOLDER` in a param value with the index value.
def _Render(template, index):
  if not isinstance(template, str):
    raise TypeError("Expected template to be a str, but got " +
                    str(type(template)))
  if not isinstance(index, int):
    raise TypeError("Expected index to be an int, but got " + str(type(index)))
  return template.replace(_INDEX_PLACEHOLDER, str(index))


# Replaces `_INDEX_PLACEHOLDER` in an event dict with the index value.
def _RenderEvent(event_template, index):
  if not isinstance(event_template, dict):
    raise TypeError("Expected event_template to be a dict, but got " +
                    str(type(event_template)))
  if 'params' not in event_template:
    return event_template
  new_params = {
      key: _Render(event_template['params'][key], index)
      for key in event_template['params']
  }
  return {
      key: event_template[key] if key != 'params' else new_params
      for key in event_template
  }


# Replaces `_INDEX_PLACEHOLDER` in a list of event dicts with the index value.
def _RenderEvents(events_template, index):
  if not isinstance(events_template, list):
    raise TypeError("Expected events_template to be a list, but got " +
                    str(type(events_template)))
  return [_RenderEvent(event, index) for event in events_template]


# Extracts origin from a URL.
def _GetOriginFromURL(url):
  parse_result = six.moves.urllib.parse.urlparse(url)
  return '://'.join([parse_result[0], parse_result[1]])


class _MetaSharedStorageStory(type):
  """Metaclass for SharedStorageStory."""

  @property
  def ABSTRACT_STORY(cls):
    """Class field marking whether the class is abstract.

    If true, the story will NOT be instantiated and added to a Shared Storage
    story set. This field is NOT inherited by subclasses (that's why it's
    defined on the metaclass).
    """
    return cls.__dict__.get('ABSTRACT_STORY', False)


class SharedStorageStory(
    six.with_metaclass(_MetaSharedStorageStory, page_module.Page)):
  """Abstract base class for SharedStorage user stories."""

  NAME = NotImplemented
  ABSTRACT_STORY = True
  # The setup script is run once per story, before the first iteration of the
  # action script. Note that this should be an empty string when
  # `RENAVIGATE_AFTER_ACTION` is True.
  SETUP_SCRIPT = ""
  # The shared storage events that should happen in the setup, as a list of
  # dictionaries.
  # See the docstring of `InspectorBackend.WaitForSharedStorageEvents()` in
  # third_party/catapult/telemetry/telemetry/internal/backends/chrome_inspector
  # /inspector_backend.py for more information.
  EXPECTED_SETUP_EVENTS = []
  # Template for script of the action to be iterated. Instances of
  # `_INDEX_PLACEHOLDER` will be replaced with the value of iteration's index.
  ACTION_SCRIPT_TEMPLATE = NotImplemented
  # The shared storage events that should happen in the action, as a list of
  # dictionaries.
  # See the docstring of `InspectorBackend.WaitForSharedStorageEvents()` in
  # third_party/catapult/telemetry/telemetry/internal/backends/chrome_inspector
  # /inspector_backend.py for more information.
  EXPECTED_ACTION_EVENTS_TEMPLATE = NotImplemented
  # Whether the page should be reloaded after each action iteration in order to
  # refresh the database. Note that this should not be set to True when using an
  # nonempty `SETUP_SCRIPT`.
  RENAVIGATE_AFTER_ACTION = False
  # The number of "Storage.SharedStorage.Worklet.Timing.<METHOD>.Next"
  # histograms expected to be recorded for the iterator <METHOD> being tested
  # by this story, written as a string literal to be evaluated by `eval()`, so
  # so that the value can depend on `self.SIZE`.
  EXPECTED_ITERATOR_HISTOGRAM_COUNT = "0"

  def __init__(self,
               story_set,
               url,
               size,
               shared_page_state_class,
               enable_memory_metric,
               iterations=_PLACEHOLDER_ITERATIONS,
               verbosity=0):
    super(SharedStorageStory,
          self).__init__(shared_page_state_class=shared_page_state_class,
                         page_set=story_set,
                         name=self.NAME,
                         url=url)
    self._size = size
    self._enable_memory_metric = enable_memory_metric
    self._iterations = iterations
    self._verbosity = verbosity

    if len(self.SETUP_SCRIPT) > 0 and self.RENAVIGATE_AFTER_ACTION:
      # The expected histogram count would have been incorrect because it
      # assumes this condition won't happen.
      msg_list = [
          '`RENAVIGATE_AFTER_ACTION` is True with nonempty',
          ' `SETUP_SCRIPT`: %s' % self.SETUP_SCRIPT,
          '\n`SETUP_SCRIPT` will only be run during the initial ',
          'navigation.\n It will not run during subsequent ',
          're-navigations.\nConsider incorporting content of ',
          '`SETUP_SCRIPT` into `ACTION_SCRIPT_TEMPLATE`.'
      ]
      raise RuntimeError(''.join(msg_list))

  @property
  def SIZE(self):
    return self._size

  # TODO(crbug.com/41489492): Wait for relevant Shared Storage timing histograms
  # to be recorded in each step, rather than simply the event notifications.
  #
  # Note that this will require retrieving histograms from renderer processes;
  # the current DevTools Protocol 'Browser.getHistograms' method only retrieves
  # browser-process histograms.
  #
  # Alternatively, implement the ability to log what the expected histogram
  # total counts should be at the end of the test run.
  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete(_NAVIGATION_TIMEOUT)
    self._LogMetadataIfVerbose(action_runner, False)

    action_runner.tab.EnableSharedStorageNotifications()
    self._RunSharedStorageSetUp(action_runner)

    for index in range(self._iterations):
      self._PrintProgressBarIfNonVerbose(index)
      try:
        self._RunSharedStorageAction(action_runner, index)
      except timeout as t:
        logging.warning("%s's action timed out after %d seconds: %s" %
                        (self.NAME, _ACTION_TIMEOUT, repr(t)))
      except websocket.WebSocketTimeoutException as w:
        logging.warning("%s's action timed out after %d seconds: %s" %
                        (self.NAME, _ACTION_TIMEOUT, repr(w)))

      # Reload the page if necessary. Otherwise, skip.
      if self.RENAVIGATE_AFTER_ACTION and index < self._iterations - 1:
        url = self.file_path_url_with_scheme if self.is_file else self.url
        action_runner.Navigate(url,
                               self.script_to_evaluate_on_commit,
                               timeout_in_seconds=_NAVIGATION_TIMEOUT)
        action_runner.tab.WaitForDocumentReadyStateToBeComplete(
            _NAVIGATION_TIMEOUT)
        action_runner.tab.ClearSharedStorageNotifications()

    # Sleep for a little to allow histograms to finish recording.
    time.sleep(_SLEEP_TIME)

    if self._enable_memory_metric:
      action_runner.MeasureMemory(deterministic_mode=True)

    self._LogMetadataIfVerbose(action_runner, True)
    self._WriteExpectedHistogramCountsIfNeeded()

    # Navigate away to an untracked page to trigger recording of metrics
    # requiring document destruction.
    action_runner.Navigate('about:blank',
                           self.script_to_evaluate_on_commit,
                           timeout_in_seconds=_NAVIGATION_TIMEOUT)
    action_runner.tab.DisableSharedStorageNotifications()

  def _RunSharedStorageSetUp(self, action_runner):
    if self.SETUP_SCRIPT == "":
      logging.info("no setup")
      return
    logging.info("".join(["running set up: ", self.SETUP_SCRIPT]))
    action_runner.tab.EvaluateJavaScript(self.SETUP_SCRIPT, promise=True)
    action_runner.tab.WaitForSharedStorageEvents(self.EXPECTED_SETUP_EVENTS,
                                                 mode='strict',
                                                 timeout=_ACTION_TIMEOUT)
    action_runner.tab.ClearSharedStorageNotifications()

  def _RunSharedStorageAction(self, action_runner, index):
    logging.info("".join(
        ["running iteration ",
         str(index), ": ", self.ACTION_SCRIPT_TEMPLATE]))
    if self.ACTION_SCRIPT_TEMPLATE.find(_INDEX_PLACEHOLDER) != -1:
      action_runner.tab.EvaluateJavaScript(self.ACTION_SCRIPT_TEMPLATE,
                                           promise=True,
                                           index=index)
    else:
      action_runner.tab.EvaluateJavaScript(self.ACTION_SCRIPT_TEMPLATE,
                                           promise=True)
    expected_events = _RenderEvents(self.EXPECTED_ACTION_EVENTS_TEMPLATE,
                                    index=index)
    action_runner.tab.WaitForSharedStorageEvents(expected_events,
                                                 mode='strict',
                                                 timeout=_ACTION_TIMEOUT)
    action_runner.tab.ClearSharedStorageNotifications()

  def _LogMetadataIfVerbose(self, action_runner, is_post):
    if self._verbosity < 1:
      return
    prefix = 'Post' if is_post else 'Pre'
    template = "-test shared storage metadata:\norigin: %s\n%s\n"
    try:
      origin = _GetOriginFromURL(action_runner.tab.url)
      metadata = action_runner.tab.GetSharedStorageMetadata(origin)
      json_data = json.dumps(metadata, indent=2)
      log_msg = prefix + (template % (origin, json_data))
      logging.info(log_msg)
    except timeout as t:
      logging.warning("%s timed out getting %s-test metadata: %s" %
                      (self.NAME, prefix, repr(t)))
    except websocket.WebSocketTimeoutException as w:
      logging.warning("%s timed out getting %s-test metadata: %s" %
                      (self.NAME, prefix, repr(w)))

  def _PrintProgressBarIfNonVerbose(self, index):
    if self._verbosity >= 1:
      # We don't need a progress bar because we are already logging information
      # to track each action iteration.
      return

    progress = ''.join(
        ['[', '#' * (index + 1), ' ' * (self._iterations - 1 - index), '] '])
    fraction = ''.join([str(index + 1), ' / ', str(self._iterations)])

    # We use `print()` instead of logging so that the progress bar will print
    # with no prefix and in spite of having `self._verbosity < 1`.
    print(f'{progress}{fraction} iterations', end='\r')
    if index == self._iterations - 1:
      print()

  def _WriteExpectedHistogramCountsIfNeeded(self):
    story_counts_so_far = utils.GetExpectedHistogramsDictionary()
    if self.NAME in story_counts_so_far:
      return
    current_counts = self._CalculateExpectedHistogramCountsPerRepeat()
    story_counts_so_far[self.NAME] = current_counts
    logging.info("Story '%s' expected histogram counts: %s" %
                 (self.NAME, utils.JsonDump(current_counts)))
    with open(utils.GetExpectedHistogramsFile(), 'w') as f:
      f.write(utils.JsonDump(story_counts_so_far))
    logging.info('Wrote expected histograms for "%s" to file://%s.' %
                 (self.NAME, utils.GetExpectedHistogramsFile()))

  def _GetHistogramCountsFromEvents(self, events):
    event_counts = Counter(event['type'] for event in events)
    histogram_counts = Counter()
    for event_type, count in event_counts.items():
      for name in utils.GetHistogramsFromEventType(event_type):
        if name in utils.GetSharedStorageIteratorHistograms():
          histogram_counts[name] = eval(self.EXPECTED_ITERATOR_HISTOGRAM_COUNT)
        else:
          histogram_counts[name] = count
    return histogram_counts

  def _MultiplyCounterValuesByIterations(self, counts):
    return Counter(
        dict(map(lambda h: (h[0], h[1] * self._iterations), counts.items())))

  def _CalculateExpectedHistogramCountsPerRepeat(self):
    # The number of histograms we expect to be recorded based on navigation to
    # self.URL.
    counts = Counter({
        "Storage.SharedStorage.Document.Timing.AddModule": 1,
        "Storage.SharedStorage.Document.Timing.Clear": 1,
        "Storage.SharedStorage.Document.Timing.Set": self._size,
    })

    if self.RENAVIGATE_AFTER_ACTION:
      counts = self._MultiplyCounterValuesByIterations(counts)
    elif (len(self.SETUP_SCRIPT) > 0 and len(self.EXPECTED_SETUP_EVENTS) > 0):
      setup_counts = self._GetHistogramCountsFromEvents(
          self.EXPECTED_SETUP_EVENTS)
      counts += Counter(setup_counts)
    if len(self.EXPECTED_ACTION_EVENTS_TEMPLATE) > 0:
      action_counts = self._GetHistogramCountsFromEvents(
          self.EXPECTED_ACTION_EVENTS_TEMPLATE)
      counts += self._MultiplyCounterValuesByIterations(action_counts)
    return counts


def _IterAllSharedStorageStoryClasses():
  """Generator for SharedStorage stories.

  Yields:
    All appropriate SharedStorageStory subclasses defining stories.
  """
  start_dir = os.path.dirname(os.path.abspath(__file__))
  # Sort the classes by their names so that their order is stable and
  # deterministic.
  for unused_cls_name, cls in sorted(
      py_utils.discover.DiscoverClasses(
          start_dir=start_dir,
          top_level_dir=os.path.dirname(start_dir),
          base_class=SharedStorageStory).items()):
    yield cls


class SharedStorageStorySet(story.StorySet):

  def __init__(self,
               url,
               size,
               enable_memory_metric,
               user_agent='desktop',
               iterations=_PLACEHOLDER_ITERATIONS,
               verbosity=0,
               xvfb_process=None):
    super(SharedStorageStorySet, self).__init__()
    self.xvfb_process = xvfb_process
    if user_agent == 'mobile':
      shared_page_state_class = state.SharedStorageSharedMobilePageState
    elif user_agent == 'desktop':
      shared_page_state_class = state.SharedStorageSharedDesktopPageState
    else:
      raise ValueError('user_agent %s is unrecognized' % user_agent)

    def IncludeStory(story_class):
      return not story_class.ABSTRACT_STORY

    for story_class in _IterAllSharedStorageStoryClasses():
      if IncludeStory(story_class):
        if user_agent == 'mobile':
          # Extra browser args are disabled in the mobile user agent
          story_class.EXTRA_BROWSER_ARGUMENTS = []
          logging.warning(''.join([
              'Extra browser arguments are not ',
              'available; unable to enable shared ',
              'storage from the command line.'
          ]))
        self.AddStory(
            story_class(self,
                        url=url,
                        size=size,
                        shared_page_state_class=shared_page_state_class,
                        enable_memory_metric=enable_memory_metric,
                        iterations=iterations,
                        verbosity=verbosity))
