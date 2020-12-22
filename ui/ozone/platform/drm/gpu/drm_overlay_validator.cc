// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_validator.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/platform_file.h"
#include "base/metrics/histogram_macros.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/gbm_pixmap.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

namespace ui {

namespace {

scoped_refptr<DrmFramebuffer> GetBufferForPageFlipTest(
    const DrmWindow* drm_window,
    const OverlaySurfaceCandidate& overlay_surface,
    std::vector<scoped_refptr<DrmFramebuffer>>* reusable_buffers,
    size_t* total_allocated_memory_size) {
  if (overlay_surface.native_pixmap) {
    return static_cast<GbmPixmap*>(overlay_surface.native_pixmap.get())
        ->framebuffer();
  }
  uint32_t fourcc_format =
      overlay_surface.is_opaque
          ? GetFourCCFormatForOpaqueFramebuffer(overlay_surface.format)
          : GetFourCCFormatFromBufferFormat(overlay_surface.format);
  gfx::Size size = overlay_surface.buffer_size;
  bool is_0th_plane = !overlay_surface.plane_z_order;
  // Force the 0th plane (e.g. primary plane and fullscreen) with the modifiers
  // used in existing buffers to keep the test consistent with the subsequent
  // flip commits.
  std::vector<uint64_t> modifiers =
      is_0th_plane
          ? drm_window->GetController()->GetFormatModifiers(fourcc_format)
          : std::vector<uint64_t>();

  // Check if we can re-use existing buffers.
  for (const auto& buf : *reusable_buffers) {
    if (buf->framebuffer_pixel_format() == fourcc_format &&
        buf->size() == size && buf->preferred_modifiers() == modifiers) {
      return buf;
    }
  }

  scoped_refptr<DrmDevice> drm_device =
      drm_window->GetController()->GetDrmDevice();

  std::unique_ptr<GbmBuffer> buffer =
      is_0th_plane ? drm_device->gbm_device()->CreateBufferWithModifiers(
                         fourcc_format, size, GBM_BO_USE_SCANOUT, modifiers)
                   : drm_device->gbm_device()->CreateBuffer(fourcc_format, size,
                                                            GBM_BO_USE_SCANOUT);

  if (!buffer)
    return nullptr;

  for (size_t i = 0; i < buffer->GetNumPlanes(); ++i)
    *total_allocated_memory_size += buffer->GetPlaneSize(i);

  scoped_refptr<DrmFramebuffer> drm_framebuffer =
      DrmFramebuffer::AddFramebuffer(drm_device, buffer.get(),
                                     buffer->GetSize(), modifiers);
  if (!drm_framebuffer)
    return nullptr;

  reusable_buffers->push_back(drm_framebuffer);
  return drm_framebuffer;
}

}  // namespace

DrmOverlayValidator::DrmOverlayValidator(DrmWindow* window) : window_(window) {}

DrmOverlayValidator::~DrmOverlayValidator() {}

OverlayStatusList DrmOverlayValidator::TestPageFlip(
    const OverlaySurfaceCandidateList& params,
    const DrmOverlayPlaneList& last_used_planes) {
  OverlayStatusList returns(params.size());
  HardwareDisplayController* controller = window_->GetController();
  if (!controller) {
    // The controller is not yet installed.
    for (auto& param : returns)
      param = OVERLAY_STATUS_NOT;

    return returns;
  }

  DrmOverlayPlaneList test_list;
  std::vector<scoped_refptr<DrmFramebuffer>> reusable_buffers;
  scoped_refptr<DrmDevice> drm = controller->GetDrmDevice();

  for (const auto& plane : last_used_planes)
    reusable_buffers.push_back(plane.buffer);

  size_t total_allocated_memory_size = 0;

  for (size_t i = 0; i < params.size(); ++i) {
    if (!params[i].overlay_handled) {
      returns[i] = OVERLAY_STATUS_NOT;
      continue;
    }

    scoped_refptr<DrmFramebuffer> buffer = GetBufferForPageFlipTest(
        window_, params[i], &reusable_buffers, &total_allocated_memory_size);

    DrmOverlayPlane plane(buffer, params[i].plane_z_order, params[i].transform,
                          gfx::ToNearestRect(params[i].display_rect),
                          params[i].crop_rect, !params[i].is_opaque,
                          /*gpu_fence=*/nullptr);
    test_list.push_back(std::move(plane));

    if (buffer && controller->TestPageFlip(test_list)) {
      returns[i] = OVERLAY_STATUS_ABLE;
    } else {
      // If test failed here, platform cannot support this configuration
      // with current combination of layers. This is usually the case when this
      // plane has requested post processing capability which needs additional
      // hardware resources and they might be already in use by other planes.
      // For example this plane has requested scaling capabilities and all
      // available scalars are already in use by other planes.
      returns[i] = OVERLAY_STATUS_NOT;
      test_list.pop_back();
    }
  }

  UMA_HISTOGRAM_MEMORY_KB(
      "Compositing.Display.DrmOverlayManager.TotalTestBufferMemorySize",
      total_allocated_memory_size / 1024);

  return returns;
}

}  // namespace ui
