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

ParcelWrapper::ParcelWrapper(Parcel parcel) : parcel_(std::move(parcel)) {}

ParcelWrapper::~ParcelWrapper() = default;

IpczResult ParcelWrapper::Close() {
  return IPCZ_RESULT_OK;
}

IpczResult ParcelWrapper::Reject(uintptr_t context) {
  const Ref<NodeLink>& remote_source = parcel_.remote_source();
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
                              size_t* num_handles) {
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
                               ? std::min(parcel_.data_size(), data_capacity)
                               : parcel_.data_size();
  const size_t handles_size =
      allow_partial ? std::min(parcel_.num_objects(), handles_capacity)
                    : parcel_.num_objects();
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

  memcpy(data, parcel_.data_view().data(), data_size);
  parcel_.Consume(data_size, absl::MakeSpan(handles, handles_size));
  return IPCZ_RESULT_OK;
}

IpczResult ParcelWrapper::BeginGet(const void** data,
                                   size_t* num_data_bytes,
                                   size_t* num_handles) {
  if (in_two_phase_get_) {
    return IPCZ_RESULT_ALREADY_EXISTS;
  }

  if (data) {
    *data = parcel_.data_view().data();
  }
  if (num_data_bytes) {
    *num_data_bytes = parcel_.data_size();
  }
  if (num_handles) {
    *num_handles = parcel_.num_objects();
  }
  if ((parcel_.data_size() && (!data || !num_data_bytes)) ||
      (parcel_.num_objects() && !num_handles)) {
    return IPCZ_RESULT_RESOURCE_EXHAUSTED;
  }

  in_two_phase_get_ = true;
  return IPCZ_RESULT_OK;
}

IpczResult ParcelWrapper::CommitGet(size_t num_data_bytes_consumed,
                                    absl::Span<IpczHandle> handles) {
  if (!in_two_phase_get_) {
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  if (num_data_bytes_consumed > parcel_.data_size() ||
      handles.size() > parcel_.num_objects()) {
    return IPCZ_RESULT_OUT_OF_RANGE;
  }

  parcel_.Consume(num_data_bytes_consumed, handles);
  in_two_phase_get_ = false;
  return IPCZ_RESULT_OK;
}

IpczResult ParcelWrapper::AbortGet() {
  if (!in_two_phase_get_) {
    return IPCZ_RESULT_FAILED_PRECONDITION;
  }

  in_two_phase_get_ = false;
  return IPCZ_RESULT_OK;
}

}  // namespace ipcz
