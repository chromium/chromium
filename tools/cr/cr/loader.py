# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module scan and load system.

The main interface to this module is the Scan function, which triggers a
recursive scan of all packages and modules below cr, with modules being
imported as they are found.
This allows all the plugins in the system to self register.
The aim is to make writing plugins as simple as possible, minimizing the
boilerplate so the actual functionality is clearer.
"""

from __future__ import print_function

from importlib import import_module
import os
import sys

import cr

# This is the name of the variable inserted into modules to track which
# scanners have been applied.
_MODULE_SCANNED_TAG = '_CR_MODULE_SCANNED'


class AutoExport(object):
  """A marker for classes that should be promoted up into the cr namespace."""


def _AutoExportScanner(module):
  """Scan the modules for things that need wiring up automatically."""
  for name, value in module.__dict__.items():
    if isinstance(value, type) and issubclass(value, AutoExport):
      # Add this straight to the cr module.
      if not hasattr(cr, name):
        setattr(cr, name, value)


scan_hooks = [_AutoExportScanner]


def _Import(name):
  """Import a module or package if it is not already imported."""
  module = sys.modules.get(name, None)
  if module is not None:
    return module
  return import_module(name, None)


def _TryImport(name):
  """Try to import a module or package if it is not already imported."""
  try:
    return _Import(name)
  except ImportError:
    if cr.context.verbose:
      print('Warning: Failed to load module', name)
    return None


def _ScanModule(module):
  """Runs all the scan_hooks for a module."""
  scanner_tags = getattr(module, _MODULE_SCANNED_TAG, None)
  if scanner_tags is None:
    # First scan, add the scanned marker set.
    scanner_tags = set()
    setattr(module, _MODULE_SCANNED_TAG, scanner_tags)
  for scan in scan_hooks:
    if scan not in scanner_tags:
      scanner_tags.add(scan)
      scan(module)


def _ScanPackage(package):
  """Scan a package for child packages and modules."""
  modules = []
  # Recurse sub folders.
  for path in package.__path__:
    try:
      basenames = sorted(os.listdir(path))
    except OSError:
      basenames = []
    packages = []
    for basename in basenames:
      fullpath = os.path.join(path, basename)
      if os.path.isdir(fullpath):
        name = '.'.join([package.__name__, basename])
        packages.append(name)
      elif basename.endswith('.py') and not basename.startswith('_'):
        name = '.'.join([package.__name__, basename[:-3]])
        module = _TryImport(name)
        if module:
          _ScanModule(module)
          modules.append(module)
    for name in packages:
      child = _TryImport(name)
      if child:
        modules.extend(_ScanPackage(child))
  return modules


def Import(package, name):
  module = _Import(package + '.' + name)
  path = getattr(module, '__path__', None)
  if path:
    _ScanPackage(module)
  else:
    _ScanModule(module)
  return module


def Scan():
  """Scans from the cr package down, loading modules as needed.

  This finds all packages and modules below the cr package, by scanning the
  file system. It imports all the packages, and then runs post import hooks on
  each module to do any automated work. One example of this is the hook that
  finds all classes that extend AutoExport and copies them up into the cr
  namespace directly.

  Modules are allowed to refer to each other, their import will be retried
  until it succeeds or no progress can be made on any module.
  """
  modules = _ScanPackage(cr)
  # Now scan all the found modules one more time.
  # This happens after all imports, in case any imports register scan hooks.
  for module in modules:
    _ScanModule(module)
