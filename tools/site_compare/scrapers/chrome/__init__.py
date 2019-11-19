#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Selects the appropriate scraper for Chrome."""

from __future__ import print_function


def GetScraper(version):
  """Returns the scraper module for the given version.

  Args:
    version: version string of Chrome, or None for most recent

  Returns:
    scrape module for given version
  """
  if version is None:
    version = "0.1.101.0"

  parsed_version = [int(x) for x in version.split(".")]

  if (parsed_version[0] > 0 or
      parsed_version[1] > 1 or
      parsed_version[2] > 97 or
      parsed_version[3] > 0):
    scraper_version = "chrome011010"
  else:
    scraper_version = "chrome01970"

  return __import__(scraper_version, globals(), locals(), [''])


# if invoked rather than imported, test
if __name__ == "__main__":
  print(GetScraper("0.1.101.0").version)
