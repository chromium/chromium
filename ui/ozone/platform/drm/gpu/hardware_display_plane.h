// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_H_

#include <stddef.h>
#include <stdint.h>

#include <xf86drmMode.h>

#include <cstdint>
#include <vector>

#include "base/containers/flat_set.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/ozone/platform/drm/common/drm_wrapper.h"

namespace ui {

class DrmDevice;

class HardwareDisplayPlane {
 public:
  explicit HardwareDisplayPlane(uint32_t id);

  HardwareDisplayPlane(const HardwareDisplayPlane&) = delete;
  HardwareDisplayPlane& operator=(const HardwareDisplayPlane&) = delete;

  virtual ~HardwareDisplayPlane();

  virtual bool Initialize(DrmDevice* drm);

  bool IsSupportedFormat(uint32_t format);

  std::vector<uint64_t> ModifiersForFormat(uint32_t format) const;

  const base::flat_set<uint32_t>& GetCompatibleCrtcIds() const {
    return possible_crtc_ids_;
  }
  bool CanUseForCrtcId(uint32_t crtc_id) const;

  // Adds trace records to |context|.
  void WriteIntoTrace(perfetto::TracedValue context) const;

  std::ostream& DumpProperties(std::ostream& out) const;

  bool in_use() const { return in_use_; }
  void set_in_use(bool in_use) { in_use_ = in_use; }

  uint32_t id() const { return id_; }

  uint32_t type() const { return type_; }

  void set_owning_crtc(uint32_t crtc) { owning_crtc_ = crtc; }
  uint32_t owning_crtc() const { return owning_crtc_; }

  const std::vector<uint32_t>& supported_formats() const;
  const std::vector<gfx::Size>& supported_cursor_sizes() const;

 protected:
  struct Properties {
    Properties();
    ~Properties();
    // These properties are mandatory on DRM atomic. On legacy they may or may
    // not be present.
    DrmWrapper::Property crtc_id;
    DrmWrapper::Property crtc_x;
    DrmWrapper::Property crtc_y;
    DrmWrapper::Property crtc_w;
    DrmWrapper::Property crtc_h;
    DrmWrapper::Property fb_id;
    DrmWrapper::Property src_x;
    DrmWrapper::Property src_y;
    DrmWrapper::Property src_w;
    DrmWrapper::Property src_h;
    DrmWrapper::Property type;

    // Optional properties.
    DrmWrapper::Property rotation;
    DrmWrapper::Property in_formats;
    DrmWrapper::Property in_fence_fd;
    DrmWrapper::Property plane_color_encoding;
    DrmWrapper::Property plane_color_range;
    DrmWrapper::Property plane_fb_damage_clips;
    DrmWrapper::Property size_hints;
  };

  const uint32_t id_;

  Properties properties_ = {};

  base::flat_set<uint32_t> possible_crtc_ids_;
  uint32_t owning_crtc_ = 0;
  uint32_t last_used_format_ = 0;
  bool in_use_ = false;
  uint32_t type_ = DRM_PLANE_TYPE_PRIMARY;
  std::vector<uint32_t> supported_formats_;
  std::vector<gfx::Size> supported_cursor_sizes_;
  std::vector<drm_format_modifier> supported_format_modifiers_;

  uint64_t color_encoding_bt601_ = 0u;
  uint64_t color_encoding_bt709_ = 0u;
  uint64_t color_range_limited_ = 0u;

 private:
  void InitializeProperties(DrmDevice* drm);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_H_
