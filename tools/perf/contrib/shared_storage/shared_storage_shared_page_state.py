# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import py_utils

from telemetry.page import shared_page_state


class SharedStorageSharedPageState(shared_page_state.SharedPageState):
  _xvfb_process = None

  def __init__(self, test, finder_options, story_set, possible_browser):
    super(SharedStorageSharedPageState,
          self).__init__(test, finder_options, story_set, possible_browser)

    if 'xvfb_process' not in story_set.__dict__:
      msg = "story_set is of type %s but expected type " % type(story_set)
      msg += "<class 'contrib.shared_storage.page_set.SharedStorageStorySet'>"
      raise TypeError(msg)
    self._xvfb_process = story_set.xvfb_process

  def RunStory(self, results):
    # We use a print statement instead of logging here so that we can display
    # the run index (i.e. which pageset repeat we are on) even if the logging
    # level is non-verbose. Also, it's not necessary to persist to logs, as the
    # index number is used in the directory path in which the log artifact is
    # written.
    print('[ RUN #%3d ]' % results.current_story_run.index)
    super(SharedStorageSharedPageState, self).RunStory(results)

  def TearDownState(self):
    super(SharedStorageSharedPageState, self).TearDownState()
    self._TerminateOrKillXvfbIfNeeded()

  def _TerminateOrKillXvfbIfNeeded(self):
    if not self._xvfb_process:
      return

    done = False
    repr_proc = repr(self._xvfb_process)
    try:
      self._xvfb_process.terminate()
      py_utils.WaitFor(lambda: self._xvfb_process.poll() is not None, 10)
      done = True
    except py_utils.TimeoutException:
      try:
        self._xvfb_process.kill()
        py_utils.WaitFor(lambda: self._xvfb_process.poll() is not None, 10)
        done = True
      except py_utils.TimeoutException:
        pass
    if done:
      self._xvfb_process = None
      logging.info('Shut down xvfb process: %s' % repr_proc)
    else:
      msg = 'Failed to terminate/kill the xvfb process %s' % repr_proc
      msg += ' after 20 seconds.'
      logging.warning(msg)

class SharedStorageSharedMobilePageState(SharedStorageSharedPageState):
  _device_type = 'mobile'


class SharedStorageSharedDesktopPageState(SharedStorageSharedPageState):
  _device_type = 'desktop'
