# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GDB pretty-printers for absl::{flat,node}_hash_{map,set}."""

import gdb
import gdb.printing
import os

sys.path.insert(
    1, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'util'))
import reload_helper


# Returns the type of each base class field of type t.
def _base_classes(t):
  return [field.type for field in t.fields() if field.is_base_class]


class SwissTablePrinter(object):
  short_type_name = None
  is_map = False

  def __init__(self, val):
    self.val = val

    # The behavior of raw_hash_set is controlled by a Policy template argument.
    # There is not enough detail in debug info to call Policy::element directly,
    # so we deduce which one it is and hard-code the right behavior.

    # Distinguish between the flat and node/string containers.  All of them
    # derive from a "raw" type in absl::container_internal that has a policy
    # template argument we can use to tell the different kinds apart.
    actual_type = self.val.type
    if actual_type.code == gdb.TYPE_CODE_REF:
      actual_type = actual_type.target()

    # Get the slot_type. If base is raw_hash_map, we need to go to its
    # parent-class (raw_hash_set).
    base = actual_type
    while not base.name.startswith('absl::container_internal::raw_hash_set'):
      base = _base_classes(base)[0]
    policy_type_name = base.template_argument(0).name
    self._is_flat = policy_type_name.startswith(
        'absl::container_internal::FlatHash')
    slot_type_name = f'{base.name}::slot_type'
    self._slot_type_ptr = gdb.lookup_type(slot_type_name).pointer()

  def _common(self):
    return self.val['settings_']['value']

  def _heap(self):
    return self._common()['heap_or_soo_']['heap']

  def _size(self):
    size = self._common()['size_']
    data = size['data_']
    shift = size['kSizeShift']
    return int(data) >> shift

  def _yield_slot(self, slot, i):
    """Yields the value_type at index i."""
    if not self._is_flat:
      slot = slot.dereference()

    if self.is_map:
      if self._is_flat:
        key, val = slot['value']['first'], slot['value']['second']
      else:
        key, val = slot['first'], slot['second']
      yield f'key[{i}]', key
      yield f'val[{i}]', val
    else:
      yield f'[{i}]', slot.cast(gdb.types.get_basic_type(slot.type))

  def to_string(self):
    return f'{self.short_type_name} of length {self._size()}'

  def display_hint(self):
    return 'map' if self.is_map else None

  def children(self):
    for i in range(self._common()['capacity_']):
      ctrl = int(self._heap()['control']['p'][i])
      if ctrl & 0x80:
        # Empty or deleted.
        continue
      slot = self._heap()['slot_array']['p'].cast(self._slot_type_ptr)[i]
      for y in self._yield_slot(slot, i):
        yield y

  def num_children(self, max_count):
    del max_count
    if (self.is_map):
      return int(self._size()) * 2
    return int(self._size())


class FlatHashSetPrinter(SwissTablePrinter):
  """Pretty-printer for flat_hash_set."""
  short_type_name = 'flat_hash_set'


class FlatHashMapPrinter(SwissTablePrinter):
  """Pretty-printer for flat_hash_map."""
  short_type_name = 'flat_hash_map'
  is_map = True


class NodeHashSetPrinter(SwissTablePrinter):
  """Pretty-printer for node_hash_set."""
  short_type_name = 'node_hash_set'


class NodeHashMapPrinter(SwissTablePrinter):
  """Pretty-printer for node_hash_map."""
  short_type_name = 'node_hash_map'
  is_map = True


absl_printer = reload_helper.find_or_create_printer('absl')


def _add(subprinter_name, regex, printer):
  reload_helper.remove_printer(absl_printer, subprinter_name)
  absl_printer.add_printer(subprinter_name, regex, printer)


_add('FlatHashSet', 'absl::flat_hash_set<.*>', FlatHashSetPrinter)
_add('FlatHashMap', 'absl::flat_hash_map<.*>', FlatHashMapPrinter)
_add('NodeHashSet', 'absl::node_hash_set<.*>', NodeHashSetPrinter)
_add('NodeHashMap', 'absl::node_hash_map<.*>', NodeHashMapPrinter)
