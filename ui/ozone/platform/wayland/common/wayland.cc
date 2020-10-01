// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/common/wayland.h"

namespace wl {

void* bind_registry(struct wl_registry* registry,
                    uint32_t name,
                    const struct wl_interface* interface,
                    uint32_t version) {
  if (dlsym(RTLD_DEFAULT, "wl_proxy_marshal_constructor_versioned")) {
    return wl_registry_bind(registry, name, interface, version);
  } else {
    return wl_proxy_marshal_constructor(reinterpret_cast<wl_proxy*>(registry),
                                        WL_REGISTRY_BIND, interface, name,
                                        interface->name, version, nullptr);
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace wl
