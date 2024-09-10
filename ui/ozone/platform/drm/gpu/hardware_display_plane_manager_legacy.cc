// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"

#include <errno.h>
#include <sync/sync.h>

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/thread_pool.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

namespace ui {

namespace {

// We currently wait for the fences serially, but it's possible
// that merging the fences and waiting on the merged fence fd
// is more efficient. We should revisit once we have more info.
DrmOverlayPlaneList WaitForPlaneFences(DrmOverlayPlaneList planes) {
  for (const auto& plane : planes) {
    if (plane.gpu_fence)
      plane.gpu_fence->Wait();
  }
  return planes;
}

bool CommitPendingCrtcProperty(
    DrmDevice* device,
    uint32_t crtc_id,
    DrmWrapper::Property& prop,
    std::optional<ScopedDrmPropertyBlob>& pending_blob) {
  if (!pending_blob.has_value()) {
    return true;
  }
  ScopedDrmPropertyBlob blob = std::move(pending_blob.value());
  pending_blob = std::nullopt;
  if (!prop.id) {
    return true;
  }

  prop.value = blob ? blob->id() : 0;
  int ret = device->SetObjectProperty(crtc_id, DRM_MODE_OBJECT_CRTC, prop.id,
                                      prop.value);
  if (ret < 0) {
    return false;
  }
  return true;
}

}  // namespace

HardwareDisplayPlaneManagerLegacy::HardwareDisplayPlaneManagerLegacy(
    DrmDevice* drm)
    : HardwareDisplayPlaneManager(drm) {}

HardwareDisplayPlaneManagerLegacy::~HardwareDisplayPlaneManagerLegacy() =
    default;

bool HardwareDisplayPlaneManagerLegacy::Commit(CommitRequest commit_request,
                                               uint32_t flags) {
  if (flags & DRM_MODE_ATOMIC_TEST_ONLY)
    // Legacy DRM does not support testing.
    return true;

  bool status = true;
  for (const auto& crtc_request : commit_request) {
    if (crtc_request.should_enable_crtc()) {
      // Overlays are not supported in legacy hence why we're only looking at
      // the primary plane.
      uint32_t fb_id = DrmOverlayPlane::GetPrimaryPlane(crtc_request.overlays())
                           ->buffer->opaque_framebuffer_id();
      status &=
          drm_->SetCrtc(crtc_request.crtc_id(), fb_id,
                        std::vector<uint32_t>(1, crtc_request.connector_id()),
                        crtc_request.mode());
    } else {
      drm_->DisableCrtc(crtc_request.crtc_id());
    }
  }

  if (status)
    UpdateCrtcAndPlaneStatesAfterModeset(commit_request);

  return status;
}

bool HardwareDisplayPlaneManagerLegacy::Commit(
    HardwareDisplayPlaneList* plane_list,
    scoped_refptr<PageFlipRequest> page_flip_request,
    gfx::GpuFenceHandle* release_fence) {
  bool test_only = !page_flip_request;
  if (test_only) {
    for (HardwareDisplayPlane* plane : plane_list->plane_list) {
      plane->set_in_use(false);
    }
    plane_list->plane_list.clear();
    plane_list->legacy_page_flips.clear();
    return true;
  }
  if (plane_list->plane_list.empty())  // No assigned planes, nothing to do.
    return true;

  bool ret = true;
  for (const auto& flip : plane_list->legacy_page_flips) {
    if (!drm_->PageFlip(flip.crtc_id, flip.framebuffer, page_flip_request)) {
      // 1) Permission Denied is a legitimate error.
      // 2) EBUSY or ENODEV are possible if we're page flipping a disconnected
      // CRTC. Pretend we're fine since a hotplug event is supposed to be on
      // its way.
      // NOTE: We could be getting EBUSY if we're trying to page flip a CRTC
      // that has a pending page flip, however the contract is that the caller
      // will never attempt this (since the caller should be waiting for the
      // page flip completion message).
      if (errno != EACCES && errno != EBUSY && errno != ENODEV) {
        PLOG(ERROR) << "Cannot page flip: crtc=" << flip.crtc_id
                    << " framebuffer=" << flip.framebuffer;
        ret = false;
      }
    }
  }

  if (ret) {
    plane_list->plane_list.swap(plane_list->old_plane_list);
    plane_list->plane_list.clear();
    plane_list->legacy_page_flips.clear();
  } else {
    ResetCurrentPlaneList(plane_list);
  }

  return ret;
}

bool HardwareDisplayPlaneManagerLegacy::TestSeamlessMode(
    int32_t crtc_id,
    const drmModeModeInfo& mode) {
  return false;
}

bool HardwareDisplayPlaneManagerLegacy::DisableOverlayPlanes(
    HardwareDisplayPlaneList* plane_list) {
  // We're never going to ship legacy pageflip with overlays enabled.
  DCHECK(!base::Contains(plane_list->old_plane_list,
                         static_cast<uint32_t>(DRM_PLANE_TYPE_OVERLAY),
                         &HardwareDisplayPlane::type));
  return true;
}

bool HardwareDisplayPlaneManagerLegacy::ValidatePrimarySize(
    const DrmOverlayPlane& primary,
    const drmModeModeInfo& mode) {
  DCHECK(primary.buffer.get());

  return primary.buffer->size() == gfx::Size(mode.hdisplay, mode.vdisplay);
}

void HardwareDisplayPlaneManagerLegacy::RequestPlanesReadyCallback(
    DrmOverlayPlaneList planes,
    base::OnceCallback<void(DrmOverlayPlaneList planes)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&WaitForPlaneFences, std::move(planes)),
      std::move(callback));
}

