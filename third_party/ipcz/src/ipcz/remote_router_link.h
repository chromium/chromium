// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_REMOTE_ROUTER_LINK_H_
#define IPCZ_SRC_IPCZ_REMOTE_ROUTER_LINK_H_

#include <atomic>

#include "ipcz/fragment_ref.h"
#include "ipcz/link_side.h"
#include "ipcz/link_type.h"
#include "ipcz/router_link.h"
#include "ipcz/router_link_state.h"
#include "ipcz/sublink_id.h"
#include "util/ref_counted.h"

namespace ipcz {

class NodeLink;

// One side of a link between two Routers living on different nodes. A
// RemoteRouterLink uses a NodeLink plus a SublinkId as its transport between
// the routers. On the other end (on another node) is another RemoteRouterLink
// using a NodeLink back to this node, with the same SublinkId.
//
// As with other RouterLink instances, each RemoteRouterLink is assigned a
// LinkSide at construction time. This assignment is arbitrary but will always
// be the opposite of the LinkSide assigned to the RemoteRouteLink on the other
// end.
//
// NOTE: This implementation must take caution when calling into any Router. See
// note on RouterLink's own class documentation.
class RemoteRouterLink : public RouterLink {
 public:
  // Constructs a new RemoteRouterLink which sends messages over `node_link`
  // using `sublink` specifically. `side` is the side of this link on which
  // this RemoteRouterLink falls (side A or B), and `type` indicates what type
  // of link it is -- which for remote links must be either kCentral,
  // kPeripheralInward, or kPeripheralOutward. If the link is kCentral, a
  // non-null `link_state` may be provided to use as the link's RouterLinkState.
  static Ref<RemoteRouterLink> Create(Ref<NodeLink> node_link,
                                      SublinkId sublink,
                                      FragmentRef<RouterLinkState> link_state,
                                      LinkType type,
                                      LinkSide side);

  const Ref<NodeLink>& node_link() const { return node_link_; }
  SublinkId sublink() const { return sublink_; }

  // Sets this link's RouterLinkState.
  //
  // If `state` is null and this link is on side B, this call is a no-op. If
  // `state` is null and this link is on side A, this call will kick off an
  // asynchronous allocation of a new RouterLinkState. When that completes, the
  // new state will be adopted by side A and shared with side B.
  //
  // If `state` references a pending fragment and this link is on side A, the
  // call is a no-op. If `state` references a pending fragment and this link
  // is on side B, this operation will be automatically deferred until the
  // NodeLink acquires a mapping of the buffer referenced by `state` and the
  // fragment can be resolved to an addressable one.
  //
  // Finally, if `state` references a valid, addressable fragment, it is
  // adopted as-is.
  void SetLinkState(FragmentRef<RouterLinkState> state);

  // RouterLink:
  LinkType GetType() const override;
  RouterLinkState* GetLinkState() const override;
  bool HasLocalPeer(const Router& router) override;
  bool IsRemoteLinkTo(const NodeLink& node_link, SublinkId sublink) override;
  void AcceptParcel(Parcel& parcel) override;
  void AcceptRouteClosure(SequenceNumber sequence_length) override;
  void MarkSideStable() override;
  bool TryLockForBypass(const NodeName& bypass_request_source) override;
  bool TryLockForClosure() override;
  void Unlock() override;
  bool FlushOtherSideIfWaiting() override;
  bool CanNodeRequestBypass(const NodeName& bypass_request_source) override;
  void Deactivate() override;
  std::string Describe() const override;

 private:
  RemoteRouterLink(Ref<NodeLink> node_link,
                   SublinkId sublink,
                   FragmentRef<RouterLinkState> link_state,
                   LinkType type,
                   LinkSide side);

  ~RemoteRouterLink() override;

  void AllocateAndShareLinkState();

  const Ref<NodeLink> node_link_;
  const SublinkId sublink_;
  const LinkType type_;
  const LinkSide side_;

  // Local atomic cache of whether this side of the link is marked stable. If
  // MarkSideStable() is called when no RouterLinkState is present, this will be
  // used to remember it once a RouterLinkState is finally established.
  std::atomic<bool> side_is_stable_{false};

  // A reference to the shared memory Fragment containing the RouterLinkState
  // shared by both ends of this RouterLink. Always null for non-central links,
  // and may be null for a central links if its RouterLinkState has not yet been
  // allocated or shared.
  //
  // Must be set at most once and is only retained by this object to keep the
  // fragment allocated. Access is unguarded and is restricted to
  // SetLinkState(), and only allowed while `link_state_` below is still null.
  // Any other access is unsafe. Use GetLinkState() to get a usable reference to
  // the RouterLinkState instance.
  FragmentRef<RouterLinkState> link_state_fragment_;

  // Cached address of the shared RouterLinkState referenced by
  // `link_state_fragment_`. Once this is set to a non-null value it retains
  // that value indefinitely, so any non-null value loaded from this field is
  // safe to dereference for the duration of the RemoteRouterLink's lifetime.
  std::atomic<RouterLinkState*> link_state_{nullptr};
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_REMOTE_ROUTER_LINK_H_
