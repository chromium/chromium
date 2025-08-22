# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GDB pretty-printers for common classes in ipcz:: namespace."""

import gdb
import gdb.printing
import os

sys.path.insert(
    1, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'util'))
import reload_helper


class RefPrinter:
  """Pretty-printer for ipcz::Ref<T>."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    raw_ptr = self.val['ptr_']
    if not raw_ptr or raw_ptr == 0:
      return 'nullptr'
    try:
      pointer_type = raw_ptr.dynamic_type
      casted_ptr = raw_ptr.cast(pointer_type)
      return f'Ref to {casted_ptr.dereference()}'
    except gdb.error:
      return str(raw_ptr)


class RouteEdgePrinter:
  """Pretty-printer for ipcz::RouteEdge."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    return 'RouteEdge'

  def children(self):
    yield 'primary_link_', self.val['primary_link_']
    yield 'decaying_link_', self.val['decaying_link_']


class LocalRouterLinkPrinter:
  """Pretty-printer for ipcz::LocalRouterLink."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    return 'LocalRouterLink'

  def children(self):
    # The state_ member is a Ref<SharedState>. The members below are on the
    # SharedState object.
    try:
      state = self.val['state_']['ptr_']
      if not state or state == 0:
        yield 'state_', 'nullptr'
        return

      # Deliberately not dereferencing the Ref to routers to avoid
      # common infinite recursion.
      yield 'router_a_', state['router_a_']['ptr_']
      yield 'router_b_', state['router_b_']['ptr_']
      yield 'link_state_', state['link_state_']
    except gdb.error:
      yield 'state_', '<error reading state_>'


class RouterLinkStatePrinter:
  """Pretty-printer for ipcz::RouterLinkState."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    try:
      status = int(self.val['status'])
      side_a_parts = []
      if status & 1:
        side_a_parts.append('stable')
      else:
        side_a_parts.append('unstable')
      if status & 4:
        side_a_parts.append('waiting')

      side_b_parts = []
      if status & 2:
        side_b_parts.append('stable')
      else:
        side_b_parts.append('unstable')
      if status & 8:
        side_b_parts.append('waiting')

      lock_str = ''
      if status & 16:
        lock_str = ', locked by A'
      elif status & 32:
        lock_str = ', locked by B'

      return (f'RouterLinkState {{A: {', '.join(side_a_parts)}; '
              f'B: {', '.join(side_b_parts)}{lock_str}}}')
    except gdb.error:
      return 'RouterLinkState'

  def children(self):
    yield 'status', self.val['status']
    yield 'allowed_bypass_request_source', self.val[
        'allowed_bypass_request_source']


class RemoteRouterLinkPrinter:
  """Pretty-printer for ipcz::RemoteRouterLink."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    link_type = str(self.val['type_']['value_'])
    link_type_str = 'unknown link type'
    if link_type.endswith('kCentral'):
      link_type_str = 'central'
    elif link_type.endswith('kPeripheralInward'):
      link_type_str = 'peripheral inward'
    elif link_type.endswith('kPeripheralOutward'):
      link_type_str = 'peripheral outward'
    elif link_type.endswith('kBridge'):
      link_type_str = 'bridge'

    side_str = '.B'
    if str(self.val['side_']['value_']).endswith('kA'):
      side_str = '.A'
    # Convert to str to use the default printer because the gdb.Value for
    # std::atomic<bool> does not convert to python bool correctly.
    stable_str = str(self.val['side_is_stable_'])
    stable_msg = 'not stable'
    if str(stable_str) == 'true':
      stable_msg = 'stable side'
    sublink_value = self.val['sublink_']['value_']
    return (f'RemoteRouterLink with sublink {sublink_value}' +
            f'{side_str} ({stable_msg}, {link_type_str})')


class RouterPrinter:
  """Pretty-printer for ipcz::Router."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    return f'Router {self.val.address}'

  def children(self):
    status_str = None
    try:
      status_int = int(self.val['status_flags_'])
      statuses = []
      if (status_int & 1):
        statuses.append('closed')
      if (status_int & 2):
        statuses.append('dead')
      if not statuses:
        status_str = 'normal'
      else:
        status_str = ', '.join(statuses)
    except gdb.error:
      status_str = 'unknown'
    yield 'status_flags_', status_str
    # Reorder the remaining members according to subjective importance.
    members = [
        'outward_edge_',
        'inward_edge_',
        'is_peer_closed_',
        'is_disconnected_',
        'bridge_',
        'inbound_parcels_',
        'outbound_parcels_',
        'traps_',
        'pending_gets_',
        'pending_puts_',
        'is_pending_get_exclusive_',
    ]
    for m in members:
      yield m, self.val[m]


