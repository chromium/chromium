// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"

#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"

namespace ui {
namespace {

uint32_t OverlayTransformToDrmRotationPropertyValue(
    gfx::OverlayTransform transform) {
  switch (transform) {
    case gfx::OVERLAY_TRANSFORM_NONE:
      return DRM_MODE_ROTATE_0;
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_0;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_0;
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      return DRM_MODE_ROTATE_90;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return DRM_MODE_ROTATE_180;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return DRM_MODE_ROTATE_270;
    default:
      NOTREACHED();
  }
  return 0;
}

// Rotations are dependent on modifiers. Tiled formats can be rotated,
// linear formats cannot. Atomic tests currently ignore modifiers, so there
// isn't a way of determining if the rotation is supported.
// TODO(https://crbug/880464): Remove this.
bool IsRotationTransformSupported(gfx::OverlayTransform transform) {
  if ((transform == gfx::OVERLAY_TRANSFORM_ROTATE_90) ||
      (transform == gfx::OVERLAY_TRANSFORM_ROTATE_270)) {
    return false;
  }

  return true;
}

}  // namespace

HardwareDisplayPlaneAtomic::HardwareDisplayPlaneAtomic(uint32_t id)
    : HardwareDisplayPlane(id) {}

HardwareDisplayPlaneAtomic::~HardwareDisplayPlaneAtomic() = default;

bool HardwareDisplayPlaneAtomic::Initialize(DrmDevice* drm) {
  if (!HardwareDisplayPlane::Initialize(drm))
    return false;

  // Check that all the required properties have been found.
  bool ret = properties_.crtc_id.id && properties_.crtc_x.id &&
             properties_.crtc_y.id && properties_.crtc_w.id &&
             properties_.crtc_h.id && properties_.fb_id.id &&
             properties_.src_x.id && properties_.src_y.id &&
             properties_.src_w.id && properties_.src_h.id;
  LOG_IF(ERROR, !ret) << "Failed to find all required properties for plane="
                      << id_;
  return ret;
}

bool HardwareDisplayPlaneAtomic::SetPlaneData(
    drmModeAtomicReq* property_set,
    uint32_t crtc_id,
    uint32_t framebuffer,
    const gfx::Rect& crtc_rect,
    const gfx::Rect& src_rect,
    const gfx::OverlayTransform transform,
    int in_fence_fd) {
  if (transform != gfx::OVERLAY_TRANSFORM_NONE && !properties_.rotation.id)
    return false;

  if (!IsRotationTransformSupported(transform))
    return false;

  properties_.crtc_id.value = crtc_id;
  properties_.crtc_x.value = crtc_rect.x();
  properties_.crtc_y.value = crtc_rect.y();
  properties_.crtc_w.value = crtc_rect.width();
  properties_.crtc_h.value = crtc_rect.height();
  properties_.fb_id.value = framebuffer;
  properties_.src_x.value = src_rect.x();
  properties_.src_y.value = src_rect.y();
  properties_.src_w.value = src_rect.width();
  properties_.src_h.value = src_rect.height();

  bool plane_set_succeeded =
      AddPropertyIfValid(property_set, id_, properties_.crtc_id) &&
      AddPropertyIfValid(property_set, id_, properties_.crtc_x) &&
      AddPropertyIfValid(property_set, id_, properties_.crtc_y) &&
      AddPropertyIfValid(property_set, id_, properties_.crtc_w) &&
      AddPropertyIfValid(property_set, id_, properties_.crtc_h) &&
      AddPropertyIfValid(property_set, id_, properties_.fb_id) &&
      AddPropertyIfValid(property_set, id_, properties_.src_x) &&
      AddPropertyIfValid(property_set, id_, properties_.src_y) &&
      AddPropertyIfValid(property_set, id_, properties_.src_w) &&
      AddPropertyIfValid(property_set, id_, properties_.src_h);

  if (properties_.rotation.id) {
    properties_.rotation.value =
        OverlayTransformToDrmRotationPropertyValue(transform);
    plane_set_succeeded =
        plane_set_succeeded &&
        AddPropertyIfValid(property_set, id_, properties_.rotation);
  }

  if (properties_.in_fence_fd.id && in_fence_fd >= 0) {
    properties_.in_fence_fd.value = in_fence_fd;
    plane_set_succeeded =
        plane_set_succeeded &&
        AddPropertyIfValid(property_set, id_, properties_.in_fence_fd);
  }

  if (!plane_set_succeeded) {
    LOG(ERROR) << "Failed to set plane data";
    return false;
  }

  return true;
}

bool HardwareDisplayPlaneAtomic::SetPlaneCtm(drmModeAtomicReq* property_set,
                                             uint32_t ctm_blob_id) {
  if (!properties_.plane_ctm.id)
    return false;

  properties_.plane_ctm.value = ctm_blob_id;
  return AddPropertyIfValid(property_set, id_, properties_.plane_ctm);
}

}  // namespace ui
