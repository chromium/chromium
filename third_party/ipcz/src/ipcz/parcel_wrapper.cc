// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/parcel_wrapper.h"

#include "ipcz/driver_object.h"
#include "ipcz/driver_transport.h"
#include "ipcz/ipcz.h"
#include "ipcz/node.h"
#include "ipcz/node_link.h"
#include "util/ref_counted.h"

namespace ipcz {

ParcelWrapper::ParcelWrapper(std::unique_ptr<Parcel> parcel)
    : parcel_(std::move(parcel)) {}

ParcelWrapper::~ParcelWrapper() = default;

IpczResult ParcelWrapper::Close() {
  return IPCZ_RESULT_OK;
}

IpczResult ParcelWrapper::Reject(uintptr_t context) {
  const Ref<NodeLink>& remote_source = parcel_->remote_source();
  if (!remote_source) {
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  const IpczDriver& driver = remote_source->node()->driver();
  const Ref<DriverTransport>& transport = remote_source->transport();
  driver.ReportBadTransportActivity(transport->driver_object().handle(),
                                    context, IPCZ_NO_FLAGS, nullptr);
  return IPCZ_RESULT_OK;
}

IpczResult ParcelWrapper::Get(IpczGetFlags flags,
                              void* data,
                              size_t* num_bytes,
                              IpczHandle* handles,
                              size_t* num_handles,
                              IpczHandle* parcel) {
  if (in_two_phase_get_) {
    return IPCZ_RESULT_ALREADY_EXISTS;
  }

  const bool allow_partial = (flags & IPCZ_GET_PARTIAL) != 0;
  const size_t data_capacity = num_bytes ? *num_bytes : 0;
  const size_t handles_capacity = num_handles ? *num_handles : 0;
  if ((data_capacity && !data) || (handles_capacity && !handles)) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  const size_t data_size = allow_partial
                               ? std::min(parcel_->data_size(), data_capacity)
                               : parcel_->data_size();
  const size_t handles_size =
      allow_partial ? std::min(parcel_->num_objects(), handles_capacity)
                    : parcel_->num_objects();
  if (num_bytes) {
    *num_bytes = data_size;
  }
  if (num_handles) {
    *num_handles = handles_size;
  }

  const bool consuming_whole_parcel =
      (data_capacity >= data_size && handles_capacity >= handles_size);
  if (!consuming_whole_parcel && !allow_partial) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  memcpy(data, parcel_->data_view().data(), data_size);
  parcel_->ConsumeHandles(absl::MakeSpan(handles, handles_size));

  if (parcel) {
    // Allow the caller to acquire another handle to this wrapper. Not
    // particularly useful, but consistent with Router::Get().
    *parcel = APIObject::ReleaseAsHandle(WrapRefCounted(this));
  }

  return IPCZ_RESULT_OK;
}

IpczResult ParcelWrapper::BeginGet(IpczBeginGetFlags flags,
                                   const volatile void** data,
                                   size_t* num_data_bytes,
                                   IpczHandle* handles,
                                   size_t* num_handles,
                                   IpczTransaction* transaction) {
  if (flags & IPCZ_BEGIN_GET_OVERLAPPED) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (num_handles && *num_handles && !handles) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  if (in_two_phase_get_) {
    return IPCZ_RESULT_ALREADY_EXISTS;
  }

  if (data) {
    *data = parcel_->data_view().data();
  }
  if (num_data_bytes) {
    *num_data_bytes = parcel_->data_size();
  }

  const bool allow_partial = flags & IPCZ_BEGIN_GET_PARTIAL;
  const size_t handle_capacity = num_handles ? *num_handles : 0;
  const size_t num_objects = parcel_->num_objects();
  const size_t num_handles_to_consume = std::min(num_objects, handle_capacity);
  if (num_handles_to_consume < num_objects && !allow_partial) {
    if (num_handles) {
      *num_handles = num_objects;
    }
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  if (num_handles) {
    *num_handles = num_handles_to_consume;
  }
  parcel_->ConsumeHandles(absl::MakeSpan(handles, num_handles_to_consume));

  *transaction = reinterpret_cast<IpczTransaction>(parcel_.get());
  in_two_phase_get_ = true;
  return IPCZ_RESULT_OK;
}

IpczResult ParcelWrapper::EndGet(IpczTransaction transaction,
                                 IpczEndGetFlags flags,
                                 IpczHandle* parcel) {
  if (reinterpret_cast<IpczTransaction>(parcel_.get()) != transaction ||
      !in_two_phase_get_) {
    return IPCZ_RESULT_INVALID_ARGUMENT;
  }

  in_two_phase_get_ = false;

  if (parcel) {
    *parcel = APIObject::ReleaseAsHandle(WrapRefCounted(this));
  }
  return IPCZ_RESULT_OK;
}

}  // namespace ipcz
