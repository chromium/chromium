// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/local_router_link.h"

#include <sstream>
#include <utility>

#include "ipcz/link_side.h"
#include "ipcz/link_type.h"
#include "ipcz/router.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "util/ref_counted.h"

namespace ipcz {

class LocalRouterLink::SharedState : public RefCounted {
 public:
  SharedState(LinkType type, Ref<Router> router_a, Ref<Router> router_b)
      : type_(type),
        router_a_(std::move(router_a)),
        router_b_(std::move(router_b)) {}

  LinkType type() const { return type_; }

  Ref<Router> GetRouter(LinkSide side) {
    absl::MutexLock lock(&mutex_);
    switch (side.value()) {
      case LinkSide::kA:
        return router_a_;

      case LinkSide::kB:
        return router_b_;
    }
  }

  void Deactivate(LinkSide side) {
    absl::MutexLock lock(&mutex_);
    switch (side.value()) {
      case LinkSide::kA:
        router_a_.reset();
        break;

      case LinkSide::kB:
        router_b_.reset();
        break;
    }
  }

 private:
  ~SharedState() override = default;

  const LinkType type_;

  absl::Mutex mutex_;
  Ref<Router> router_a_ ABSL_GUARDED_BY(mutex_);
  Ref<Router> router_b_ ABSL_GUARDED_BY(mutex_);
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
  return state_->GetRouter(side_.opposite()).get() == &router;
}

void LocalRouterLink::AcceptParcel(Parcel& parcel) {
  if (Ref<Router> receiver = state_->GetRouter(side_.opposite())) {
    receiver->AcceptInboundParcel(parcel);
  }
}

void LocalRouterLink::AcceptRouteClosure(SequenceNumber sequence_length) {
  if (Ref<Router> receiver = state_->GetRouter(side_.opposite())) {
    receiver->AcceptRouteClosureFrom(state_->type(), sequence_length);
  }
}

void LocalRouterLink::Deactivate() {
  state_->Deactivate(side_);
}

std::string LocalRouterLink::Describe() const {
  std::stringstream ss;
  ss << side_.ToString() << "-side link to local peer "
     << state_->GetRouter(side_.opposite()).get() << " on "
     << side_.opposite().ToString() << " side";
  return ss.str();
}

}  // namespace ipcz
