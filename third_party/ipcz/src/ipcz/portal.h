// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_PORTAL_H_
#define IPCZ_SRC_IPCZ_PORTAL_H_

#include <cstdint>
#include <utility>

#include "ipcz/api_object.h"
#include "ipcz/ipcz.h"
#include "util/ref_counted.h"

namespace ipcz {

class Node;
class Router;

// A Portal owns a terminal Router along a route. Portals are thread-safe and
// are manipulated directly by public ipcz API calls.
class Portal : public APIObjectImpl<Portal, APIObject::kPortal> {
 public:
  using Pair = std::pair<Ref<Portal>, Ref<Portal>>;

  // Creates a new portal which assumes control over `router` and which lives on
  // `node`.
  Portal(Ref<Node> node, Ref<Router> router);

  const Ref<Node>& node() const { return node_; }
  const Ref<Router>& router() const { return router_; }

  // Creates a new pair of portals which live on `node` and which are directly
  // connected to each other by a LocalRouterLink.
  static Pair CreatePair(Ref<Node> node);

  // APIObject:
  IpczResult Close() override;

  // ipcz portal API implementation:
  IpczResult QueryStatus(IpczPortalStatus& status);

 private:
  ~Portal() override;

  const Ref<Node> node_;
  const Ref<Router> router_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_PORTAL_H_
