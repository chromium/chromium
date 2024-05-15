#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script takes in a list of test targets to be run on both Linux and
# Fuchsia devices and then compares their output to each other, extracting the
# relevant performance data from the output of gtest.

from __future__ import print_function

import argparse
import logging
import os
import re
import subprocess
import sys
import time

from collections import defaultdict
from typing import Tuple, Dict, List

import target_spec
import test_results


def RunCommand(command: List[str], msg: str) -> str:
  """Runs a command and returns the standard output.

  Args:
    command (List[str]): The list of command chunks to use in subprocess.run.
        ex: ['git', 'grep', 'cat'] to find all instances of cat in a repo.
    msg (str): An error message in case the subprocess fails for some reason.

  Raises:
    subprocess.SubprocessError: Raises this with the command that failed in the
        event that the return code of the process is non-zero.

  Returns:
    str: the standard output of the subprocess.
  """
  command = [piece for piece in command if piece != ""]
  proc = subprocess.run(
      command,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      stdin=subprocess.DEVNULL)
  out = proc.stdout.decode("utf-8", errors="ignore")
  err = proc.stderr.decode("utf-8", errors="ignore")
  if proc.returncode != 0:
    sys.stderr.write("{}\nreturn code: {}\nstdout: {}\nstderr: {}".format(
        msg, proc.returncode, out, err))
    raise subprocess.SubprocessError(
        "Command failed to complete successfully. {}".format(command))
  return out


# TODO(crbug.com/41392149): replace with --test-launcher-filter-file directly
def ParseFilterFile(filepath: str, p_filts: List[str],
                    n_filts: List[str]) -> str:
  """Takes a path to a filter file, parses it, and constructs a gtest_filter
  string for test execution.

  Args:
    filepath (str): The path to the filter file to be parsed into a
        --gtest_filter flag.
    p_filts (List[str]): An initial set of positive filters passed in a flag.
    n_filts (List[str]): An initial set of negative filters passed in a flag.

  Returns:
    str: The properly-joined together gtest_filter flag.
  """
  positive_filters = p_filts
  negative_filters = n_filts
  with open(filepath, "r") as file:
    for line in file:
      # Only take the part of a line before a # sign
      line = line.split("#", 1)[0].strip()
      if line == "":
        continue
      elif line.startswith("-"):
        negative_filters.append(line[1:])
      else:
        positive_filters.append(line)

  return "--gtest_filter={}-{}".format(":".join(positive_filters),
                                       ":".join(negative_filters))


