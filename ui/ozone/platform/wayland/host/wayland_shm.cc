// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_shm.h"

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;

// Given that buffers for canvas surfaces are submitted with alpha disabled,
// using a format with alpha channel results in popup surfaces that have black
// background when they are shown with fade out animation. Thus, disable this
// channel so that exo sets the background of these canvas surface transparent.
constexpr uint32_t kShmFormat =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    WL_SHM_FORMAT_XRGB8888;
#else
    WL_SHM_FORMAT_ARGB8888;
#endif
}  // namespace

// static
constexpr char WaylandShm::kInterfaceName[];

// static
void WaylandShm::Instantiate(WaylandConnection* connection,
                             wl_registry* registry,
                             uint32_t name,
                             const std::string& interface,
                             uint32_t version) {
  DCHECK_EQ(interface, kInterfaceName);

  if (connection->shm_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto shm = wl::Bind<wl_shm>(registry, name, kMinVersion);
  if (!shm) {
    LOG(ERROR) << "Failed to bind to wl_shm global";
    return;
  }
  connection->shm_ = std::make_unique<WaylandShm>(shm.release(), connection);
}

WaylandShm::WaylandShm(wl_shm* shm, WaylandConnection* connection)
    : shm_(shm), connection_(connection) {}

WaylandShm::~WaylandShm() = default;

wl::Object<wl_buffer> WaylandShm::CreateBuffer(const base::ScopedFD& fd,
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
