// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_util.h"

#include <wayland-client-protocol.h>

#include "base/run_loop.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace wl {

void SyncDisplay(wl_display* display_proxy, wl_display& display) {
  base::RunLoop run_loop;
  wl::Object<wl_callback> sync_callback(
      wl_display_sync(display_proxy ? display_proxy : &display));
  wl_callback_listener listener = {
      [](void* data, struct wl_callback* cb, uint32_t time) {
        static_cast<base::RunLoop*>(data)->Quit();
      }};
  wl_callback_add_listener(sync_callback.get(), &listener, &run_loop);
  wl_display_flush(&display);
  run_loop.Run();
}

}  // namespace wl
