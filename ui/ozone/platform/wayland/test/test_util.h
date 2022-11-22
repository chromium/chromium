// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_UTIL_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_UTIL_H_

struct wl_display;

namespace wl {

// Sets up a sync callback via wl_display.sync and waits until it's received.
// Requests are handled in-order and events are delivered in-order, thus sync
// is used as a barrier to ensure all previous requests and the resulting
// events have been handled. A client may choose whether it uses a proxy wrapper
// or an original display object to create a sync object. If |display_proxy| is
// null, a callback is created for the original display object. In other words,
// a default event queue is used.
void SyncDisplay(wl_display* display_proxy, wl_display& display);

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_UTIL_H_
