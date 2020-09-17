// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_CRTC_CONTROLLER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_CRTC_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

namespace ui {

class DrmDevice;

// Wrapper around a CRTC.
//
// One CRTC can be paired up with one or more connectors. The simplest
// configuration represents one CRTC driving one monitor, while pairing up a
// CRTC with multiple connectors results in hardware mirroring.
class CrtcController {
 public:
  CrtcController(const scoped_refptr<DrmDevice>& drm,
                 uint32_t crtc,
                 uint32_t connector);
  ~CrtcController();

  drmModeModeInfo mode() const { return mode_; }
  uint32_t crtc() const { return crtc_; }
  uint32_t connector() const { return connector_; }
  const scoped_refptr<DrmDevice>& drm() const { return drm_; }
  bool is_disabled() const { return is_disabled_; }

  // Calls the appropriate Plane Manager to perform the initial modesetting
  // operation using |plane| as the buffer for the primary plane. The CRTC
  // configuration is specified by |mode|.
  bool Modeset(const DrmOverlayPlane& plane,
               const drmModeModeInfo& mode,
               const ui::HardwareDisplayPlaneList& plane_list);

  // Disables the controller.
  bool Disable();

  bool AssignOverlayPlanes(HardwareDisplayPlaneList* plane_list,
                           const DrmOverlayPlaneList& planes,
                           bool is_modesetting);

  // Returns a vector of format modifiers for the given fourcc format
  // on this CRTCs primary plane. A format modifier describes the
  // actual layout of the buffer, such as whether it's linear, tiled
  // one way or another or maybe compressed. Except for generic
  // modifiers such as DRM_FORMAT_MOD_NONE (linear), the modifier
  // values are 64 bit values that we don't understand at this
  // level. We pass the modifers to gbm_bo_create_with_modifiers() and
  // gbm will pick a modifier as it allocates the bo.
  std::vector<uint64_t> GetFormatModifiers(uint32_t fourcc_format);

  void SetCursor(uint32_t handle, const gfx::Size& size);
  void MoveCursor(const gfx::Point& location);

  void OnPageFlipComplete();

 private:
  void DisableCursor();

  const scoped_refptr<DrmDevice> drm_;

  const uint32_t crtc_;

  // TODO(dnicoara) Add support for hardware mirroring (multiple connectors).
  const uint32_t connector_;

  drmModeModeInfo mode_ = {};

  scoped_refptr<DrmFramebuffer> modeset_framebuffer_;

  // Keeps track of the CRTC state. If a surface has been bound, then the value
  // is set to false. Otherwise it is true.
  bool is_disabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(CrtcController);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_CRTC_CONTROLLER_H_
