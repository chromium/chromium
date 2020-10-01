// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_H_

#include <dlfcn.h>
// This header includes wayland-client-core.h and wayland-client-protocol.h
#include <wayland-client.h>

#include "base/notreached.h"

namespace wl {

template <typename T>
uint32_t get_version_of_object(T* obj) {
  if (dlsym(RTLD_DEFAULT, "wl_proxy_get_version"))
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
