# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.login_helpers import login_utils

from page_sets.helpers import override_online


def _LoginAccount(action_runner, credential, credentials_path):
  account_name, password = login_utils.GetAccountNameAndPassword(
      credential, credentials_path=credentials_path)

  action_runner.Navigate('https://www.pinterest.co.uk/login/',
                         override_online.ALWAYS_ONLINE)
  action_runner.Wait(1) # Error page happens if this wait is not here.
  action_runner.WaitForElement(selector='button[type=submit]')

  login_utils.InputWithSelector(
      action_runner, '%s@gmail.com' % account_name, 'input[type=email]')

  login_utils.InputWithSelector(
      action_runner, password, 'input[type=password]')

  action_runner.ClickElement(selector='button[type=submit]')

def LoginDesktopAccount(action_runner, credential,
                        credentials_path=login_utils.DEFAULT_CREDENTIAL_PATH):
  """Logs in into a Pinterest account on desktop.

  This function navigates the tab into Pinterest's login page and logs in a user
  using credentials in |credential| part of the |credentials_path| file.

  Args:
    action_runner: Action runner responsible for running actions on the page.
    credential: The credential to retrieve from the credentials file (string).
    credentials_path: The path to credential file (string).

  Raises:
    exceptions.Error: See ExecuteJavaScript()
    for a detailed list of possible exceptions.
  """
  _LoginAccount(action_runner, credential, credentials_path)
  action_runner.WaitForElement(selector='input[name=searchBoxInput]')

def LoginMobileAccount(action_runner, credential,
                        credentials_path=login_utils.DEFAULT_CREDENTIAL_PATH):
  """Logs in into a Pinterest account on mobile.

  This function navigates the tab into Pinterest's login page and logs in a user
  using credentials in |credential| part of the |credentials_path| file.

  Args:
    action_runner: Action runner responsible for running actions on the page.
    credential: The credential to retrieve from the credentials file (string).
    credentials_path: The path to credential file (string).

  Raises:
    exceptions.Error: See ExecuteJavaScript()
    for a detailed list of possible exceptions.
  """
  wait_for_local_storage = """
  (function() {
    try {
      const state = JSON.parse(window.localStorage.REDUX_STATE);
      return state.users[state.session.userId].login_state;
    } catch(e) { return false; }
  })()
  """
  _LoginAccount(action_runner, credential, credentials_path)
  action_runner.WaitForJavaScriptCondition(wait_for_local_storage, timeout=20)
