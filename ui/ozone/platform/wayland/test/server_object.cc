// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

bool ResourceHasImplementation(wl_resource* resource,
                               const wl_interface* interface,
                               const void* impl) {
  return wl_resource_instance_of(resource, interface, impl);
}

void DestroyResource(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

ServerObject::ServerObject(wl_resource* resource) : resource_(resource) {}

ServerObject::~ServerObject() {
  if (resource_)
    wl_resource_destroy(resource_);
}

// static
void ServerObject::OnResourceDestroyed(wl_resource* resource) {
  auto* obj = GetUserDataAs<ServerObject>(resource);
  obj->resource_ = nullptr;
}

}  // namespace wl
