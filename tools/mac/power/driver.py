# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import subprocess
import sys
import time
import typing

import browsers
import scenarios
import utils

class Driver:
  """Class in charge of running the measurements and keeping track
  of the global state needed to do so.
  """

  def __init__(self, output_dir: str):
    """
    Args:
      output_dir: A string path of Where the results should be stored.
    """

    self._output_dir = output_dir

    # Make sure there is somewhere to put  results.
    os.makedirs(f"{self._output_dir}", exist_ok=True)

  def CheckEnv(self, throw_on_bad_env: bool):
    """Verifies that the environment is conducive to proper profiling or
    measurements.

    Args:
      throw_on_bad_env: False if executions continues no matter what and
      only warnings are printed.

    Raises:
      SystemExit: When the environment is invalid and throw_on_bad_env is
      True.
    """

    if throw_on_bad_env:
      logging_function = logging.error
    else:
      logging_function = logging.warning

    logging.warning("Trying sudo access. Possibly enter password:")
    sudo_check = subprocess.Popen(["sudo", "ls"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT)
    sudo_check.wait()

    try:
      check_env = subprocess.run(['zsh', '-c', './check_env.sh'],
                                 check=throw_on_bad_env,
                                 capture_output=True)
      logging_function(check_env.stdout.decode('ascii'))
    except subprocess.CalledProcessError as e:
      logging_function(e.stdout.decode('ascii'))
      sys.exit(-1)

    # Make sure that no browsers are running which would affect the
    # tests.
    for browser_name in browsers.PROCESS_NAMES:
      if utils.FindProcess(browser_name):
        logging_function(f"{browser_name} already running. "
                         "Make sure to close it before running again.")

        if throw_on_bad_env:
          sys.exit(-1)

  def Record(self, scenario_driver: scenarios.ScenarioOSADriver):
    """Cover the running of the scenario with powermetrics and save
    the results

    Args:
      scenario_config: A dictionary describing the scenario.
    """

    self.WriteScenarioSummary(scenario_driver)

    output_file = \
        f'{self._output_dir}/{scenario_driver.name}_powermetrics.plist'

    powermetrics_process = None
    try:
      scenario_driver.Launch()
      with open(output_file, "w") as powermetrics_output:

        # TODO(crbug.com/1224994): Narrow down samplers to only those of
        # interest.
        powermetrics_args = [
            "sudo", "powermetrics", "-f", "plist", "--samplers", "all",
            "--show-responsible-pid", "--show-process-gpu",
            "--show-process-energy", "-i", "60000"
        ]

        powermetrics_process = subprocess.Popen(powermetrics_args,
                                                stdout=powermetrics_output,
                                                stdin=subprocess.PIPE)

        # No need to add |scenario_process| to |self._started_processeds| as
        # it's explicitly waited on.
        scenario_driver.Wait()

    finally:
      scenario_driver.TearDown()
      if powermetrics_process:
        utils.TerminateRootProcess(powermetrics_process)

  def Profile(self, scenario_driver: scenarios.ScenarioWithBrowserOSADriver,
              profile_mode: str):
    """Cover the running of the scenario with DTrace and save the
    results.

    Args:
      profile_mode: A string describing the Profile mode between "wakeups"
      and "cpu_time".

    Raises:
      TimeoutExpired: When a DTrace process takes more than 30 seconds to
      terminate after the end of the scenario.
    """

    if scenario_driver.browser is None:
      raise ValueError("Scenario must have an associated browser.")
    if scenario_driver.browser.process_name != "Chromium":
      raise ValueError("Only Chromium can be profiled! Skipping.")

    self.WriteScenarioSummary(scenario_driver)

    dtraces_output_dir = os.path.join(
        self._output_dir, f"{scenario_driver.name}_dtraces_{profile_mode}")
    os.makedirs(dtraces_output_dir, exist_ok=True)
    scenario_driver.Launch()
    browser_process = scenario_driver.browser.browser_process

    # Set up the environment for correct dtrace execution.
    dtrace_env = os.environ.copy()
    dtrace_env["DYLD_SHARED_REGION"] = "avoid"

    pid_to_subprocess: typing.Dict[str, subprocess.Popen] = {}

    try:
      with open(
          os.path.join(self._output_dir,
                       f'{scenario_driver.name}_dtrace_{profile_mode}_log.txt'),
          "w") as dtrace_log:
        # Keep looking for child processes as long as the scenario is running.
        while scenario_driver.IsRunning():

          # Let some time pass to limit the overhead of this script.
          time.sleep(0.100)
          logging.info("Looking for child processes")

          # Watch for new processes and follow those too.
          for process in browser_process.children(
              recursive=True) + [browser_process]:
            pid = process.pid
            if profile_mode == "wakeups":
              probe_def = \
                f"mach_kernel::wakeup/pid == {pid}/ " \
                "{{ @[ustack(64)] = count(); }}"
            else:
              probe_def = \
                f"profile-1001/pid == {pid}/ {{ @[ustack(64)] = count(); }}"
            output_filename = os.path.join(dtraces_output_dir, f"{pid}.txt")
            dtrace_args = [
                'sudo', 'dtrace', '-p', f"{pid}", "-o", output_filename, '-n',
                probe_def
            ]

            if pid not in pid_to_subprocess:
              logging.info(f"Found new child!:{pid}")
              # No need to add |process| to |self._started_processeds| as it's
              # explicitly waited on later.
              process = subprocess.Popen(dtrace_args,
                                         env=dtrace_env,
                                         stdout=dtrace_log,
                                         stderr=dtrace_log)
              pid_to_subprocess[pid] = process

      scenario_driver.Wait()

    finally:
      scenario_driver.TearDown()

      for pid, dtrace_process in pid_to_subprocess.items():
        logging.info(f"Waiting for dtrace hooked on {pid} to exit")
        dtrace_process.wait(30)

  def WriteScenarioSummary(
      self, scenario_driver: scenarios.ScenarioWithBrowserOSADriver):
    """Outputs a json file describing `scenario_driver` arguments into the
        output directory
    """
    with open(
        os.path.join(self._output_dir, f'{scenario_driver.name}_summary.json'),
        'w') as summary_file:
      json.dump(scenario_driver.Summary(), summary_file, indent=2)
