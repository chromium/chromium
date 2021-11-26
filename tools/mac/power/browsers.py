# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import abc
import logging
import subprocess
import time
import os

import utils


class BrowserDriver(abc.ABC):
  """Abstract Base Class encapsulating browser setup and tear down.
  """

  def __init__(self, browser_name: str, process_name: str):
    self.name = browser_name
    self.process_name = process_name
    self.browser_process = None

  @abc.abstractmethod
  def Launch(self):
    """Starts the browser and ensures it is started before returning.
    """
    pass

  def TearDown(self):
    """Terminates the browser and ensures it's cleaned up before returning.
    """
    logging.info(f"Tearing down {self.process_name}")
    if self.browser_process:
      utils.TerminateProcess(self.browser_process)

  def _EnsureStarted(self):
    """Waits until a browser with the given `process_name` is running.
    """
    while not self.browser_process:
      self.browser_process = utils.FindProcess(self.process_name)
      time.sleep(0.100)
      logging.info(f"Waiting for {self.process_name} to start")
    logging.info(f"{self.process_name} started")


class SafariDriver(BrowserDriver):
  def __init__(self, extra_args=[]):
    super().__init__("safari", "Safari")
    self.extra_args = extra_args

  def Launch(self):
    subprocess.call(["open", "-a", "Safari"])
    # Call prep_safari.scpt to make sure the run starts clean. See file
    # comment for details.
    subprocess.call([
        "osascript",
        os.path.join(os.path.dirname(__file__), "driver_scripts_templates",
                     "prep_safari.scpt")
    ])
    subprocess.call(["open", "-a", "Safari", "--args"] + self.extra_args)

    self._EnsureStarted()


class ChromiumDriver(BrowserDriver):
  def __init__(self,
               browser_name: str,
               process_name: str,
               executable_path=None,
               extra_args=[]):
    super().__init__(browser_name, process_name)
    if executable_path:
      self.executable = executable_path
    else:
      self.executable = process_name
    self.extra_args = extra_args

  def Launch(self):
    subprocess.call(["open", "-a", self.executable, "--args"] +
                    ["--enable-benchmarking", "--disable-stack-profiler"] +
                    self.extra_args)

    self._EnsureStarted()


# Helper functions to get default browser configurations.


def Safari():
  return SafariDriver()


def Chrome(extra_args=[]):
  return ChromiumDriver("chrome", "Google Chrome", extra_args)


def Canary(extra_args=[]):
  return ChromiumDriver("canary", "Google Chrome Canary", extra_args)


def Chromium(executable_path=None, extra_args=[]):
  return ChromiumDriver("chromium", "Chromium", executable_path, extra_args)


def Edge(extra_args=[]):
  return ChromiumDriver("edge", "Microsoft Edge", extra_args)


PROCESS_NAMES = [
    "Safari", "Google Chrome", "Google Chrome Canary", "Chromium",
    "Microsoft Edge"
]


def MakeBrowserDriver(browser_name: str,
                      chrome_user_dir=None,
                      chromium_path=None) -> BrowserDriver:
  """Creates browser driver by name.

  Args:
    browser_name: Identifier for the browser to create. Supported browsers
      are: safari, chrome and chromium.
    chrome_user_dir: Optional user directory path to use for chrome.
  """

  if "safari" == browser_name:
    return Safari()
  if browser_name in ["chrome", "chromium"]:
    if chrome_user_dir:
      chrome_extra_arg = [f"--user-data-dir={chrome_user_dir}"]
    else:
      chrome_extra_arg = ["--guest"]
    if browser_name == "chrome":
      return Chrome(chrome_extra_arg)
    elif browser_name == "chromium":
      return Chromium(executable_path=chromium_path,
                      extra_args=chrome_extra_arg)
  return None
