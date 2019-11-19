# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import re

from common import ParseFlags
from common import TestDriver


# Platform-specific decorators.
# These decorators can be used to only run a test function for certain platforms
# by annotating the function with them.

def AndroidOnly(func):
  def wrapper(*args, **kwargs):
    if ParseFlags().android:
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test runs on Android only.')
  return wrapper

def NotAndroid(func):
  def wrapper(*args, **kwargs):
    if not ParseFlags().android:
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test does not run on Android.')
  return wrapper

def WindowsOnly(func):
  def wrapper(*args, **kwargs):
    if sys.platform == 'win32':
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test runs on Windows only.')
  return wrapper

def NotWindows(func):
  def wrapper(*args, **kwargs):
    if sys.platform != 'win32':
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test does not run on Windows.')
  return wrapper

def LinuxOnly(func):
  def wrapper(*args, **kwargs):
    if sys.platform.startswith('linux'):
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test runs on Linux only.')
  return wrapper

def NotLinux(func):
  def wrapper(*args, **kwargs):
    if sys.platform.startswith('linux'):
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test does not run on Linux.')
  return wrapper

def MacOnly(func):
  def wrapper(*args, **kwargs):
    if sys.platform == 'darwin':
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test runs on Mac OS only.')
  return wrapper

def NotMac(func):
  def wrapper(*args, **kwargs):
    if sys.platform == 'darwin':
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test does not run on Mac OS.')
  return wrapper

chrome_version = None

def GetChromeVersion():
  with TestDriver() as t:
    t.LoadURL('http://check.googlezip.net/connect')
    ua = t.ExecuteJavascriptStatement('navigator.userAgent')
    match = re.search('Chrome/[0-9\.]+', ua)
    if not match:
      raise Exception('Could not find Chrome version in User-Agent: %s' % ua)
    chrome_version = ua[match.start():match.end()]
    version = chrome_version[chrome_version.find('/') + 1:]
    version_split = version.split('.')
    milestone = int(version_split[0])
    print('Running on Chrome M%d (%s)' % (milestone, version))
    return milestone

def ChromeVersionBeforeM(milestone):
  def puesdo_wrapper(func):
    def wrapper(*args, **kwargs):
      global chrome_version
      if chrome_version == None:
        chrome_version = GetChromeVersion()
      if chrome_version < milestone:
        func(*args, **kwargs)
      else:
        args[0].skipTest('This test does not run above M%d.' % milestone)
    return wrapper
  return puesdo_wrapper

def ChromeVersionEqualOrAfterM(milestone):
  def puesdo_wrapper(func):
    def wrapper(*args, **kwargs):
      global chrome_version
      if chrome_version == None:
        chrome_version = GetChromeVersion()
      if chrome_version >= milestone:
        func(*args, **kwargs)
      else:
        args[0].skipTest('This test does not run below M%d.' % milestone)
    return wrapper
  return puesdo_wrapper

def ChromeVersionBetweenInclusiveM(after, before):
  def puesdo_wrapper(func):
    def wrapper(*args, **kwargs):
      global chrome_version
      if chrome_version == None:
        chrome_version = GetChromeVersion()
      if chrome_version <= before and chrome_version >= after:
        func(*args, **kwargs)
      else:
        args[0].skipTest('This test only runs between M%d and M%d (inclusive).'
          % (after, before))
    return wrapper
  return puesdo_wrapper


def Slow(func):
  def wrapper(*args, **kwargs):
    if ParseFlags().skip_slow:
      args[0].skipTest('Skipping slow test.')
    else:
      func(*args, **kwargs)
  return wrapper

def SkipIfForcedBrowserArg(arg):
  def puesdo_wrapper(func):
    def wrapper(*args, **kwargs):
      if ParseFlags().browser_args and arg in ParseFlags().browser_args:
        args[0].skipTest(
          'Test skipped because "%s" was given on the command line' % arg)
    return wrapper
  return puesdo_wrapper
