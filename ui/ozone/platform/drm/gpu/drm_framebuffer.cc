// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

namespace ui {

namespace {

// Some Display Controllers (e.g. Intel Gen 9.5) don't support AR/B30
// framebuffers, only XR/B30; this function indicates if an opaque format should
// be used instead of the non-opaque |buffer_format| for AddFramebuffer2().
bool ForceUsingOpaqueFormatWorkaround(
    const scoped_refptr<DrmDevice>& drm_device,
    uint32_t drm_fourcc) {
  constexpr uint32_t kHighBitDepthARGBFormats[] = {
      DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010, DRM_FORMAT_RGBA1010102,
      DRM_FORMAT_BGRA1010102};
  const bool is_high_bit_depth_format_with_alpha =
      base::Contains(kHighBitDepthARGBFormats, drm_fourcc);
  if (!is_high_bit_depth_format_with_alpha)
    return false;

  const std::vector<uint32_t>& supported_formats =
      drm_device->plane_manager()->GetSupportedFormats();
  return !base::Contains(supported_formats, drm_fourcc);
}

}  // namespace

DrmFramebuffer::AddFramebufferParams::AddFramebufferParams() = default;
DrmFramebuffer::AddFramebufferParams::AddFramebufferParams(
    const AddFramebufferParams& other) = default;
DrmFramebuffer::AddFramebufferParams::~AddFramebufferParams() = default;

// static
scoped_refptr<DrmFramebuffer> DrmFramebuffer::AddFramebuffer(
    scoped_refptr<DrmDevice> drm_device,
    DrmFramebuffer::AddFramebufferParams params) {
  uint64_t modifiers[4] = {0};
  if (params.modifier != DRM_FORMAT_MOD_INVALID) {
    for (size_t i = 0; i < params.num_planes; ++i)
      modifiers[i] = params.modifier;
  }

  const auto buffer_format = GetBufferFormatFromFourCCFormat(params.format);
  const uint32_t opaque_format =
      GetFourCCFormatForOpaqueFramebuffer(buffer_format);
  const auto drm_format =
      ForceUsingOpaqueFormatWorkaround(drm_device, params.format)
          ? opaque_format
          : params.format;

  uint32_t framebuffer_id = 0;
  if (!drm_device->AddFramebuffer2(params.width, params.height, drm_format,
                                   params.handles, params.strides,
                                   params.offsets, modifiers, &framebuffer_id,
                                   params.flags)) {
    VLOG(4) << "AddFramebuffer2:" << "size=" << params.width << "x"
            << params.height << " drm_format=" << DrmFormatToString(drm_format)
            << " fb_id=" << framebuffer_id << " flags=" << params.flags;
    return nullptr;
  }

  uint32_t opaque_framebuffer_id = 0;
  if (opaque_format != drm_format &&
      !drm_device->AddFramebuffer2(params.width, params.height, opaque_format,
                                   params.handles, params.strides,
                                   params.offsets, modifiers,
                                   &opaque_framebuffer_id, params.flags)) {
    VLOG(4) << "AddFramebuffer2:" << "size=" << params.width << "x"
            << params.height << " drm_format=" << DrmFormatToString(drm_format)
            << " fb_id=" << opaque_framebuffer_id << " flags=" << params.flags;
    drm_device->RemoveFramebuffer(framebuffer_id);
    return nullptr;
  }

  return base::MakeRefCounted<DrmFramebuffer>(
      std::move(drm_device), framebuffer_id, drm_format, opaque_framebuffer_id,
      opaque_format, params.modifier, params.preferred_modifiers,
      gfx::Size(params.width, params.height), params.is_original_buffer);
}

// static
scoped_refptr<DrmFramebuffer> DrmFramebuffer::AddFramebuffer(
    scoped_refptr<DrmDevice> drm,
    const GbmBuffer* buffer,
    const gfx::Size& framebuffer_size,
    std::vector<uint64_t> preferred_modifiers,
    bool is_original_buffer) {
  DCHECK(gfx::Rect(buffer->GetSize()).Contains(gfx::Rect(framebuffer_size)));
  AddFramebufferParams params;
  params.format = buffer->GetFormat();
  params.modifier = buffer->GetFormatModifier();
  params.width = framebuffer_size.width();
  params.height = framebuffer_size.height();
  params.num_planes = buffer->GetNumPlanes();
  params.is_original_buffer = is_original_buffer;
  params.preferred_modifiers = preferred_modifiers;
  for (size_t i = 0; i < params.num_planes; ++i) {
    params.handles[i] = buffer->GetPlaneHandle(i);
    params.strides[i] = buffer->GetPlaneStride(i);
    params.offsets[i] = buffer->GetPlaneOffset(i);
  }

  // AddFramebuffer2 only considers the modifiers if addfb_flags has
  // DRM_MODE_FB_MODIFIERS set. We only set that when we've created
  // a bo with modifiers, otherwise, we rely on the "no modifiers"
  // behavior doing the right thing.
  params.flags = 0;
  if (IsAddfb2ModifierCapable(*drm) &&
      params.modifier != DRM_FORMAT_MOD_INVALID) {
    params.flags |= DRM_MODE_FB_MODIFIERS;
  }

  return AddFramebuffer(std::move(drm), params);
}

DrmFramebuffer::DrmFramebuffer(scoped_refptr<DrmDevice> drm_device,
                               uint32_t framebuffer_id,
                               uint32_t framebuffer_pixel_format,
                               uint32_t opaque_framebuffer_id,
                               uint32_t opaque_framebuffer_pixel_format,
                               uint64_t format_modifier,
                               std::vector<uint64_t> modifiers,
                               const gfx::Size& size,
                               bool is_original_buffer)
    : drm_device_(std::move(drm_device)),
      framebuffer_id_(framebuffer_id),
      framebuffer_pixel_format_(framebuffer_pixel_format),
      opaque_framebuffer_id_(opaque_framebuffer_id),
      opaque_framebuffer_pixel_format_(opaque_framebuffer_pixel_format),
      format_modifier_(format_modifier),
      is_original_buffer_(is_original_buffer),
      preferred_modifiers_(modifiers),
      size_(size),
      modeset_sequence_id_at_allocation_(drm_device_->modeset_sequence_id()) {}

DrmFramebuffer::~DrmFramebuffer() {
  if (!drm_device_->RemoveFramebuffer(framebuffer_id_)) {
    VLOG(4) << "RemoveFramebuffer";
  }

  if (opaque_framebuffer_id_ &&
      !drm_device_->RemoveFramebuffer(opaque_framebuffer_id_)) {
    VLOG(4) << "RemoveFramebuffer";
  }
}

}  // namespace ui
