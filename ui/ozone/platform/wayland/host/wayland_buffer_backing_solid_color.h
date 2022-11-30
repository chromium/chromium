// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_SOLID_COLOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_SOLID_COLOR_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing.h"

namespace ui {

// Manager of non-backed (that is, gpu side doesn't create any backings) solid
// color wl_buffers.
class WaylandBufferBackingSolidColor : public WaylandBufferBacking {
 public:
  WaylandBufferBackingSolidColor() = delete;
  WaylandBufferBackingSolidColor(const WaylandBufferBackingSolidColor&) =
      delete;
  WaylandBufferBackingSolidColor& operator=(
      const WaylandBufferBackingSolidColor&) = delete;
  WaylandBufferBackingSolidColor(const WaylandConnection* connection,
                                 SkColor4f color,
                                 const gfx::Size& size,
                                 uint32_t buffer_id);
  ~WaylandBufferBackingSolidColor() override;

 private:
  // WaylandBufferBacking override:
  void RequestBufferHandle(
      base::OnceCallback<void(wl::Object<wl_buffer>)> callback) override;
  BufferBackingType GetBackingType() const override;

  SkColor4f color_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_SOLID_COLOR_H_
