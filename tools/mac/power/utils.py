# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import logging
import psutil
import os
import signal


def TerminateProcess(process: subprocess.Popen):
  """Terminates `process` and ensures it's cleaned up before returning.

    Raises:
      RuntimeError: When failed to terminate the process after a timeout.
  """

  logging.debug(f"Terminating PID:{process.pid}")

  try:
    process.terminate()
    process.wait(2.0)
  except (psutil.TimeoutExpired, psutil.AccessDenied, PermissionError) as e:
    logging.info(f"Terminate failed, moving on to kill.")
  except psutil.NoSuchProcess:
    return
  else:
    return

  try:
    process.kill()
    process.wait(2.0)
  except (psutil.TimeoutExpired, psutil.AccessDenied, PermissionError) as e:
    raise RuntimeError(f"Could not clean up PID:{process.pid}.") from e
  else:
    return


def SendSignalToRootProcess(process: subprocess.Popen, signal: signal.Signals):
  os.system(f"sudo kill -{signal.value} {process.pid}")


def TerminateRootProcess(process: subprocess.Popen):
  """Kills elevated `process` and ensures it's cleaned up before returning.

    Raises:
      RuntimeError: When failed to terminate the process after a timeout.
  """

  logging.debug(f"Terminating PID:{process.pid}")

  try:
    SendSignalToRootProcess(process, signal.SIGTERM)
    process.wait(2.0)
  except (psutil.TimeoutExpired, psutil.AccessDenied, PermissionError) as e:
    raise RuntimeError(f"Could not clean up PID:{process.pid}.") from e
  except psutil.NoSuchProcess:
    return
  else:
    return


def FindProcess(process_name: str) -> psutil.Process:
  """Looks for the process associated with |browser|.

  Args:
    process_name: Name of the process to return.

  Returns:
    A psutil.process representation of the process matching `process_name`.

  Raises:
    RuntimeError: When no process is found for the browser.
  """

  processes = filter(lambda p: p.name() == process_name, psutil.process_iter())
  returned_process = None
  for process in processes:
    if not returned_process:
      returned_process = process
    else:
      raise RuntimeError(
          "Too many copies of the process running, this is wrong.")

  return returned_process


def FindChromiumSrcDir(path: os.PathLike) -> os.PathLike:
  path = os.path.abspath(path)
  while path:
    (head, tail) = os.path.split(path)
    if tail == "src":
      return path
    path = head
