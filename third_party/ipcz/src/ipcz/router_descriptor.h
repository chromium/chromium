// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_ROUTER_DESCRIPTOR_H_
#define IPCZ_SRC_IPCZ_ROUTER_DESCRIPTOR_H_

#include "ipcz/fragment_descriptor.h"
#include "ipcz/ipcz.h"
#include "ipcz/node_name.h"
#include "ipcz/sequence_number.h"
#include "ipcz/sublink_id.h"

namespace ipcz {

// Serialized representation of a Router sent in a parcel. When a portal is
// transferred to a new node, we use this structure to serialize a description
// of the new Router that will back the portal at its new location. This new
// router is an inward peer of the portal's previous router on the sending node.
//
// NOTE: This is a wire structure and must remain backwards-compatible across
// changes.
struct IPCZ_ALIGN(8) RouterDescriptor {
  RouterDescriptor();
  RouterDescriptor(const RouterDescriptor&);
  RouterDescriptor& operator=(const RouterDescriptor&);
  ~RouterDescriptor();

  // If the other end of the route is already known to be closed when this
  // router is serialized, this is the total number of parcels sent from that
  // end.
  SequenceNumber closed_peer_sequence_length;

  // A new sublink and RouterLinkState fragment allocated by the sender on the
  // NodeLink which sends this descriptor. The sublink is used as a peripheral
  // link, inward to (and outward from) the new router.
  SublinkId new_sublink;

  // The SequenceNumber of the next outbound parcel which can be produced by
  // this router.
  SequenceNumber next_outgoing_sequence_number;

  // The total number of outgoing bytes produced by the router's portal so far.
  uint64_t num_bytes_produced;

  // The SequenceNumber of the next inbound parcel expected by this router.
  SequenceNumber next_incoming_sequence_number;

  // The total number of incoming bytes consumed from router's portal so far.
  uint64_t num_bytes_consumed;

  // Indicates that the other end of the route is already known to be closed.
  // In this case sending any new outbound parcels from this router would be
  // pointless, but there may still be in-flight parcels to receive from the
  // other end. `closed_peer_sequence_length` will indicate the total number of
  // parcels sent from that end, and `next_incoming_sequence_number` can be used
  // to determine whether there are any parcels left to receive.
  bool peer_closed : 1;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_ROUTER_DESCRIPTOR_H_
