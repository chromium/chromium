// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_REMOTE_ROUTER_LINK_H_
#define IPCZ_SRC_IPCZ_REMOTE_ROUTER_LINK_H_

#include <atomic>

#include "ipcz/link_side.h"
#include "ipcz/link_type.h"
#include "ipcz/router_link.h"
#include "ipcz/sublink_id.h"
#include "util/ref_counted.h"

namespace ipcz {

class NodeLink;

// One side of a link between two Routers living on different nodes. A
// RemoteRouterLink uses a NodeLink plus a SublinkId as its transport between
// the routers. On the other end (on another node) is another RemoteRouterLink
// using a NodeLink back to this node, with the same SublinkId.
//
// As with other RouterLink instances, each RemoteRouterLink is assigned a
// LinkSide at construction time. This assignment is arbitrary but will always
// be the opposite of the LinkSide assigned to the RemoteRouteLink on the other
// end.
//
// NOTE: This implementation must take caution when calling into any Router. See
// note on RouterLink's own class documentation.
class RemoteRouterLink : public RouterLink {
 public:
  // Constructs a new RemoteRouterLink which sends messages over `node_link`
  // using `sublink` specifically. `side` is the side of this link on which
  // this RemoteRouterLink falls (side A or B), and `type` indicates what type
  // of link it is -- which for remote links must be either kCentral,
  // kPeripheralInward, or kPeripheralOutward.
  static Ref<RemoteRouterLink> Create(Ref<NodeLink> node_link,
                                      SublinkId sublink,
                                      LinkType type,
                                      LinkSide side);

  const Ref<NodeLink>& node_link() const { return node_link_; }
  SublinkId sublink() const { return sublink_; }

  // RouterLink:
  LinkType GetType() const override;
  bool HasLocalPeer(const Router& router) override;
  bool IsRemoteLinkTo(const NodeLink& node_link, SublinkId sublink) override;
  void AcceptParcel(Parcel& parcel) override;
  void AcceptRouteClosure(SequenceNumber sequence_length) override;
  void Deactivate() override;
  std::string Describe() const override;

 private:
  RemoteRouterLink(Ref<NodeLink> node_link,
                   SublinkId sublink,
                   LinkType type,
                   LinkSide side);

  ~RemoteRouterLink() override;

  const Ref<NodeLink> node_link_;
  const SublinkId sublink_;
  const LinkType type_;
  const LinkSide side_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_REMOTE_ROUTER_LINK_H_
