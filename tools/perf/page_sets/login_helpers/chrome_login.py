# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.login_helpers import login_utils
from telemetry.page import action_runner as action_runner_module

import py_utils


def GetGaiaContext(tab):
  """Returns Gaia's login page context."""
  for context in tab.GetWebviewContexts():
    if context.GetUrl().startswith('https://accounts.google.com/'):
      return context
  return None


def LoginChromeAccount(action_runner, credential,
                       credentials_path=login_utils.DEFAULT_CREDENTIAL_PATH):
  """Logs in a Gaia account into Chrome.

  This function navigates the tab into Chrome's login page and logs in a user
  using credentials in |credential| part of the |credentials_path| file.

  Args:
    action_runner: Action runner responsible for running actions on the page.
    credential: The credential to retrieve from the credentials file
        (type string).
    credentials_path: The string that specifies the path to credential file.

  Raises:
    exceptions.Error: See GetWebviewContexts() and ExecuteJavaScript()
      for a detailed list of possible exceptions.
  """
  account_name, password = login_utils.GetAccountNameAndPassword(
      credential, credentials_path=credentials_path)

  action_runner.Navigate('chrome://chrome-signin')

  # Get the Gaia webview context within the sign in extension to create a Gaia
  # action_runner. The action runner will then execute JS in the Gaia context.
  gaia_context = py_utils.WaitFor(lambda: GetGaiaContext(action_runner.tab), 5)
  if not gaia_context:
    raise RuntimeError('Can not find GAIA webview context for sign in.')
  gaia_action_runner = action_runner_module.ActionRunner(gaia_context)

  new_flow = gaia_action_runner.EvaluateJavaScript(
      'document.querySelector("#gaia_firsform") != null')
  gaia_form_id = 'gaia_firstform' if new_flow else 'gaia_loginform'
  login_utils.InputForm(gaia_action_runner, account_name, input_id='Email',
                        form_id=gaia_form_id)
  if new_flow:
    gaia_action_runner.ClickElement(selector='#%s #next' % gaia_form_id)
  login_utils.InputForm(gaia_action_runner, password, input_id='Passwd',
                        form_id=gaia_form_id)
  gaia_action_runner.ClickElement(selector='#signIn')
  action_runner.WaitForNavigate()
