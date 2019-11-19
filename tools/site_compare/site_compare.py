#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SiteCompare component to handle bulk scrapes.

Invokes a list of browsers and sends them to a list of URLs,
saving the rendered results to a specified directory, then
performs comparison operations on the resulting bitmaps and
saves the results
"""

from __future__ import print_function

# This line is necessary to work around a QEMU bug
import _imaging

import os            # Functions for walking the directory tree
import types         # Runtime type-checking

import command_line  # command-line parsing
import drivers       # Functions for driving keyboard/mouse/windows, OS-specific
import operators     # Functions that, given two bitmaps as input, produce
                     # output depending on the performance of an operation
import scrapers      # Functions that know how to capture a render from
                     # particular browsers

import commands.compare2  # compare one page in two versions of same browser
import commands.maskmaker # generate a mask based on repeated scrapes
import commands.measure   # measure length of time a page takes to load
import commands.scrape    # scrape a URL or series of URLs to a bitmap

# The timeload command is obsolete (too flaky); it may be reinstated
# later but for now it's been superceded by "measure"
# import commands.timeload  # measure length of time a page takes to load

def Scrape(browsers, urls, window_size=(1024, 768),
           window_pos=(0, 0), timeout=20, save_path=None, **kwargs):
  """Invoke one or more browsers over one or more URLs, scraping renders.

  Args:
    browsers: browsers to invoke with optional version strings
    urls: URLs to visit
    window_size: size of the browser window to display
    window_pos: location of browser window
    timeout: time (in seconds) to wait for page to load
    save_path: root of save path, automatically appended with browser and
      version
    kwargs: miscellaneous keyword args, passed to scraper
  Returns:
    None

  @TODO(jhaas): more parameters, or perhaps an indefinite dictionary
  parameter, for things like length of time to wait for timeout, speed
  of mouse clicks, etc. Possibly on a per-browser, per-URL, or
  per-browser-per-URL basis
  """

  if type(browsers) in types.StringTypes: browsers = [browsers]

  if save_path is None:
    # default save path is "scrapes" off the current root
    save_path = os.path.join(os.path.split(__file__)[0], "Scrapes")

  for browser in browsers:
    # Browsers should be tuples of (browser, version)
    if type(browser) in types.StringTypes: browser = (browser, None)
    scraper = scrapers.GetScraper(browser)

    full_path = os.path.join(save_path, browser[0], scraper.version)
    drivers.windowing.PreparePath(full_path)

    scraper.Scrape(urls, full_path, window_size, window_pos, timeout, kwargs)


def Compare(base, compare, ops, root_path=None, out_path=None):
  """Compares a series of scrapes using a series of operators.

  Args:
    base: (browser, version) tuple of version to consider the baseline
    compare: (browser, version) tuple of version to compare to
    ops: list of operators plus operator arguments
    root_path: root of the scrapes
    out_path: place to put any output from the operators

  Returns:
    None

  @TODO(jhaas): this method will likely change, to provide a robust and
  well-defined way of chaining operators, applying operators conditionally,
  and full-featured scripting of the operator chain. There also needs
  to be better definition of the output; right now it's to stdout and
  a log.txt file, with operator-dependent images saved for error output
  """
  if root_path is None:
    # default save path is "scrapes" off the current root
    root_path = os.path.join(os.path.split(__file__)[0], "Scrapes")

  if out_path is None:
    out_path = os.path.join(os.path.split(__file__)[0], "Compares")

  if type(base) in types.StringTypes: base = (base, None)
  if type(compare) in types.StringTypes: compare = (compare, None)
  if type(ops) in types.StringTypes: ops = [ops]

  base_dir = os.path.join(root_path, base[0])
  compare_dir = os.path.join(root_path, compare[0])

  if base[1] is None:
    # base defaults to earliest capture
    base = (base[0], max(os.listdir(base_dir)))

  if compare[1] is None:
    # compare defaults to latest capture
    compare = (compare[0], min(os.listdir(compare_dir)))

  out_path = os.path.join(out_path, base[0], base[1], compare[0], compare[1])
  drivers.windowing.PreparePath(out_path)

  # TODO(jhaas): right now we're just dumping output to a log file
  # (and the console), which works as far as it goes but isn't nearly
  # robust enough. Change this after deciding exactly what we want to
  # change it to.
  out_file = open(os.path.join(out_path, "log.txt"), "w")
  description_string = ("Comparing %s %s to %s %s" %
                        (base[0], base[1], compare[0], compare[1]))
  out_file.write(description_string)
  print(description_string)

  base_dir = os.path.join(base_dir, base[1])
  compare_dir = os.path.join(compare_dir, compare[1])

  for filename in os.listdir(base_dir):
    out_file.write("%s: " % filename)

    if not os.path.isfile(os.path.join(compare_dir, filename)):
      out_file.write("Does not exist in target directory\n")
      print("File %s does not exist in target directory" % filename)
      continue

    base_filename = os.path.join(base_dir, filename)
    compare_filename = os.path.join(compare_dir, filename)

    for op in ops:
      if type(op) in types.StringTypes: op = (op, None)

      module = operators.GetOperator(op[0])

      ret = module.Compare(base_filename, compare_filename)
      if ret is None:
        print("%s: OK" % (filename,))
        out_file.write("OK\n")
      else:
        print("%s: %s" % (filename, ret[0]))
        out_file.write("%s\n" % (ret[0]))
        ret[1].save(os.path.join(out_path, filename))

  out_file.close()


def main():
  """Main executable. Parse the command line and invoke the command."""
  cmdline = command_line.CommandLine()

  # The below two commands are currently unstable so have been disabled
  # commands.compare2.CreateCommand(cmdline)
  # commands.maskmaker.CreateCommand(cmdline)
  commands.measure.CreateCommand(cmdline)
  commands.scrape.CreateCommand(cmdline)

  cmdline.ParseCommandLine()
  return 0


if __name__ == "__main__":
  sys.exit(main())
