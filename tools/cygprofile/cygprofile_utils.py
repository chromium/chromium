# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilites used by cygprofile scripts.
"""

import logging


class WarningCollector:
  """Collects warnings, but limits the number printed to a set value."""
  def __init__(self, max_warnings, level=logging.WARNING):
    self._warnings = 0
    self._max_warnings = max_warnings
    self._level = level

  def Write(self, message):
    """Prints a warning if fewer than max_warnings have already been printed."""
    if self._warnings < self._max_warnings:
      logging.log(self._level, message)
    self._warnings += 1

  def WriteEnd(self, message):
    """Once all warnings have been printed, use this to print the number of
    elided warnings."""
    if self._warnings > self._max_warnings:
      logging.log(self._level, '%d more warnings for: %s' % (
          self._warnings - self._max_warnings, message))
