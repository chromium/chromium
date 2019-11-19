// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_shm.h"

#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {

constexpr uint32_t kShmFormat = WL_SHM_FORMAT_ARGB8888;

}  // namespace

WaylandShm::WaylandShm(wl_shm* shm, WaylandConnection* connection)
    : shm_(shm), connection_(connection) {}

WaylandShm::~WaylandShm() = default;

wl::Object<wl_buffer> WaylandShm::CreateBuffer(base::ScopedFD fd,
                                               size_t length,
                                               const gfx::Size& size) {
  if (!fd.is_valid() || length == 0 || size.IsEmpty())
    return wl::Object<wl_buffer>(nullptr);

  wl::Object<wl_shm_pool> pool(
      wl_shm_create_pool(shm_.get(), fd.get(), length));
  if (!pool)
    return wl::Object<wl_buffer>(nullptr);

  wl::Object<wl_buffer> shm_buffer(
      wl_shm_pool_create_buffer(pool.get(), 0, size.width(), size.height(),
                                size.width() * 4, kShmFormat));

  connection_->ScheduleFlush();
  return shm_buffer;
}

}  // namespace ui
