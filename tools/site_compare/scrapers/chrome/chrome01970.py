# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Does scraping for versions of Chrome up to 0.1.97.0."""

from drivers import windowing

import chromebase

# Default version
version = "0.1.97.0"


def GetChromeRenderPane(wnd):
  return windowing.FindChildWindow(wnd, "Chrome_BrowserWindow")


def Scrape(urls, outdir, size, pos, timeout=20, **kwargs):
  """Invoke a browser, send it to a series of URLs, and save its output.

  Args:
    urls: list of URLs to scrape
    outdir: directory to place output
    size: size of browser window to use
    pos: position of browser window
    timeout: amount of time to wait for page to load
    kwargs: miscellaneous keyword args

  Returns:
    None if succeeded, else an error code
  """
  chromebase.GetChromeRenderPane = GetChromeRenderPane

  return chromebase.Scrape(urls, outdir, size, pos, timeout, kwargs)


def Time(urls, size, timeout, **kwargs):
  """Forwards the Time command to chromebase."""
  chromebase.GetChromeRenderPane = GetChromeRenderPane

  return chromebase.Time(urls, size, timeout, kwargs)
