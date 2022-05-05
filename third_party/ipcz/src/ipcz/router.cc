// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/router.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "ipcz/trap_event_dispatcher.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"

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
    link = outward_link_;
    parcel.set_sequence_number(next_outbound_sequence_number_);
    next_outbound_sequence_number_ =
        SequenceNumber{next_outbound_sequence_number_.value() + 1};
  }

  ABSL_ASSERT(link);
  link->AcceptParcel(parcel);
  return IPCZ_RESULT_OK;
}

void Router::CloseRoute() {
  TrapEventDispatcher dispatcher;

  SequenceNumber sequence_length;
  Ref<RouterLink> link;
  {
    absl::MutexLock lock(&mutex_);
    link = std::move(outward_link_);
    sequence_length = next_outbound_sequence_number_;
    traps_.RemoveAll(dispatcher);
  }

  ABSL_ASSERT(link);
  link->AcceptRouteClosure(sequence_length);
  link->Deactivate();
}

void Router::SetOutwardLink(Ref<RouterLink> link) {
  absl::MutexLock lock(&mutex_);
  ABSL_ASSERT(!outward_link_);
  outward_link_ = std::move(link);
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

}  // namespace ipcz
