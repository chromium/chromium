# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility to use a browser to visit multiple URLs.

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
import scrapers
import socket
import time

from drivers import windowing

# Constants
MAX_URL = 1024
PORT = 42492

def SetupIterationCommandLine(cmd):
  """Adds the necessary flags for iteration to a command.

  Args:
    cmd: an object created by cmdline.AddCommand
  """
  cmd.AddArgument(
    ["-b", "--browser"], "Browser to use (ie, firefox, chrome)",
    type="string", required=True)
  cmd.AddArgument(
    ["-b1v", "--browserver"], "Version of browser", metaname="VERSION")
  cmd.AddArgument(
    ["-p", "--browserpath"], "Path to browser.",
    type="string", required=False)
  cmd.AddArgument(
    ["-u", "--url"], "URL to visit")
  cmd.AddArgument(
    ["-l", "--list"], "File containing list of URLs to visit", type="readfile")
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
    type="int", default=300)
  cmd.AddArgument(
    ["-sz", "--size"], "Browser window size", default=(800, 600), type="coords")


def Iterate(command, iteration_func):
  """Iterates over a list of URLs, calling a function on each.

  Args:
    command: the command line containing the iteration flags
    iteration_func: called for each URL with (proc, wnd, url, result)
  """

  # Retrieve the browser scraper to use to invoke the browser
  scraper = scrapers.GetScraper((command["--browser"], command["--browserver"]))

  def AttachToBrowser(path, timeout):
    """Invoke the browser process and connect to the socket."""
    (proc, frame, wnd) = scraper.GetBrowser(path)

    if not wnd: raise ValueError("Could not invoke browser.")

    # Try to connect the socket. If it fails, wait and try
    # again. Do this for ten seconds
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP)

    for attempt in xrange(10):
      try:
        s.connect(("localhost", PORT))
      except socket.error:
        time.sleep(1)
        continue
      break

    try:
      s.getpeername()
    except socket.error:
      raise ValueError("Could not connect to browser")

    if command["--size"]:
      # Resize and reposition the frame
      windowing.MoveAndSizeWindow(frame, (0, 0), command["--size"], wnd)

    s.settimeout(timeout)

    Iterate.proc = proc
    Iterate.wnd = wnd
    Iterate.s = s

  def DetachFromBrowser():
    """Close the socket and kill the process if necessary."""
    if Iterate.s:
      Iterate.s.close()
      Iterate.s = None

    if Iterate.proc:
      if not windowing.WaitForProcessExit(Iterate.proc, 0):
        try:
          windowing.EndProcess(Iterate.proc)
          windowing.WaitForProcessExit(Iterate.proc, 0)
        except pywintypes.error:
          # Exception here most likely means the process died on its own
          pass
      Iterate.proc = None

  if command["--browserpath"]:
    browser = command["--browserpath"]
  else:
    browser = None

  # Read the URLs from the file
  if command["--url"]:
    url_list = [command["--url"]]
  else:
    startline = command["--startline"]
    if command["--count"]:
      endline = startline+command["--count"]
    else:
      endline = command["--endline"]

    url_list = []
    file = open(command["--list"], "r")

    for line in xrange(startline-1):
      file.readline()

    for line in xrange(endline-startline):
      url_list.append(file.readline().strip())

  timeout = command["--timeout"]

  # Loop through the URLs and send them through the socket
  Iterate.s    = None
  Iterate.proc = None
  Iterate.wnd  = None

  for url in url_list:
    # Invoke the browser if necessary
    if not Iterate.proc:
      AttachToBrowser(browser, timeout)
    # Send the URL and wait for a response
    Iterate.s.send(url + "\n")

    response = ""

    while (response.find("\n") < 0):

      try:
        recv = Iterate.s.recv(MAX_URL)
        response = response + recv

        # Workaround for an oddity: when Firefox closes
        # gracefully, somehow Python doesn't detect it.
        # (Telnet does)
        if not recv:
          raise socket.error

      except socket.timeout:
        response = url + ",hang\n"
        DetachFromBrowser()
      except socket.error:
        # If there was a socket error, it's probably a crash
        response = url + ",crash\n"
        DetachFromBrowser()

      # If we received a timeout response, restart the browser
      if response[-9:] == ",timeout\n":
        DetachFromBrowser()

      # Invoke the iteration function
      iteration_func(url, Iterate.proc, Iterate.wnd, response)

  # We're done
  DetachFromBrowser()
