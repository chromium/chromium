# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SiteCompare command to invoke the same page in two versions of a browser.

Does the easiest compatibility test: equality comparison between two different
versions of the same browser. Invoked with a series of command line options
that specify which URLs to check, which browser to use, where to store results,
etc.
"""

import os            # Functions for walking the directory tree
import tempfile      # Get a temporary directory to hold intermediates

import command_line
import drivers       # Functions for driving keyboard/mouse/windows, OS-specific
import operators     # Functions that, given two bitmaps as input, produce
                     # output depending on the performance of an operation
import scrapers      # Functions that know how to capture a render from
                     # particular browsers


def CreateCommand(cmdline):
  """Inserts the command and arguments into a command line for parsing."""
  cmd = cmdline.AddCommand(
    ["compare2"],
    "Compares the output of two browsers on the same URL or list of URLs",
    ValidateCompare2,
    ExecuteCompare2)

  cmd.AddArgument(
    ["-b1", "--browser1"], "Full path to first browser's executable",
    type="readfile", metaname="PATH", required=True)
  cmd.AddArgument(
    ["-b2", "--browser2"], "Full path to second browser's executable",
    type="readfile", metaname="PATH", required=True)
  cmd.AddArgument(
    ["-b", "--browser"], "Which browser to use", type="string",
    default="chrome")
  cmd.AddArgument(
    ["-b1v", "--browser1ver"], "Version of first browser", metaname="VERSION")
  cmd.AddArgument(
    ["-b2v", "--browser2ver"], "Version of second browser", metaname="VERSION")
  cmd.AddArgument(
    ["-b1n", "--browser1name"], "Optional name for first browser (used in "
    "directory to hold intermediate files)", metaname="NAME")
  cmd.AddArgument(
    ["-b2n", "--browser2name"], "Optional name for second browser (used in "
    "directory to hold intermediate files)", metaname="NAME")
  cmd.AddArgument(
    ["-o", "--outdir"], "Directory to store scrape files", metaname="DIR")
  cmd.AddArgument(
    ["-u", "--url"], "URL to compare")
  cmd.AddArgument(
    ["-l", "--list"], "List of URLs to compare", type="readfile")
  cmd.AddMutualExclusion(["--url", "--list"])
  cmd.AddArgument(
    ["-s", "--startline"], "First line of URL list", type="int")
  cmd.AddArgument(
    ["-e", "--endline"], "Last line of URL list (exclusive)", type="int")
  cmd.AddArgument(
    ["-c", "--count"], "Number of lines of URL file to use", type="int")
  cmd.AddDependency("--startline", "--list")
  cmd.AddRequiredGroup(["--url", "--list"])
  cmd.AddDependency("--endline", "--list")
  cmd.AddDependency("--count", "--list")
  cmd.AddMutualExclusion(["--count", "--endline"])
  cmd.AddDependency("--count", "--startline")
  cmd.AddArgument(
    ["-t", "--timeout"], "Amount of time (seconds) to wait for browser to "
    "finish loading",
    type="int", default=60)
  cmd.AddArgument(
    ["-log", "--logfile"], "File to write output", type="string", required=True)
  cmd.AddArgument(
    ["-sz", "--size"], "Browser window size", default=(800, 600), type="coords")
  cmd.AddArgument(
    ["-m", "--maskdir"], "Path that holds masks to use for comparison")
  cmd.AddArgument(
    ["-d", "--diffdir"], "Path to hold the difference of comparisons that fail")


def ValidateCompare2(command):
  """Validate the arguments to compare2. Raises ParseError if failed."""
  executables = [".exe", ".com", ".bat"]
  if (os.path.splitext(command["--browser1"])[1].lower() not in executables or
      os.path.splitext(command["--browser2"])[1].lower() not in executables):
    raise command_line.ParseError("Browser filename must be an executable")


def ExecuteCompare2(command):
  """Executes the Compare2 command."""
  if command["--url"]:
    url_list = [command["--url"]]
  else:
    startline = command["--startline"]
    if command["--count"]:
      endline = startline+command["--count"]
    else:
      endline = command["--endline"]
    url_list = [url.strip() for url in
                open(command["--list"], "r").readlines()[startline:endline]]

  log_file = open(command["--logfile"], "w")

  outdir = command["--outdir"]
  if not outdir: outdir = tempfile.gettempdir()

  scrape_info_list = []

  class ScrapeInfo(object):
    """Helper class to hold information about a scrape."""
    __slots__ = ["browser_path", "scraper", "outdir", "result"]

  for index in xrange(1, 3):
    scrape_info = ScrapeInfo()
    scrape_info.browser_path = command["--browser%d" % index]
    scrape_info.scraper = scrapers.GetScraper(
      (command["--browser"], command["--browser%dver" % index]))

    if command["--browser%dname" % index]:
      scrape_info.outdir = os.path.join(outdir,
                                        command["--browser%dname" % index])
    else:
      scrape_info.outdir = os.path.join(outdir, str(index))

    drivers.windowing.PreparePath(scrape_info.outdir)
    scrape_info_list.append(scrape_info)

  compare = operators.GetOperator("equals_with_mask")

  for url in url_list:
    success = True

    for scrape_info in scrape_info_list:
      scrape_info.result = scrape_info.scraper.Scrape(
        [url], scrape_info.outdir, command["--size"], (0, 0),
        command["--timeout"], path=scrape_info.browser_path)

      if not scrape_info.result:
        scrape_info.result = "success"
      else:
        success = False

    result = "unknown"

    if success:
      result = "equal"

      file1 = drivers.windowing.URLtoFilename(
        url, scrape_info_list[0].outdir, ".bmp")
      file2 = drivers.windowing.URLtoFilename(
        url, scrape_info_list[1].outdir, ".bmp")

      comparison_result = compare.Compare(file1, file2,
                                          maskdir=command["--maskdir"])

      if comparison_result is not None:
        result = "not-equal"

        if command["--diffdir"]:
          comparison_result[1].save(
            drivers.windowing.URLtoFilename(url, command["--diffdir"], ".bmp"))

    # TODO(jhaas): maybe use the logging module rather than raw file writes
    log_file.write("%s %s %s %s\n" % (url,
                                      scrape_info_list[0].result,
                                      scrape_info_list[1].result,
                                      result))
