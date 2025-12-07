# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper library to allow redefining pretty-printers multiple times.

Allows to source the same file with pretty-printers in a single GDB session.
This is most useful during pretty-printer development.
"""

import gdb
import gdb.printing


def find_or_create_printer(printer_name):
  """Finds the registered printer by name or creates if not present."""
  printer = None
  # This inspection of existing printers is not supported by GDB Python API.
  # May break in the future.
  for p in gdb.pretty_printers:
    if getattr(p, 'name', None) == printer_name:
      printer = p
      break
  else:
    printer = gdb.printing.RegexpCollectionPrettyPrinter(printer_name)
    gdb.printing.register_pretty_printer(gdb.current_objfile(), printer)
  return printer


def remove_printer(collection, subprinter_name):
  """
  Finds and removes a subprinter from a RegexpCollectionPrettyPrinter.

  Args:
      collection: The RegexpCollectionPrettyPrinter object.
      subprinter_name: The string name of the subprinter to remove.

  Returns:
      True if the subprinter was found and removed, False otherwise.
  """
  if not hasattr(collection, 'subprinters'):
    print('Error: no attribute "subprinters" - cannot remove printer')
    return False

  # This iteration over subprinters and fetching their 'name' is not supported
  # by GDB Python API. May break in the future.
  initial_len = len(collection.subprinters)
  collection.subprinters = [
      p for p in collection.subprinters if p.name != subprinter_name
  ]
  return len(collection.subprinters) < initial_len
