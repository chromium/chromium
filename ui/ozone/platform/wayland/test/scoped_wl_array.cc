// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/scoped_wl_array.h"

#include <wayland-server-core.h>

namespace wl {

ScopedWlArray::ScopedWlArray(const std::vector<int32_t> states) {
  wl_array_init(&array_);
  for (const auto& state : states)
    AddStateToWlArray(state);
}

ScopedWlArray::ScopedWlArray(const ScopedWlArray& rhs) {
  wl_array_init(&array_);
  wl_array_copy(&array_, const_cast<wl_array*>(&rhs.array_));
}

ScopedWlArray::ScopedWlArray(ScopedWlArray&& rhs) {
  array_ = rhs.array_;
  // wl_array_init sets rhs.array_'s fields to nullptr, so that
  // the free() in wl_array_release() is a no-op.
  wl_array_init(&rhs.array_);
}

ScopedWlArray& ScopedWlArray::operator=(ScopedWlArray&& rhs) {
  wl_array_release(&array_);
  array_ = rhs.array_;
  // wl_array_init sets rhs.array_'s fields to nullptr, so that
  // the free() in wl_array_release() is a no-op.
  wl_array_init(&rhs.array_);
  return *this;
}

ScopedWlArray::~ScopedWlArray() {
  wl_array_release(&array_);
}

void ScopedWlArray::AddStateToWlArray(uint32_t state) {
  *static_cast<uint32_t*>(wl_array_add(&array_, sizeof(uint32_t))) = state;
}

}  // namespace wl