class ParcelQueuePrinter:
  """Pretty-printer for ipcz::ParcelQueue."""

  def __init__(self, val):
    self.val = val

  # TODO(pasko): Implement it. Add children for is_final_length_known_.
  def to_string(self):
    # Make it more compact when empty.
    try:
      entries_vec = self.val['entries_']
      begin_ptr = entries_vec['__begin_']
      end_ptr = entries_vec['__end_']
      if begin_ptr == end_ptr:
        return 'ParcelQueue is empty'
    except gdb.error:
      pass
    return str(self.val.type)


class NodeLinkPrinter:
  """Pretty-printer for ipcz::NodeLink."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    return 'NodeLink'

  def children(self):
    yield 'link_side_', str(self.val['link_side_']['value_'])
    try:
      node_ptr = self.val['node_']['ptr_']
      node_name = str(node_ptr['assigned_name_'])
      yield 'node_', f'Ref to (ipcz::Node *) {node_ptr} with {node_name}'
    except:
      yield 'node_', 'unknown'
    important_members = [
        'remote_node_type_',
        'activation_state_',
        'sublinks_',
        'partial_parcels_',
        'early_parcels_for_sublink_',
        'next_referral_id_',
        'local_node_name_',
        'remote_node_name_',
    ]
    for m in important_members:
      yield m, self.val[m]
    yield 'note', 'Hiding many members in this pretty-printer'


class NodeNamePrinter:
  """Pretty-printer for ipcz::NodeName."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    try:
      high = int(self.val['high_'])
      low = int(self.val['low_'])
      return f'NodeName {high:016x}{low:016x}'
    except gdb.error:
      return '<unreadable NodeName>'


class NodePrinter:
  """Pretty-printer for ipcz::Node."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    return 'Node'

  def children(self):
    important_members = [
        'type_',
        'assigned_name_',
        'connections_',
        'pending_introductions_',
    ]
    for m in important_members:
      yield m, self.val[m]
    yield 'note', 'Hiding many members in this pretty-printer'


class DriverTransportPrinter:
  """Pretty-printer for ipcz::DriverTransport."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    return 'DriverTransport'


class ParcelPrinter:
  """Pretty-printer for ipcz::Parcel."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    try:
      data_view = self.val['data_']['view']
      data_size = data_view['size_']
      objects_view = self.val['objects_']['view']
      num_objects = objects_view['size_']
      return f'Parcel with {data_size} bytes and {num_objects} objects'
    except gdb.error:
      return 'Parcel'

  def children(self):
    yield 'sequence_number_', self.val['sequence_number_']
    yield 'objects_', self.val['objects_']
    yield 'num_subparcels_', self.val['num_subparcels_']
    yield 'subparcel_index_', self.val['subparcel_index_']
    yield 'remote_source_', self.val['remote_source_']


class BoxPrinter:
  """Pretty-printer for ipcz::Box."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    try:
      contents = self.val['contents_']
      index = contents.index
      if index == 0:
        return 'Box (empty)'
      if index == 1:
        o = contents['driver_object']
        return f'Box (DriverObject: {o})'
      if index == 2:
        o = contents['application_object']
        return f'Box (ApplicationObject: {o})'
      if index == 3:
        o = contents['subparcel']
        return f'Box (Subparcel: {o})'
    except gdb.error:
      return 'Box'


class NodeConnectorForBrokerToNonBrokerPrinter:
  """Pretty-printer for ipcz::NodeConnectorForBrokerToNonBroker."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    return 'NodeConnectorForBrokerToNonBroker'

  def children(self):
    yield 'broker_name_', self.val['broker_name_']
    yield 'new_remote_node_name_', self.val['new_remote_node_name_']
    yield 'waiting_routers_', self.val['waiting_routers_']


def _decode_trap_condition_flags(flags_val):
  flags = int(flags_val)
  flag_names = []
  if flags & 1:
    flag_names.append('DEAD')
  if flags & 2:
    flag_names.append('NEW_PARCEL_COUNT')
  if flags & 4:
    flag_names.append('NEW_PARCEL_BYTES')
  if not flag_names:
    return 'NONE'
  return ' | '.join(flag_names)


class TrapConditionsPrinter:
  """Pretty-printer for IpczTrapConditions."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    try:
      flags = _decode_trap_condition_flags(self.val['flags'])
      threshold = self.val['threshold']
      return f'{{flags={flags}, threshold={threshold}}}'
    except gdb.error:
      return 'IpczTrapConditions'


