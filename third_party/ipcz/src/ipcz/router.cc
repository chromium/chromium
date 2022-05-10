// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/router.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <utility>

#include "ipcz/ipcz.h"
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
  absl::MutexLock lock(&mutex_);
  const SequenceNumber sequence_number = parcel.sequence_number();
  if (!inbound_parcels_.Push(sequence_number, std::move(parcel))) {
    return false;
  }

  status_.num_local_parcels = inbound_parcels_.GetNumAvailableElements();
  status_.num_local_bytes = inbound_parcels_.GetTotalAvailableElementSize();
  traps_.UpdatePortalStatus(status_, TrapSet::UpdateReason::kNewLocalParcel,
                            dispatcher);
  return true;
}

bool Router::AcceptRouteClosureFrom(LinkType link_type,
                                    SequenceNumber sequence_length) {
  TrapEventDispatcher dispatcher;
  ABSL_ASSERT(link_type == LinkType::kCentral);
  {
    absl::MutexLock lock(&mutex_);
    if (!inbound_parcels_.SetFinalSequenceLength(sequence_length)) {
      return false;
    }

    status_.flags |= IPCZ_PORTAL_STATUS_PEER_CLOSED;
    if (inbound_parcels_.IsSequenceFullyConsumed()) {
      status_.flags |= IPCZ_PORTAL_STATUS_DEAD;
    }
    traps_.UpdatePortalStatus(status_, TrapSet::UpdateReason::kPeerClosed,
                              dispatcher);
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

void Router::Flush() {
  Ref<RouterLink> outward_link;
  Ref<RouterLink> dead_outward_link;
  absl::InlinedVector<Parcel, 2> outbound_parcels;
  absl::optional<SequenceNumber> final_outward_sequence_length;
  {
    absl::MutexLock lock(&mutex_);
    outward_link = outward_link_;

    // Collect any outbound parcels which are safe to transmit now. Note that we
    // do not transmit anything or generally call into any RouterLinks while
    // `mutex_` is held, because such calls may ultimately re-enter this Router
    // (e.g. if a link is a LocalRouterLink, or even a RemoteRouterLink with a
    // fully synchronous driver.)
    Parcel parcel;
    while (outbound_parcels_.HasNextElement() && outward_link) {
      bool ok = outbound_parcels_.Pop(parcel);
      ABSL_ASSERT(ok);
      outbound_parcels.push_back(std::move(parcel));
    }

    if (outward_link && outbound_parcels_.IsSequenceFullyConsumed()) {
      // This implies that the outbound sequence has been finalized, which in
      // turn means the route has been closed on our end.  Capture the final
      // sequence length so we can inform the peer below.
      final_outward_sequence_length = outbound_parcels_.final_sequence_length();

      if (!inbound_parcels_.ExpectsMoreElements()) {
        // If there will also be no more inbound parcels received because the
        // peer portal has been closed, we can drop the outward link altogether.
        dead_outward_link = std::move(outward_link_);
      }
    }
  }

  for (Parcel& parcel : outbound_parcels) {
    outward_link->AcceptParcel(parcel);
  }

  if (final_outward_sequence_length) {
    outward_link->AcceptRouteClosure(*final_outward_sequence_length);
  }

  if (dead_outward_link) {
    dead_outward_link->Deactivate();
  }
}

}  // namespace ipcz
