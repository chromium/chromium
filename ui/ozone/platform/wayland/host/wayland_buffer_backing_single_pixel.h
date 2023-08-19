// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_SINGLE_PIXEL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_SINGLE_PIXEL_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing.h"

namespace ui {

// Manager of single pixel solid color wl_buffers.
class WaylandBufferBackingSinglePixel : public WaylandBufferBacking {
 public:
  WaylandBufferBackingSinglePixel() = delete;
  WaylandBufferBackingSinglePixel(const WaylandBufferBackingSinglePixel&) =
      delete;
  WaylandBufferBackingSinglePixel& operator=(
      const WaylandBufferBackingSinglePixel&) = delete;
  WaylandBufferBackingSinglePixel(const WaylandConnection* connection,
                                  SkColor4f color,
                                  uint32_t buffer_id);
  ~WaylandBufferBackingSinglePixel() override;

 private:
  // WaylandBufferBacking override:
  void RequestBufferHandle(
      base::OnceCallback<void(wl::Object<wl_buffer>)> callback) override;
  BufferBackingType GetBackingType() const override;

  SkColor4f color_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_SINGLE_PIXEL_H_
