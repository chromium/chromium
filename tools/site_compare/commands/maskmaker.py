# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Component for automatically creating masks of changing areas of a website.

Works by repeated invokation of a browser and scraping of the resulting page.
Areas that differ will be added to the auto-generated mask. The mask generator
considers the mask complete when further scrapes fail to produce any differences
in the mask.
"""

from __future__ import print_function

import os            # Functions for walking the directory tree
import tempfile      # Get a temporary directory to hold intermediates
import time          # Used for sleep() and naming masks by time

import command_line
import drivers
from PIL import Image
from PIL import ImageChops
import scrapers


def CreateCommand(cmdline):
  """Inserts the command and arguments into a command line for parsing."""
  cmd = cmdline.AddCommand(
    ["maskmaker"],
    "Automatically generates a mask from a list of URLs",
    ValidateMaskmaker,
    ExecuteMaskmaker)

  cmd.AddArgument(
    ["-bp", "--browserpath"], "Full path to browser's executable",
    type="readfile", metaname="PATH")
  cmd.AddArgument(
    ["-b", "--browser"], "Which browser to use", type="string",
    default="chrome")
  cmd.AddArgument(
    ["-bv", "--browserver"], "Version of the browser", metaname="VERSION")
  cmd.AddArgument(
    ["-o", "--outdir"], "Directory to store generated masks", metaname="DIR",
    required=True)
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
    ["-w", "--wait"],
    "Amount of time (in seconds) to wait between successive scrapes",
    type="int", default=60)
  cmd.AddArgument(
    ["-sc", "--scrapes"],
    "Number of successive scrapes which must result in no change to a mask "
    "before mask creation is considered complete", type="int", default=10)
  cmd.AddArgument(
    ["-sz", "--size"], "Browser window size", default=(800, 600), type="coords")
  cmd.AddArgument(["-sd", "--scrapedir"], "Directory to store scrapes")
  cmd.AddArgument(
    ["-gu", "--giveup"],
    "Number of times to scrape before giving up", type="int", default=50)
  cmd.AddArgument(
    ["-th", "--threshhold"],
    "Percentage of different pixels (0-100) above which the scrape will be"
    "discarded and the mask not updated.", type="int", default=100)
  cmd.AddArgument(
    ["--er", "--errors"],
    "Number of times a scrape can fail before giving up on the URL.",
    type="int", default=1)


def ValidateMaskmaker(command):
  """Validate the arguments to maskmaker. Raises ParseError if failed."""
  executables = [".exe", ".com", ".bat"]
  if command["--browserpath"]:
    if os.path.splitext(command["--browserpath"])[1].lower() not in executables:
      raise command_line.ParseError("Browser filename must be an executable")


def ExecuteMaskmaker(command):
  """Performs automatic mask generation."""

  # Get the list of URLs to generate masks for
  class MaskmakerURL(object):
    """Helper class for holding information about a URL passed to maskmaker."""
    __slots__ = ['url', 'consecutive_successes', 'errors']
    def __init__(self, url):
      self.url = url
      self.consecutive_successes = 0
      self.errors = 0

  if command["--url"]:
    url_list = [MaskmakerURL(command["--url"])]
  else:
    startline = command["--startline"]
    if command["--count"]:
      endline = startline+command["--count"]
    else:
      endline = command["--endline"]
    url_list = [MaskmakerURL(url.strip()) for url in
                open(command["--list"], "r").readlines()[startline:endline]]

  complete_list = []
  error_list = []

  outdir = command["--outdir"]
  scrapes = command["--scrapes"]
  errors = command["--errors"]
  size = command["--size"]
  scrape_pass = 0

  scrapedir = command["--scrapedir"]
  if not scrapedir: scrapedir = tempfile.gettempdir()

  # Get the scraper
  scraper = scrapers.GetScraper((command["--browser"], command["--browserver"]))

  # Repeatedly iterate through the list of URLs until either every URL has
  # a successful mask or too many errors, or we've exceeded the giveup limit
  while url_list and scrape_pass < command["--giveup"]:
    # Scrape each URL
    for url in url_list:
      print("Processing %r..." % url.url)
      mask_filename = drivers.windowing.URLtoFilename(url.url, outdir, ".bmp")

      # Load the existing mask. This is in a loop so we can try to recover
      # from error conditions
      while True:
        try:
          mask = Image.open(mask_filename)
          if mask.size != size:
            print("  %r already exists and is the wrong size! (%r vs %r)" %
                  (mask_filename, mask.size, size))
            mask_filename = "%s_%r%s" % (
              mask_filename[:-4], size, mask_filename[-4:])
            print("  Trying again as %r..." % mask_filename)
            continue
          break
        except IOError:
          print("  %r does not exist, creating" % mask_filename)
          mask = Image.new("1", size, 1)
          mask.save(mask_filename)

      # Find the stored scrape path
      mask_scrape_dir = os.path.join(
        scrapedir, os.path.splitext(os.path.basename(mask_filename))[0])
      drivers.windowing.PreparePath(mask_scrape_dir)

      # Find the baseline image
      mask_scrapes = os.listdir(mask_scrape_dir)
      mask_scrapes.sort()

      if not mask_scrapes:
        print("  No baseline image found, mask will not be updated")
        baseline = None
      else:
        baseline = Image.open(os.path.join(mask_scrape_dir, mask_scrapes[0]))

      mask_scrape_filename = os.path.join(mask_scrape_dir,
                                          time.strftime("%y%m%d-%H%M%S.bmp"))

      # Do the scrape
      result = scraper.Scrape(
        [url.url], mask_scrape_dir, size, (0, 0),
        command["--timeout"], path=command["--browserpath"],
        filename=mask_scrape_filename)

      if result:
        # Return value other than None means an error
        print("  Scrape failed with error '%r'" % result)
        url.errors += 1
        if url.errors >= errors:
          print("  ** Exceeded maximum error count for this URL, giving up")
        continue

      # Load the new scrape
      scrape = Image.open(mask_scrape_filename)

      # Calculate the difference between the new scrape and the baseline,
      # subject to the current mask
      if baseline:
        diff = ImageChops.multiply(ImageChops.difference(scrape, baseline),
                                   mask.convert(scrape.mode))

        # If the difference is none, there's nothing to update
        if max(diff.getextrema()) == (0, 0):
          print("  Scrape identical to baseline, no change in mask")
          url.consecutive_successes += 1
          if url.consecutive_successes >= scrapes:
            print("  ** No change for %r scrapes, done!" % scrapes)
        else:
          # convert the difference to black and white, then change all
          # black pixels (where the scrape and the baseline were identical)
          # to white, all others (where the scrape and the baseline differed)
          # to black.
          #
          # Since the below command is a little unclear, here's how it works.
          #    1. convert("L") converts the RGB image to grayscale
          #    2. point() maps grayscale values (or the individual channels)
          #       of an RGB image) to different ones. Because it operates on
          #       individual channels, the grayscale conversion from step 1
          #       is necessary.
          #    3. The "1" second parameter to point() outputs the result as
          #       a monochrome bitmap. If the original RGB image were converted
          #       directly to monochrome, PIL would dither it.
          diff = diff.convert("L").point([255]+[0]*255, "1")

          # count the number of different pixels
          diff_pixels = diff.getcolors()[0][0]

          # is this too much?
          diff_pixel_percent = diff_pixels * 100.0 / (mask.size[0]*mask.size[1])
          if diff_pixel_percent > command["--threshhold"]:
            print("  Scrape differed from baseline by %.2f percent, ignoring" %
                  diff_pixel_percent)
          else:
            print("  Scrape differed in %d pixels, updating mask" % diff_pixels)
            mask = ImageChops.multiply(mask, diff)
            mask.save(mask_filename)

            # reset the number of consecutive "good" scrapes
            url.consecutive_successes = 0

    # Remove URLs whose mask is deemed done
    complete_list.extend(
      [url for url in url_list if url.consecutive_successes >= scrapes])
    error_list.extend(
      [url for url in url_list if url.errors >= errors])
    url_list = [
      url for url in url_list if
      url.consecutive_successes < scrapes and
      url.errors < errors]

    scrape_pass += 1
    print("**Done with scrape pass %d\n" % scrape_pass)

    if scrape_pass >= command["--giveup"]:
      print("**Exceeded giveup threshhold. Giving up.")
    else:
      print("Waiting %d seconds..." % command["--wait"])
      time.sleep(command["--wait"])

  print()
  print("*** MASKMAKER COMPLETE ***")
  print("Summary report:")
  print("  %d masks successfully generated" % len(complete_list))
  for url in complete_list:
    print("    ", url.url)
  print("  %d masks failed with too many errors" % len(error_list))
  for url in error_list:
    print("    ", url.url)
  if scrape_pass >= command["--giveup"]:
    print("  %d masks were not completed before "
          "reaching the giveup threshhold" % len(url_list))
    for url in url_list:
      print("    ", url.url)
