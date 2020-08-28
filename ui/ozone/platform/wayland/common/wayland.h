// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_H_

// This header includes wayland-client-core.h and wayland-client-protocol.h
#include <wayland-client.h>

#include "base/notreached.h"

#define WEAK_WAYLAND_FN(x) extern "C" __attribute__((weak)) decltype(x) x

// These functions are used by wl_proxy_get_version and wl_registry_bind.
// However, they were introduced in libwayland 1.10, and if we run Chromium with
// older library, these symbols are undefined. Thus, check their availability
// and use some older API.
//
// TODO(msisov): Remove these once support for Ubuntu Trusty is dropped.
WEAK_WAYLAND_FN(wl_proxy_marshal_constructor_versioned);
WEAK_WAYLAND_FN(wl_proxy_get_version);

namespace wl {

template <typename T>
uint32_t get_version_of_object(T* obj) {
  if (wl_proxy_get_version)
    return wl_proxy_get_version(reinterpret_cast<wl_proxy*>(obj));
  // Older version of the libwayland-client didn't support version of objects.
  return 0;
}

void* bind_registry(struct wl_registry* registry,
                    uint32_t name,
                    const struct wl_interface* interface,
                    uint32_t version);

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_H_
