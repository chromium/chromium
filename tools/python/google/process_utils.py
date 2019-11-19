# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared process-related utility functions."""

from __future__ import print_function

import errno
import os
import subprocess
import sys

class CommandNotFound(Exception): pass


TASKKILL = os.path.join(os.environ['WINDIR'], 'system32', 'taskkill.exe')
TASKKILL_PROCESS_NOT_FOUND_ERR = 128
# On windows 2000 there is no taskkill.exe, we need to have pskill somewhere
# in the path.
PSKILL = 'pskill.exe'
PSKILL_PROCESS_NOT_FOUND_ERR = -1

def KillAll(executables):
  """Tries to kill all copies of each process in the processes list.  Returns
  an error if any running processes couldn't be killed.
  """
  result = 0
  if os.path.exists(TASKKILL):
    command = [TASKKILL, '/f', '/im']
    process_not_found_err = TASKKILL_PROCESS_NOT_FOUND_ERR
  else:
    command = [PSKILL, '/t']
    process_not_found_err = PSKILL_PROCESS_NOT_FOUND_ERR

  for name in executables:
    new_error = RunCommand(command + [name])
    # Ignore "process not found" error.
    if new_error != 0 and new_error != process_not_found_err:
      result = new_error
  return result

def RunCommandFull(command, verbose=True, collect_output=False,
                   print_output=True):
  """Runs the command list.

  Prints the given command (which should be a list of one or more strings).
  If specified, prints its stderr (and optionally stdout) to stdout,
  line-buffered, converting line endings to CRLF (see note below).  If
  specified, collects the output as a list of lines and returns it.  Waits
  for the command to terminate and returns its status.

  Args:
    command: the full command to run, as a list of one or more strings
    verbose: if True, combines all output (stdout and stderr) into stdout.
             Otherwise, prints only the command's stderr to stdout.
    collect_output: if True, collects the output of the command as a list of
                    lines and returns it
    print_output: if True, prints the output of the command

  Returns:
    A tuple consisting of the process's exit status and output.  If
    collect_output is False, the output will be [].

  Raises:
    CommandNotFound if the command executable could not be found.
  """
  print(
      '\n' + subprocess.list2cmdline(command).replace('\\', '/') + '\n',
      end=' ')

  if verbose:
    out = subprocess.PIPE
    err = subprocess.STDOUT
  else:
    out = file(os.devnull, 'w')
    err = subprocess.PIPE
  try:
    proc = subprocess.Popen(command, stdout=out, stderr=err, bufsize=1)
  except OSError, e:
    if e.errno == errno.ENOENT:
      raise CommandNotFound('Unable to find "%s"' % command[0])
    raise

  output = []

  if verbose:
    read_from = proc.stdout
  else:
    read_from = proc.stderr
  line = read_from.readline()
  while line:
    line = line.rstrip()

    if collect_output:
      output.append(line)

    if print_output:
      # Windows Python converts \n to \r\n automatically whenever it
      # encounters it written to a text file (including stdout).  The only
      # way around it is to write to a binary file, which isn't feasible for
      # stdout. So we end up with \r\n here even though we explicitly write
      # \n.  (We could write \r instead, which doesn't get converted to \r\n,
      # but that's probably more troublesome for people trying to read the
      # files.)
      print(line + '\n', end=' ')

      # Python on windows writes the buffer only when it reaches 4k. This is
      # not fast enough for all purposes.
      sys.stdout.flush()
    line = read_from.readline()

  # Make sure the process terminates.
  proc.wait()

  if not verbose:
    out.close()
  return (proc.returncode, output)

def RunCommand(command, verbose=True):
  """Runs the command list, printing its output and returning its exit status.

  Prints the given command (which should be a list of one or more strings),
  then runs it and prints its stderr (and optionally stdout) to stdout,
  line-buffered, converting line endings to CRLF.  Waits for the command to
  terminate and returns its status.

  Args:
    command: the full command to run, as a list of one or more strings
    verbose: if True, combines all output (stdout and stderr) into stdout.
             Otherwise, prints only the command's stderr to stdout.

  Returns:
    The process's exit status.

  Raises:
    CommandNotFound if the command executable could not be found.
  """
  return RunCommandFull(command, verbose)[0]

def RunCommandsInParallel(commands, verbose=True, collect_output=False,
                          print_output=True):
  """Runs a list of commands in parallel, waits for all commands to terminate
  and returns their status. If specified, the ouput of commands can be
  returned and/or printed.

  Args:
    commands: the list of commands to run, each as a list of one or more
              strings.
    verbose: if True, combines stdout and stderr into stdout.
             Otherwise, prints only the command's stderr to stdout.
    collect_output: if True, collects the output of the each command as a list
                    of lines and returns it.
    print_output: if True, prints the output of each command.

  Returns:
    A list of tuples consisting of each command's exit status and output.  If
    collect_output is False, the output will be [].

  Raises:
    CommandNotFound if any of the command executables could not be found.
  """

  command_num = len(commands)
  outputs = [[] for i in xrange(command_num)]
  procs = [None for i in xrange(command_num)]
  eofs = [False for i in xrange(command_num)]

  for command in commands:
    print(
        '\n' + subprocess.list2cmdline(command).replace('\\', '/') + '\n',
        end=' ')

  if verbose:
    out = subprocess.PIPE
    err = subprocess.STDOUT
  else:
    out = file(os.devnull, 'w')
    err = subprocess.PIPE

  for i in xrange(command_num):
    try:
      command = commands[i]
      procs[i] = subprocess.Popen(command, stdout=out, stderr=err, bufsize=1)
    except OSError, e:
      if e.errno == errno.ENOENT:
        raise CommandNotFound('Unable to find "%s"' % command[0])
      raise
      # We could consider terminating the processes already started.
      # But Popen.kill() is only available in version 2.6.
      # For now the clean up is done by KillAll.

  while True:
    eof_all = True
    for i in xrange(command_num):
      if eofs[i]:
        continue
      if verbose:
        read_from = procs[i].stdout
      else:
        read_from = procs[i].stderr
      line = read_from.readline()
      if line:
        eof_all = False
        line = line.rstrip()
        outputs[i].append(line)
        if print_output:
          # Windows Python converts \n to \r\n automatically whenever it
          # encounters it written to a text file (including stdout).  The only
          # way around it is to write to a binary file, which isn't feasible
          # for stdout. So we end up with \r\n here even though we explicitly
          # write \n.  (We could write \r instead, which doesn't get converted
          # to \r\n, but that's probably more troublesome for people trying to
          # read the files.)
          print(line + '\n', end=' ')
      else:
        eofs[i] = True
    if eof_all:
      break

  # Make sure the process terminates.
  for i in xrange(command_num):
    procs[i].wait()

  if not verbose:
    out.close()

  return [(procs[i].returncode, outputs[i]) for i in xrange(command_num)]