class TestTarget(object):
  """TestTarget encapsulates a single BUILD.gn target, extracts a name from the
  target string, and manages the building and running of the target for both
  Linux and Fuchsia.
  """

  def __init__(self, target: str, p_filts: List[str], n_filts: List[str]):
    self._target = target
    self._name = target.split(":")[-1]
    self._filter_file = "testing/buildbot/filters/fuchsia.{}.filter".format(
        self._name)
    if not os.path.isfile(self._filter_file):
      self._filter_flag = ""
      self._filter_file = ""
    else:
      self._filter_flag = ParseFilterFile(self._filter_file, p_filts, n_filts)

  def ExecFuchsia(self, out_dir: str, run_locally: bool) -> str:
    """Execute this test target's test on Fuchsia, either with QEMU or on actual
    hardware.

    Args:
      out_dir (str): The Fuchsia output directory.
      run_locally (bool): Whether to use QEMU(true) or a physical device(false)

    Returns:
      str: The standard output of the test process.
    """

    runner_name = "{}/bin/run_{}".format(out_dir, self._name)
    command = [runner_name, self._filter_flag, "--exclude-system-logs"]
    if not run_locally:
      command.append("-d")
    return RunCommand(command,
                      "Test {} failed on Fuchsia!".format(self._target))

  def ExecLinux(self, out_dir: str, run_locally: bool) -> str:
    """Execute this test target's test on Linux, either with QEMU or on actual
    hardware.

    Args:
      out_dir (str): The Linux output directory.
      run_locally (bool): Whether to use the host machine(true) or a physical
          device(false)

    Returns:
      str: The standard output of the test process.
    """
    command = []  # type: List[str]
    user = target_spec.linux_device_user
    ip = target_spec.linux_device_ip
    host_machine = "{0}@{1}".format(user, ip)
    if not run_locally:
      # Next is the transfer of all the directories to the destination device.
      self.TransferDependencies(out_dir, host_machine)
      command = [
          "ssh", "{}@{}".format(user, ip), "{1}/{0}/{1} -- {2}".format(
              out_dir, self._name, self._filter_flag)
      ]
    else:
      local_path = "{}/{}".format(out_dir, self._name)
      command = [local_path, "--", self._filter_flag]
    return RunCommand(command, "Test {} failed on linux!".format(self._target))

  def TransferDependencies(self, out_dir: str, host: str):
    """Transfer the dependencies of this target to the machine to execute the
    test.

    Args:
      out_dir (str): The output directory to find the dependencies in.
      host (str): The IP address of the host to receive the dependencies.
    """

    gn_desc = ["gn", "desc", out_dir, self._target, "runtime_deps"]
    out = RunCommand(
        gn_desc, "Failed to get dependencies of target {}".format(self._target))

    paths = []
    for line in out.split("\n"):
      if line == "":
        continue
      line = out_dir + "/" + line.strip()
      line = os.path.abspath(line)
      paths.append(line)
    common = os.path.commonpath(paths)
    paths = [os.path.relpath(path, common) for path in paths]

    archive_name = self._name + ".tar.gz"
    # Compress the dependencies of the test.
    command = ["tar", "-czf", archive_name] + paths
    if self._filter_file != "":
      command.append(self._filter_file)
    RunCommand(
        command,
        "{} dependency compression failed".format(self._target),
    )
    # Make sure the containing directory exists on the host, for easy cleanup.
    RunCommand(["ssh", host, "mkdir -p {}".format(self._name)],
               "Failed to create directory on host for {}".format(self._target))
    # Transfer the test deps to the host.
    RunCommand(
        [
            "scp", archive_name, "{}:{}/{}".format(host, self._name,
                                                   archive_name)
        ],
        "{} dependency transfer failed".format(self._target),
    )
    # Decompress the dependencies once they're on the host.
    RunCommand(
        [
            "ssh", host, "tar -xzf {0}/{1} -C {0}".format(
                self._name, archive_name)
        ],
        "{} dependency decompression failed".format(self._target),
    )
    # Clean up the local copy of the archive that is no longer needed.
    RunCommand(
        ["rm", archive_name],
        "{} dependency archive cleanup failed".format(self._target),
    )


def RunTest(target: TestTarget, run_locally: bool = False) -> None:
  """Run the given TestTarget on both Linux and Fuchsia

  Args:
    target (TestTarget): The TestTarget to run.
    run_locally (bool, optional): Defaults to False. Whether the test should be
        run on the host machine, or sent to remote devices for execution.

  Returns:
    None: Technically an IO (), as it writes to the results files
  """

  linux_out = target.ExecLinux(target_spec.linux_out_dir, run_locally)
  linux_result = test_results.TargetResultFromStdout(linux_out.splitlines(),
                                                     target._name)
  print("Ran Linux")
  fuchsia_out = target.ExecFuchsia(target_spec.fuchsia_out_dir, run_locally)
  fuchsia_result = test_results.TargetResultFromStdout(fuchsia_out.splitlines(),
                                                       target._name)
  print("Ran Fuchsia")
  outname = "{}.{}.json".format(target._name, time.time())
  linux_result.WriteToJson("{}/{}".format(target_spec.raw_linux_dir, outname))
  fuchsia_result.WriteToJson("{}/{}".format(target_spec.raw_fuchsia_dir,
                                            outname))
  print("Wrote result files")


