# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import abc
import logging
import os
import plistlib
import subprocess
import time
import typing

import utils


class BrowserDriver(abc.ABC):
  """Abstract Base Class encapsulating browser setup and tear down.
  """

  def __init__(self, browser_name: str, process_name: str):
    self.name = browser_name
    self.process_name = process_name
    self.browser_process = None

    # LaunchServices can get confused when an application is launched from
    # more than one location and break AppleScript commands. Always launch
    # browsers from /Applications to avoid such problems.
    self.executable_path = (os.path.join("/Applications",
                                         f"{self.process_name}.app"))

    if not os.path.exists(self.executable_path):
      raise ValueError(f"Application doesn't exist for {browser_name}.")

  @abc.abstractmethod
  def Launch(self):
    """Starts the browser and ensures it is started before returning.
    """
    pass

  def TearDown(self):
    """Terminates the browser and ensures it's cleaned up before returning.
    """
    logging.debug(f"Tearing down {self.process_name}")
    if self.browser_process:
      utils.TerminateProcess(self.browser_process)

  def GetApplicationInfo(self) -> typing.Dict:
    """ Returns the Info.plist data in the application folder. """
    plist_path = os.path.join(self.executable_path, "Contents", "Info.plist")
    with open(plist_path, 'rb') as plist_file:
      return plistlib.load(plist_file)

  def Summary(self):
    """Returns a dictionary describing the browser.
    """
    info = self.GetApplicationInfo()
    return {
        'name': self.name,
        'version': info['CFBundleShortVersionString'],
        'identifier': info['CFBundleIdentifier']
    }

  def _EnsureStarted(self):
    """Waits until a browser with the given `process_name` is running.
    """
    while not self.browser_process:
      self.browser_process = utils.FindProcess(self.process_name)
      time.sleep(0.100)
      logging.debug(f"Waiting for {self.process_name} to start")
    logging.debug(f"{self.process_name} started")


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
               variation: str,
               process_name: str,
               extra_args=[]):
    if variation != "":
      browser_name += f"_{variation}"
    super().__init__(browser_name, process_name)
    self.extra_args = extra_args


  def Launch(self):
    open_args = ["-a", self.process_name]
    subprocess.call(["open"] + open_args + ["--args"] + [
        "--enable-benchmarking", "--disable-stack-profiler", "--no-first-run",
        "--no-default-browser-check"
    ] + self.extra_args)

    self._EnsureStarted()

  def Summary(self):
    """Returns a dictionary describing the browser.
    """
    info = self.GetApplicationInfo()
    return {
        'name': self.name,
        'identifier': info['CFBundleIdentifier'],
        'version': info['CFBundleShortVersionString'],
        'commit': info['SCMRevision'],
        'extra_args': self.extra_args
    }


# Helper functions to get default browser configurations.


def Safari():
  return SafariDriver()


def Chrome(variation, extra_args=[]):
  return ChromiumDriver("chrome",
                        variation,
                        "Google Chrome",
                        extra_args=extra_args)


def Canary(variation, extra_args=[]):
  return ChromiumDriver("canary",
                        variation,
                        "Google Chrome Canary",
                        extra_args=extra_args)


def Chromium(variation, extra_args=[]):
  return ChromiumDriver("chromium",
                        variation,
                        "Chromium",
                        extra_args=extra_args)


def Edge(variation, extra_args=[]):
  return ChromiumDriver("edge",
                        variation,
                        "Microsoft Edge",
                        extra_args=extra_args)


PROCESS_NAMES = [
    "Safari", "Google Chrome", "Google Chrome Canary", "Chromium",
    "Microsoft Edge"
]


def MakeBrowserDriver(browser_name: str,
                      variation: str,
                      chrome_user_dir=None,
                      output_dir=None,
                      tracing_mode=False,
                      extra_command_line=None) -> BrowserDriver:
  """Creates browser driver by name.

  Args:
    browser_name: Identifier for the browser to create. Supported browsers
      are: safari, chrome and chromium.
    chrome_user_dir: Optional user directory path to use for chrome.
  """

  if "safari" == browser_name:
    return Safari()
  if browser_name in ["chrome", "chromium", "canary", "edge"]:
    if chrome_user_dir:
      chrome_extra_arg = [f"--user-data-dir={chrome_user_dir}"]
    else:
      chrome_extra_arg = ["--guest"]
    if variation == 'AlignWakeUps':
      chrome_extra_arg += ['--enable-features=AlignWakeUps']

    if tracing_mode:
      chrome_extra_arg += [
          '--enable-tracing=toplevel,toplevel.flow,mojom,navigation'
      ]
      trace_path = os.path.join(output_dir, "chrometrace.log")
      chrome_extra_arg += [
          f'--trace-startup-file={os.path.abspath(trace_path)}'
      ]

    if extra_command_line:
      for command in extra_command_line:
        # Quotes are needed to avoid to avoid cli replacement.
        command = command.replace('"', '')
        chrome_extra_arg += [command]

    if browser_name == "chrome":
      return Chrome(variation, extra_args=chrome_extra_arg)
    if browser_name == "canary":
      return Canary(variation, extra_args=chrome_extra_arg)
    elif browser_name == "chromium":
      return Chromium(variation, extra_args=chrome_extra_arg)
    elif browser_name == "edge":
      return Edge(variation, extra_args=chrome_extra_arg)
  return None
