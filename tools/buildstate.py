#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script checks to see what build commands are currently running by printing
the command lines of any processes that are the children of ninja processes.

The idea is that if the build is serialized (not many build steps running) then
you can run this to see what it is serialized on.

This uses python3 on Windows and vpython elsewhere (for psutil).
"""

import sys


def main():
  parents = []
  processes = []

  print('Gathering process data...')
  # Ninja's name on Linux is ninja-linux64, presumably different elsewhere, so
  # we look for a matching prefix.
  ninja_prefix = 'ninja.exe' if sys.platform in ['win32', 'cygwin'] else 'ninja'

  if sys.platform in ['win32', 'cygwin']:
    # psutil handles short-lived ninja descendants poorly on Windows (it misses
    # most of them) so use wmic instead.
    import subprocess
    cmd = 'wmic process get Caption,ParentProcessId,ProcessId,CommandLine'
    lines = subprocess.check_output(cmd, universal_newlines=True).splitlines()

    # Find the offsets for the various data columns by looking at the labels in
    # the first line of output.
    CAPTION_OFF = 0
    COMMAND_LINE_OFF = lines[0].find('CommandLine')
    PARENT_PID_OFF = lines[0].find('ParentProcessId')
    PID_OFF = lines[0].find(' ProcessId') + 1

    for line in lines[1:]:
      # Ignore blank lines
      if not line.strip():
        continue
      command = line[:COMMAND_LINE_OFF].strip()
      command_line = line[COMMAND_LINE_OFF:PARENT_PID_OFF].strip()
      parent_pid = int(line[PARENT_PID_OFF:PID_OFF].strip())
      pid = int(line[PID_OFF:].strip())
      processes.append((command, command_line, parent_pid, pid))

  else:
    # Portable process-collection code, but works badly on Windows.
    import psutil
    for proc in psutil.process_iter(['pid', 'ppid', 'name', 'cmdline']):
      try:
        cmdline = proc.cmdline()
        # Convert from list to a single string.
        cmdline = ' '.join(cmdline)
      except psutil.AccessDenied:
        cmdline = "Access denied"
      processes.append(
          (proc.name()[:], cmdline, int(proc.ppid()), int(proc.pid)))

  # Scan the list of processes to find ninja.
  for process in processes:
    command, command_line, parent_pid, pid = process
    if command.startswith(ninja_prefix):
      parents.append(pid)

  if not parents:
    print('No interesting parent processes found.')
    return 1

  print('Tracking the children of these PIDs:')
  print(', '.join(map(lambda x: str(x), parents)))

  print()

  # Print all the processes that have parent-processes of interest.
  count = 0
  for process in processes:
    command, command_line, parent_pid, pid = process
    if parent_pid in parents:
      if not command_line:
        command_line = command
      print('%5d: %s' % (pid, command_line[:160]))
      count += 1
  print('Found %d children' % count)
  return 0


if __name__ == '__main__':
  main()
