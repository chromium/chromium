// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_ROUTER_LINK_H_
#define IPCZ_SRC_IPCZ_ROUTER_LINK_H_

#include "ipcz/link_type.h"
#include "ipcz/sequence_number.h"
#include "util/ref_counted.h"

namespace ipcz {

class Parcel;
class Router;

// A RouterLink represents one endpoint of a link between two Routers. All
// subclasses must be thread-safe.
class RouterLink : public RefCounted {
 public:
  using Pair = std::pair<Ref<RouterLink>, Ref<RouterLink>>;

  // Indicates what type of link this is. See LinkType documentation.
  virtual LinkType GetType() const = 0;

  // Returns true iff this is a LocalRouterLink whose peer router is `router`.
  virtual bool HasLocalPeer(const Router& router) = 0;

  // Passes a parcel to the Router on the other side of this link to be queued
  // and/or router further.
  virtual void AcceptParcel(Parcel& parcel) = 0;

  // Notifies the Router on the other side of the link that the route has been
  // closed from this side. `sequence_length` is the total number of parcels
  // transmitted from the closed side before it was closed.
  virtual void AcceptRouteClosure(SequenceNumber sequence_length) = 0;

 protected:
  ~RouterLink() override = default;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_ROUTER_LINK_H_
