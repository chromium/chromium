// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_VALIDATOR_H_
#define IPCZ_SRC_IPCZ_VALIDATOR_H_

#include "ipcz/validator.h"

#include "ipcz/api_object.h"
#include "util/ref_counted.h"

namespace ipcz {

class NodeLink;

// A validator object retains context associated with a specific inbound parcel.
// Applications can use these objects to report their own application-level
// validation failures to ipcz, and ipcz can use the context within to propagate
// the failure out to an appropriate driver transport.
class Validator : public APIObjectImpl<Validator, APIObject::kValidator> {
 public:
  explicit Validator(Ref<NodeLink> remote_source);

  // APIObject:
  IpczResult Close() override;

  // Signals application-level rejection of whatever this validator is
  // associated with. `context` is an opaque value passed by the application
  // and propagated to the driver when appropriate. See the Reject() API.
  IpczResult Reject(uintptr_t context);

 private:
  ~Validator() override;

  // The remote source which sent the parcel to the local node. If this is null,
  // the parcel originated from the local node.
  const Ref<NodeLink> remote_source_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_VALIDATOR_H_
