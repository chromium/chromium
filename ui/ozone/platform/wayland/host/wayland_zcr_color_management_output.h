// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_MANAGEMENT_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_MANAGEMENT_OUTPUT_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space.h"

namespace ui {

// WaylandZcrColorManagementOutput tracks the color space of its associated
// Wayland Output.
class WaylandZcrColorManagementOutput {
 public:
  explicit WaylandZcrColorManagementOutput(
      WaylandOutput* wayland_output,
      zcr_color_management_output_v1* management_output);
  WaylandZcrColorManagementOutput(const WaylandZcrColorManagementOutput&) =
      delete;
  WaylandZcrColorManagementOutput& operator=(
      const WaylandZcrColorManagementOutput&) = delete;
  ~WaylandZcrColorManagementOutput();

  gfx::ColorSpace* gfx_color_space() const { return gfx_color_space_.get(); }
  WaylandZcrColorSpace* color_space() const { return color_space_.get(); }

 private:
  // zcr_color_management_output_v1_listener callbacks:
  static void OnColorSpaceChanged(void* data,
                                  zcr_color_management_output_v1* cmo);
  static void OnExtendedDynamicRange(void* data,
                                     zcr_color_management_output_v1* cmo,
                                     uint32_t value);

  void OnColorSpaceDone(const gfx::ColorSpace& color_space);

  const raw_ptr<WaylandOutput> wayland_output_;
  wl::Object<zcr_color_management_output_v1> zcr_color_management_output_;
  std::unique_ptr<gfx::ColorSpace> gfx_color_space_;
  scoped_refptr<WaylandZcrColorSpace> color_space_;
  base::WeakPtrFactory<WaylandZcrColorManagementOutput> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZCR_COLOR_MANAGEMENT_OUTPUT_H_
