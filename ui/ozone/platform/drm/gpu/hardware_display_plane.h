// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_H_

#include <stddef.h>
#include <stdint.h>

#include <xf86drmMode.h>

#include <vector>

#include "base/macros.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

class HardwareDisplayPlane {
 public:
  enum Type { kDummy, kPrimary, kOverlay, kCursor };

  HardwareDisplayPlane(uint32_t id);

  virtual ~HardwareDisplayPlane();

  virtual bool Initialize(DrmDevice* drm);

  bool IsSupportedFormat(uint32_t format);

  std::vector<uint64_t> ModifiersForFormat(uint32_t format) const;

  bool CanUseForCrtc(uint32_t crtc_index) const;

  bool in_use() const { return in_use_; }
  void set_in_use(bool in_use) { in_use_ = in_use; }

  uint32_t id() const { return id_; }

  Type type() const { return type_; }
  void set_type(const Type type) { type_ = type; }

  void set_owning_crtc(uint32_t crtc) { owning_crtc_ = crtc; }
  uint32_t owning_crtc() const { return owning_crtc_; }

  const std::vector<uint32_t>& supported_formats() const;

 protected:
  struct Properties {
    // These properties are mandatory on DRM atomic. On legacy they may or may
    // not be present.
    DrmDevice::Property crtc_id;
    DrmDevice::Property crtc_x;
    DrmDevice::Property crtc_y;
    DrmDevice::Property crtc_w;
    DrmDevice::Property crtc_h;
    DrmDevice::Property fb_id;
    DrmDevice::Property src_x;
    DrmDevice::Property src_y;
    DrmDevice::Property src_w;
    DrmDevice::Property src_h;
    DrmDevice::Property type;

    // Optional properties.
    DrmDevice::Property rotation;
    DrmDevice::Property in_formats;
    DrmDevice::Property in_fence_fd;
    DrmDevice::Property plane_ctm;
  };

  const uint32_t id_;
  uint32_t crtc_mask_ = 0;

  Properties properties_ = {};

  uint32_t owning_crtc_ = 0;
  uint32_t last_used_format_ = 0;
  bool in_use_ = false;
  Type type_ = kPrimary;
  std::vector<uint32_t> supported_formats_;
  std::vector<drm_format_modifier> supported_format_modifiers_;

 private:
  void InitializeProperties(DrmDevice* drm);

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlane);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_H_
