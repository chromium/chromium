// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

// This file defines the internal messages which can be sent on a NodeLink
// between two ipcz nodes.

// Notifies a node that the route has been closed on one side. This message
// always pertains to the side of the route opposite of the router receiving it,
// guaranteed by the fact that the closed side of the route only transmits this
// message outward once its terminal router is adjacent to the central link.
IPCZ_MSG_BEGIN(RouteClosed, IPCZ_MSG_ID(22), IPCZ_MSG_VERSION(0))
  // In the context of the receiving NodeLink, this identifies the specific
  // Router to receive this message.
  IPCZ_MSG_PARAM(SublinkId, sublink)

  // The total number of parcels sent from the side of the route which closed,
  // before closing. Because parcels may arrive out-of-order from each other
  // and from messages like this one under various conditions (broker relays,
  // different transport mechanisms, etc.), parcels are tagged with strictly
  // increasing SequenceNumbers by the sender. This field informs the recipient
  // that the closed endpoint has transmitted exactly `sequence_length` parcels,
  // from SequenceNumber 0 to `sequence_length-1`. The recipient can use this
  // to know, for example, that it must still expect some additional parcels to
  // arrive before completely forgetting about the route's link(s).
  IPCZ_MSG_PARAM(SequenceNumber, sequence_length)
IPCZ_MSG_END()
