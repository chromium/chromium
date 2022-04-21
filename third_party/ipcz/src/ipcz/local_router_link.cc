// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/local_router_link.h"

#include <utility>

#include "ipcz/link_side.h"
#include "ipcz/link_type.h"
#include "ipcz/router.h"
#include "util/ref_counted.h"

namespace ipcz {

class LocalRouterLink::SharedState : public RefCounted {
 public:
  SharedState(LinkType type, Ref<Router> router_a, Ref<Router> router_b)
      : type_(type),
        router_a_(std::move(router_a)),
        router_b_(std::move(router_b)) {}

  LinkType type() const { return type_; }

  const Ref<Router>& side(LinkSide side) const {
    if (side == LinkSide::kA) {
      return router_a_;
    }
    return router_b_;
  }

 private:
  ~SharedState() override = default;

  const LinkType type_;
  const Ref<Router> router_a_;
  const Ref<Router> router_b_;
};

// static
void LocalRouterLink::ConnectRouters(LinkType type,
                                     const Router::Pair& routers) {
  ABSL_ASSERT(type == LinkType::kCentral || type == LinkType::kBridge);
  auto state = MakeRefCounted<SharedState>(type, routers.first, routers.second);
  routers.first->SetOutwardLink(
      AdoptRef(new LocalRouterLink(LinkSide::kA, state)));
  routers.second->SetOutwardLink(
      AdoptRef(new LocalRouterLink(LinkSide::kB, state)));
}

LocalRouterLink::LocalRouterLink(LinkSide side, Ref<SharedState> state)
    : side_(side), state_(std::move(state)) {}

LocalRouterLink::~LocalRouterLink() = default;

LinkType LocalRouterLink::GetType() const {
  return state_->type();
}

bool LocalRouterLink::HasLocalPeer(const Router& router) {
  return state_->side(side_.opposite()).get() == &router;
}

void LocalRouterLink::AcceptParcel(Parcel& parcel) {
  Router& receiver = *state_->side(side_.opposite());
  receiver.AcceptInboundParcel(parcel);
}

void LocalRouterLink::AcceptRouteClosure(SequenceNumber sequence_length) {
  Router& receiver = *state_->side(side_.opposite());
  receiver.AcceptRouteClosureFrom(state_->type(), sequence_length);
}

}  // namespace ipcz
