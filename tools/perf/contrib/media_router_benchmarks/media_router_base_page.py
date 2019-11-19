# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import time

from telemetry import page
from telemetry.core import exceptions


class MediaRouterBasePage(page.Page):
  """Abstract Cast page for Media Router Telemetry tests."""

  def ChooseSink(self, tab, sink_name):
    """Chooses a specific sink in the list."""

    tab.ExecuteJavaScript("""
        var sinks = window.document.getElementById("media-router-container").
            shadowRoot.getElementById("sink-list").getElementsByTagName("span");
        for (var i=0; i<sinks.length; i++) {
          if(sinks[i].textContent.trim() == {{ sink_name }}) {
            sinks[i].click();
            break;
        }}
        """,
        sink_name=sink_name)

  def CloseDialog(self, tab):
    """Closes media router dialog."""

    try:
      tab.ExecuteJavaScript(
          'window.document.getElementById("media-router-container").' +
          'shadowRoot.getElementById("container-header").shadowRoot.' +
          'getElementById("close-button").click();')
    except (exceptions.DevtoolsTargetCrashException,
            exceptions.EvaluateException,
            exceptions.TimeoutException):
      # Ignore the crash exception, this exception is caused by the js
      # code which closes the dialog, it is expected.
      # Ignore the evaluate exception, this exception maybe caused by the dialog
      # is closed/closing when the JS is executing.
      # Ignore the timeout exception, this exception can be caused by finding
      # the close-button on a dialog that is already closed.
      pass


  def CloseExistingRoute(self, action_runner, sink_name):
    """Closes the existing route if it exists, otherwise does nothing."""

    action_runner.TapElement(selector='#start_session_button')
    action_runner.Wait(5)
    for tab in action_runner.tab.browser.tabs:
      if tab.url == 'chrome://media-router/':
        if self.CheckIfExistingRoute(tab, sink_name):
          self.ChooseSink(tab, sink_name)
          tab.ExecuteJavaScript(
              "window.document.getElementById('media-router-container')."
              "shadowRoot.getElementById('route-details').shadowRoot."
              "getElementById('close-route-button').click();")
        self.CloseDialog(tab)
    # Wait for 5s to make sure the route is closed.
    action_runner.Wait(5)

  def CheckIfExistingRoute(self, tab, sink_name):
    """"Checks if there is existing route for the specific sink."""

    tab.ExecuteJavaScript("""
        var sinks = window.document.getElementById('media-router-container').
          allSinks;
        var sink_id = null;
        for (var i=0; i<sinks.length; i++) {
          if (sinks[i].name == {{ sink_name }}) {
            console.info('sink id: ' + sinks[i].id);
            sink_id = sinks[i].id;
            break;
          }
        }
        var routes = window.document.getElementById('media-router-container').
          routeList;
        for (var i=0; i<routes.length; i++) {
          if (!!sink_id && routes[i].sinkId == sink_id) {
            window.__telemetry_route_id = routes[i].id;
            break;
          }
        }""",
        sink_name=sink_name)
    route = tab.EvaluateJavaScript('!!window.__telemetry_route_id')
    logging.info('Is there existing route? ' + str(route))
    return route

  def ExecuteAsyncJavaScript(self, action_runner, script, verify_func,
                             error_message, timeout=5, retry=1):
    """Executes async javascript function and waits until it finishes."""
    exception = None
    for _ in xrange(retry):
      try:
        action_runner.ExecuteJavaScript(script)
        self._WaitForResult(
            action_runner, verify_func, error_message, timeout=timeout)
        exception = None
        break
      except RuntimeError as e:
        exception = e
    if exception:
      raise Exception(exception)

  def WaitUntilDialogLoaded(self, action_runner, tab):
    """Waits until dialog is fully loaded."""

    self._WaitForResult(
        action_runner,
        lambda: tab.EvaluateJavaScript(
             '!!window.document.getElementById('
             '"media-router-container") &&'
             'window.document.getElementById('
             '"media-router-container").sinksToShow_ &&'
             'window.document.getElementById('
             '"media-router-container").sinksToShow_.length'),
        'The dialog is not fully loaded within 15s.',
         timeout=15)

  def WaitForSink(self, action_runner, target_sink, error_message, timeout=5):
    sink_name_list = [sink['name'] for sink in action_runner.tab.GetCastSinks()]
    start_time = time.time()
    while (target_sink not in sink_name_list
           and time.time() - start_time < timeout):
      action_runner.tab.EnableCast()
      sink_name_list = [
          sink['name'] for sink in action_runner.tab.GetCastSinks()]
      action_runner.Wait(1)
    if target_sink not in sink_name_list:
      raise RuntimeError(error_message)

  def _WaitForResult(self, action_runner, verify_func, error_message,
                     timeout=5):
    """Waits until the function finishes or timeout."""

    start_time = time.time()
    while (not verify_func() and
           time.time() - start_time < timeout):
      action_runner.Wait(1)
    if not verify_func():
      raise RuntimeError(error_message)

  def _GetOSEnviron(self, environ_variable):
    """Gets an OS environment variable on the machine."""

    if (environ_variable not in os.environ or
        not os.environ.get(environ_variable)):
      raise RuntimeError(
          'Your test machine is not set up correctly, '
          '%s enviroment variable is missing.', environ_variable)
    return os.environ[environ_variable]
