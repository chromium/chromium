// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_SCOPED_WL_ARRAY_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_SCOPED_WL_ARRAY_H_

#include <wayland-server-core.h>

#include <vector>

namespace wl {

class ScopedWlArray {
 public:
  explicit ScopedWlArray(const std::vector<int32_t> states);
  ScopedWlArray(const ScopedWlArray& rhs);
  ScopedWlArray(ScopedWlArray&& rhs);
  ScopedWlArray& operator=(ScopedWlArray&& rhs);
  ~ScopedWlArray();

  wl_array* get() { return &array_; }

  void AddStateToWlArray(uint32_t state);

 private:
  wl_array array_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_SCOPED_WL_ARRAY_H_
