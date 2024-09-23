// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_validator.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gbm_device.h"
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
    std::vector<scoped_refptr<DrmFramebuffer>>* reusable_buffers) {
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
          ? drm_window->GetController()->GetSupportedModifiers(fourcc_format)
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

  constexpr bool kIsOriginalBuffer = false;
  scoped_refptr<DrmFramebuffer> drm_framebuffer =
      DrmFramebuffer::AddFramebuffer(drm_device, buffer.get(),
                                     buffer->GetSize(), modifiers,
                                     kIsOriginalBuffer);
  if (!drm_framebuffer)
    return nullptr;

  reusable_buffers->push_back(drm_framebuffer);
  return drm_framebuffer;
}

}  // namespace

DrmOverlayValidator::DrmOverlayValidator(DrmWindow* window) : window_(window) {}

DrmOverlayValidator::~DrmOverlayValidator() = default;

DrmOverlayPlane DrmOverlayValidator::MakeOverlayPlane(
    const OverlaySurfaceCandidate& param,
    std::vector<scoped_refptr<DrmFramebuffer>>& reusable_buffers) {
  scoped_refptr<DrmFramebuffer> buffer =
      GetBufferForPageFlipTest(window_, param, &reusable_buffers);

  return DrmOverlayPlane(buffer, param.color_space, param.plane_z_order,
                         absl::get<gfx::OverlayTransform>(param.transform),
                         gfx::ToNearestRect(param.display_rect),
                         gfx::ToNearestRect(param.display_rect),
                         param.crop_rect, !param.is_opaque,
                         /*gpu_fence=*/nullptr);
}

OverlayStatusList DrmOverlayValidator::TestPageFlip(
    const OverlaySurfaceCandidateList& params,
    const DrmOverlayPlaneList& last_used_planes) {
  OverlayStatusList returns(params.size());
  HardwareDisplayController* controller = window_->GetController();
  if (!controller) {
    // The controller is not yet installed.
    for (auto& status : returns) {
      status = OVERLAY_STATUS_NOT;
    }

    return returns;
  }

  DrmOverlayPlaneList test_list;
  std::vector<scoped_refptr<DrmFramebuffer>> reusable_buffers;
  scoped_refptr<DrmDevice> drm = controller->GetDrmDevice();

  for (const auto& plane : last_used_planes) {
    reusable_buffers.push_back(plane.buffer);
  }

  std::vector<size_t> plane_indices;
  for (size_t i = 0; i < params.size(); ++i) {
    auto& param = params[i];
    // Skip candidates that have already been disqualified.
    if (!param.overlay_handled) {
      returns[i] = OVERLAY_STATUS_NOT;
      continue;
    }

    DrmOverlayPlane plane = MakeOverlayPlane(param, reusable_buffers);
    if (!plane.buffer) {
      returns[i] = OVERLAY_STATUS_NOT;
      continue;
    }

    test_list.push_back(std::move(plane));
    // We need to save the indices because we're skipping some planes.
    plane_indices.push_back(i);
  }

  // Test the whole list, then gradually remove the last plane and retest until
  // we have success, or no more planes to test.
  while (!test_list.empty()) {
    bool test_result = controller->TestPageFlip(test_list);
    if (test_result) {
      break;
    }

    // If test failed here, platform cannot support this configuration
    // with current combination of layers. This is usually the case when this
    // plane has requested post processing capability which needs additional
    // hardware resources and they might be already in use by other planes.
    // For example this plane has requested scaling capabilities and all
    // available scalars are already in use by other planes.

    // Drop the last plane from the test list and set it to OVERLAY_STATUS_NOT.
    returns[plane_indices.back()] = OVERLAY_STATUS_NOT;
    plane_indices.pop_back();
    test_list.pop_back();
  }

  // Set OVERLAY_STATUS_ABLE for all planes left in the test_list.
  for (size_t index : plane_indices) {
    returns[index] = OVERLAY_STATUS_ABLE;
  }

  return returns;
}

}  // namespace ui
