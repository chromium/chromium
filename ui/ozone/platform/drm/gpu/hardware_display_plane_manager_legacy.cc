// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"

#include <errno.h>
#include <sync/sync.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/post_task.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_dummy.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

namespace ui {

namespace {

// We currently wait for the fences serially, but it's possible
// that merging the fences and waiting on the merged fence fd
// is more efficient. We should revisit once we have more info.
ui::DrmOverlayPlaneList WaitForPlaneFences(ui::DrmOverlayPlaneList planes) {
  for (const auto& plane : planes) {
    if (plane.gpu_fence)
      plane.gpu_fence->Wait();
  }
  return planes;
}

}  // namespace

HardwareDisplayPlaneManagerLegacy::HardwareDisplayPlaneManagerLegacy(
    DrmDevice* drm)
    : HardwareDisplayPlaneManager(drm) {}

HardwareDisplayPlaneManagerLegacy::~HardwareDisplayPlaneManagerLegacy() {
}

bool HardwareDisplayPlaneManagerLegacy::Commit(
    HardwareDisplayPlaneList* plane_list,
    scoped_refptr<PageFlipRequest> page_flip_request,
    std::unique_ptr<gfx::GpuFence>* out_fence) {
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
      // 2) Device or resource busy is possible if we're page flipping a
      // disconnected CRTC. Pretend we're fine since a hotplug event is supposed
      // to be on its way.
      // NOTE: We could be getting EBUSY if we're trying to page flip a CRTC
      // that has a pending page flip, however the contract is that the caller
      // will never attempt this (since the caller should be waiting for the
      // page flip completion message).
      if (errno != EACCES && errno != EBUSY) {
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

bool HardwareDisplayPlaneManagerLegacy::DisableOverlayPlanes(
    HardwareDisplayPlaneList* plane_list) {
  // We're never going to ship legacy pageflip with overlays enabled.
  DCHECK(std::find_if(plane_list->old_plane_list.begin(),
                      plane_list->old_plane_list.end(),
                      [](HardwareDisplayPlane* plane) {
                        return plane->type() == HardwareDisplayPlane::kOverlay;
                      }) == plane_list->old_plane_list.end());
  return true;
}

bool HardwareDisplayPlaneManagerLegacy::SetColorCorrectionOnAllCrtcPlanes(
    uint32_t crtc_id,
    ScopedDrmColorCtmPtr ctm_blob_data) {
  NOTREACHED()
      << "HardwareDisplayPlaneManagerLegacy doesn't support per plane CTM";
  return false;
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
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
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
    if (plane->type() == HardwareDisplayPlane::kOverlay)
      continue;

    planes_.push_back(std::move(plane));
  }

  // https://crbug.com/464085: if driver reports no primary planes for a crtc,
  // create a dummy plane for which we can assign exactly one overlay.
  if (!has_universal_planes_) {
    for (size_t i = 0; i < crtc_state_.size(); ++i) {
      uint32_t id = crtc_state_[i].properties.id - 1;
      if (std::find_if(
              planes_.begin(), planes_.end(),
              [id](const std::unique_ptr<HardwareDisplayPlane>& plane) {
                return plane->id() == id;
              }) == planes_.end()) {
        std::unique_ptr<HardwareDisplayPlane> dummy_plane(
            new HardwareDisplayPlaneDummy(id, 1 << i));
        if (dummy_plane->Initialize(drm_)) {
          planes_.push_back(std::move(dummy_plane));
        }
      }
    }
  }

  return true;
}

bool HardwareDisplayPlaneManagerLegacy::SetPlaneData(
    HardwareDisplayPlaneList* plane_list,
    HardwareDisplayPlane* hw_plane,
    const DrmOverlayPlane& overlay,
    uint32_t crtc_id,
    const gfx::Rect& src_rect,
    CrtcController* crtc) {
  // Legacy modesetting rejects transforms.
  if (overlay.plane_transform != gfx::OVERLAY_TRANSFORM_NONE)
    return false;

  if (plane_list->legacy_page_flips.empty() ||
      plane_list->legacy_page_flips.back().crtc_id != crtc_id) {
    plane_list->legacy_page_flips.push_back(
        HardwareDisplayPlaneList::PageFlipInfo(
            crtc_id, overlay.buffer->opaque_framebuffer_id(), crtc));
  } else {
    return false;
  }

  return true;
}

bool HardwareDisplayPlaneManagerLegacy::IsCompatible(
    HardwareDisplayPlane* plane,
    const DrmOverlayPlane& overlay,
    uint32_t crtc_index) const {
  if (plane->type() == HardwareDisplayPlane::kCursor ||
      !plane->CanUseForCrtc(crtc_index))
    return false;

  // When using legacy kms we always scanout only one plane (the primary),
  // and we always use the opaque fb. Refer to SetPlaneData above.
  const uint32_t format = overlay.buffer->opaque_framebuffer_pixel_format();
  return plane->IsSupportedFormat(format);
}

bool HardwareDisplayPlaneManagerLegacy::CommitColorMatrix(
    const CrtcProperties& crtc_props) {
  return drm_->SetObjectProperty(crtc_props.id, DRM_MODE_OBJECT_CRTC,
                                 crtc_props.ctm.id, crtc_props.ctm.value);
}

bool HardwareDisplayPlaneManagerLegacy::CommitGammaCorrection(
    const CrtcProperties& crtc_props) {
  DCHECK(crtc_props.degamma_lut.id || crtc_props.gamma_lut.id);

  if (crtc_props.degamma_lut.id) {
    int ret = drm_->SetObjectProperty(crtc_props.id, DRM_MODE_OBJECT_CRTC,
                                      crtc_props.degamma_lut.id,
                                      crtc_props.degamma_lut.value);
    if (ret < 0) {
      LOG(ERROR) << "Failed to set DEGAMMA_LUT property for crtc="
                 << crtc_props.id;
      return false;
    }
  }

  if (crtc_props.gamma_lut.id) {
    int ret = drm_->SetObjectProperty(crtc_props.id, DRM_MODE_OBJECT_CRTC,
                                      crtc_props.gamma_lut.id,
                                      crtc_props.gamma_lut.value);
    if (ret < 0) {
      LOG(ERROR) << "Failed to set GAMMA_LUT property for crtc="
                 << crtc_props.id;
      return false;
    }
  }

  return true;
}

}  // namespace ui
