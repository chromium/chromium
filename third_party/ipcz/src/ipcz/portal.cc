// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/portal.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ipcz/api_object.h"
#include "ipcz/local_router_link.h"
#include "ipcz/operation_context.h"
#include "ipcz/router.h"
#include "ipcz/trap_event_dispatcher.h"
#include "third_party/abseil-cpp/absl/types/span.h"
#include "util/log.h"
#include "util/overloaded.h"
#include "util/ref_counted.h"

namespace ipcz {

namespace {

bool ValidateAndAcquireObjectsForTransitFrom(
    Portal& sender,
    absl::Span<const IpczHandle> handles,
    std::vector<Ref<APIObject>>& objects) {
  objects.resize(handles.size());
  for (size_t i = 0; i < handles.size(); ++i) {
    auto* object = APIObject::FromHandle(handles[i]);
    if (!object || !object->CanSendFrom(sender)) {
      return false;
    }
    objects[i] = WrapRefCounted(object);
  }
  return true;
}

}  // namespace

Portal::Portal(Ref<Node> node, Ref<Router> router)
    : node_(std::move(node)), router_(std::move(router)) {}

Portal::~Portal() = default;

// static
Portal::Pair Portal::CreatePair(Ref<Node> node) {
  Router::Pair routers{MakeRefCounted<Router>(), MakeRefCounted<Router>()};
  DVLOG(5) << "Created new portal pair with routers " << routers.first.get()
           << " and " << routers.second.get();

  const OperationContext context{OperationContext::kAPICall};
  auto links = LocalRouterLink::CreatePair(LinkType::kCentral, routers,
                                           LocalRouterLink::kStable);
  routers.first->SetOutwardLink(context, std::move(links.first));
  routers.second->SetOutwardLink(context, std::move(links.second));
  return {MakeRefCounted<Portal>(node, std::move(routers.first)),
          MakeRefCounted<Portal>(node, std::move(routers.second))};
}

IpczResult Portal::Close() {
  router_->CloseRoute();
  return IPCZ_RESULT_OK;
}

bool Portal::CanSendFrom(Portal& sender) {
  return &sender != this && !sender.router()->HasLocalPeer(*router_);
}

IpczResult Portal::QueryStatus(IpczPortalStatus& status) {
  router_->QueryStatus(status);
  return IPCZ_RESULT_OK;
}

IpczResult Portal::Merge(Portal& other) {
  return router_->MergeRoute(other.router());
}

IpczResult Portal::Put(absl::Span<const uint8_t> data,
                       absl::Span<const IpczHandle> handles,
                       const IpczPutLimits* limits) {
  std::vector<Ref<APIObject>> objects;
  if (!ValidateAndAcquireObjectsForTransitFrom(*this, handles, objects)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (router_->IsPeerClosed()) {
    return IPCZ_RESULT_NOT_FOUND;
  }

  if (limits && router_->GetOutboundCapacityInBytes(*limits) < data.size()) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  Parcel parcel;
  const IpczResult allocate_result = router_->AllocateOutboundParcel(
      data.size(), /*allow_partial=*/false, parcel);
  if (allocate_result != IPCZ_RESULT_OK) {
    return allocate_result;
  }

  if (!data.empty()) {
    memcpy(parcel.data_view().data(), data.data(), data.size());
  }
  parcel.CommitData(data.size());
  parcel.SetObjects(std::move(objects));
  const IpczResult result = router_->SendOutboundParcel(parcel);
  if (result == IPCZ_RESULT_OK) {
    // If the parcel was sent, the sender relinquishes handle ownership and
    // therefore implicitly releases its ref to each object.
    for (IpczHandle handle : handles) {
      std::ignore = APIObject::TakeFromHandle(handle);
    }
  }

  return result;
}

IpczResult Portal::BeginPut(IpczBeginPutFlags flags,
                            const IpczPutLimits* limits,
                            size_t& num_data_bytes,
                            void*& data) {
  const bool allow_partial = (flags & IPCZ_BEGIN_PUT_ALLOW_PARTIAL) != 0;
  if (limits) {
    size_t max_num_data_bytes = router_->GetOutboundCapacityInBytes(*limits);
    if (max_num_data_bytes < num_data_bytes) {
      num_data_bytes = max_num_data_bytes;
      if (!allow_partial || max_num_data_bytes == 0) {
        return IPCZ_RESULT_RESOURCE_EXHAUSTED;
      }
    }
  }

  if (router_->IsPeerClosed()) {
    return IPCZ_RESULT_NOT_FOUND;
  }

  // Always request a non-zero size for two-phase puts so that we always have
  // a non-null data address upon which to key the operation in EndPut().
  const size_t num_bytes_to_request = num_data_bytes ? num_data_bytes : 1;
  Parcel parcel;
  const IpczResult allocation_result = router_->AllocateOutboundParcel(
      num_bytes_to_request, allow_partial, parcel);
  absl::MutexLock lock(&mutex_);
  if (allocation_result != IPCZ_RESULT_OK) {
    return allocation_result;
  }

  num_data_bytes = parcel.data_view().size();
  data = parcel.data_view().data();
  absl::visit(Overloaded{[&](absl::monostate) {
                           pending_parcels_.emplace<Parcel>(std::move(parcel));
                         },
                         [&](Parcel& first_parcel) {
                           const void* first_key =
                               first_parcel.data_view().data();
                           PendingParcelMap parcels;
                           parcels[first_key] = std::move(first_parcel);
                           parcels[data] = std::move(parcel);
                           pending_parcels_.emplace<PendingParcelMap>(
                               std::move(parcels));
                         },
                         [&](PendingParcelMap& parcels) {
                           parcels[data] = std::move(parcel);
                         }},
              pending_parcels_);
  return IPCZ_RESULT_OK;
}

IpczResult Portal::CommitPut(const void* data,
                             size_t num_data_bytes_produced,
                             absl::Span<const IpczHandle> handles) {
  std::vector<Ref<APIObject>> objects;
  if (!ValidateAndAcquireObjectsForTransitFrom(*this, handles, objects)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  Parcel parcel;
  {
    absl::MutexLock lock(&mutex_);
    const bool is_request_valid = absl::visit(
        Overloaded{
            [&](absl::monostate) { return false; },
            [&](Parcel& first_parcel) {
              if (first_parcel.data_view().data() != data ||
                  num_data_bytes_produced > first_parcel.data_view().size()) {
                return false;
              }

              parcel = std::move(first_parcel);
              pending_parcels_ = absl::monostate{};
              return true;
            },
            [&](PendingParcelMap& parcels) {
              auto it = parcels.find(data);
              if (it == parcels.end() ||
                  num_data_bytes_produced > it->second.data_view().size()) {
                return false;
              }

              parcel = std::move(it->second);
              parcels.erase(it);
              return true;
            }},
        pending_parcels_);
    if (!is_request_valid) {
      return IPCZ_RESULT_INVALID_ARGUMENT;
    }
  }

  parcel.CommitData(num_data_bytes_produced);
  parcel.SetObjects(std::move(objects));
  IpczResult result = router_->SendOutboundParcel(parcel);
  if (result == IPCZ_RESULT_OK) {
    // If the parcel was sent, the sender relinquishes handle ownership and
    // therefore implicitly releases its ref to each object.
    for (IpczHandle handle : handles) {
      APIObject::TakeFromHandle(handle);
    }
  }

  return result;
}

IpczResult Portal::AbortPut(const void* data) {
  absl::MutexLock lock(&mutex_);
  const bool is_request_valid =
      absl::visit(Overloaded{[&](absl::monostate) { return false; },
                             [&](Parcel& first_parcel) {
                               if (first_parcel.data_view().data() != data) {
                                 return false;
                               }

                               pending_parcels_ = absl::monostate{};
                               return true;
                             },
                             [&](PendingParcelMap& parcels) {
                               auto it = parcels.find(data);
                               if (it == parcels.end()) {
                                 return false;
                               }

                               parcels.erase(it);
                               return true;
                             }},
                  pending_parcels_);
  if (!is_request_valid) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  return IPCZ_RESULT_OK;
}

IpczResult Portal::Get(IpczGetFlags flags,
                       void* data,
                       size_t* num_data_bytes,
                       IpczHandle* handles,
                       size_t* num_handles,
                       IpczHandle* parcel) {
  return router_->GetNextInboundParcel(flags, data, num_data_bytes, handles,
                                       num_handles, parcel);
}

IpczResult Portal::BeginGet(const void** data,
                            size_t* num_data_bytes,
                            size_t* num_handles) {
  absl::MutexLock lock(&mutex_);
  if (in_two_phase_get_) {
    return IPCZ_RESULT_ALREADY_EXISTS;
  }

  if (router_->IsRouteDead()) {
    return IPCZ_RESULT_NOT_FOUND;
  }

  const IpczResult result =
      router_->BeginGetNextIncomingParcel(data, num_data_bytes, num_handles);
  if (result == IPCZ_RESULT_OK) {
    in_two_phase_get_ = true;
  }
  return result;
}

IpczResult Portal::CommitGet(size_t num_data_bytes_consumed,
                             absl::Span<IpczHandle> handles) {
  TrapEventDispatcher dispatcher;
  absl::MutexLock lock(&mutex_);
  if (!in_two_phase_get_) {
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  IpczResult result = router_->CommitGetNextIncomingParcel(
      num_data_bytes_consumed, handles, dispatcher);
  if (result == IPCZ_RESULT_OK) {
    in_two_phase_get_ = false;
  }
  return result;
}

IpczResult Portal::AbortGet() {
  absl::MutexLock lock(&mutex_);
  if (!in_two_phase_get_) {
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  in_two_phase_get_ = false;
  return IPCZ_RESULT_OK;
}

}  // namespace ipcz
