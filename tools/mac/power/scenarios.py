# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import abc
import subprocess
import jinja2
import tempfile
import datetime
import logging
import typing
import os
import pyautogui
from AppKit import NSBundle  # used to suppress macOS dock icon pop up/bounce

import utils
import browsers


def GetTemplateFileForBrowser(browser_driver: browsers.BrowserDriver,
                              template_file: str) -> str:
  if browser_driver.name == "safari":
    return f"safari_{template_file}"
  else:
    return template_file


class ScenarioOSADriver(abc.ABC):
  """Base Class encapsulating OSA script driving a scenario, with setup and tear
     down.
  """

  def __init__(self, scenario_name, duration: datetime.timedelta):
    self.name = scenario_name
    self.script_process = None
    self.osa_script = None
    self.duration = duration
    self.tag = ""

  def Launch(self):
    """Starts the driver script.
    """
    app_info = NSBundle.mainBundle().infoDictionary()
    # Suppress macOS dock icon pop up/bounce.
    app_info["LSBackgroundOnly"] = "1"

    # Disable aborting the sequence of movements by moving to the corner of
    # the screen. This is fine because there isn't a sequence but a single move.
    pyautogui.FAILSAFE = False

    # Move the cursor in the midle of the screen so it's always in the same
    # spot. This cannot be 0,0 as this keeps the task bar visible in
    # full-screen scenarios.
    width, height = pyautogui.size()
    pyautogui.moveTo(width / 2, height / 2)

    assert self.osa_script is not None
    logging.debug(f"Starting scenario {self.name}")
    self.script_process = subprocess.Popen(['osascript', self.osa_script.name])

  def Wait(self):
    """Waits for the script to complete.
    """
    assert self.script_process is not None, "Driver wasn't launched."
    logging.debug(f"Waiting for scenario {self.name}")
    self.script_process.wait()

  def TearDown(self):
    """Terminates the script if currently running and ensures related processes
       are cleaned up.
    """
    logging.debug(f"Tearing down scenario {self.name}")
    if self.script_process:
      utils.TerminateProcess(self.script_process)
    self.osa_script.close()

  @abc.abstractmethod
  def Summary(self):
    """Returns a dictionary describing the scenarios parameters.
    """
    pass

  def CycleDuration(self) -> datetime.timedelta:
    """Returns the duration of a cycle in the scenario. Not all
     scenario have a notion of cycles. For those which don't a
     cycle is defined as the duration of the whole scenario"""
    return self.duration

  def IsRunning(self) -> bool:
    """Returns true if the script is currently running.
    """
    return self.script_process.poll() is None

  def _CompileTemplate(self, template_file: str, extra_args: typing.Dict):
    """Compiles script `template_file`, feeding `extra_args` into a temporary
       file.
    """
    loader = jinja2.FileSystemLoader(
        os.path.join(os.path.dirname(__file__), "driver_scripts_templates"))
    env = jinja2.Environment(loader=loader)
    template = env.get_template(template_file)
    self.osa_script = tempfile.NamedTemporaryFile('w+t')
    self.osa_script.write(template.render(**extra_args))
    self.osa_script.flush()
    self._args = extra_args

  def Summary(self):
    """Returns a dictionary describing the scenarios parameters.
    """
    return {'name': self.name, 'tag': self.tag, **self._args}


class ScenarioWithBrowserOSADriver(ScenarioOSADriver):
  """Specialisation for OSA script that runs with a browser.
  """

  def __init__(self, scenario_name, browser_driver: browsers.BrowserDriver,
               duration: datetime.timedelta):
    super().__init__(f"{browser_driver.name}_{scenario_name}", duration)
    self.browser = browser_driver

  def Launch(self):
    self.browser.Launch()
    super().Launch()

  def TearDown(self):
    super().TearDown()
    self.browser.TearDown()

  def Summary(self):
    """Returns a dictionary describing the scenarios parameters.
    """
    return {**super().Summary(), 'browser': self.browser.Summary()}

  def _CompileTemplate(self, template_file, extra_args: typing.Dict):
    return super()._CompileTemplate(template_file, {
        "browser": self.browser.process_name,
        **extra_args
    })


