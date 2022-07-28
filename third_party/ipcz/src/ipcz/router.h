// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_ROUTER_H_
#define IPCZ_SRC_IPCZ_ROUTER_H_

#include <cstdint>
#include <utility>

#include "ipcz/fragment_ref.h"
#include "ipcz/ipcz.h"
#include "ipcz/parcel_queue.h"
#include "ipcz/route_edge.h"
#include "ipcz/router_descriptor.h"
#include "ipcz/router_link.h"
#include "ipcz/sequence_number.h"
#include "ipcz/sublink_id.h"
#include "ipcz/trap_set.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "util/ref_counted.h"

namespace ipcz {

class NodeLink;
class RemoteRouterLink;
struct RouterLinkState;

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

  // Returns true iff this is a LocalRouterLink whose peer router is `router`.
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

  // Accepts an outbound parcel here from some other Router. The parcel is
  // transmitted immediately or queued for later transmission over the Router's
  // outward link. Called only on proxying Routers.
  bool AcceptOutboundParcel(Parcel& parcel);

  // Accepts notification that the other end of the route has been closed and
  // that the closed end transmitted a total of `sequence_length` parcels before
  // closing.
  bool AcceptRouteClosureFrom(LinkType link_type,
                              SequenceNumber sequence_length);

  // Accepts notification from a link bound to this Router that some node along
  // the route (in the direction of that link) has been disconnected, e.g. due
  // to a crash, and that the route is no longer functional as a result. This is
  // similar to route closure, except no effort can realistically be made to
  // deliver the complete sequence of parcels transmitted from that end of the
  // route. `link_type` specifies the type of link which is propagating the
  // notification to this rouer.
  bool AcceptRouteDisconnectedFrom(LinkType link_type);

  // Retrieves the next available inbound parcel from this Router, if present.
  IpczResult GetNextInboundParcel(IpczGetFlags flags,
                                  void* data,
                                  size_t* num_bytes,
                                  IpczHandle* handles,
                                  size_t* num_handles);

  // Attempts to install a new trap on this Router, to invoke `handler` as soon
  // as one or more conditions in `conditions` is met. This method effectively
  // implements the ipcz Trap() API. See its description in ipcz.h for details.
  IpczResult Trap(const IpczTrapConditions& conditions,
                  IpczTrapEventHandler handler,
                  uint64_t context,
                  IpczTrapConditionFlags* satisfied_condition_flags,
                  IpczPortalStatus* status);

  // Deserializes a new Router from `descriptor` received over `from_node_link`.
  static Ref<Router> Deserialize(const RouterDescriptor& descriptor,
                                 NodeLink& from_node_link);

  // Serializes a description of a new Router which will be used to extend this
  // Router's route across `to_node_link` by introducing a new Router on the
  // remote node.
  void SerializeNewRouter(NodeLink& to_node_link, RouterDescriptor& descriptor);

  // Configures this Router to begin proxying incoming parcels toward (and
  // outgoing parcels from) the Router described by `descriptor`, living on the
  // remote node of `to_node_link`.
  void BeginProxyingToNewRouter(NodeLink& to_node_link,
                                const RouterDescriptor& descriptor);

  // Notifies this router that it should reach out to its outward peer's own
  // outward peer in order to establish a direct link. `requestor` is the link
  // over which this request arrived, and it must be this router's current
  // outward peer in order for the request to be valid.
  //
  // Note that the requestor and its own outward peer must exist on different
  // nodes in order for this method to be called. `bypass_target_node`
  // identifies the node where that router lives, and `bypass_target_sublink`
  // identifies the Sublink used to route between that router and the requestor;
  // i.e., it identifies the link to be bypassed.
  //
  // If the requestor's own outward peer lives on a different node from this
  // router, this router proceeds with the bypass by allocating a new link
  // between itself and the requestor's outward peer and sharing it with that
  // router's node via an AcceptBypassLink message, which will ultimately invoke
  // AcceptBypassLink() on the targeted router.
  //
  // If the requestor's outward peer lives on the same node as this router,
  // bypass is completed immediately by establishing a new LocalRotuerLink
  // between the two routers. In this case a StopProxying message is sent back
  // to the requestor in order to finalize the bypass.
  bool BypassPeer(RemoteRouterLink& requestor,
                  const NodeName& bypass_target_node,
                  SublinkId bypass_target_sublink);

  // Begins decaying this router's outward link and replaces it with a new link
  // over `new_node_link` via `new_sublink`, and using (optional)
  // `new_link_state` for its shared state.
  //
  // `inbound_sequence_length_from_bypassed_link` conveys the final length of
  // sequence of inbound parcels to expect over the decaying link from the peer.
  // See comments on the BypassPeer definition in node_messages_generator.h.
  bool AcceptBypassLink(
      Ref<NodeLink> new_node_link,
      SublinkId new_sublink,
      FragmentRef<RouterLinkState> new_link_state,
      SequenceNumber inbound_sequence_length_from_bypassed_link);

