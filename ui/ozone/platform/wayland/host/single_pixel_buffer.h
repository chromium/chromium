// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SINGLE_PIXEL_BUFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SINGLE_PIXEL_BUFFER_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class Size;
}

namespace ui {

class WaylandConnection;

// Wraps the single-pixel-buffer, which is provided via
// single_pixel_buffer interface.
class SinglePixelBuffer : public wl::GlobalObjectRegistrar<SinglePixelBuffer> {
 public:
  static constexpr char kInterfaceName[] = "wp_single_pixel_buffer_manager_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  SinglePixelBuffer(
      wp_single_pixel_buffer_manager_v1* single_pixel_buffer,
      WaylandConnection* connection);
  SinglePixelBuffer(const SinglePixelBuffer&) = delete;
  SinglePixelBuffer& operator=(const SinglePixelBuffer&) = delete;
  ~SinglePixelBuffer();

  wl::Object<wl_buffer> CreateSinglePixelBuffer(const SkColor4f& color);

 private:
  // Wayland object wrapped by this class.
  wl::Object<wp_single_pixel_buffer_manager_v1> single_pixel_buffer_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SINGLE_PIXEL_BUFFER_H_
