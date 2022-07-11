// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_ROUTER_LINK_H_
#define IPCZ_SRC_IPCZ_ROUTER_LINK_H_

#include "ipcz/link_type.h"
#include "ipcz/node_name.h"
#include "ipcz/sequence_number.h"
#include "ipcz/sublink_id.h"
#include "util/ref_counted.h"

namespace ipcz {

class NodeLink;
class Parcel;
class Router;
struct RouterLinkState;

// A RouterLink represents one endpoint of a link between two Routers. All
// subclasses must be thread-safe.
//
// NOTE: Implementations of this class must take caution when calling into
// Routers, since such calls may re-enter the RouterLink implementation to
// deactivate it. As a general rule, calls into Router should be made using a
// Router reference owned on the calling stack rather than a reference owned by
// the RouterLink.
class RouterLink : public RefCounted {
 public:
  using Pair = std::pair<Ref<RouterLink>, Ref<RouterLink>>;

  // Indicates what type of link this is. See LinkType documentation.
  virtual LinkType GetType() const = 0;

  // Returns a pointer to the link's RouterLinkState, if it has one. Otherwise
  // returns null.
  virtual RouterLinkState* GetLinkState() const = 0;

  // Returns true iff this is a LocalRouterLink whose peer router is `router`.
  virtual bool HasLocalPeer(const Router& router) = 0;

  // Returns true iff this is a RemoteRouterLink routing over `node_link` via
  // `sublink`.
  virtual bool IsRemoteLinkTo(const NodeLink& node_link, SublinkId sublink) = 0;

  // Passes a parcel to the Router on the other side of this link to be queued
  // and/or router further.
  virtual void AcceptParcel(Parcel& parcel) = 0;

  // Notifies the Router on the other side of the link that the route has been
  // closed from this side. `sequence_length` is the total number of parcels
  // transmitted from the closed side before it was closed.
  virtual void AcceptRouteClosure(SequenceNumber sequence_length) = 0;

  // Signals that this side of the link is in a stable state suitable for one
  // side or the other to lock the link, either for bypass or closure
  // propagation. Only once both sides are marked stable can either side lock
  // the link with TryLock* methods below.
  virtual void MarkSideStable() = 0;

  // Attempts to lock the link for the router on this side to coordinate its own
  // bypass. Returns true if and only if successful, meaning the link is locked
  // and it's safe for the router who locked it to coordinate its own bypass by
  // providing its inward and outward peers with a new central link over which
  // they may communicate directly.
  //
  // On success, `bypass_request_source` is also stashed in this link's shared
  // state so that the other side of the link can authenticate a bypass request
  // coming from that node. This parameter may be omitted if the bypass does not
  // not require authentication, e.g. because the requesting inward peer's node
  // is the same as the proxy's own node, or that of the proxy's current outward
  // peer.
  [[nodiscard]] virtual bool TryLockForBypass(
      const NodeName& bypass_request_source = {}) = 0;

  // Attempts to lock the link for the router on this side to propagate route
  // closure toward the other side. Returns true if and only if successful,
  // meaning no further bypass operations will proceed on the link.
  [[nodiscard]] virtual bool TryLockForClosure() = 0;

  // Unlocks a link previously locked by one of the TryLock* methods above.
  virtual void Unlock() = 0;

  // Asks the other side to flush its router if and only if the side marked
  // itself as waiting for both sides of the link to become stable, and both
  // sides of the link are stable. Returns true if and only if a flush was
  // actually issued to the other side.
  virtual bool FlushOtherSideIfWaiting() = 0;

  // Indicates whether this link can be bypassed by a request from the named
  // node to one side of the link. True if and only if the proxy on the other
  // side of this link has already initiated bypass and `bypass_request_source`
  // matches the NodeName it stored in this link's shared state at that time.
  virtual bool CanNodeRequestBypass(const NodeName& bypass_request_source) = 0;

  // Deactivates this RouterLink to sever any binding it may have to a specific
  // Router. Note that deactivation is not necessarily synchronous, so some
  // in-progress calls into a Router may still complete on behalf of this
  // RouterLink after Deactivate() returns. This call only ensures that the link
  // will stop calling into (and generally stop referencing) the Router ASAP.
  virtual void Deactivate() = 0;

  // Generates a string description of this RouterLink for debug logging.
  virtual std::string Describe() const = 0;

 protected:
  ~RouterLink() override = default;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_ROUTER_LINK_H_
