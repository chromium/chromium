// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/remote_router_link.h"

#include <utility>

#include "ipcz/node_link.h"
#include "ipcz/node_messages.h"
#include "ipcz/router.h"

namespace ipcz {

RemoteRouterLink::RemoteRouterLink(Ref<NodeLink> node_link,
                                   SublinkId sublink,
                                   LinkType type,
                                   LinkSide side)
    : node_link_(std::move(node_link)),
      sublink_(sublink),
      type_(type),
      side_(side) {}

RemoteRouterLink::~RemoteRouterLink() = default;

// static
Ref<RemoteRouterLink> RemoteRouterLink::Create(Ref<NodeLink> node_link,
                                               SublinkId sublink,
                                               LinkType type,
                                               LinkSide side) {
  return AdoptRef(
      new RemoteRouterLink(std::move(node_link), sublink, type, side));
}

LinkType RemoteRouterLink::GetType() const {
  return type_;
}

bool RemoteRouterLink::HasLocalPeer(const Router& router) {
  return false;
}

void RemoteRouterLink::AcceptParcel(Parcel& parcel) {
  // Not yet implemented.
  ABSL_ASSERT(false);
}

void RemoteRouterLink::AcceptRouteClosure(SequenceNumber sequence_length) {
  msg::RouteClosed route_closed;
  route_closed.params().sublink = sublink_;
  route_closed.params().sequence_length = sequence_length;
  node_link()->Transmit(route_closed);
}

void RemoteRouterLink::Deactivate() {
  node_link()->RemoveRemoteRouterLink(sublink_);
}

}  // namespace ipcz
