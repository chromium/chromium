#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Dump Chrome's ATK accessibility tree to the command line.

Accerciser is slow and buggy. This is a quick way to check that Chrome is
exposing its interface to ATK from the command line.
"""

from __future__ import print_function

import pyatspi


# Helper function to check application name
def AppNameFinder(name):
  if (name.lower().find('chromium') != 0 and name.lower().find('chrome') != 0
      and name.lower().find('google chrome') != 0):
    return False
  return True


def Dump(obj, indent):
  if not obj:
    return
  indent_str = '  ' * indent
  role = obj.get_role_name()
  name = obj.get_name()
  bounds = obj.get_extents(pyatspi.DESKTOP_COORDS)
  bounds_str = '(%d, %d) size (%d x %d)' % (bounds.x, bounds.y, bounds.width,
                                            bounds.height)
  print('%s%s name="%s" %s' % (indent_str, role, name, bounds_str))

  # Don't recurse into applications other than Chrome
  if role == 'application':
    if (not AppNameFinder(name)):
      return

  for i in range(obj.get_child_count()):
    Dump(obj.get_child_at_index(i), indent + 1)


desktop = pyatspi.Registry.getDesktop(0)
Dump(desktop, 0)
