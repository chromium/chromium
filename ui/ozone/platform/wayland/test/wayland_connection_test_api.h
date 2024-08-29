// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_CONNECTION_TEST_API_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_CONNECTION_TEST_API_H_

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_shape.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_cursor_shapes.h"

namespace ui {

// Allows tests to get and override internal implementation details of
// WaylandConnection.
class WaylandConnectionTestApi {
 public:
  explicit WaylandConnectionTestApi(WaylandConnection* impl);
  WaylandConnectionTestApi(const WaylandConnectionTestApi&) = delete;
  WaylandConnectionTestApi& operator=(const WaylandConnectionTestApi&) = delete;
  ~WaylandConnectionTestApi() = default;

  void SetCursorShape(std::unique_ptr<WaylandCursorShape> obj);

  void SetZcrCursorShapes(std::unique_ptr<WaylandZcrCursorShapes> obj);

  // Sets up a sync callback via wl_display.sync and waits until it's received.
  // Requests are handled in-order and events are delivered in-order, thus sync
  // is used as a barrier to ensure all previous requests and the resulting
  // events have been handled.
  void SyncDisplay();

 private:
  const raw_ptr<WaylandConnection> impl_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_CONNECTION_TEST_API_H_
