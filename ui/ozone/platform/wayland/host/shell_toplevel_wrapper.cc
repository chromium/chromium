// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/shell_toplevel_wrapper.h"

#include "base/containers/contains.h"

namespace ui {

XDGToplevelWrapperImpl* ShellToplevelWrapper::AsXDGToplevelWrapper() {
  return nullptr;
}

bool CheckIfWlArrayHasValue(struct wl_array* wl_array, uint32_t value) {
  // wl_array_for_each has a bug in upstream. It tries to assign void* to
  // uint32_t *, which is not allowed in C++. Explicit cast should be
  // performed. In other words, one just cannot assign void * to other pointer
  // type implicitly in C++ as in C. We can't modify wayland-util.h, because
  // it is fetched with gclient sync. Thus, use own loop.

  // SAFETY: Wayland ensures that `wl_array->data` and `wl_array->size`
  // correspond to a valid buffer. The contents are additionally
  // guaranteed to be sufficiently aligned for `uint32_t` because
  // `wl_array` contents are always allocated with `malloc`.
  auto span =
      UNSAFE_BUFFERS(base::span(reinterpret_cast<uint32_t*>(wl_array->data),
                                wl_array->size / sizeof(uint32_t)));
  return base::Contains(span, value);
}

}  // namespace ui
