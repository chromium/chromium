// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_LOCAL_ROUTER_LINK_H_
#define IPCZ_SRC_IPCZ_LOCAL_ROUTER_LINK_H_

#include "ipcz/link_side.h"
#include "ipcz/router.h"
#include "ipcz/router_link.h"
#include "util/ref_counted.h"

namespace ipcz {

// Local link between two Routers on the same node. This class is thread-safe.
//
// NOTE: This implementation must take caution when calling into any Router. See
// note on RouterLink's own class documentation.
class LocalRouterLink : public RouterLink {
 public:
  // Creates a new pair of LocalRouterLinks linking the given pair of Routers
  // together. The Routers must not currently have outward links. `type` must
  // be either kCentral or kBridge, as local links may never be peripheral.
  static void ConnectRouters(LinkType type, const Router::Pair& routers);

  // RouterLink:
  LinkType GetType() const override;
  bool HasLocalPeer(const Router& router) override;
  bool IsRemoteLinkTo(const NodeLink& node_link, SublinkId sublink) override;
  void AcceptParcel(Parcel& parcel) override;
  void AcceptRouteClosure(SequenceNumber sequence_length) override;
  void Deactivate() override;
  std::string Describe() const override;

 private:
  class SharedState;

  LocalRouterLink(LinkSide side, Ref<SharedState> state);
  ~LocalRouterLink() override;

  const LinkSide side_;
  const Ref<SharedState> state_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_LOCAL_ROUTER_LINK_H_
