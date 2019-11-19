// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper.h"

namespace ui {

bool CheckIfWlArrayHasValue(struct wl_array* wl_array, uint32_t value) {
  // wl_array_for_each has a bug in upstream. It tries to assign void* to
  // uint32_t *, which is not allowed in C++. Explicit cast should be
  // performed. In other words, one just cannot assign void * to other pointer
  // type implicitly in C++ as in C. We can't modify wayland-util.h, because
  // it is fetched with gclient sync. Thus, use own loop.
  uint32_t* data = reinterpret_cast<uint32_t*>(wl_array->data);
  size_t array_size = wl_array->size / sizeof(uint32_t);
  for (size_t i = 0; i < array_size; i++) {
    if (data[i] == value)
      return true;
  }
  return false;
}

}  // namespace ui