class IdleScenario(ScenarioOSADriver):
  """Scenario that lets the system idle.
  """

  def __init__(self, duration: datetime.timedelta, scenario_name="idle"):
    super().__init__(scenario_name, duration)
    self._CompileTemplate("idle", {
        "delay": duration.total_seconds(),
    })


class IdleOnSiteScenario(ScenarioWithBrowserOSADriver):
  """Scenario that lets a browser idle on a web page.
  """

  def __init__(self, browser_driver: browsers.BrowserDriver,
               duration: datetime.timedelta, site_url: str, scenario_name,
               send_full_screen_key):
    super().__init__(scenario_name, browser_driver, duration)
    self._CompileTemplate(
        GetTemplateFileForBrowser(browser_driver, "idle_on_site"), {
            "idle_site": site_url,
            "delay": duration.total_seconds(),
            "send_full_screen_key": send_full_screen_key,
        })

  @staticmethod
  def Wiki(browser_driver: browsers.BrowserDriver,
           duration: datetime.timedelta):
    return IdleOnSiteScenario(browser_driver,
                              duration,
                              "http://www.wikipedia.com/wiki/Alessandro_Volta",
                              "idle_on_wiki",
                              send_full_screen_key=False)

  @staticmethod
  def Youtube(browser_driver: browsers.BrowserDriver,
              duration: datetime.timedelta):
    return IdleOnSiteScenario(
        browser_driver,
        duration,
        # A nature video confirmed to use AV1 and that lasts long enough.
        # Set to always start a time 1, no matter the progress made previously.
        "https://www.youtube.com/watch?v=rV_ERKtNyNA?t=1",
        "idle_on_youtube",
        send_full_screen_key=True)

  @staticmethod
  def Netflix(browser_driver: browsers.BrowserDriver,
              duration: datetime.timedelta):
    return IdleOnSiteScenario(
        browser_driver,
        duration,
        # A movie that lasts long enough. Set to always restart at time 0.
        "https://www.netflix.com/watch/81198930?t=0",
        "idle_on_netflix",
        send_full_screen_key=True)


class ZeroWindowScenario(ScenarioWithBrowserOSADriver):
  """Scenario that lets a browser idle with no window.
  """

  def __init__(self,
               browser_driver: browsers.BrowserDriver,
               duration: datetime.timedelta,
               scenario_name="zero_window"):
    super().__init__(scenario_name, browser_driver, duration)
    self._CompileTemplate(
        GetTemplateFileForBrowser(browser_driver, "zero_window"), {
            "delay": duration.total_seconds(),
        })


class NavigationScenario(ScenarioWithBrowserOSADriver):
  """Scenario that has a browser navigating on web pages in a loop.
  """

  def __init__(self, browser_driver: browsers.BrowserDriver,
               navigation_duration: datetime.timedelta, navigation_cycles: int,
               sites, scenario_name):
    super().__init__(scenario_name, browser_driver,
                     navigation_duration * navigation_cycles * len(sites))
    self._CompileTemplate(
        GetTemplateFileForBrowser(browser_driver, "navigation"), {
            "per_navigation_delay": navigation_duration.total_seconds(),
            "navigation_cycles": navigation_cycles,
            "sites": ",".join([f'"{site}"' for site in sites])
        })

    self.cycle_duration = len(sites) * navigation_duration

  def CycleDuration(self) -> datetime.timedelta:
    return self.cycle_duration


class MeetScenario(ScenarioWithBrowserOSADriver):
  """Scenario that has the browser join a Google Meet room.
  """

  def __init__(self,
               browser_driver: browsers.BrowserDriver,
               duration: datetime.timedelta,
               meeting_id: int,
               scenario_name="meet"):
    super().__init__(scenario_name, browser_driver, duration)
    self._CompileTemplate(GetTemplateFileForBrowser(browser_driver, "meet"), {
        "delay": duration.total_seconds(),
        "meeting_id": meeting_id
    })


