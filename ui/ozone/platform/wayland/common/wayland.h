// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_H_

#include <wayland-client.h>

namespace wl {

template <typename T>
uint32_t get_version_of_object(T* obj) {
  return wl_proxy_get_version(reinterpret_cast<wl_proxy*>(obj));
}

template <typename T>
T* bind_registry(struct wl_registry* registry,
                 uint32_t name,
                 const struct wl_interface* interface,
                 uint32_t version) {
  return static_cast<T*>(wl_registry_bind(registry, name, interface, version));
}

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_H_
