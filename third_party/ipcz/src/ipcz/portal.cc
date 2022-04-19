// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/portal.h"

#include <utility>

#include "ipcz/api_object.h"
#include "ipcz/local_router_link.h"
#include "ipcz/router.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/log.h"

namespace ipcz {

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

IpczResult Portal::QueryStatus(IpczPortalStatus& status) {
  router_->QueryStatus(status);
  return IPCZ_RESULT_OK;
}

}  // namespace ipcz
