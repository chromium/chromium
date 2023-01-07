# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.login_helpers import login_utils


def LoginAccount(action_runner, credential,
                 credentials_path=login_utils.DEFAULT_CREDENTIAL_PATH):
  """Logs in into a Dropbox account.

  This function navigates the tab into Dropbox's login page and logs in a user
  using credentials in |credential| part of the |credentials_path| file.

  Args:
    action_runner: Action runner responsible for running actions on the page.
    credential: The credential to retrieve from the credentials file (string).
    credentials_path: The path to credential file (string).

  Raises:
    exceptions.Error: See ExecuteJavaScript()
    for a detailed list of possible exceptions.
  """
  account_name, password = login_utils.GetAccountNameAndPassword(
      credential, credentials_path=credentials_path)

  action_runner.Navigate('https://www.dropbox.com/login')
  login_utils.InputWithSelector(
      action_runner, account_name, 'input[name=login_email]')
  login_utils.InputWithSelector(
      action_runner, password, 'input[name=login_password]')

  # Wait until the "Sign in" button is enabled and then click it.
  login_button_selector = '.login-form .login-button'
  action_runner.WaitForJavaScriptCondition('''
      (function() {
        var loginButton = document.querySelector({{ selector }});
        if (!loginButton)
          return false;
        return !loginButton.disabled;
      })();''',
      selector=login_button_selector)
  action_runner.ClickElement(selector=login_button_selector)
  action_runner.WaitForNavigate()
