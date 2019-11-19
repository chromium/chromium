// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"

#include <utility>

#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/common/linux/gbm_buffer.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

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

  uint32_t framebuffer_id = 0;
  if (!drm_device->AddFramebuffer2(params.width, params.height, params.format,
                                   params.handles, params.strides,
                                   params.offsets, modifiers, &framebuffer_id,
                                   params.flags)) {
    DPLOG(WARNING) << "AddFramebuffer2";
    return nullptr;
  }

  uint32_t opaque_format = GetFourCCFormatForOpaqueFramebuffer(
      GetBufferFormatFromFourCCFormat(params.format));
  uint32_t opaque_framebuffer_id = 0;
  if (opaque_format != params.format &&
      !drm_device->AddFramebuffer2(params.width, params.height, opaque_format,
                                   params.handles, params.strides,
                                   params.offsets, modifiers,
                                   &opaque_framebuffer_id, params.flags)) {
    DPLOG(WARNING) << "AddFramebuffer2";
    drm_device->RemoveFramebuffer(framebuffer_id);
    return nullptr;
  }

  return base::MakeRefCounted<DrmFramebuffer>(
      std::move(drm_device), framebuffer_id, params.format,
      opaque_framebuffer_id, opaque_format, params.modifier,
      params.preferred_modifiers, gfx::Size(params.width, params.height));
}

// static
scoped_refptr<DrmFramebuffer> DrmFramebuffer::AddFramebuffer(
    scoped_refptr<DrmDevice> drm,
    const GbmBuffer* buffer,
    std::vector<uint64_t> preferred_modifiers) {
  gfx::Size size = buffer->GetSize();
  AddFramebufferParams params;
  params.format = buffer->GetFormat();
  params.modifier = buffer->GetFormatModifier();
  params.width = size.width();
  params.height = size.height();
  params.num_planes = buffer->GetNumPlanes();
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
  if (drm->allow_addfb2_modifiers() &&
      params.modifier != DRM_FORMAT_MOD_INVALID)
    params.flags |= DRM_MODE_FB_MODIFIERS;

  return AddFramebuffer(std::move(drm), params);
}

DrmFramebuffer::DrmFramebuffer(scoped_refptr<DrmDevice> drm_device,
                               uint32_t framebuffer_id,
                               uint32_t framebuffer_pixel_format,
                               uint32_t opaque_framebuffer_id,
                               uint32_t opaque_framebuffer_pixel_format,
                               uint64_t format_modifier,
                               std::vector<uint64_t> modifiers,
                               const gfx::Size& size)
    : drm_device_(std::move(drm_device)),
      framebuffer_id_(framebuffer_id),
      framebuffer_pixel_format_(framebuffer_pixel_format),
      opaque_framebuffer_id_(opaque_framebuffer_id),
      opaque_framebuffer_pixel_format_(opaque_framebuffer_pixel_format),
      format_modifier_(format_modifier),
      preferred_modifiers_(modifiers),
      size_(size) {}

DrmFramebuffer::~DrmFramebuffer() {
  if (!drm_device_->RemoveFramebuffer(framebuffer_id_))
    PLOG(WARNING) << "RemoveFramebuffer";
  if (opaque_framebuffer_id_ &&
      !drm_device_->RemoveFramebuffer(opaque_framebuffer_id_))
    PLOG(WARNING) << "RemoveFramebuffer";
}

}  // namespace ui
