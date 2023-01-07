// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/global_object.h"

#include <algorithm>

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

void GlobalObject::Deleter::operator()(wl_global* global) {
  wl_global_destroy(global);
}

GlobalObject::GlobalObject(const wl_interface* interface,
                           const void* implementation,
                           uint32_t version)
    : interface_(interface),
      implementation_(implementation),
      version_(version) {}

GlobalObject::~GlobalObject() {}

bool GlobalObject::Initialize(wl_display* display) {
  global_.reset(wl_global_create(display, interface_, version_, this, &Bind));
  return global_ != nullptr;
}

void GlobalObject::DestroyGlobal() {
  global_.reset();
}

// static
void GlobalObject::Bind(wl_client* client,
                        void* data,
                        uint32_t version,
                        uint32_t id) {
  auto* global = static_cast<GlobalObject*>(data);
  wl_resource* resource = wl_resource_create(
      client, global->interface_, std::min(version, global->version_), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  if (!global->resource_)
    global->resource_ = resource;
  wl_resource_set_implementation(resource, global->implementation_, global,
                                 &GlobalObject::OnResourceDestroyed);
  global->OnBind();
}

// static
void GlobalObject::OnResourceDestroyed(wl_resource* resource) {
  auto* global = GetUserDataAs<GlobalObject>(resource);
  if (global->resource_ == resource)
    global->resource_ = nullptr;
}

}  // namespace wl
