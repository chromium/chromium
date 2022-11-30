# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command for measuring how long pages take to load in a browser.

Prerequisites:
  1. The command_line package from tools/site_compare
  2. Either the IE BHO or Firefox extension (or both)

Installation:
  1. Build the IE BHO, or call regsvr32 on a prebuilt binary
  2. Add a file called "measurepageloadtimeextension@google.com" to
     the default Firefox profile directory under extensions, containing
     the path to the Firefox extension root

Invoke with the command line arguments as documented within
the command line.
"""

import command_line
import win32process

from drivers import windowing
from utils import browser_iterate

def CreateCommand(cmdline):
  """Inserts the command and arguments into a command line for parsing."""
  cmd = cmdline.AddCommand(
    ["measure"],
    "Measures how long a series of URLs takes to load in one or more browsers.",
    None,
    ExecuteMeasure)

  browser_iterate.SetupIterationCommandLine(cmd)
  cmd.AddArgument(
    ["-log", "--logfile"], "File to write output", type="string", required=True)


def ExecuteMeasure(command):
  """Executes the Measure command."""

  def LogResult(url, proc, wnd, result):
    """Write the result of the browse to the log file."""
    log_file.write(result)

  log_file = open(command["--logfile"], "w")

  browser_iterate.Iterate(command, LogResult)

  # Close the log file and return. We're done.
  log_file.close()