class TrapPrinter:
  """Pretty-printer for ipcz::TrapSet::Trap."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    return 'Trap'

  def children(self):
    yield 'conditions', self.val['conditions']
    yield 'handler', self.val['handler']
    yield 'context', self.val['context']


class TrapSetPrinter:
  """Pretty-printer for ipcz::TrapSet."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    try:
      traps_vec = self.val['traps_']
      begin_ptr = traps_vec['__begin_']
      end_ptr = traps_vec['__end_']
      size = end_ptr - begin_ptr
      if size == 1:
        return 'TrapSet with 1 trap'
      return f'TrapSet with {size} traps'
    except gdb.error:
      return 'TrapSet'

  def children(self):
    yield 'traps_', self.val['traps_']


class SublinkIdPrinter:
  """Pretty-printer for ipcz::SublinkId.

       The real type is ipcz::StrongAlias<class SublinkIdTag, uint64_t>.
    """

  def __init__(self, val):
    self.val = val

  def to_string(self):
    try:
      kLinkSideBIdBit = 63
      kIdMask = (1 << kLinkSideBIdBit) - 1
      value = int(self.val['value_'])
      numeric_part = value & kIdMask
      suffix = '.B' if (value >> kLinkSideBIdBit) else '.A'
      return f'SublinkId {numeric_part}{suffix}'
    except gdb.error:
      return '<unreadable SublinkId>'


class SublinkPrinter:
  """Pretty-printer for ipcz::NodeLink::Sublink."""

  def __init__(self, val):
    self.val = val

  def to_string(self):
    return 'Sublink'

  def children(self):
    yield 'router_link', self.val['router_link']
    receiver_ptr = self.val['receiver']['ptr_']
    yield 'receiver', f'Ref to (ipcz::Router *) {receiver_ptr}'


ipcz_printer = reload_helper.find_or_create_printer('ipcz')


def _add(subprinter_name, regex, printer):
  reload_helper.remove_printer(ipcz_printer, subprinter_name)
  ipcz_printer.add_printer(subprinter_name, regex, printer)


_add('Box', '^ipcz::Box$', BoxPrinter)
_add('DriverTransport', '^ipcz::DriverTransport$', DriverTransportPrinter)
_add('IpczRef', '^ipcz::Ref<.*>$', RefPrinter)
_add('LocalRouterLink', '^ipcz::LocalRouterLink$', LocalRouterLinkPrinter)
_add('NodeConnectorForBrokerToNonBroker',
     '^ipcz::.*NodeConnectorForBrokerToNonBroker$',
     NodeConnectorForBrokerToNonBrokerPrinter)
_add('Node', '^ipcz::Node$', NodePrinter)
_add('NodeLink', '^ipcz::NodeLink$', NodeLinkPrinter)
_add('NodeName', '^ipcz::NodeName$', NodeNamePrinter)
_add('Parcel', '^ipcz::Parcel$', ParcelPrinter)
_add('ParcelQueue', '^ipcz::SequencedQueue<.*ipcz::ParcelQueueTraits>$',
     ParcelQueuePrinter)
_add('RemoteRouterLink', '^ipcz::RemoteRouterLink$', RemoteRouterLinkPrinter)
_add('RouteEdge', '^ipcz::RouteEdge$', RouteEdgePrinter)
_add('Router', '^ipcz::Router$', RouterPrinter)
_add('RouterLinkState', '^ipcz::RouterLinkState$', RouterLinkStatePrinter)
_add('SublinkId', '^ipcz::StrongAlias<ipcz::SublinkIdTag', SublinkIdPrinter)
_add('Sublink', '^ipcz::NodeLink::Sublink$', SublinkPrinter)
_add('TrapConditions', '^IpczTrapConditions$', TrapConditionsPrinter)
_add('Trap', '^ipcz::TrapSet::Trap$', TrapPrinter)
_add('TrapSet', '^ipcz::TrapSet$', TrapSetPrinter)
