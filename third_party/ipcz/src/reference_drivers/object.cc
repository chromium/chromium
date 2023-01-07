// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/object.h"

#include "ipcz/ipcz.h"

namespace ipcz::reference_drivers {

Object::Object(Type type) : type_(type) {}

Object::~Object() = default;

IpczResult Object::Close() {
  return IPCZ_RESULT_OK;
}

}  // namespace ipcz::reference_drivers
