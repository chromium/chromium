// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_CRTC_COMMIT_REQUEST_H_
#define UI_OZONE_PLATFORM_DRM_GPU_CRTC_COMMIT_REQUEST_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"

namespace ui {

struct HardwareDisplayPlaneList;

class CrtcCommitRequest;
using CommitRequest = std::vector<CrtcCommitRequest>;

// Container holding information for a single CRTC that need to be modeset.
// TODO(markyacoub): PAGE_FLIP could re-use the same CrtcCommitRequest. The
// difference between MODESET and PAGE_FLIP are minimal.
class CrtcCommitRequest {
 public:
  CrtcCommitRequest(const CrtcCommitRequest& other);
  ~CrtcCommitRequest();

  static CrtcCommitRequest EnableCrtcRequest(
      uint32_t crtc_id,
      uint32_t connector_id,
      drmModeModeInfo mode,
      gfx::Point origin,
      HardwareDisplayPlaneList* plane_list,
      DrmOverlayPlaneList overlays);

  static CrtcCommitRequest DisableCrtcRequest(
      uint32_t crtc_id,
      uint32_t connector_id,
      HardwareDisplayPlaneList* plane_list = nullptr);

  bool should_enable() const { return should_enable_; }
  uint32_t crtc_id() const { return crtc_id_; }
  uint32_t connector_id() const { return connector_id_; }
  const drmModeModeInfo& mode() const { return mode_; }
  const gfx::Point& origin() const { return origin_; }
  HardwareDisplayPlaneList* plane_list() const { return plane_list_; }
  const DrmOverlayPlaneList& overlays() const { return overlays_; }

  // Adds trace records to |context|.
  void WriteIntoTrace(perfetto::TracedValue context) const;

 private:
  CrtcCommitRequest(uint32_t crtc_id,
                    uint32_t connector_id,
                    drmModeModeInfo mode,
                    gfx::Point origin,
                    HardwareDisplayPlaneList* plane_list,
                    DrmOverlayPlaneList overlays,
                    bool should_enable);

  const bool should_enable_ = false;
  const uint32_t crtc_id_ = 0;
  const uint32_t connector_id_ = 0;
  const drmModeModeInfo mode_ = {};
  const gfx::Point origin_;
  HardwareDisplayPlaneList* plane_list_ = nullptr;
  const DrmOverlayPlaneList overlays_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_CRTC_COMMIT_REQUEST_H_
