# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import shared_page_state


class DesktopUISharedState(shared_page_state.SharedPageState):
  """ Ensures the browser is restarted for each test, for all platforms. """

  def ShouldReuseBrowserForAllStoryRuns(self):
    return False
