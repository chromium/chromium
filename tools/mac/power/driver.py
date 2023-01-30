# Copyright 2021 The Chromium Authors
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
import datetime
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

    # Launch a power_sampler whose job is only to simulate an active user. This
    # is applicable to both profiling and measuring so should always be done.
    power_sampler_args = [
        self._power_sample_path, "--no-samplers", "--simulate-user-active"
    ]
    self._power_sampler_process = subprocess.Popen(power_sampler_args,
                                                   stdout=subprocess.PIPE,
                                                   stdin=subprocess.PIPE)

    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    utils.TerminateProcess(self._caffeinate_process)
    utils.TerminateProcess(self._power_sampler_process)

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

    powermetrics_process = None
    power_sampler_process = None
    browser_process = None

    cycle_length_in_secs = scenario_driver.CycleDuration().total_seconds()
    if cycle_length_in_secs < 60:
      raise ValueError("Cycle length must be more than 60 seconds!")
    elif cycle_length_in_secs % 60 != 0:
      logging.warning("Cycle has a duration not divisible by 60 secs."
                      "This is suboptimal for measurment variance.")
    try:
      scenario_driver.Launch()
      if hasattr(scenario_driver, 'browser'):
        browser_process = scenario_driver.browser.browser_process

      # "-i 60000" to emit a sample every minute. This is the same frequency as
      # power_sampler, which emits a sample on IOPMPowerSource notification,
      # which happens every minute.
      powermetrics_args = [
          "sudo", "powermetrics", "-f", "plist", "--samplers",
          "tasks,cpu_power,gpu_power,thermal,disk,network",
          "--show-process-coalition", "--show-process-gpu",
          "--show-process-energy", "-i", "60000", "--output-file",
          powermetrics_output
      ]

      powermetrics_process = subprocess.Popen(powermetrics_args)

      power_sampler_args = [
          self._power_sample_path, f"--sample-on-notification",
          f"--initial-sample",
          "--samplers=battery,smc,user_idle_level,main_display",
          f"--timeout={int(scenario_driver.duration.total_seconds())}",
          f"--json-output-file={power_sampler_output}"
      ]

      if browser_process is not None:
        power_sampler_args += [
            f"--resource-coalition-pid={browser_process.pid}"
        ]
      power_sampler_process = subprocess.Popen(power_sampler_args)

      scenario_driver.Wait()

      logging.debug("Waiting for power_sampler to exit")
      power_sampler_process.wait()
      logging.debug(
          f"power_sampler returned {power_sampler_process.returncode}")

    finally:
      scenario_driver.TearDown()
      if power_sampler_process:
        utils.TerminateProcess(power_sampler_process)
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

    try:
      with open(
          os.path.join(self._output_dir, scenario_driver.name,
                       f'dtrace_{profile_mode}_log.txt'), "w") as dtrace_log:

        scripts_dir = os.path.join(os.path.dirname(__file__), "dtrace_scripts")
        if profile_mode == "wakeups":
          script = os.path.join(scripts_dir, "iwakeups.d")
        else:
          script = os.path.join(scripts_dir, "profile.d")

        pid = browser_process.pid
        iteration = 1

        # Capture until the scenario is done running.
        while True:
          output_filename = os.path.join(dtraces_output_dir,
                                         f"{pid}_{iteration}.txt")
          dtrace_args = [
              'sudo', 'dtrace', '-p', f"{pid}", "-o", output_filename, '-s',
              script, f"{pid}"
          ]

          dtrace_process = subprocess.Popen(dtrace_args,
                                            env=dtrace_env,
                                            stdout=dtrace_log,
                                            stderr=dtrace_log)

          # This timeout was chosen experimentally to on an M1 MBA to avoid
          # running for too long and losing samples (because of suspected
          #  bug in Dtrace) and running too little and thus having too much
          # overhead.
          time.sleep(7.199)
          utils.TerminateRootProcess(dtrace_process)
          iteration = iteration + 1

          if scenario_driver.script_process.poll() is not None:
            break

    finally:
      scenario_driver.TearDown()

    logging.debug(f"Waiting for dtrace to exit")

  def Trace(self, scenario_driver: scenarios.ScenarioOSADriver):
    self.WriteScenarioSummary(scenario_driver)

    try:
      scenario_driver.Launch()
      scenario_driver.Wait()
    finally:
      scenario_driver.TearDown()

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
