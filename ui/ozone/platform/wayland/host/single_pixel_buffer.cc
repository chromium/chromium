// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/single_pixel_buffer.h"

#include <single-pixel-buffer-v1-client-protocol.h>
#include <wayland-util.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kVersion = 1;
}

// static
constexpr char SinglePixelBuffer::kInterfaceName[];

// static
void SinglePixelBuffer::Instantiate(WaylandConnection* connection,
                                    wl_registry* registry,
                                    uint32_t name,
                                    const std::string& interface,
                                    uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->single_pixel_buffer_ ||
      !wl::CanBind(interface, version, kVersion, kVersion)) {
    return;
  }

  auto single_pixel_buffer =
      wl::Bind<wp_single_pixel_buffer_manager_v1>(registry, name, kVersion);
  if (!single_pixel_buffer) {
    LOG(ERROR) << "Failed to bind surface_augmenter";
    return;
  }
  connection->single_pixel_buffer_ = std::make_unique<SinglePixelBuffer>(
      single_pixel_buffer.release(), connection);
}

SinglePixelBuffer::SinglePixelBuffer(
    wp_single_pixel_buffer_manager_v1* single_pixel_buffer,
    WaylandConnection* connection)
    : single_pixel_buffer_(single_pixel_buffer) {}

SinglePixelBuffer::~SinglePixelBuffer() = default;

wl::Object<wl_buffer> SinglePixelBuffer::CreateSinglePixelBuffer(
    const SkColor4f& color) {
  // Single Pixel Buffer protocol uses premultiplied color.
  uint32_t red = UINT_MAX * (double)color.fR * (double)color.fA;
  uint32_t green = UINT_MAX * (double)color.fG * (double)color.fA;
  uint32_t blue = UINT_MAX * (double)color.fB * (double)color.fA;
  uint32_t alpha = UINT_MAX * (double)color.fA;

  auto buffer = wl::Object<wl_buffer>(
      wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(
          single_pixel_buffer_.get(), red, green, blue, alpha));
  return buffer;
}

}  // namespace ui
