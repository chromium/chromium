// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_shm.h"

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
}  // namespace

// static
constexpr char WaylandShm::kInterfaceName[];

// static
void WaylandShm::Instantiate(WaylandConnection* connection,
                             wl_registry* registry,
                             uint32_t name,
                             const std::string& interface,
                             uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  auto* buffer_factory = connection->buffer_factory();
  if (buffer_factory->wayland_shm_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto shm = wl::Bind<wl_shm>(registry, name, kMinVersion);
  if (!shm) {
    LOG(ERROR) << "Failed to bind to wl_shm global";
    return;
  }
  buffer_factory->wayland_shm_ =
      std::make_unique<WaylandShm>(shm.release(), connection);
}

WaylandShm::WaylandShm(wl_shm* shm, WaylandConnection* connection)
    : shm_(shm), connection_(connection) {}

WaylandShm::~WaylandShm() = default;

wl::Object<wl_buffer> WaylandShm::CreateBuffer(const base::ScopedFD& fd,
                                               size_t length,
                                               const gfx::Size& size,
                                               bool with_alpha_channel) {
  if (!fd.is_valid() || length == 0 || size.IsEmpty())
    return wl::Object<wl_buffer>(nullptr);

  wl::Object<wl_shm_pool> pool(
      wl_shm_create_pool(shm_.get(), fd.get(), length));
  if (!pool)
    return wl::Object<wl_buffer>(nullptr);

  const uint32_t format =
      with_alpha_channel ? WL_SHM_FORMAT_ARGB8888 : WL_SHM_FORMAT_XRGB8888;
  wl::Object<wl_buffer> shm_buffer(wl_shm_pool_create_buffer(
      pool.get(), 0, size.width(), size.height(), size.width() * 4, format));
  connection_->Flush();
  return shm_buffer;
}

}  // namespace ui
