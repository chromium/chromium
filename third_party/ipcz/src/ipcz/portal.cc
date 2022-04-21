// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/portal.h"

#include <utility>
#include <vector>

#include "ipcz/api_object.h"
#include "ipcz/local_router_link.h"
#include "ipcz/router.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/log.h"
#include "util/ref_counted.h"

namespace ipcz {

namespace {

bool ValidateAndAcquireObjectsForTransitFrom(
    Portal& sender,
    absl::Span<const IpczHandle> handles,
    std::vector<Ref<APIObject>>& objects) {
  objects.resize(handles.size());
  for (size_t i = 0; i < handles.size(); ++i) {
    auto* object = APIObject::FromHandle(handles[i]);
    if (!object || !object->CanSendFrom(sender)) {
      return false;
    }
    objects[i] = WrapRefCounted(object);
  }
  return true;
}

}  // namespace

Portal::Portal(Ref<Node> node, Ref<Router> router)
    : node_(std::move(node)), router_(std::move(router)) {}

Portal::~Portal() = default;

// static
Portal::Pair Portal::CreatePair(Ref<Node> node) {
  Router::Pair routers{MakeRefCounted<Router>(), MakeRefCounted<Router>()};
  DVLOG(5) << "Created new portal pair with routers " << routers.first.get()
           << " and " << routers.second.get();

  LocalRouterLink::ConnectRouters(LinkType::kCentral, routers);
  return {MakeRefCounted<Portal>(node, std::move(routers.first)),
          MakeRefCounted<Portal>(node, std::move(routers.second))};
}

IpczResult Portal::Close() {
  router_->CloseRoute();
  return IPCZ_RESULT_OK;
}

bool Portal::CanSendFrom(Portal& sender) {
  return &sender != this && !sender.router()->HasLocalPeer(*router_);
}

IpczResult Portal::QueryStatus(IpczPortalStatus& status) {
  router_->QueryStatus(status);
  return IPCZ_RESULT_OK;
}

IpczResult Portal::Put(absl::Span<const uint8_t> data,
                       absl::Span<const IpczHandle> handles,
                       const IpczPutLimits* limits) {
  std::vector<Ref<APIObject>> objects;
  if (!ValidateAndAcquireObjectsForTransitFrom(*this, handles, objects)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (router_->IsPeerClosed()) {
    return IPCZ_RESULT_NOT_FOUND;
  }

  Parcel parcel;
  parcel.SetInlinedData(std::vector<uint8_t>(data.begin(), data.end()));
  parcel.SetObjects(std::move(objects));
  const IpczResult result = router_->SendOutboundParcel(parcel);
  if (result == IPCZ_RESULT_OK) {
    // If the parcel was sent, the sender relinquishes handle ownership and
    // therefore implicitly releases its ref to each object.
    for (IpczHandle handle : handles) {
      std::ignore = APIObject::TakeFromHandle(handle);
    }
  }

  return result;
}

IpczResult Portal::Get(IpczGetFlags flags,
                       void* data,
                       size_t* num_data_bytes,
                       IpczHandle* handles,
                       size_t* num_handles) {
  return router_->GetNextInboundParcel(flags, data, num_data_bytes, handles,
                                       num_handles);
}

}  // namespace ipcz
