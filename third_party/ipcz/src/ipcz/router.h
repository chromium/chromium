// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_ROUTER_H_
#define IPCZ_SRC_IPCZ_ROUTER_H_

#include <utility>

#include "ipcz/ipcz.h"
#include "ipcz/parcel_queue.h"
#include "ipcz/router_link.h"
#include "ipcz/sequence_number.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "util/ref_counted.h"

namespace ipcz {

// The Router is the main primitive responsible for routing parcels between ipcz
// portals. This class is thread-safe.
//
// Before a Router can participate in any actual routing, it must have an
// outward link to another Router (see SetOutwardLink()). To establish a locally
// connected pair of Routers, pass both to LocalRouterLink::ConnectRouters(),
// which internally calls SetOutwardLink() on both:
//
//     Router::Pair routers = {MakeRefCounted<Router>(),
//                             MakeRefCounted<Router>()};
//     LocalRouterLink::ConnectRouters(LinkType::kCentral, routers);
//
// Each ipcz Portal directly controls a terminal Router along its route, and
// all routes stabilize to eventually consist of only two interconnected
// terminal Routers. When a portal moves, its side of the route is extended by
// creating a new terminal Router at the portal's new location. The previous
// terminal Router remains as a proxying hop to be phased out eventually.
class Router : public RefCounted {
 public:
  using Pair = std::pair<Ref<Router>, Ref<Router>>;

  Router();

  // Indicates whether the terminal router on the other side of the central link
  // is known to be closed.
  bool IsPeerClosed();

  // Indicates whether the terminal router on the other side of the central link
  // is known to be closed AND there are no more inbound parcels to be
  // retrieved.
  bool IsRouteDead();

  // Fills in an IpczPortalStatus corresponding to the current state of this
  // Router.
  void QueryStatus(IpczPortalStatus& status);

  // Returns true iff this Router's outward link is a LocalRouterLink between
  // `this` and `other`.
  bool HasLocalPeer(Router& router);

  // Attempts to send an outbound parcel originating from this Router. Called
  // only as a direct result of a Put() or EndPut() call on the router's owning
  // portal.
  IpczResult SendOutboundParcel(Parcel& parcel);

  // Closes this side of the Router's own route. Only called on a Router to
  // which a Portal is currently attached, and only by that Portal.
  void CloseRoute();

  // Uses `link` as this Router's new outward link. This is the primary link on
  // which the router transmits parcels and control messages directed toward the
  // other side of its route. Must only be called on a Router which has no
  // outward link.
  //
  // NOTE: This is NOT safe to call when the other side of the link is already
  // in active use by another Router, as `this` Router may already be in a
  // transitional state and must be able to block decay around `link` from
  // within this call.
  void SetOutwardLink(Ref<RouterLink> link);

  // Accepts an inbound parcel from the outward edge of this router, either to
  // queue it for retrieval or forward it further inward.
  bool AcceptInboundParcel(Parcel& parcel);

  // Accepts notification that the other end of the route has been closed and
  // that the close end transmitted a total of `sequence_length` parcels before
  // closing.
  bool AcceptRouteClosureFrom(LinkType link_type,
                              SequenceNumber sequence_length);

  // Retrieves the next available inbound parcel from this Router, if present.
  IpczResult GetNextInboundParcel(IpczGetFlags flags,
                                  void* data,
                                  size_t* num_bytes,
                                  IpczHandle* handles,
                                  size_t* num_handles);

 private:
  ~Router() override;

  absl::Mutex mutex_;

  // The SequenceNumber of the next outbound parcel to be sent from this router.
  SequenceNumber next_outbound_sequence_number_ ABSL_GUARDED_BY(mutex_){0};

  // The current computed portal status to be reflected by a portal controlling
  // this router, iff this is a terminal router.
  IpczPortalStatus status_ ABSL_GUARDED_BY(mutex_) = {sizeof(status_)};

  // Parcels received from the other end of the route. If this is a terminal
  // router, these may be retrieved by the application via a controlling portal.
  ParcelQueue inbound_parcels_ ABSL_GUARDED_BY(mutex_);

  // A link to this router's peer.
  //
  // TODO(rockot): Replace this with a dynamic link that can be incrementally
  // decayed and replaced.
  Ref<RouterLink> outward_link_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_ROUTER_H_
