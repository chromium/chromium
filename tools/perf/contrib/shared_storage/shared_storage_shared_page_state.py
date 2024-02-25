# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import shared_page_state


class SharedStorageSharedPageState(shared_page_state.SharedPageState):

  def RunStory(self, results):
    # We use a print statement instead of logging here so that we can display
    # the run index (i.e. which pageset repeat we are on) even if the logging
    # level is non-verbose. Also, it's not necessary to persist to logs, as the
    # index number is used in the directory path in which the log artifact is
    # written.
    print('[ RUN #%3d ]' % results.current_story_run.index)
    super(SharedStorageSharedPageState, self).RunStory(results)


class SharedStorageSharedMobilePageState(SharedStorageSharedPageState):
  _device_type = 'mobile'


class SharedStorageSharedDesktopPageState(SharedStorageSharedPageState):
  _device_type = 'desktop'
