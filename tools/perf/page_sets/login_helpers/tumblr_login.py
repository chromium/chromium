# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.login_helpers import login_utils


def LoginDesktopAccount(action_runner, credential,
                 credentials_path=login_utils.DEFAULT_CREDENTIAL_PATH):
  """Logs in into a Tumblr account."""

  account_name, password = login_utils.GetAccountNameAndPassword(
      credential, credentials_path=credentials_path)

  action_runner.Navigate('https://www.tumblr.com/login')
  login_utils.InputWithSelector(
      action_runner, account_name, 'input[type=email]')

  next_button = '.signup_determine_btn'
  enter_password_button = '.forgot_password_link'
  action_runner.WaitForElement(selector=next_button)
  action_runner.ClickElement(selector=next_button)
  action_runner.Wait(1)
  action_runner.WaitForElement(selector=enter_password_button)
  action_runner.ClickElement(selector=enter_password_button)
  action_runner.Wait(1)
  login_utils.InputWithSelector(
      action_runner, password, 'input[type=password]')
  action_runner.Wait(1)
  action_runner.WaitForElement(selector=next_button)
  action_runner.ClickElement(selector=next_button)
