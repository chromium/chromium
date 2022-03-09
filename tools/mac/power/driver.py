# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ctypes
import io
import json
import logging
import os
import pandas as pd
import signal
import subprocess
import sys
import time
import typing

import browsers
import scenarios
import utils


class DriverContext:
  """Class in charge of running the measurements and keeping track
  of the global state needed to do so.
  """

  def __init__(self, output_dir: str, power_sample_path: str):
    """
    Args:
      output_dir: A string path of Where the results should be stored.
    """

    self._output_dir = output_dir
    self._power_sample_path = power_sample_path

    # Make sure there is somewhere to put  results.
    os.makedirs(f"{self._output_dir}", exist_ok=True)

  def __enter__(self):

    self._caffeinate_process = subprocess.Popen([
        "caffeinate",
        "-d",  # Prevent the display from sleeping.
    ])
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    utils.TerminateProcess(self._caffeinate_process)

  def SetMainDisplayBrightness(self, brightness_level: int):
    # This function imitates the open-source "brightness" tool at
    # https://github.com/nriley/brightness.
    # Since the benchmark doesn't care about older MacOSen, multiple displays
    # or other complications that tool has to consider, setting the brightness
    # level boils down to calling this function for the main display.
    CoreGraphics = ctypes.CDLL(
        "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
    main_display = CoreGraphics.CGMainDisplayID()
    DisplayServices = ctypes.CDLL(
        "/System/Library/PrivateFrameworks/DisplayServices.framework"
        "/DisplayServices")
    DisplayServices.DisplayServicesSetBrightness.argtypes = [
        ctypes.c_int, ctypes.c_float
    ]
    DisplayServices.DisplayServicesSetBrightness(main_display,
                                                 brightness_level / 100)

  def WaitBatteryNotFull(self):
    # Empirical evidence has shown that right after a full battery charge, the
    # current capacity stays equal to the maximum capacity for several minutes,
    # despite the fact that power is definitely consumed. To ensure that power
    # consumption estimates from battery level are meaningful, wait until the
    # battery is no longer reporting being fully charged before benchmarking.

    power_sampler_args = [
        self._power_sample_path, "--sample-on-notification",
        "--samplers=battery", "--sample-count=1"
    ]

    logging.info('Waiting for battery to no longer be full')

    while True:
      power_sampler_output = subprocess.check_output(power_sampler_args)
      power_sampler_data = pd.read_csv(io.BytesIO(power_sampler_output))
      max_capacity = power_sampler_data.iloc[0]['battery_max_capacity(Ah)']
      current_capacity = power_sampler_data.iloc[0][
          'battery_current_capacity(Ah)']

      logging.info(
          f'Battery level is {100 * current_capacity / max_capacity:.2f}%')

      if max_capacity != current_capacity:
        return

    logging.info('Battery is no longer full')

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
      check_env = subprocess.run([
          'zsh', '-c',
          os.path.join(os.path.dirname(__file__), 'check_env.sh')
      ],
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

    powermetrics_output = os.path.join(self._output_dir, scenario_driver.name,
                                       "powermetrics.plist")
    power_sampler_output = os.path.join(self._output_dir, scenario_driver.name,
                                        "power_sampler.json")
    power_sampler_battery_output = os.path.join(self._output_dir,
                                                scenario_driver.name,
                                                "power_sampler_battery.json")

    powermetrics_process = None
    power_sampler_process = None
    power_sampler_battery_process = None
    browser_process = None
    try:
      scenario_driver.Launch()
      if hasattr(scenario_driver, 'browser'):
        browser_process = scenario_driver.browser.browser_process

      powermetrics_args = [
          "sudo", "powermetrics", "-f", "plist", "--samplers",
          "tasks,cpu_power,gpu_power,thermal,disk,network",
          "--show-process-coalition", "--show-process-gpu",
          "--show-process-energy", "-i", "10000", "--output-file",
          powermetrics_output
      ]
      powermetrics_process = subprocess.Popen(powermetrics_args,
                                              stdout=subprocess.PIPE,
                                              stdin=subprocess.PIPE)
      power_sampler_battery_args = [
          self._power_sample_path, "--sample-on-notification",
          "--samplers=battery", "--simulate-user-active",
          f"--timeout={int(scenario_driver.duration.total_seconds())}",
          f"--json-output-file={power_sampler_battery_output}"
      ]
      power_sampler_battery_process = subprocess.Popen(
          power_sampler_battery_args,
          stdout=subprocess.PIPE,
          stdin=subprocess.PIPE)

      # No need to simulate the user is active from both power_sampler
      # instances.
      power_sampler_args = [
          self._power_sample_path, "--sample-interval=10",
          "--samplers=smc,user_idle_level,main_display",
          f"--timeout={int(scenario_driver.duration.total_seconds())}",
          f"--json-output-file={power_sampler_output}"
      ]
      if browser_process is not None:
        power_sampler_args += [
            f"--resource-coalition-pid={browser_process.pid}"
        ]
      power_sampler_process = subprocess.Popen(power_sampler_args,
                                               stdout=subprocess.PIPE,
                                               stdin=subprocess.PIPE)
      scenario_driver.Wait()
      power_sampler_process.wait()
      power_sampler_battery_process.wait()

    finally:
      scenario_driver.TearDown()
      if power_sampler_process:
        utils.TerminateProcess(power_sampler_process)
      if power_sampler_battery_process:
        utils.TerminateProcess(power_sampler_battery_process)
      if powermetrics_process:
        # Force powermetrics to flush data.
        utils.SendSignalToRootProcess(powermetrics_process, signal.SIGIO)
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
    if scenario_driver.browser.name not in ["chromium", "canary", "chrome"]:
      raise ValueError("Only Chromium can be profiled! Skipping.")

    self.WriteScenarioSummary(scenario_driver)

    dtraces_output_dir = os.path.join(self._output_dir, scenario_driver.name,
                                      f"dtraces_{profile_mode}")
    os.makedirs(dtraces_output_dir, exist_ok=True)
    scenario_driver.Launch()
    browser_process = scenario_driver.browser.browser_process

    # Set up the environment for correct dtrace execution.
    dtrace_env = os.environ.copy()
    dtrace_env["DYLD_SHARED_REGION"] = "avoid"

    pid_to_subprocess: typing.Dict[str, subprocess.Popen] = {}

    try:
      with open(
          os.path.join(self._output_dir, scenario_driver.name,
                       f'dtrace_{profile_mode}_log.txt'), "w") as dtrace_log:
        # Keep looking for child processes as long as the scenario is running.
        while scenario_driver.IsRunning():

          # Let some time pass to limit the overhead of this script.
          time.sleep(0.100)
          logging.debug("Looking for child processes")

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
              logging.debug(f"Found new child!:{pid}")
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
        logging.debug(f"Waiting for dtrace hooked on {pid} to exit")
        dtrace_process.wait(30)

  def WriteScenarioSummary(
      self, scenario_driver: scenarios.ScenarioWithBrowserOSADriver):
    """Outputs a json file describing `scenario_driver` arguments into the
        output directory
    """
    os.makedirs(os.path.join(self._output_dir, scenario_driver.name),
                exist_ok=True)
    with open(
        os.path.join(self._output_dir, scenario_driver.name, 'metadata.json'),
        'w') as summary_file:
      json.dump(scenario_driver.Summary(), summary_file, indent=2)
