// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/blob.h"

#include <iterator>

#include "util/ref_counted.h"

namespace ipcz::reference_drivers {

Blob::RefCountedFlag::RefCountedFlag() = default;

Blob::RefCountedFlag::~RefCountedFlag() = default;

Blob::Blob(const IpczDriver& driver,
           std::string_view message,
           absl::Span<IpczDriverHandle> handles)
    : driver_(driver),
      message_(message),
      handles_(std::move_iterator(handles.begin()),
               std::move_iterator(handles.end())) {}

Blob::~Blob() {
  for (IpczDriverHandle handle : handles_) {
    driver_.Close(handle, IPCZ_NO_FLAGS, nullptr);
  }
}

IpczResult Blob::Close() {
  destruction_flag_for_testing_->set(true);
  return IPCZ_RESULT_OK;
}

// static
Blob* Blob::FromHandle(IpczDriverHandle handle) {
  Object* object = Object::FromHandle(handle);
  if (!object || object->type() != kBlob) {
    return nullptr;
  }

  return static_cast<Blob*>(object);
}

// static
Ref<Blob> Blob::TakeFromHandle(IpczDriverHandle handle) {
  return AdoptRef(FromHandle(handle));
}

}  // namespace ipcz::reference_drivers
