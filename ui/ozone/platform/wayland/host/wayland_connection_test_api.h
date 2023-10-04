// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_TEST_API_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_TEST_API_H_

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_cursor_shapes.h"

namespace ui {

// Allows tests to override private data in a WaylandConnection.
class WaylandConnectionTestApi {
 public:
  explicit WaylandConnectionTestApi(WaylandConnection* impl) : impl_(impl) {}
  WaylandConnectionTestApi(const WaylandConnectionTestApi&) = delete;
  WaylandConnectionTestApi& operator=(const WaylandConnectionTestApi&) = delete;
  ~WaylandConnectionTestApi() = default;

  void SetZcrCursorShapes(std::unique_ptr<WaylandZcrCursorShapes> obj) {
    impl_->zcr_cursor_shapes_ = std::move(obj);
  }

 private:
  const raw_ptr<WaylandConnection> impl_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_TEST_API_H_
