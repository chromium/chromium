#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Does scraping for all currently-known versions of Chrome"""

import pywintypes
import types

from drivers import keyboard
from drivers import mouse
from drivers import windowing


# TODO: this has moved, use some logic to find it. For now,
# expects a subst k:.
DEFAULT_PATH = r"k:\chrome.exe"


def InvokeBrowser(path):
  """Invoke the Chrome browser.

  Args:
    path: full path to browser

  Returns:
    A tuple of (main window, process handle, address bar, render pane)
  """

  # Reuse an existing instance of the browser if we can find one. This
  # may not work correctly, especially if the window is behind other windows.

  # TODO(jhaas): make this work with Vista
  wnds = windowing.FindChildWindows(0, "Chrome_XPFrame")
  if len(wnds):
    wnd = wnds[0]
    proc = None
  else:
    # Invoke Chrome
    (proc, wnd) = windowing.InvokeAndWait(path)

  # Get windows we'll need
  address_bar = windowing.FindChildWindow(wnd, "Chrome_AutocompleteEdit")
  render_pane = GetChromeRenderPane(wnd)

  return (wnd, proc, address_bar, render_pane)


def Scrape(urls, outdir, size, pos, timeout, kwargs):
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

  (wnd, proc, address_bar, render_pane) = InvokeBrowser(path)

  # Resize and reposition the frame
  windowing.MoveAndSizeWindow(wnd, pos, size, render_pane)

  # Visit each URL we're given
  if type(urls) in types.StringTypes: urls = [urls]

  timedout = False

  for url in urls:
    # Double-click in the address bar, type the name, and press Enter
    mouse.ClickInWindow(address_bar)
    keyboard.TypeString(url, 0.1)
    keyboard.TypeString("\n")

    # Wait for the page to finish loading
    load_time = windowing.WaitForThrobber(wnd, (20, 16, 36, 32), timeout)
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

  if proc:
    windowing.SetForegroundWindow(wnd)

    # Send Alt-F4, then wait for process to end
    keyboard.TypeString(r"{\4}", use_modifiers=True)
    if not windowing.WaitForProcessExit(proc, timeout):
      windowing.EndProcess(proc)
      return "crashed"

  if timedout:
    return "timeout"

  return None


def Time(urls, size, timeout, kwargs):
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
        (wnd, proc, address_bar, render_pane) = InvokeBrowser(path)

        # Resize and reposition the frame
        windowing.MoveAndSizeWindow(wnd, (0,0), size, render_pane)

      # Double-click in the address bar, type the name, and press Enter
      mouse.ClickInWindow(address_bar)
      keyboard.TypeString(url, 0.1)
      keyboard.TypeString("\n")

      # Wait for the page to finish loading
      load_time = windowing.WaitForThrobber(wnd, (20, 16, 36, 32), timeout)

      timedout = load_time < 0

      if timedout:
        load_time = "timeout"

        # Send an alt-F4 to make the browser close; if this times out,
        # we've probably got a crash
        windowing.SetForegroundWindow(wnd)

        keyboard.TypeString(r"{\4}", use_modifiers=True)
        if not windowing.WaitForProcessExit(proc, timeout):
          windowing.EndProcess(proc)
          load_time = "crashed"
        proc = None
    except pywintypes.error:
      proc = None
      load_time = "crashed"

    ret.append( (url, load_time) )

  if proc:
    windowing.SetForegroundWindow(wnd)
    keyboard.TypeString(r"{\4}", use_modifiers=True)
    if not windowing.WaitForProcessExit(proc, timeout):
      windowing.EndProcess(proc)

  return ret


def main():
  # We're being invoked rather than imported, so run some tests
  path = r"c:\sitecompare\scrapes\chrome\0.1.97.0"
  windowing.PreparePath(path)

  # Scrape three sites and save the results
  Scrape([
    "http://www.microsoft.com",
    "http://www.google.com",
    "http://www.sun.com"],
         path, (1024, 768), (0, 0))
  return 0


if __name__ == "__main__":
  sys.exit(main())
