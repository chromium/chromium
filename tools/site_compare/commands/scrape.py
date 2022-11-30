# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command for scraping images from a URL or list of URLs.

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

from drivers import windowing
from utils import browser_iterate

def CreateCommand(cmdline):
  """Inserts the command and arguments into a command line for parsing."""
  cmd = cmdline.AddCommand(
    ["scrape"],
    "Scrapes an image from a URL or series of URLs.",
    None,
    ExecuteScrape)

  browser_iterate.SetupIterationCommandLine(cmd)
  cmd.AddArgument(
    ["-log", "--logfile"], "File to write text output", type="string")
  cmd.AddArgument(
    ["-out", "--outdir"], "Directory to store scrapes", type="string", required=True)


def ExecuteScrape(command):
  """Executes the Scrape command."""

  def ScrapeResult(url, proc, wnd, result):
    """Capture and save the scrape."""
    if log_file: log_file.write(result)

    # Scrape the page
    image = windowing.ScrapeWindow(wnd)
    filename = windowing.URLtoFilename(url, command["--outdir"], ".bmp")
    image.save(filename)

  if command["--logfile"]: log_file = open(command["--logfile"], "w")
  else: log_file = None

  browser_iterate.Iterate(command, ScrapeResult)

  # Close the log file and return. We're done.
  if log_file: log_file.close()
