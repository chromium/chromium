# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from page_sets.login_helpers import login_utils


def LoginWithDesktopSite(action_runner,
                         credential,
                         credentials_path=login_utils.DEFAULT_CREDENTIAL_PATH):
  account_name, password = login_utils.GetAccountNameAndPassword(
      credential, credentials_path=credentials_path)

  action_runner.Navigate('https://accounts.autodesk.com/logon')
  user_selector = '() => document.querySelector("#userName")'
  action_runner.WaitForElement(element_function=user_selector)
  # We would like to use {action_runner.ClickElement} here but it does not
  # work. It shows the error message that {click} does not exist, even though
  # it exists. Calling {click} in {action_runner.WaitForJavaScriptCondition}
  # works just fine.
  action_runner.WaitForJavaScriptCondition(
      'let user_field = document.querySelector("#userName");'
      'user_field.select();'
      'true;')
  action_runner.EnterText(account_name)
  action_runner.WaitForJavaScriptCondition(
      'document.querySelector("#verify_user_btn").click();'
      'true;')
  # The password field is already there, just hidden. We cannot wait for it,
  # so we wait for fixed 2 seconds.
  action_runner.Wait(2)
  action_runner.WaitForJavaScriptCondition(
      'let password_field = document.querySelector("#password");'
      'password_field.select();'
      'true;')
  action_runner.EnterText(password)
  action_runner.WaitForJavaScriptCondition(
      'document.querySelector("#btnSubmit").click();'
      'true;')
  action_runner.WaitForNavigate()
