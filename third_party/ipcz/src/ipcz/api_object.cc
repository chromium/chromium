// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/api_object.h"

namespace ipcz {

APIObject::APIObject(ObjectType type) : type_(type) {}

APIObject::~APIObject() = default;

bool APIObject::CanSendFrom(Portal& sender) {
  return false;
}

}  // namespace ipcz