bool HardwareDisplayPlaneManagerLegacy::InitializePlanes() {
  ScopedDrmPlaneResPtr plane_resources = drm_->GetPlaneResources();
  if (!plane_resources) {
    PLOG(ERROR) << "Failed to get plane resources.";
    return false;
  }

  for (uint32_t i = 0; i < plane_resources->count_planes; ++i) {
    std::unique_ptr<HardwareDisplayPlane> plane(
        CreatePlane(plane_resources->planes[i]));

    if (!plane->Initialize(drm_))
      continue;

    // Overlays are not supported on the legacy path, so ignore all overlay
    // planes.
    if (plane->type() == DRM_PLANE_TYPE_OVERLAY)
      continue;

    planes_.push_back(std::move(plane));
  }

  return true;
}

bool HardwareDisplayPlaneManagerLegacy::SetPlaneData(
    HardwareDisplayPlaneList* plane_list,
    HardwareDisplayPlane* hw_plane,
    const DrmOverlayPlane& overlay,
    uint32_t crtc_id,
    std::optional<gfx::Point>,
    const gfx::Rect& src_rect) {
  // Legacy modesetting rejects transforms.
  if (overlay.plane_transform != gfx::OVERLAY_TRANSFORM_NONE)
    return false;

  if (plane_list->legacy_page_flips.empty() ||
      plane_list->legacy_page_flips.back().crtc_id != crtc_id) {
    plane_list->legacy_page_flips.emplace_back(
        crtc_id, overlay.buffer->opaque_framebuffer_id());
  } else {
    return false;
  }

  return true;
}

bool HardwareDisplayPlaneManagerLegacy::IsCompatible(
    HardwareDisplayPlane* plane,
    const DrmOverlayPlane& overlay,
    uint32_t crtc_id) const {
  if (plane->in_use() || plane->type() == DRM_PLANE_TYPE_CURSOR ||
      !plane->CanUseForCrtcId(crtc_id))
    return false;

  // When using legacy kms we always scanout only one plane (the primary),
  // and we always use the opaque fb. Refer to SetPlaneData above.
  const uint32_t format = overlay.buffer->opaque_framebuffer_pixel_format();
  return plane->IsSupportedFormat(format);
}

bool HardwareDisplayPlaneManagerLegacy::CommitPendingCrtcState(
    CrtcState& crtc_state) {
  CrtcProperties& crtc_props = crtc_state.properties;
  bool result = true;

  if (!CommitPendingCrtcProperty(drm_, crtc_props.id, crtc_props.ctm,
                                 crtc_state.pending_ctm_blob)) {
    LOG(ERROR) << "Failed to set CTM property for crtc=" << crtc_props.id;
    result = false;
  }
  if (!CommitPendingCrtcProperty(drm_, crtc_props.id, crtc_props.gamma_lut,
                                 crtc_state.pending_gamma_lut_blob)) {
    LOG(ERROR) << "Failed to set GAMMA_LUT property for crtc=" << crtc_props.id;
    result = false;
  }
  if (!CommitPendingCrtcProperty(drm_, crtc_props.id, crtc_props.degamma_lut,
                                 crtc_state.pending_degamma_lut_blob)) {
    LOG(ERROR) << "Failed to set DEGAMMA_LUT property for crtc="
               << crtc_props.id;
    result = false;
  }
  return result;
}

}  // namespace ui
