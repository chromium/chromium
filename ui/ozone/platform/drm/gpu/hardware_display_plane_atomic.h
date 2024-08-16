// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_ATOMIC_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_ATOMIC_H_

#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"

#include <stdint.h>
#include <xf86drmMode.h>

namespace gfx {
class ColorSpace;
class Rect;
}  // namespace gfx

namespace ui {

class HardwareDisplayPlaneAtomic : public HardwareDisplayPlane {
 public:
  explicit HardwareDisplayPlaneAtomic(uint32_t id);

  HardwareDisplayPlaneAtomic(const HardwareDisplayPlaneAtomic&) = delete;
  HardwareDisplayPlaneAtomic& operator=(const HardwareDisplayPlaneAtomic&) =
      delete;

  ~HardwareDisplayPlaneAtomic() override;

  bool Initialize(DrmDevice* drm) override;

  // Saves the props locally onto the plane to be committed later.
  virtual bool AssignPlaneProps(DrmDevice* drm,
                                uint32_t crtc_id,
                                uint32_t framebuffer,
                                const gfx::Rect& crtc_rect,
                                const gfx::Rect& src_rect,
                                const gfx::Rect& damage_rect,
                                const gfx::OverlayTransform transform,
                                const gfx::ColorSpace& color_space,
                                int in_fence_fd,
                                uint32_t format_fourcc,
                                bool is_original_buffer);
  // Sets the props on |property_set| for commit.
  bool SetPlaneProps(drmModeAtomicReq* property_set);

  // Saves the props to disable the plane locally to be committed later.
  void AssignDisableProps();

  uint32_t AssignedCrtcId() const;

 private:
  // Intermediate variable between Assign()ment and Set()ting.
  HardwareDisplayPlane::Properties assigned_props_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_ATOMIC_H_
