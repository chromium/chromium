#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Does scraping for Firefox 2.0."""

import pywintypes
import time
import types

from drivers import keyboard
from drivers import mouse
from drivers import windowing

# Default version
version = "2.0.0.6"

DEFAULT_PATH = r"c:\program files\mozilla firefox\firefox.exe"

# TODO(jhaas): the Firefox scraper is a bit rickety at the moment. Known
# issues: 1) won't work if the default profile puts toolbars in different
# locations, 2) uses sleep() statements rather than more robust checks,
# 3) fails badly if an existing Firefox window is open when the scrape
# is invoked. This needs to be fortified at some point.

def GetBrowser(path):
  """Invoke the Firefox browser and return the process and window.

  Args:
    path: full path to browser

  Returns:
    A tuple of (process handle, render pane)
  """
  if not path: path = DEFAULT_PATH

  # Invoke Firefox
  (proc, wnd) = windowing.InvokeAndWait(path)

  # Get the content pane
  render_pane = windowing.FindChildWindow(
    wnd,
    "MozillaWindowClass/MozillaWindowClass/MozillaWindowClass")

  return (proc, wnd, render_pane)


def InvokeBrowser(path):
  """Invoke the Firefox browser.

  Args:
    path: full path to browser

  Returns:
    A tuple of (main window, process handle, render pane)
  """
  # Reuse an existing instance of the browser if we can find one. This
  # may not work correctly, especially if the window is behind other windows.
  wnds = windowing.FindChildWindows(0, "MozillaUIWindowClass")
  if len(wnds):
    wnd = wnds[0]
    proc = None
  else:
    # Invoke Firefox
    (proc, wnd) = windowing.InvokeAndWait(path)

  # Get the content pane
  render_pane = windowing.FindChildWindow(
    wnd,
    "MozillaWindowClass/MozillaWindowClass/MozillaWindowClass")

  return (wnd, proc, render_pane)


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
    None if success, else an error string
  """
  if "path" in kwargs and kwargs["path"]: path = kwargs["path"]
  else: path = DEFAULT_PATH

  (wnd, proc, render_pane) = InvokeBrowser(path)

  # Resize and reposition the frame
  windowing.MoveAndSizeWindow(wnd, pos, size, render_pane)

  time.sleep(3)

  # Firefox is a bit of a pain: it doesn't use standard edit controls,
  # and it doesn't display a throbber when there's no tab. Let's make
  # sure there's at least one tab, then select the first one

  mouse.ClickInWindow(wnd)
  keyboard.TypeString("[t]", True)
  mouse.ClickInWindow(wnd, (30, 115))
  time.sleep(2)

  timedout = False

  # Visit each URL we're given
  if type(urls) in types.StringTypes: urls = [urls]

  for url in urls:

    # Use keyboard shortcuts
    keyboard.TypeString("{d}", True)
    keyboard.TypeString(url)
    keyboard.TypeString("\n")

    # Wait for the page to finish loading
    load_time = windowing.WaitForThrobber(wnd, (10, 96, 26, 112), timeout)
    timedout = load_time < 0

    if timedout:
      break

    # Scrape the page
    image = windowing.ScrapeWindow(render_pane)

    # Save to disk
    if "filename" in kwargs:
      if callable(kwargs["filename"]):
        filename = kwargs["filename"](url)
      else:
        filename = kwargs["filename"]
    else:
      filename = windowing.URLtoFilename(url, outdir, ".bmp")
    image.save(filename)

  # Close all the tabs, cheesily
  mouse.ClickInWindow(wnd)

  while len(windowing.FindChildWindows(0, "MozillaUIWindowClass")):
    keyboard.TypeString("[w]", True)
    time.sleep(1)

  if timedout:
    return "timeout"


def Time(urls, size, timeout, **kwargs):
  """Measure how long it takes to load each of a series of URLs

  Args:
    urls: list of URLs to time
    size: size of browser window to use
    timeout: amount of time to wait for page to load
    kwargs: miscellaneous keyword args

  Returns:
    A list of tuples (url, time). "time" can be "crashed" or "timeout"
  """
  if "path" in kwargs and kwargs["path"]: path = kwargs["path"]
  else: path = DEFAULT_PATH
  proc = None

  # Visit each URL we're given
  if type(urls) in types.StringTypes: urls = [urls]

  ret = []
  for url in urls:
    try:
      # Invoke the browser if necessary
      if not proc:
        (wnd, proc, render_pane) = InvokeBrowser(path)

        # Resize and reposition the frame
        windowing.MoveAndSizeWindow(wnd, (0,0), size, render_pane)

        time.sleep(3)

        # Firefox is a bit of a pain: it doesn't use standard edit controls,
        # and it doesn't display a throbber when there's no tab. Let's make
        # sure there's at least one tab, then select the first one

        mouse.ClickInWindow(wnd)
        keyboard.TypeString("[t]", True)
        mouse.ClickInWindow(wnd, (30, 115))
        time.sleep(2)

      # Use keyboard shortcuts
      keyboard.TypeString("{d}", True)
      keyboard.TypeString(url)
      keyboard.TypeString("\n")

      # Wait for the page to finish loading
      load_time = windowing.WaitForThrobber(wnd, (10, 96, 26, 112), timeout)
      timedout = load_time < 0

      if timedout:
        load_time = "timeout"

        # Try to close the browser; if this fails it's probably a crash
        mouse.ClickInWindow(wnd)

        count = 0
        while (len(windowing.FindChildWindows(0, "MozillaUIWindowClass"))
          and count < 5):
          keyboard.TypeString("[w]", True)
          time.sleep(1)
          count = count + 1

        if len(windowing.FindChildWindows(0, "MozillaUIWindowClass")):
          windowing.EndProcess(proc)
          load_time = "crashed"

        proc = None
    except pywintypes.error:
      proc = None
      load_time = "crashed"

    ret.append( (url, load_time) )

  if proc:
    count = 0
    while (len(windowing.FindChildWindows(0, "MozillaUIWindowClass"))
      and count < 5):
      keyboard.TypeString("[w]", True)
      time.sleep(1)
      count = count + 1
  return ret


def main():
  # We're being invoked rather than imported, so run some tests
  path = r"c:\sitecompare\scrapes\Firefox\2.0.0.6"
  windowing.PreparePath(path)

  # Scrape three sites and save the results
  Scrape(
    ["http://www.microsoft.com", "http://www.google.com",
     "http://www.sun.com"],
    path, (1024, 768), (0, 0))
  return 0


if __name__ == "__main__":
  sys.exit(main())
