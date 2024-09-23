// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"

#include <drm_fourcc.h>

#include "base/files/platform_file.h"
#include "base/logging.h"
#include "build/chromeos_buildflags.h"
#include "media/media_buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
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
    // Driver code swaps 90 and 270 to be compliant with how xrandr uses these
    // values, so we need to invert them here as well to get them back to the
    // proper value.
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
      return DRM_MODE_ROTATE_270;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
      return DRM_MODE_ROTATE_180;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
      return DRM_MODE_ROTATE_90;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90:
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270:
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return 0;
}

// Rotations are dependent on modifiers. Tiled formats can be rotated,
// linear formats cannot. Atomic tests currently ignore modifiers, so there
// isn't a way of determining if the rotation is supported.
// TODO(https://b/172210707): Atomic tests should work if we are using
// the original buffers as they have the correct modifiers. See
// kUseRealBuffersForPageFlipTest and the 'GetBufferForPageFlipTest' function.
// Intel driver reference on rotated and flipped buffers with modifiers:
// https://code.woboq.org/linux/linux/drivers/gpu/drm/i915/intel_sprite.c.html#1471
bool IsRotationTransformSupported(gfx::OverlayTransform transform,
                                  uint32_t format_fourcc,
                                  bool is_original_buffer) {
  if ((transform == gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90) ||
      (transform == gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270) ||
      (transform == gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL)) {
    if (is_original_buffer && (format_fourcc == DRM_FORMAT_NV12 ||
                               format_fourcc == DRM_FORMAT_P010)) {
      return true;
    }
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

  ret &= (properties_.plane_color_encoding.id == 0) ==
         (properties_.plane_color_range.id == 0);
  LOG_IF(ERROR, !ret) << "Inconsistent color management properties for plane="
                      << id_;

  return ret;
}

bool HardwareDisplayPlaneAtomic::AssignPlaneProps(
    DrmDevice* drm,
    uint32_t crtc_id,
    uint32_t framebuffer,
    const gfx::Rect& crtc_rect,
    const gfx::Rect& src_rect,
    const gfx::Rect& damage_rect,
    const gfx::OverlayTransform transform,
    const gfx::ColorSpace& color_space,
    int in_fence_fd,
    uint32_t format_fourcc,
    bool is_original_buffer) {
  if (transform != gfx::OVERLAY_TRANSFORM_NONE && !properties_.rotation.id)
    return false;

  if (!IsRotationTransformSupported(transform, format_fourcc,
                                    is_original_buffer))
    return false;

  // Make a copy of properties to get the props IDs for the new intermediate
  // values.
  assigned_props_ = properties_;

  assigned_props_.crtc_id.value = crtc_id;
  assigned_props_.crtc_x.value = crtc_rect.x();
  assigned_props_.crtc_y.value = crtc_rect.y();
  assigned_props_.crtc_w.value = crtc_rect.width();
  assigned_props_.crtc_h.value = crtc_rect.height();
  assigned_props_.fb_id.value = framebuffer;
  assigned_props_.src_x.value = src_rect.x();
  assigned_props_.src_y.value = src_rect.y();
  assigned_props_.src_w.value = src_rect.width();
  assigned_props_.src_h.value = src_rect.height();

  if (assigned_props_.rotation.id) {
    assigned_props_.rotation.value =
        OverlayTransformToDrmRotationPropertyValue(transform);
  }

  // Log an error if the clip is not in bounds. Restrict logging to when PSR2 is
  // enabled. (plane_fb_damage_clips has non-zero ID).
  const bool clip_in_bounds = crtc_rect.Contains(damage_rect);
  if (!clip_in_bounds && assigned_props_.plane_fb_damage_clips.id) {
    LOG(ERROR) << "Damage clip dmg: " << damage_rect.ToString()
               << " not contained inside display bounds"
               << " crtc: " << crtc_rect.ToString();
  }
  if (drm && assigned_props_.plane_fb_damage_clips.id) {
    ScopedDrmModeRectPtr dmg_clip_blob_data = CreateDCBlob(damage_rect);
    // dmg_clip_blob needs to live long enough to be committed.
    static ScopedDrmPropertyBlob dmg_clip_blob = drm->CreatePropertyBlob(
        dmg_clip_blob_data.get(), sizeof(drm_mode_rect));
    assigned_props_.plane_fb_damage_clips.value = dmg_clip_blob->id();
  }

  if (assigned_props_.plane_color_encoding.id) {
    assigned_props_.plane_color_encoding.value =
        color_space.GetMatrixID() == gfx::ColorSpace::MatrixID::BT709
            ? color_encoding_bt709_
            : color_encoding_bt601_;
  }

  if (assigned_props_.in_fence_fd.id)
    assigned_props_.in_fence_fd.value = static_cast<uint64_t>(in_fence_fd);

  return true;
}

bool HardwareDisplayPlaneAtomic::SetPlaneProps(drmModeAtomicReq* property_set) {
  bool plane_set_succeeded =
      AddPropertyIfValid(property_set, id_, assigned_props_.crtc_id) &&
      AddPropertyIfValid(property_set, id_, assigned_props_.crtc_x) &&
      AddPropertyIfValid(property_set, id_, assigned_props_.crtc_y) &&
      AddPropertyIfValid(property_set, id_, assigned_props_.crtc_w) &&
      AddPropertyIfValid(property_set, id_, assigned_props_.crtc_h) &&
      AddPropertyIfValid(property_set, id_, assigned_props_.fb_id) &&
      AddPropertyIfValid(property_set, id_, assigned_props_.src_x) &&
      AddPropertyIfValid(property_set, id_, assigned_props_.src_y) &&
      AddPropertyIfValid(property_set, id_, assigned_props_.src_w) &&
      AddPropertyIfValid(property_set, id_, assigned_props_.src_h);

  if (assigned_props_.rotation.id) {
    plane_set_succeeded &=
        AddPropertyIfValid(property_set, id_, assigned_props_.rotation);
  }

  if (assigned_props_.plane_fb_damage_clips.id) {
    plane_set_succeeded &= AddPropertyIfValid(
        property_set, id_, assigned_props_.plane_fb_damage_clips);
  }

  if (assigned_props_.in_fence_fd.id) {
    plane_set_succeeded &=
        AddPropertyIfValid(property_set, id_, assigned_props_.in_fence_fd);
  }

  if (assigned_props_.plane_color_encoding.id) {
    // TODO(markyacoub): |color_range_limited_| is only set in Initialize(). The
    // properties could be set once in there and these member variables could be
    // removed.
    assigned_props_.plane_color_range.value = color_range_limited_;
    plane_set_succeeded &= AddPropertyIfValid(
        property_set, id_, assigned_props_.plane_color_encoding);
    plane_set_succeeded &= AddPropertyIfValid(
        property_set, id_, assigned_props_.plane_color_range);
  }

  if (!plane_set_succeeded) {
    LOG(ERROR) << "Failed to set plane data";
    return false;
  }

  // Update properties_ if the setting the props succeeded.
  properties_ = assigned_props_;
  return true;
}

void HardwareDisplayPlaneAtomic::AssignDisableProps() {
  set_in_use(false);
  set_owning_crtc(0);
  AssignPlaneProps(nullptr, 0, 0, gfx::Rect(), gfx::Rect(), gfx::Rect(),
                   gfx::OVERLAY_TRANSFORM_NONE, gfx::ColorSpace(),
                   base::kInvalidPlatformFile, DRM_FORMAT_INVALID, false);
}

uint32_t HardwareDisplayPlaneAtomic::AssignedCrtcId() const {
  return assigned_props_.crtc_id.value;
}

}  // namespace ui
