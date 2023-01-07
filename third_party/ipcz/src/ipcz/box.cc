// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/box.h"

#include <utility>

#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {

Box::Box(DriverObject object) : object_(std::move(object)) {}

Box::~Box() = default;

IpczResult Box::Close() {
  object_.reset();
  return IPCZ_RESULT_OK;
}

bool Box::CanSendFrom(Portal& sender) {
  return object_.is_valid() && object_.IsSerializable();
}

}  // namespace ipcz
