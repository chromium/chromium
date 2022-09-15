#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SiteCompare module for invoking, locating, and manipulating windows.

This module is a catch-all wrapper for operating system UI functionality
that doesn't belong in other modules. It contains functions for finding
particular windows, scraping their contents, and invoking processes to
create them.
"""

import os
import string
import time

import PIL.ImageGrab
import pywintypes
import win32event
import win32gui
import win32process


def FindChildWindows(hwnd, path):
  """Find a set of windows through a path specification.

  Args:
    hwnd: Handle of the parent window
    path: Path to the window to find. Has the following form:
      "foo/bar/baz|foobar/|foobarbaz"
      The slashes specify the "path" to the child window.
      The text is the window class, a pipe (if present) is a title.
      * is a wildcard and will find all child windows at that level

  Returns:
    A list of the windows that were found
  """
  windows_to_check = [hwnd]

  # The strategy will be to take windows_to_check and use it
  # to find a list of windows that match the next specification
  # in the path, then repeat with the list of found windows as the
  # new list of windows to check
  for segment in path.split("/"):
    windows_found = []
    check_values = segment.split("|")

    # check_values is now a list with the first element being
    # the window class, the second being the window caption.
    # If the class is absent (or wildcarded) set it to None
    if check_values[0] == "*" or not check_values[0]: check_values[0] = None

    # If the window caption is also absent, force it to None as well
    if len(check_values) == 1: check_values.append(None)

    # Loop through the list of windows to check
    for window_check in windows_to_check:
      window_found = None
      while window_found != 0:  # lint complains, but 0 != None
        if window_found is None: window_found = 0
        try:
          # Look for the next sibling (or first sibling if window_found is 0)
          # of window_check with the specified caption and/or class
          window_found = win32gui.FindWindowEx(
            window_check, window_found, check_values[0], check_values[1])
        except pywintypes.error, e:
          # FindWindowEx() raises error 2 if not found
          if e[0] == 2:
            window_found = 0
          else:
            raise e

        # If FindWindowEx struck gold, add to our list of windows found
        if window_found: windows_found.append(window_found)

    # The windows we found become the windows to check for the next segment
    windows_to_check = windows_found

  return windows_found


def FindChildWindow(hwnd, path):
  """Find a window through a path specification.

  This method is a simple wrapper for FindChildWindows() for the
  case (the majority case) where you expect to find a single window

  Args:
    hwnd: Handle of the parent window
    path: Path to the window to find. See FindChildWindows()

  Returns:
    The window that was found
  """
  return FindChildWindows(hwnd, path)[0]


def ScrapeWindow(hwnd, rect=None):
  """Scrape a visible window and return its contents as a bitmap.

  Args:
    hwnd: handle of the window to scrape
    rect: rectangle to scrape in client coords, defaults to the whole thing
          If specified, it's a 4-tuple of (left, top, right, bottom)

  Returns:
    An Image containing the scraped data
  """
  # Activate the window
  SetForegroundWindow(hwnd)

  # If no rectangle was specified, use the fill client rectangle
  if not rect: rect = win32gui.GetClientRect(hwnd)

  upper_left  = win32gui.ClientToScreen(hwnd, (rect[0], rect[1]))
  lower_right = win32gui.ClientToScreen(hwnd, (rect[2], rect[3]))
  rect = upper_left+lower_right

  return PIL.ImageGrab.grab(rect)


def SetForegroundWindow(hwnd):
  """Bring a window to the foreground."""
  win32gui.SetForegroundWindow(hwnd)


def InvokeAndWait(path, cmdline="", timeout=10, tick=1.):
  """Invoke an application and wait for it to bring up a window.

  Args:
    path: full path to the executable to invoke
    cmdline: command line to pass to executable
    timeout: how long (in seconds) to wait before giving up
    tick: length of time to wait between checks

  Returns:
    A tuple of handles to the process and the application's window,
    or (None, None) if it timed out waiting for the process
  """

  def EnumWindowProc(hwnd, ret):
    """Internal enumeration func, checks for visibility and proper PID."""
    if win32gui.IsWindowVisible(hwnd):  # don't bother even checking hidden wnds
      pid = win32process.GetWindowThreadProcessId(hwnd)[1]
      if pid == ret[0]:
        ret[1] = hwnd
        return 0    # 0 means stop enumeration
    return 1        # 1 means continue enumeration

  # We don't need to change anything about the startupinfo structure
  # (the default is quite sufficient) but we need to create it just the
  # same.
  sinfo = win32process.STARTUPINFO()

  proc = win32process.CreateProcess(
    path,                # path to new process's executable
    cmdline,             # application's command line
    None,                # process security attributes (default)
    None,                # thread security attributes (default)
    False,               # inherit parent's handles
    0,                   # creation flags
    None,                # environment variables
    None,                # directory
    sinfo)               # default startup info

  # Create process returns (prochandle, pid, threadhandle, tid). At
  # some point we may care about the other members, but for now, all
  # we're after is the pid
  pid = proc[2]

  # Enumeration APIs can take an arbitrary integer, usually a pointer,
  # to be passed to the enumeration function. We'll pass a pointer to
  # a structure containing the PID we're looking for, and an empty out
  # parameter to hold the found window ID
  ret = [pid, None]

  tries_until_timeout = timeout/tick
  num_tries = 0

  # Enumerate top-level windows, look for one with our PID
  while num_tries < tries_until_timeout and ret[1] is None:
    try:
      win32gui.EnumWindows(EnumWindowProc, ret)
    except pywintypes.error, e:
      # error 0 isn't an error, it just meant the enumeration was
      # terminated early
      if e[0]: raise e

    time.sleep(tick)
    num_tries += 1

  # TODO(jhaas): Should we throw an exception if we timeout? Or is returning
  # a window ID of None sufficient?
  return (proc[0], ret[1])


def WaitForProcessExit(proc, timeout=None):
  """Waits for a given process to terminate.

  Args:
    proc: handle to process
    timeout: timeout (in seconds). None = wait indefinitely

  Returns:
    True if process ended, False if timed out
  """
  if timeout is None:
    timeout = win32event.INFINITE
  else:
    # convert sec to msec
    timeout *= 1000

  return (win32event.WaitForSingleObject(proc, timeout) ==
          win32event.WAIT_OBJECT_0)


def WaitForThrobber(hwnd, rect=None, timeout=20, tick=0.1, done=10):
  """Wait for a browser's "throbber" (loading animation) to complete.

  Args:
    hwnd: window containing the throbber
    rect: rectangle of the throbber, in client coords. If None, whole window
    timeout: if the throbber is still throbbing after this long, give up
    tick: how often to check the throbber
    done: how long the throbber must be unmoving to be considered done

  Returns:
    Number of seconds waited, -1 if timed out
  """
  if not rect: rect = win32gui.GetClientRect(hwnd)

  # last_throbber will hold the results of the preceding scrape;
  # we'll compare it against the current scrape to see if we're throbbing
  last_throbber = ScrapeWindow(hwnd, rect)
  start_clock = time.clock()
  timeout_clock = start_clock + timeout
  last_changed_clock = start_clock;

  while time.clock() < timeout_clock:
    time.sleep(tick)

    current_throbber = ScrapeWindow(hwnd, rect)
    if current_throbber.tostring() != last_throbber.tostring():
      last_throbber = current_throbber
      last_changed_clock = time.clock()
    else:
      if time.clock() - last_changed_clock > done:
        return last_changed_clock - start_clock

  return -1


def MoveAndSizeWindow(wnd, position=None, size=None, child=None):
  """Moves and/or resizes a window.

  Repositions and resizes a window. If a child window is provided,
  the parent window is resized so the child window has the given size

  Args:
    wnd: handle of the frame window
    position: new location for the frame window
    size: new size for the frame window (or the child window)
    child: handle of the child window

  Returns:
    None
  """
  rect = win32gui.GetWindowRect(wnd)

  if position is None: position = (rect[0], rect[1])
  if size is None:
    size = (rect[2]-rect[0], rect[3]-rect[1])
  elif child is not None:
    child_rect = win32gui.GetWindowRect(child)
    slop = (rect[2]-rect[0]-child_rect[2]+child_rect[0],
            rect[3]-rect[1]-child_rect[3]+child_rect[1])
    size = (size[0]+slop[0], size[1]+slop[1])

  win32gui.MoveWindow(wnd,          # window to move
                      position[0],  # new x coord
                      position[1],  # new y coord
                      size[0],      # new width
                      size[1],      # new height
                      True)         # repaint?


def EndProcess(proc, code=0):
  """Ends a process.

  Wraps the OS TerminateProcess call for platform-independence

  Args:
    proc: process ID
    code: process exit code

  Returns:
    None
  """
  win32process.TerminateProcess(proc, code)


def URLtoFilename(url, path=None, extension=None):
  """Converts a URL to a filename, given a path.

  This in theory could cause collisions if two URLs differ only
  in unprintable characters (eg. http://www.foo.com/?bar and
  http://www.foo.com/:bar. In practice this shouldn't be a problem.

  Args:
    url: The URL to convert
    path: path to the directory to store the file
    extension: string to append to filename

  Returns:
    filename
  """
  trans = string.maketrans(r'\/:*?"<>|', '_________')

  if path is None: path = ""
  if extension is None: extension = ""
  if len(path) > 0 and path[-1] != '\\': path += '\\'
  url = url.translate(trans)
  return "%s%s%s" % (path, url, extension)


def PreparePath(path):
  """Ensures that a given path exists, making subdirectories if necessary.

  Args:
    path: fully-qualified path of directory to ensure exists

  Returns:
    None
  """
  try:
    os.makedirs(path)
  except OSError, e:
    if e[0] != 17: raise e   # error 17: path already exists


def main():
  PreparePath(r"c:\sitecompare\scrapes\ie7")
  # We're being invoked rather than imported. Let's do some tests

  # Hardcode IE's location for the purpose of this test
  (proc, wnd) = InvokeAndWait(
    r"c:\program files\internet explorer\iexplore.exe")

  # Find the browser pane in the IE window
  browser = FindChildWindow(
    wnd, "TabWindowClass/Shell DocObject View/Internet Explorer_Server")

  # Move and size the window
  MoveAndSizeWindow(wnd, (0, 0), (1024, 768), browser)

  # Take a screenshot
  i = ScrapeWindow(browser)

  i.show()

  EndProcess(proc, 0)


if __name__ == "__main__":
  sys.exit(main())
