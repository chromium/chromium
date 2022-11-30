// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_H_

#include <stddef.h>
#include <stdint.h>

#include <xf86drmMode.h>

#include <vector>

#include "base/containers/flat_set.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

class HardwareDisplayPlane {
 public:
  HardwareDisplayPlane(uint32_t id);

  HardwareDisplayPlane(const HardwareDisplayPlane&) = delete;
  HardwareDisplayPlane& operator=(const HardwareDisplayPlane&) = delete;

  virtual ~HardwareDisplayPlane();

  virtual bool Initialize(DrmDevice* drm);

  bool IsSupportedFormat(uint32_t format);

  std::vector<uint64_t> ModifiersForFormat(uint32_t format) const;

  bool CanUseForCrtcId(uint32_t crtc_id) const;

  // Adds trace records to |context|.
  void WriteIntoTrace(perfetto::TracedValue context) const;

  bool in_use() const { return in_use_; }
  void set_in_use(bool in_use) { in_use_ = in_use; }

  uint32_t id() const { return id_; }

  uint32_t type() const { return type_; }

  void set_owning_crtc(uint32_t crtc) { owning_crtc_ = crtc; }
  uint32_t owning_crtc() const { return owning_crtc_; }

  const std::vector<uint32_t>& supported_formats() const;

 protected:
  struct Properties {
    Properties();
    ~Properties();
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
    DrmDevice::Property plane_color_encoding;
    DrmDevice::Property plane_color_range;
  };

  const uint32_t id_;

  Properties properties_ = {};

  base::flat_set<uint32_t> possible_crtc_ids_;
  uint32_t owning_crtc_ = 0;
  uint32_t last_used_format_ = 0;
  bool in_use_ = false;
  uint32_t type_ = DRM_PLANE_TYPE_PRIMARY;
  std::vector<uint32_t> supported_formats_;
  std::vector<drm_format_modifier> supported_format_modifiers_;

  uint64_t color_encoding_bt601_;
  uint64_t color_range_limited_;

 private:
  void InitializeProperties(DrmDevice* drm);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_H_
