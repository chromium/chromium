// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_PARCEL_WRAPPER_
#define IPCZ_SRC_IPCZ_PARCEL_WRAPPER_

#include <cstddef>

#include "ipcz/api_object.h"
#include "ipcz/ipcz.h"
#include "ipcz/parcel.h"
#include "util/ref_counted.h"

namespace ipcz {

// A parcel wrapper owns a Parcel received from a portal, after the Parcel has
// been retrieved from the portal by the application.
//
// Applications can use these objects to perform two-phase reads of parcels
// without blocking the receiving portal, and to report their own
// application-level validation failures to ipcz via the Reject() API.
class ParcelWrapper : public APIObjectImpl<ParcelWrapper, APIObject::kParcel> {
 public:
  explicit ParcelWrapper(Parcel parcel);

  Parcel& parcel() { return parcel_; }

  // APIObject:
  IpczResult Close() override;

  // Signals application-level rejection of this parcel. `context` is an opaque
  // value passed by the application and propagated to the driver when
  // appropriate. See the Reject() API.
  IpczResult Reject(uintptr_t context);

  IpczResult Get(IpczGetFlags flags,
                 void* data,
                 size_t* num_data_bytes,
                 IpczHandle* handles,
                 size_t* num_handles);
  IpczResult BeginGet(const void** data,
                      size_t* num_bytes,
                      size_t* num_handles);
  IpczResult CommitGet(size_t num_data_bytes_consumed,
                       absl::Span<IpczHandle> handles);
  IpczResult AbortGet();

 private:
  ~ParcelWrapper() override;

  Parcel parcel_;
  bool in_two_phase_get_ = false;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_PARCEL_WRAPPER_
