# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import psutil
import subprocess
import sys
import time

import utils

class Driver:
  """Class in charge of running the measurements and keeping track
  of the global state needed to do so.
  """

  def __init__(self, output_dir):
    """
    Args:
      output_dir: A string path of Where the results should be stored.
    """

    self._started_processeds = []
    self.__output_dir = output_dir

    # Make sure there is somewhere to put  results.
    os.makedirs(f"{self.__output_dir}", exist_ok=True)

  def CheckEnv(self, throw_on_bad_env):
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
    for browser in utils.get_browser_process_names():
      if self.FindBrowserProcess(browser):
        logging_function(f"{browser} already running. \
                Make sure to close it before running again.")

        if throw_on_bad_env:
          sys.exit(-1)

  def Teardown(self):
    """Cleans up global state after all calls to Record()/Profile().

    Makes sure that all processes started for measurement/profiling are exited
    and that the execution can move on to the next one.
    """

    # Cleanup can't be achieved with a simple call to psutil.waitprocs() because
    # some executables are started with sudo and will end up as a zombie
    # processes.
    for process in self._started_processeds:
      logging.info(f"Terminating PID:{process.pid}")

      try:
        process.terminate()
        process.wait(0.5)
      except psutil.NoSuchProcess:
        continue
      except (psutil.TimeoutExpired, psutil.AccessDenied) as e:
        logging.info(f"Terminate failed, moving on to kill.")

      try:
        process.kill()
        process.wait(0.5)
      except psutil.NoSuchProcess:
        continue
      except (psutil.TimeoutExpired, psutil.AccessDenied) as e:
        logging.info(f"Kill failed, trying sudo kill.")

      try:
        os.system(f"sudo kill {process.pid}")
        process.wait(0.5)
      except psutil.NoSuchProcess:
        continue
      except psutil.TimeoutExpired:
        logging.error(f"Could not clean up PID:{process.pid}. Aborting")
        sys.exit(-1)

    # Start over for next round.
    self._started_processeds.clear()

  def FindBrowserProcess(self, browser):
    """Looks for the process associated with |browser|.

    Args:
      browser: A string of the browser name

    Returns:
      A psutil.process representation of the browser process.

    Raises:
      SystemExit: When no process is found for the browser.
    """

    process_name = utils.get_browser_property(browser, 'process_name')
    processes = filter(lambda p: p.name() == process_name,
                       psutil.process_iter())
    browser_process = None

    for process in processes:
      if not browser_process:
        browser_process = process
      else:
        logging.error("Too many copies of the browser running, this is wrong")
        sys.exit(-1)

    return browser_process

  def RunScenario(self, scenario_config):
    """Start the browser and initiate the scenario

    Args:
      scenario_config: A dictionary describing the scenario.

    Returns: A psutil.process representation the driver script process.
    """

    scenario_browser = scenario_config["browser"]

    if scenario_browser is not None:
      browser_executable = utils.get_browser_property(scenario_browser,
                                                      'executable')
      if scenario_browser in ["Chromium", "Chrome", "Canary", "Edge"]:
        subprocess.call(["open", "-a", browser_executable, "--args"] +
                        ["--enable-benchmarking", "--disable-stack-profiler"] +
                        scenario_config["extra_args"])
      elif scenario_browser == "Safari":
        subprocess.call(["open", "-a", browser_executable])
        # Call prep_safari.scpt to make sure the run starts clean. See file
        # comment for details.
        subprocess.call(["osascript", './driver_scripts/prep_safari.scpt'])
        subprocess.call(["open", "-a", browser_executable, "--args"] +
                        scenario_config["extra_args"])

    # Wait for the browser to be started and ready for AppleScript commands.
    if scenario_browser:
      browser_process_name = utils.get_browser_property(scenario_browser,
                                                        'process_name')
      browser_process = None
      while not browser_process:
        browser_process = self.FindBrowserProcess(scenario_config["browser"])
        time.sleep(0.100)
        logging.info(f"Waiting for {browser_process_name} to start")

    self._started_processeds.append(browser_process)

    driver_script_args = [
        "osascript", f'./driver_scripts/{scenario_config["driver_script"]}.scpt'
    ]
    process = subprocess.Popen(driver_script_args)

    return process

  def Record(self, scenario_config):
    """Cover the running of the scenario with powermetrics and save
    the results

    Args:
      scenario_config: A dictionary describing the scenario.
    """

    output_file = \
        f'./{self.__output_dir}/{scenario_config["name"]}_powermetrics.plist'

    with open(output_file, "w") as powermetrics_output:

      # TODO(crbug.com/1224994): Narrow down samplers to only those of interest.
      powermetrics_args = [
          "sudo", "powermetrics", "-f", "plist", "--samplers", "all",
          "--show-responsible-pid", "--show-process-gpu",
          "--show-process-energy", "-i", "60000"
      ]

      powermetrics_process = subprocess.Popen(powermetrics_args,
                                              stdout=powermetrics_output,
                                              stdin=subprocess.PIPE)

      self._started_processeds.append(psutil.Process(powermetrics_process.pid))


    # No need to add |scenario_process| to |self._started_processeds| as it's
    # explicitly waited on.
    scenario_process = self.RunScenario(scenario_config)
    scenario_process.wait()

    self.Teardown()


  def GetAllPids(self, browser_process):
    """Get the pids for the browser and all children. w

    Args:
      browser_process: A psutil.Process object associated with the
      browser process.

    Returns:
      A list of pids as integers.
    """

    pids = [browser_process.pid]
    try:
      children = browser_process.children(recursive=True)
    except psutil.NoSuchProcess:
      return []

    for child in children:
      pids.append(child.pid)

    return pids

  def Profile(self, scenario_config, profile_mode):
    """Cover the running of the scenario with DTrace and save the
    results.

    Args:
      profile_mode: A string describing the Profile mode between "wakeups"
      and "cpu_time".

    Raises:
      TimeoutExpired: When a DTrace process takes more than 30 seconds to
      terminate after the end of the scenario.
    """

    if scenario_config["browser"] != "Chromium":
      logging.error("Only Chromium can be profiled! Skipping.")
      return

    script_process = self.RunScenario(scenario_config)
    browser_process = self.FindBrowserProcess(scenario_config["browser"])

    # Set up the environment for correct dtrace execution.
    dtrace_env = os.environ.copy()
    dtrace_env["DYLD_SHARED_REGION"] = "avoid"

    pid_to_subprocess = {}

    with open('./dtrace_log.txt', "w") as dtrace_log:
      # Keep looking for child processes as long as the scenario is running.
      while script_process.poll() is None:

        # Let some time pass to limit the overhead of this script.
        time.sleep(0.100)
        logging.info("Looking for child processes")

        # Watch for new processes and follow those too.
        for pid in self.GetAllPids(browser_process):
          if profile_mode == "wakeups":
            probe_def = \
              f"mach_kernel::wakeup/pid == {pid}/ {{ @[ustack()] = count(); }}"
          else:
            probe_def = \
              f"profile-1001/pid == {pid}/ {{ @[ustack()] = count(); }}"

          dtrace_args = [
              'sudo', 'dtrace', '-p', f"{pid}", "-o",
              f"{self.__output_dir}/{pid}.txt", '-n', probe_def
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

    script_process.wait()

    # Cleanup executed before waiting for the DTrace processes so that they
    # see that the browse is gone.
    self.Teardown()

    for pid, dtrace_process in pid_to_subprocess.items():
      logging.info(f"Waiting for dtrace hooked on {pid} to exit")
      dtrace_process.wait(30)

