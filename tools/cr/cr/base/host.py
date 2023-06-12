# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for build host support."""

from __future__ import print_function

import os
import shlex
import signal
import subprocess

import cr

# Controls what verbosity level turns on command trail logging
_TRAIL_VERBOSITY = 2

def PrintTrail(trail):
  print('Command expanded the following variables:')
  for key, value in trail:
    if value == None:
      value = ''
    print('   ', key, '=', value)


class Host(cr.Plugin, cr.Plugin.Type):
  """Base class for implementing cr hosts.

  The host is the main access point to services provided by the machine cr
  is running on. It exposes information about the machine, and runs external
  commands on behalf of the actions.
  """

  def __init__(self):
    super(Host, self).__init__()

  def Matches(self):
    """Detects whether this is the correct host implementation.

    This method is overridden by the concrete implementations.
    Returns:
      true if the plugin matches the machine it is running on.
    """
    return False

  @classmethod
  def Select(cls):
    for host in cls.Plugins():
      if host.Matches():
        return host

  def _Execute(self, command,
               shell=False, capture=False, silent=False,
               ignore_dry_run=False, return_status=False,
               ignore_interrupt_signal=False):
    """This is the only method that launches external programs.

    It is a thin wrapper around subprocess.Popen that handles cr specific
    issues. The command is expanded in the active context so that variables
    are substituted.
    Args:
      command: the command to run.
      shell: whether to run the command using the shell.
      capture: controls wether the output of the command is captured.
      ignore_dry_run: Normally, if the context is in dry run mode the command is
        printed but not executed. This flag overrides that behaviour, causing
        the command to be run anyway.
      return_status: switches the function to returning the status code rather
        the output.
      ignore_interrupt_signal: Ignore the interrupt signal (i.e., Ctrl-C) while
        the command is running. Useful for letting interactive programs manage
        Ctrl-C by themselves.
    Returns:
      the status if return_status is true, or the output if capture is true,
      otherwise nothing.
    """
    with cr.context.Trace():
      command = [cr.context.Substitute(arg) for arg in command if arg]
      command = filter(bool, command)
    trail = cr.context.trail
    if not command:
      print('Empty command passed to execute')
      exit(1)
    if cr.context.verbose:
      print(' '.join(command))
      if cr.context.verbose >= _TRAIL_VERBOSITY:
        PrintTrail(trail)
    if ignore_dry_run or not cr.context.dry_run:
      out = None
      if capture:
        out = subprocess.PIPE
      elif silent:
        out = open(os.devnull, "w")
      try:
        p = subprocess.Popen(
            command, shell=shell,
            env={k: str(v) for k, v in cr.context.exported.items()},
            stdout=out)
      except OSError:
        print('Failed to exec', command)
        # Don't log the trail if we already have
        if cr.context.verbose < _TRAIL_VERBOSITY:
          PrintTrail(trail)
        exit(1)
      try:
        if ignore_interrupt_signal:
          signal.signal(signal.SIGINT, signal.SIG_IGN)
        output, _ = p.communicate()
      finally:
        if ignore_interrupt_signal:
          signal.signal(signal.SIGINT, signal.SIG_DFL)
        if silent:
          out.close()
      if return_status:
        return p.returncode
      if p.returncode != 0:
        print('Error {0} executing command {1}'.format(p.returncode, command))
        exit(p.returncode)
      if output is not None:
        output = output.decode('utf-8')
      return output or ''
    return ''

  @cr.Plugin.activemethod
  def Shell(self, *command):
    command = ' '.join([shlex.quote(arg) for arg in command])
    return self._Execute([command], shell=True, ignore_interrupt_signal=True)

  @cr.Plugin.activemethod
  def Execute(self, *command):
    return self._Execute(command, shell=False)

  @cr.Plugin.activemethod
  def ExecuteSilently(self, *command):
    return self._Execute(command, shell=False, silent=True)

  @cr.Plugin.activemethod
  def CaptureShell(self, *command):
    return self._Execute(command,
                         shell=True, capture=True, ignore_dry_run=True)

  @cr.Plugin.activemethod
  def Capture(self, *command):
    return self._Execute(command, capture=True, ignore_dry_run=True)

  @cr.Plugin.activemethod
  def ExecuteStatus(self, *command):
    return self._Execute(command,
                         ignore_dry_run=True, return_status=True)

  @cr.Plugin.activemethod
  def YesNo(self, question, default=True):
    """Ask the user a yes no question

    This blocks until the user responds.
    Args:
      question: The question string to show the user
      default: True if the default response is Yes
    Returns:
      True if the response was yes.
    """
    options = 'Y/n' if default else 'y/N'
    result = raw_input(question + ' [' + options + '] ').lower()
    if result == '':
      return default
    return result in ['y', 'yes']

  @classmethod
  def SearchPath(cls, name, paths=[]):
    """Searches the PATH for an executable.

    Args:
      name: the name of the binary to search for.
    Returns:
      the set of executables found, or an empty list if none.
    """
    result = []
    extensions = ['']
    extensions.extend(os.environ.get('PATHEXT', '').split(os.pathsep))
    paths = [cr.context.Substitute(path) for path in paths if path]
    paths = paths + os.environ.get('PATH', '').split(os.pathsep)
    for path in paths:
      partial = os.path.join(path, name)
      for extension in extensions:
        filename = partial + extension
        if os.path.exists(filename) and filename not in result:
          result.append(filename)
    return result
