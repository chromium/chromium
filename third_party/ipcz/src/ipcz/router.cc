// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/router.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <utility>

#include "ipcz/ipcz.h"
#include "ipcz/node_link.h"
#include "ipcz/remote_router_link.h"
#include "ipcz/sequence_number.h"
#include "ipcz/trap_event_dispatcher.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#include "util/log.h"

namespace ipcz {

Router::Router() = default;

Router::~Router() {
  // A Router MUST be serialized or closed before it can be destroyed. Both
  // operations clear `traps_` and imply that no further traps should be added.
  absl::MutexLock lock(&mutex_);
  ABSL_ASSERT(traps_.empty());
}

bool Router::IsPeerClosed() {
  absl::MutexLock lock(&mutex_);
  return (status_.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED) != 0;
}

bool Router::IsRouteDead() {
  absl::MutexLock lock(&mutex_);
  return (status_.flags & IPCZ_PORTAL_STATUS_DEAD) != 0;
}

void Router::QueryStatus(IpczPortalStatus& status) {
  absl::MutexLock lock(&mutex_);
  const size_t size = std::min(status.size, status_.size);
  memcpy(&status, &status_, size);
  status.size = size;
}

bool Router::HasLocalPeer(Router& router) {
  absl::MutexLock lock(&mutex_);
  if (!outward_link_) {
    return false;
  }
  return outward_link_->HasLocalPeer(router);
}

IpczResult Router::SendOutboundParcel(Parcel& parcel) {
  Ref<RouterLink> link;
  {
    absl::MutexLock lock(&mutex_);
    const SequenceNumber sequence_number =
        outbound_parcels_.GetCurrentSequenceLength();
    parcel.set_sequence_number(sequence_number);
    if (outward_link_ &&
        outbound_parcels_.MaybeSkipSequenceNumber(sequence_number)) {
      // If there are no unsent parcels ahead of this one in the outbound
      // sequence, and we have an active outward link, we can immediately
      // transmit the parcel without any intermediate queueing step. This is the
      // most common case.
      link = outward_link_;
    } else {
      DVLOG(4) << "Queuing outbound " << parcel.Describe();
      const bool push_ok =
          outbound_parcels_.Push(sequence_number, std::move(parcel));
      ABSL_ASSERT(push_ok);
    }
  }

  if (link) {
    link->AcceptParcel(parcel);
  } else {
    Flush();
  }
  return IPCZ_RESULT_OK;
}

void Router::CloseRoute() {
  TrapEventDispatcher dispatcher;
  Ref<RouterLink> link;
  {
    absl::MutexLock lock(&mutex_);
    bool ok = outbound_parcels_.SetFinalSequenceLength(
        outbound_parcels_.GetCurrentSequenceLength());
    ABSL_ASSERT(ok);

    traps_.RemoveAll(dispatcher);
  }

  Flush();
}

void Router::SetOutwardLink(Ref<RouterLink> link) {
  {
    absl::MutexLock lock(&mutex_);
    ABSL_ASSERT(!outward_link_);
    outward_link_ = std::move(link);
  }

  Flush();
}

bool Router::AcceptInboundParcel(Parcel& parcel) {
  TrapEventDispatcher dispatcher;
  {
    absl::MutexLock lock(&mutex_);
    const SequenceNumber sequence_number = parcel.sequence_number();
    if (!inbound_parcels_.Push(sequence_number, std::move(parcel))) {
      return false;
    }

    status_.num_local_parcels = inbound_parcels_.GetNumAvailableElements();
    status_.num_local_bytes = inbound_parcels_.GetTotalAvailableElementSize();
    traps_.UpdatePortalStatus(status_, TrapSet::UpdateReason::kNewLocalParcel,
                              dispatcher);
  }

  Flush();
  return true;
}

bool Router::AcceptOutboundParcel(Parcel& parcel) {
  {
    absl::MutexLock lock(&mutex_);

    // Proxied outbound parcels are always queued in a ParcelQueue even if they
    // will be forwarded immediately. This allows us to track the full sequence
    // of forwarded parcels so we can know with certainty when we're done
    // forwarding.
    //
    // TODO: Using a queue here may increase latency along the route, because it
    // it unnecessarily forces in-order forwarding. We could use an unordered
    // queue for forwarding, but we'd still need some lighter-weight abstraction
    // that tracks complete sequences from potentially fragmented contributions.
    const SequenceNumber sequence_number = parcel.sequence_number();
    if (!outbound_parcels_.Push(sequence_number, std::move(parcel))) {
      return false;
    }
  }

  Flush();
  return true;
}

bool Router::AcceptRouteClosureFrom(LinkType link_type,
                                    SequenceNumber sequence_length) {
  TrapEventDispatcher dispatcher;
  {
    absl::MutexLock lock(&mutex_);
    if (link_type.is_outward()) {
      if (!inbound_parcels_.SetFinalSequenceLength(sequence_length)) {
        return false;
      }

      if (inward_link_) {
        inward_link_->AcceptRouteClosure(sequence_length);
      } else {
        status_.flags |= IPCZ_PORTAL_STATUS_PEER_CLOSED;
        if (inbound_parcels_.IsSequenceFullyConsumed()) {
          status_.flags |= IPCZ_PORTAL_STATUS_DEAD;
        }
        traps_.UpdatePortalStatus(status_, TrapSet::UpdateReason::kPeerClosed,
                                  dispatcher);
      }
    } else if (link_type.is_peripheral_inward()) {
      if (!outbound_parcels_.SetFinalSequenceLength(sequence_length)) {
        return false;
      }
    }
  }

  Flush();
  return true;
}

IpczResult Router::GetNextInboundParcel(IpczGetFlags flags,
                                        void* data,
                                        size_t* num_bytes,
                                        IpczHandle* handles,
                                        size_t* num_handles) {
  TrapEventDispatcher dispatcher;
  absl::MutexLock lock(&mutex_);
  if (inbound_parcels_.IsSequenceFullyConsumed()) {
    return IPCZ_RESULT_NOT_FOUND;
  }
  if (!inbound_parcels_.HasNextElement()) {
    return IPCZ_RESULT_UNAVAILABLE;
  }

  Parcel& p = inbound_parcels_.NextElement();
  const bool allow_partial = (flags & IPCZ_GET_PARTIAL) != 0;
  const size_t data_capacity = num_bytes ? *num_bytes : 0;
  const size_t handles_capacity = num_handles ? *num_handles : 0;
  const size_t data_size =
      allow_partial ? std::min(p.data_size(), data_capacity) : p.data_size();
  const size_t handles_size = allow_partial
                                  ? std::min(p.num_objects(), handles_capacity)
                                  : p.num_objects();
  if (num_bytes) {
    *num_bytes = data_size;
  }
  if (num_handles) {
    *num_handles = handles_size;
  }

  const bool consuming_whole_parcel =
      data_capacity >= data_size && handles_capacity >= handles_size;
  if (!consuming_whole_parcel && !allow_partial) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  memcpy(data, p.data_view().data(), data_size);
  const bool ok = inbound_parcels_.Consume(
      data_size, absl::MakeSpan(handles, handles_size));
  ABSL_ASSERT(ok);

  status_.num_local_parcels = inbound_parcels_.GetNumAvailableElements();
  status_.num_local_bytes = inbound_parcels_.GetTotalAvailableElementSize();
  if (inbound_parcels_.IsSequenceFullyConsumed()) {
    status_.flags |= IPCZ_PORTAL_STATUS_DEAD;
  }
  traps_.UpdatePortalStatus(
      status_, TrapSet::UpdateReason::kLocalParcelConsumed, dispatcher);
  return IPCZ_RESULT_OK;
}

IpczResult Router::Trap(const IpczTrapConditions& conditions,
                        IpczTrapEventHandler handler,
                        uint64_t context,
                        IpczTrapConditionFlags* satisfied_condition_flags,
                        IpczPortalStatus* status) {
  absl::MutexLock lock(&mutex_);
  return traps_.Add(conditions, handler, context, status_,
                    satisfied_condition_flags, status);
}

// static
Ref<Router> Router::Deserialize(const RouterDescriptor& descriptor,
                                NodeLink& from_node_link) {
  auto router = MakeRefCounted<Router>();
  {
    absl::MutexLock lock(&router->mutex_);
    router->outbound_parcels_.ResetInitialSequenceNumber(
        descriptor.next_outgoing_sequence_number);
    router->inbound_parcels_.ResetInitialSequenceNumber(
        descriptor.next_incoming_sequence_number);
    if (descriptor.peer_closed) {
      router->status_.flags |= IPCZ_PORTAL_STATUS_PEER_CLOSED;
      if (!router->inbound_parcels_.SetFinalSequenceLength(
              descriptor.closed_peer_sequence_length)) {
        return nullptr;
      }
      if (router->inbound_parcels_.IsSequenceFullyConsumed()) {
        router->status_.flags |= IPCZ_PORTAL_STATUS_DEAD;
      }
    }

    Ref<RemoteRouterLink> new_link = from_node_link.AddRemoteRouterLink(
        descriptor.new_sublink, LinkType::kPeripheralOutward, LinkSide::kB,
        router);
    if (!new_link) {
      return nullptr;
    }
    router->outward_link_ = std::move(new_link);

    DVLOG(4) << "Route extended from "
             << from_node_link.remote_node_name().ToString() << " to "
             << from_node_link.local_node_name().ToString() << " via sublink "
             << descriptor.new_sublink;
  }

  router->Flush();
  return router;
}

void Router::SerializeNewRouter(NodeLink& to_node_link,
                                RouterDescriptor& descriptor) {
  TrapEventDispatcher dispatcher;
  {
    absl::MutexLock lock(&mutex_);
    traps_.RemoveAll(dispatcher);

    descriptor.next_outgoing_sequence_number =
        outbound_parcels_.current_sequence_number();
    descriptor.next_incoming_sequence_number =
        inbound_parcels_.current_sequence_number();

    DVLOG(4) << "Extending route to new router with outbound sequence length "
             << descriptor.next_outgoing_sequence_number
             << " and current inbound sequence number "
             << descriptor.next_incoming_sequence_number;

    if (status_.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED) {
      descriptor.peer_closed = true;
      descriptor.closed_peer_sequence_length =
          *inbound_parcels_.final_sequence_length();
    }
  }

  const SublinkId new_sublink = to_node_link.memory().AllocateSublinkIds(1);
  descriptor.new_sublink = new_sublink;

  // Once `descriptor` is transmitted to the destination node and the new Router
  // is created there, it may immediately begin transmitting messages back to
  // this node regarding `new_sublink`. We establish a new RemoteRouterLink now
  // and register it to `new_sublink` on `to_node_link`, so that any such
  // incoming messages are routed to `this`.
  //
  // NOTE: We do not yet provide `this` itself with a reference to the new
  // RemoteRouterLink, because it's not yet safe for us to send messages to the
  // remote node regarding `new_sublink`. `descriptor` must be transmitted
  // first.
  to_node_link.AddRemoteRouterLink(new_sublink, LinkType::kPeripheralInward,
                                   LinkSide::kA, WrapRefCounted(this));
}

void Router::BeginProxyingToNewRouter(NodeLink& to_node_link,
                                      const RouterDescriptor& descriptor) {
  // Acquire a reference to the sublink created by an earlier call to
  // SerializeNewRouter().
  const absl::optional<NodeLink::Sublink> new_sublink =
      to_node_link.GetSublink(descriptor.new_sublink);
  if (!new_sublink) {
    // The sublink has been torn down, presumably because of node disconnection.
    // Nowhere to proxy now, so we're done.
    return;
  }

  bool deactivate_link = false;
  {
    absl::MutexLock lock(&mutex_);
    ABSL_ASSERT(!inward_link_);

    // It's possible that the new router was already closed and we've already
    // received a notification about this and forwarded any parcels it may have
    // sent. In that case it would be pointless to establish an inward link, so
    // we'll just drop it instead.
    if (outbound_parcels_.IsSequenceFullyConsumed()) {
      deactivate_link = true;
    } else {
      // TODO: Initiate proxy removal ASAP now that we're proxying.
      inward_link_ = new_sublink->router_link;
    }
  }

  if (deactivate_link) {
    new_sublink->router_link->Deactivate();
    return;
  }

  // We may have inbound parcels queued which need to be forwarded to the new
  // Router, so give them a chance to be flushed out.
  Flush();
}

void Router::NotifyLinkDisconnected(const NodeLink& node_link,
                                    SublinkId sublink) {
  Ref<RouterLink> dead_outward_link;
  SequenceNumber inbound_sequence_length;
  Ref<RouterLink> dead_inward_link;
  SequenceNumber outbound_sequence_length;
  {
    absl::MutexLock lock(&mutex_);
    if (outward_link_ && outward_link_->IsRemoteLinkTo(node_link, sublink)) {
      dead_outward_link = std::move(outward_link_);
      inbound_sequence_length = inbound_parcels_.GetCurrentSequenceLength();
    } else if (inward_link_ &&
               inward_link_->IsRemoteLinkTo(node_link, sublink)) {
      dead_inward_link = std::move(inward_link_);
      outbound_sequence_length = outbound_parcels_.GetCurrentSequenceLength();
    }
  }

  if (dead_outward_link) {
    AcceptRouteClosureFrom(dead_outward_link->GetType(),
                           inbound_sequence_length);
  }

  if (dead_inward_link) {
    AcceptRouteClosureFrom(dead_inward_link->GetType(),
                           outbound_sequence_length);
  }
}

void Router::Flush() {
  Ref<RouterLink> outward_link;
  Ref<RouterLink> inward_link;
  Ref<RouterLink> dead_inward_link;
  Ref<RouterLink> dead_outward_link;
  absl::InlinedVector<Parcel, 2> inbound_parcels;
  absl::InlinedVector<Parcel, 2> outbound_parcels;
  absl::optional<SequenceNumber> final_outward_sequence_length;
  {
    absl::MutexLock lock(&mutex_);
    outward_link = outward_link_;
    inward_link = inward_link_;

    // Collect any outbound parcels which are safe to transmit now. Note that we
    // do not transmit anything or generally call into any RouterLinks while
    // `mutex_` is held, because such calls may ultimately re-enter this Router
    // (e.g. if a link is a LocalRouterLink, or even a RemoteRouterLink with a
    // fully synchronous driver.) Instead we accumulate work within this block,
    // and then perform any transmissions or link deactivations after the mutex
    // is released further below.
    Parcel parcel;
    while (outbound_parcels_.HasNextElement() && outward_link) {
      bool ok = outbound_parcels_.Pop(parcel);
      ABSL_ASSERT(ok);
      outbound_parcels.push_back(std::move(parcel));
    }

    // If we have an inward link, then we're a proxy. Collect any queued inbound
    // parcels to forward over that link.
    while (inbound_parcels_.HasNextElement() && inward_link) {
      bool ok = inbound_parcels_.Pop(parcel);
      ABSL_ASSERT(ok);
      inbound_parcels.push_back(std::move(parcel));
    }

    if (outward_link && outbound_parcels_.IsSequenceFullyConsumed()) {
      // Notify the other end of the route that this end is closed. See the
      // AcceptRouteClosure() invocation further below.
      final_outward_sequence_length =
          *outbound_parcels_.final_sequence_length();

      // We also have no more use for either outward or inward links: trivially
      // there are no more outbound parcels to send outward, and there no longer
      // exists an ultimate destination for any forwarded inbound parcels. So we
      // drop both links now.
      dead_outward_link = std::move(outward_link_);
      dead_inward_link = std::move(inward_link_);
    }
  }

  for (Parcel& parcel : outbound_parcels) {
    outward_link->AcceptParcel(parcel);
  }

  for (Parcel& parcel : inbound_parcels) {
    inward_link->AcceptParcel(parcel);
  }

  if (final_outward_sequence_length) {
    outward_link->AcceptRouteClosure(*final_outward_sequence_length);
  }

  if (dead_outward_link) {
    dead_outward_link->Deactivate();
  }

  if (dead_inward_link) {
    dead_inward_link->Deactivate();
  }
}

}  // namespace ipcz
