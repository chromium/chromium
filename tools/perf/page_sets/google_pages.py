# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from page_sets.login_helpers import google_login

from telemetry.page import page as page_module
from telemetry.page import shared_page_state


def _DeterministicPerformanceCounters():
  with open(os.path.join(os.path.dirname(__file__),
          'deterministic_performance_counters.js')) as f:
    return f.read()

class GooglePages(page_module.Page):
  def __init__(self, url, page_set, shared_page_state_class,
               name=''):
    super(GooglePages, self).__init__(
        url=url, page_set=page_set, name=name,
        shared_page_state_class=shared_page_state_class)
    self.script_to_evaluate_on_commit = _DeterministicPerformanceCounters()


class GmailPage(GooglePages):
  def __init__(self, page_set,
               shared_page_state_class=shared_page_state.SharedPageState):
    super(GmailPage, self).__init__(
        url='https://mail.google.com/mail/',
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name='https://mail.google.com/mail/')

  def RunNavigateSteps(self, action_runner):
    google_login.LoginGoogleAccount(action_runner, 'google')
    super(GmailPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined &&'
        'document.getElementById("gb") !== null')