def RunGnForDirectory(dir_name: str, target_os: str, is_debug: bool) -> None:
  """Create the output directory for test builds for an operating system.

  Args:
    dir_name (str): The name to use for the output directory. This will be
        created if it does not exist.
    target_os (str): The operating system to initialize this directory for.
    is_debug (bool): Whether or not this is a debug build of the tests in
        question.

  Returns:
    None: It has a side effect of replacing args.gn
  """

  if not os.path.exists(dir_name):
    os.makedirs(dir_name)

  debug_str = str(is_debug).lower()

  with open("{}/{}".format(dir_name, "args.gn"), "w") as args_file:
    args_file.write("is_debug = {}\n".format(debug_str))
    args_file.write("dcheck_always_on = false\n")
    args_file.write("is_component_build = false\n")
    args_file.write("use_remoteexec = true\n")
    args_file.write("target_os = \"{}\"\n".format(target_os))

  subprocess.run(["gn", "gen", dir_name]).check_returncode()


def GenerateTestData(do_config: bool, do_build: bool, num_reps: int,
                     is_debug: bool, filter_flag: str):
  """Initializes directories, builds test targets, and repeatedly executes them
  on both operating systems

  Args:
    do_config (bool): Whether or not to run GN for the output directories
    do_build (bool): Whether or not to run ninja for the test targets.
    num_reps (int): How many times to run each test on a given device.
    is_debug (bool): Whether or not this should be a debug build of the tests.
    filter_flag (str): The --gtest_filter flag, to be parsed as such.
  """
  # Find and make the necessary directories
  DIR_SOURCE_ROOT = os.path.abspath(
      os.path.join(os.path.dirname(__file__), *([os.pardir] * 3)))
  os.chdir(DIR_SOURCE_ROOT)
  os.makedirs(target_spec.results_dir, exist_ok=True)
  os.makedirs(target_spec.raw_linux_dir, exist_ok=True)
  os.makedirs(target_spec.raw_fuchsia_dir, exist_ok=True)

  # Grab parameters from config file.
  linux_dir = target_spec.linux_out_dir
  fuchsia_dir = target_spec.fuchsia_out_dir

  # Parse filters passed in by flag
  pos_filter_chunk, neg_filter_chunk = filter_flag.split("-", 1)
  pos_filters = pos_filter_chunk.split(":")
  neg_filters = neg_filter_chunk.split(":")

  test_input = []  # type: List[TestTarget]
  for target in target_spec.test_targets:
    test_input.append(TestTarget(target, pos_filters, neg_filters))
  print("Test targets collected:\n{}".format(",".join(
      [test._target for test in test_input])))
  if do_config:
    RunGnForDirectory(linux_dir, "linux", is_debug)
    RunGnForDirectory(fuchsia_dir, "fuchsia", is_debug)
    print("Ran GN")
  elif is_debug:
    logging.warning("The --is_debug flag is ignored unless --do_config is also \
                     specified")

  if do_build:
    # Build test targets in both output directories.
    for directory in [linux_dir, fuchsia_dir]:
      build_command = ["autoninja", "-C", directory] \
                    + [test._target for test in test_input]
      RunCommand(build_command,
                 "autoninja failed in directory {}".format(directory))
    print("Builds completed.")

  # Execute the tests, one at a time, per system, and collect their results.
  for i in range(0, num_reps):
    print("Running Test set {}".format(i))
    for test_target in test_input:
      print("Running Target {}".format(test_target._name))
      RunTest(test_target)
      print("Finished {}".format(test_target._name))

  print("Tests Completed")


def main() -> int:
  cmd_flags = argparse.ArgumentParser(
      description="Execute tests repeatedly and collect performance data.")
  cmd_flags.add_argument(
      "--do-config",
      action="store_true",
      help="WARNING: This flag over-writes args.gn in the directories "
           "configured. GN is executed before running the tests.")
  cmd_flags.add_argument(
      "--do-build",
      action="store_true",
      help="Build the tests before running them.")
  cmd_flags.add_argument(
      "--is-debug",
      action="store_true",
      help="This config-and-build cycle is a debug build")
  cmd_flags.add_argument(
      "--num-repetitions",
      type=int,
      default=1,
      help="The number of times to execute each test target.")
  cmd_flags.add_argument(
      "--gtest_filter",
      type=str,
      default="",
  )
  cmd_flags.parse_args()
  GenerateTestData(cmd_flags.do_config, cmd_flags.do_build,
                   cmd_flags.num_repetitions, cmd_flags.is_debug,
                   cmd_flags.gtest_filter)
  return 0


if __name__ == "__main__":
  sys.exit(main())
