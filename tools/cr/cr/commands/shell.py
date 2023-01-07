# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the shell command."""

from __future__ import print_function

import os
import tempfile

import cr


class ShellCommand(cr.Command):
  """The implementation of the shell command.

  The shell command is the escape hatch that lets user run any program in the
  same environment that cr would use if it were running it.
  """

  def __init__(self):
    super(ShellCommand, self).__init__()
    self.help = 'Launch a shell'
    self.description = ("""
        If no arguments are present, this launches an interactive system
        shell (ie bash) with the environment modified to that used for the
        build systems.
        If any arguments are present, they are used as a command line to run
        in that shell.
        This allows you to run commands that are not yet available natively
        in cr.
        """)

  def AddArguments(self, subparsers):
    parser = super(ShellCommand, self).AddArguments(subparsers)
    self.ConsumeArgs(parser, 'the shell')
    return parser

  def Run(self):
    if cr.context.remains:
      cr.Host.Shell(*cr.context.remains)
      return
    # If we get here, we are trying to launch an interactive shell
    shell = os.environ.get('SHELL', None)
    if shell is None:
      print('Don\'t know how to run a shell on this system')
    elif shell.endswith('bash'):
      ps1 = '[CR] ' + os.environ.get('PS1', '')
      with tempfile.NamedTemporaryFile() as rcfile:
        rcfile.write('source ~/.bashrc\nPS1="'+ps1+'"')
        rcfile.flush()
        cr.Host.Execute(shell, '--rcfile', rcfile.name)
    else:
      cr.Host.Execute(shell)