def MakeScenarioDriver(scenario_name,
                       browser_driver: browsers.BrowserDriver,
                       meet_meeting_id=None) -> ScenarioOSADriver:
  """Creates scenario driver by name.

  Args:
    scenario_name: Identifier for the scenario to create. Supported scenarios
      are: meet, idle_on_wiki, idle_on_youtube, idle_on_netflix,
      navigation_top_sites,navigation_heavy_sites, zero_window and idle.
    browser_driver: Browser the scenario is created with.
    meet_meeting_id: Optional meeting id used for meet scenario.
  """

  if "idle" == scenario_name:
    return IdleScenario(datetime.timedelta(minutes=60))
  if not browser_driver:
    return None
  if "prep" == scenario_name:
    return IdleOnSiteScenario.Wiki(browser_driver,
                                   datetime.timedelta(minutes=1))
  if "meet" == scenario_name:
    return MeetScenario(browser_driver,
                        datetime.timedelta(minutes=60),
                        meeting_id=meet_meeting_id)
  if "idle_on_wiki" == scenario_name:
    return IdleOnSiteScenario.Wiki(browser_driver,
                                   datetime.timedelta(minutes=60))
  if "idle_on_youtube" == scenario_name:
    return IdleOnSiteScenario.Youtube(browser_driver,
                                      datetime.timedelta(minutes=60))
  if "idle_on_netflix" == scenario_name:
    return IdleOnSiteScenario.Netflix(browser_driver,
                                      datetime.timedelta(minutes=60))

  if scenario_name.startswith("navigation"):

    if "navigation_top_sites" == scenario_name:
      NAVIGATED_SITES = [
          "https://amazon.com",
          "https://www.amazon.com/s?k=computer&ref=nb_sb_noss_2",
          "https://google.com", "https://www.google.com/search?q=computers",
          "https://www.youtube.com",
          "https://www.youtube.com/results?search_query=computers",
          "https://docs.google.com/document/d/1Ll-8Nvo6JlhzKEttst8GHWCc7_A8Hluy2fX99cy4Sfg/edit?usp=sharing"
      ]
    elif "navigation_heavy_sites" == scenario_name:
      NAVIGATED_SITES = [
          "https://www.chase.com/",
          "https://www.nytimes.com/ca/section/technology",
          "https://www.macys.com/",
          "https://fr.airbnb.ca/s/Montr%C3%A9al/homes?place_id=ChIJDbdkHFQayUwR7-8fITgxTmU&query=Montr%C3%A9al&refinement_paths%5B%5D=%2Fhomes&tab_id=home_tab",
          "https://www.homedepot.ca/search?q=computer#!q=computer",
          "https://polygon.com"
      ]
    else:
      # For navigation scenarios that are not predefined a site list is needed.
      if os.path.exists("sites.txt"):
        NAVIGATED_SITES = []
        with open("sites.txt") as sites_file:
          for site in sites_file:
            NAVIGATED_SITES.append(site.replace("\n", ""))
      else:
        raise ValueError(
            "Use predefined navigation scenarios or create sites.txt")

    # Aim for a benchmark that lasts up to 1 hour.
    navigation_duration_in_seconds = 15
    navigation_cycles = (
        60 * 60) / len(NAVIGATED_SITES) / navigation_duration_in_seconds

    return NavigationScenario(browser_driver,
                              navigation_duration=datetime.timedelta(
                                  seconds=navigation_duration_in_seconds),
                              navigation_cycles=navigation_cycles,
                              sites=NAVIGATED_SITES,
                              scenario_name=scenario_name)
  if "zero_window" == scenario_name:
    return ZeroWindowScenario(browser_driver, datetime.timedelta(minutes=60))
  return None