  // Configures the final inbound and outbound sequence lengths of this router's
  // decaying links. Once these lengths are set and sequences have progressed
  // to the specified length in each direction, those decaying links -- and
  // eventually the router itself -- are dropped.
  bool StopProxying(SequenceNumber inbound_sequence_length,
                    SequenceNumber outbound_sequence_length);

  // Configures the final length of the inbound parcel sequence coming from the
  // this router's decaying outward link. Once this length is set and the
  // decaying link has forwarded the full sequence of parcels up to this limit,
  // the decaying link can be dropped.
  bool NotifyProxyWillStop(SequenceNumber inbound_sequence_length);

  // Begins decaying this router's outward link and replaces it with a new link
  // using `new_sublink` over `from_node_link`, the node issuing this request.
  // `new_link_state` if non-null specifies the shared memory location of the
  // RouterLinkState for this link.
  //
  // `inbound_sequence_length` conveys the final length of the sequence of
  // inbound parcels to expect over the decaying link.
  bool BypassPeerWithLink(NodeLink& from_node_link,
                          SublinkId new_sublink,
                          FragmentRef<RouterLinkState> new_link_state,
                          SequenceNumber inbound_sequence_length);

  // Configures the final sequence length of outbound parcels to expect on this
  // proxying Router's decaying inward link. Once this is set and the decaying
  // link has received the full sequence of parcels, the link can be dropped.
  bool StopProxyingToLocalPeer(SequenceNumber outbound_sequence_length);

  // Notifies this Router that one of its links has been disconnected from a
  // remote node. The link is identified by a combination of a specific NodeLink
  // and SublinkId.
  //
  // Note that this is invoked if ANY RemoteRouterLink bound to this router is
  // disconnected at its underlying NodeLink, and the result is aggressive
  // teardown of the route in both directions across any remaining (i.e. primary
  // and/or decaying) links.
  //
  // For a proxying router which is generally only kept alive by the links
  // which are bound to it, this call will typically be followed by imminent
  // destruction of this Router once the caller releases its own reference.
  void NotifyLinkDisconnected(RemoteRouterLink& link);

  // Flushes any inbound or outbound parcels, as well as any route closure
  // notifications. RouterLinks which are no longer needed for the operation of
  // this Router may be deactivated by this call.
  //
  // Since this may be called by many other Router methods, RouterLink
  // implementations must exercise caution when calling into a Router to ensure
  // that their own potentially reentrant deactivation by Flush() won't end up
  // dropping the last reference and deleting `this` before Flush() returns.
  //
  // A safe way to ensure that is for RouterLink implementations to only call
  // into Router using a reference held on the calling stack.
  void Flush();

 private:
  ~Router() override;

  // Attempts to initiate bypass of this router by its peers, and ultimately to
  // remove this router from its route.
  //
  // Called during a Flush() if this is a proxying router which just dropped its
  // last decaying link, or if Flush() was called with kForceProxyBypassAttempt,
  // indicating that some significant state has changed on the route which might
  // unblock our bypass.
  bool MaybeStartSelfBypass();

  absl::Mutex mutex_;

  // The current computed portal status to be reflected by a portal controlling
  // this router, iff this is a terminal router.
  IpczPortalStatus status_ ABSL_GUARDED_BY(mutex_) = {sizeof(status_)};

  // A set of traps installed via a controlling portal where applicable. These
  // traps are notified about any interesting state changes within the router.
  TrapSet traps_ ABSL_GUARDED_BY(mutex_);

  // The edge connecting this router outward to another, toward the portal on
  // the other side of the route.
  RouteEdge outward_edge_ ABSL_GUARDED_BY(mutex_);

  // The edge connecting this router inward to another, closer to the portal on
  // our own side of the route. Only present for proxying routers: terminal
  // routers by definition can have no inward edge.
  absl::optional<RouteEdge> inward_edge_ ABSL_GUARDED_BY(mutex_);

  // Parcels received from the other end of the route. If this is a terminal
  // router, these may be retrieved by the application via a controlling portal;
  // otherwise they will be forwarded along `inward_edge_` as soon as possible.
  ParcelQueue inbound_parcels_ ABSL_GUARDED_BY(mutex_);

  // Parcels transmitted directly from this router (if sent by a controlling
  // portal) or received from an inward peer which sent them outward toward this
  // Router. These parcels generally only accumulate if there is no outward link
  // present when attempting to transmit them, and they are forwarded along
  // `outward_edge_` as soon as possible.
  ParcelQueue outbound_parcels_ ABSL_GUARDED_BY(mutex_);

  // Tracks whether this router has been unexpectedly disconnected from its
  // links. This may be used to prevent additional links from being established.
  bool is_disconnected_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_ROUTER_H_
