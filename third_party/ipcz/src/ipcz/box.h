// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_BOX_
#define IPCZ_SRC_IPCZ_BOX_

#include "ipcz/api_object.h"
#include "ipcz/driver_object.h"

namespace ipcz {

// Generic handle wrapper around a DriverObject, allowing driver objects to be
// passed wherever IpczHandles are accepted. More to the point, this allows
// serializable driver objects to be transferred through portals.
class Box : public APIObjectImpl<Box, APIObject::kBox> {
 public:
  explicit Box(DriverObject object);

  DriverObject& object() { return object_; }

  // APIObject:
  IpczResult Close() override;
  bool CanSendFrom(Portal& sender) override;

 private:
  ~Box() override;

  DriverObject object_;
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_BOX_
