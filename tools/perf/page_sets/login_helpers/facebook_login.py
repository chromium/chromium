# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from page_sets.login_helpers import login_utils


def LoginWithMobileSite(
    action_runner,
    credential='facebooktest', # Recommended testing account.
    credentials_path=login_utils.DEFAULT_CREDENTIAL_PATH):
  """Logs in into mobile Facebook account.

  This function navigates the tab into Facebook's login page and logs in a user
  using credentials in |credential| part of the |credentials_path| file.

  Args:
    action_runner: Action runner responsible for running actions on the page.
    credential: The credential to retrieve from the credentials file
        (type string). It's recommended to use 'facebooktest' account since
        it's a test account and will not trigger credential verification.
        However, the limitation is the test account cannot access actual
        facebook's pages & people (more restrictions in
        https://developers.facebook.com/docs/apps/test-users#rules)
    credentials_path: The string that specifies the path to credential file.

  Raises:
    exceptions.Error: See ExecuteJavaScript()
    for a detailed list of possible exceptions.
  """
  account_name, password = login_utils.GetAccountNameAndPassword(
      credential, credentials_path=credentials_path)

  action_runner.Navigate(
       'https://m.facebook.com/login?continue=https%3A%2F%2Fm.facebook.com')
  login_utils.InputWithSelector(action_runner, account_name, '[name=email]')
  login_utils.InputWithSelector(action_runner, password, '[name=pass]')
  action_runner.ClickElement(selector='[name=login]')
  action_runner.WaitForElement(text='OK')
  action_runner.ClickElement(text='OK')
  action_runner.WaitForNavigate()


def LoginWithDesktopSite(
    action_runner, credential,
    credentials_path=login_utils.DEFAULT_CREDENTIAL_PATH):
  account_name, password = login_utils.GetAccountNameAndPassword(
      credential, credentials_path=credentials_path)

  action_runner.Navigate('https://facebook.com/login')
  login_utils.InputWithSelector(action_runner, account_name, '[name=email]')
  login_utils.InputWithSelector(action_runner, password, '[name=pass]')
  action_runner.ClickElement(selector='[name=login]')
  action_runner.WaitForNavigate()
