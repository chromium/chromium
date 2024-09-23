// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/wayland_connection_test_api.h"

#include "base/run_loop.h"

namespace ui {

WaylandConnectionTestApi::WaylandConnectionTestApi(WaylandConnection* impl)
    : impl_(impl) {}

void WaylandConnectionTestApi::SetCursorShape(
    std::unique_ptr<WaylandCursorShape> obj) {
  impl_->cursor_shape_ = std::move(obj);
}

void WaylandConnectionTestApi::SetZcrCursorShapes(
    std::unique_ptr<WaylandZcrCursorShapes> obj) {
  impl_->zcr_cursor_shapes_ = std::move(obj);
}

void WaylandConnectionTestApi::SyncDisplay() {
  base::RunLoop run_loop;
  wl::Object<wl_callback> sync_callback(
      wl_display_sync(impl_->display_wrapper()));
  wl_callback_listener listener = {
      [](void* data, struct wl_callback* cb, uint32_t time) {
        static_cast<base::RunLoop*>(data)->Quit();
      }};
  wl_callback_add_listener(sync_callback.get(), &listener, &run_loop);
  wl_display_flush(impl_->display());
  run_loop.Run();
}
}  // namespace ui
