// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"

#include <sync/sync.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/platform_file.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

namespace ui {

namespace {

std::unique_ptr<gfx::GpuFence> CreateMergedGpuFenceFromFDs(
    std::vector<base::ScopedFD> fence_fds) {
  base::ScopedFD merged_fd;

  for (auto& fd : fence_fds) {
    if (merged_fd.is_valid()) {
      merged_fd.reset(sync_merge("", merged_fd.get(), fd.get()));
      DCHECK(merged_fd.is_valid());
    } else {
      merged_fd = std::move(fd);
    }
  }

  if (merged_fd.is_valid()) {
    gfx::GpuFenceHandle handle;
    handle.type = gfx::GpuFenceHandleType::kAndroidNativeFenceSync;
    handle.native_fd = base::FileDescriptor(std::move(merged_fd));
    return std::make_unique<gfx::GpuFence>(handle);
  }

  return nullptr;
}

}  // namespace

HardwareDisplayPlaneManagerAtomic::HardwareDisplayPlaneManagerAtomic(
    DrmDevice* drm)
    : HardwareDisplayPlaneManager(drm) {}

HardwareDisplayPlaneManagerAtomic::~HardwareDisplayPlaneManagerAtomic() {
}

bool HardwareDisplayPlaneManagerAtomic::Commit(
    HardwareDisplayPlaneList* plane_list,
    scoped_refptr<PageFlipRequest> page_flip_request,
    std::unique_ptr<gfx::GpuFence>* out_fence) {
  bool test_only = !page_flip_request;
  for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
    if (!base::Contains(plane_list->plane_list, plane)) {
      // This plane is being released, so we need to zero it.
      plane->set_in_use(false);
      HardwareDisplayPlaneAtomic* atomic_plane =
          static_cast<HardwareDisplayPlaneAtomic*>(plane);
      atomic_plane->SetPlaneData(
          plane_list->atomic_property_set.get(), 0, 0, gfx::Rect(), gfx::Rect(),
          gfx::OVERLAY_TRANSFORM_NONE, base::kInvalidPlatformFile);
    }
  }

  std::vector<CrtcController*> crtcs;
  for (HardwareDisplayPlane* plane : plane_list->plane_list) {
    HardwareDisplayPlaneAtomic* atomic_plane =
        static_cast<HardwareDisplayPlaneAtomic*>(plane);
    if (crtcs.empty() || crtcs.back() != atomic_plane->crtc())
      crtcs.push_back(atomic_plane->crtc());
  }

  drmModeAtomicReqPtr request = plane_list->atomic_property_set.get();
  for (auto* const crtc : crtcs) {
    int idx = LookupCrtcIndex(crtc->crtc());

#if defined(COMMIT_PROPERTIES_ON_PAGE_FLIP)
    // Apply all CRTC properties in the page-flip so we don't block the
    // swap chain for a vsync.
    // TODO(dnicoara): See if we can apply these properties async using
    // DRM_MODE_ATOMIC_ASYNC_UPDATE flag when committing.
    AddPropertyIfValid(request, crtc->crtc(),
                       crtc_state_[idx].properties.degamma_lut);
    AddPropertyIfValid(request, crtc->crtc(),
                       crtc_state_[idx].properties.gamma_lut);
    AddPropertyIfValid(request, crtc->crtc(), crtc_state_[idx].properties.ctm);
#endif

    AddPropertyIfValid(request, crtc->crtc(),
                       crtc_state_[idx].properties.background_color);
  }

  if (test_only) {
    for (HardwareDisplayPlane* plane : plane_list->plane_list) {
      plane->set_in_use(false);
    }
  } else {
    plane_list->plane_list.swap(plane_list->old_plane_list);
  }

  uint32_t flags = 0;
  if (test_only) {
    flags = DRM_MODE_ATOMIC_TEST_ONLY;
  } else {
    flags = DRM_MODE_ATOMIC_NONBLOCK;
  }

  // After we perform the atomic commit, and if the caller has requested an
  // out-fence, the out_fence_fds vector will contain any provided out-fence
  // fds for the crtcs, therefore the scope of out_fence_fds needs to outlive
  // the CommitProperties call. CommitProperties will write into the Receiver,
  // and the Receiver's destructor writes into ScopedFD, so we need to ensure
  // that the Receivers are destructed before we attempt to use their
  // corresponding ScopedFDs.
  std::vector<base::ScopedFD> out_fence_fds;
  {
    std::vector<base::ScopedFD::Receiver> out_fence_fd_receivers;
    if (out_fence) {
      if (!AddOutFencePtrProperties(plane_list->atomic_property_set.get(),
                                    crtcs, &out_fence_fds,
                                    &out_fence_fd_receivers)) {
        ResetCurrentPlaneList(plane_list);
        return false;
      }
    }

    if (!drm_->CommitProperties(plane_list->atomic_property_set.get(), flags,
                                crtcs.size(), page_flip_request)) {
      if (!test_only) {
        PLOG(ERROR) << "Failed to commit properties for page flip.";
      } else {
        VPLOG(2) << "Failed to commit properties for MODE_ATOMIC_TEST_ONLY.";
      }

      ResetCurrentPlaneList(plane_list);
      return false;
    }
  }

  if (out_fence)
    *out_fence = CreateMergedGpuFenceFromFDs(std::move(out_fence_fds));

  plane_list->plane_list.clear();
  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
  return true;
}

bool HardwareDisplayPlaneManagerAtomic::DisableOverlayPlanes(
    HardwareDisplayPlaneList* plane_list) {
  for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
    if (plane->type() != HardwareDisplayPlane::kOverlay)
      continue;
    plane->set_in_use(false);
    plane->set_owning_crtc(0);

    HardwareDisplayPlaneAtomic* atomic_plane =
        static_cast<HardwareDisplayPlaneAtomic*>(plane);
    atomic_plane->SetPlaneData(
        plane_list->atomic_property_set.get(), 0, 0, gfx::Rect(), gfx::Rect(),
        gfx::OVERLAY_TRANSFORM_NONE, base::kInvalidPlatformFile);
  }
  bool ret = drm_->CommitProperties(plane_list->atomic_property_set.get(),
                                    DRM_MODE_ATOMIC_NONBLOCK, 0, nullptr);
  PLOG_IF(ERROR, !ret) << "Failed to commit properties for page flip.";

  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
  return ret;
}

bool HardwareDisplayPlaneManagerAtomic::SetColorCorrectionOnAllCrtcPlanes(
    uint32_t crtc_id,
    ScopedDrmColorCtmPtr ctm_blob_data) {
  ScopedDrmAtomicReqPtr property_set(drmModeAtomicAlloc());
  ScopedDrmPropertyBlob property_blob(
      drm_->CreatePropertyBlob(ctm_blob_data.get(), sizeof(drm_color_ctm)));

  const int crtc_index = LookupCrtcIndex(crtc_id);
  DCHECK_GE(crtc_index, 0);

  for (auto& plane : planes_) {
    HardwareDisplayPlaneAtomic* atomic_plane =
        static_cast<HardwareDisplayPlaneAtomic*>(plane.get());

    // This assumes planes can only belong to one crtc.
    if (!atomic_plane->CanUseForCrtc(crtc_index))
      continue;

    if (!atomic_plane->SetPlaneCtm(property_set.get(), property_blob->id())) {
      LOG(ERROR) << "Failed to set PLANE_CTM for plane=" << atomic_plane->id();
      return false;
    }
  }

  return drm_->CommitProperties(property_set.get(), DRM_MODE_ATOMIC_NONBLOCK, 0,
                                nullptr);
}

bool HardwareDisplayPlaneManagerAtomic::ValidatePrimarySize(
    const DrmOverlayPlane& primary,
    const drmModeModeInfo& mode) {
  // Atomic KMS allows for primary planes that don't match the size of
  // the current mode.
  return true;
}

void HardwareDisplayPlaneManagerAtomic::RequestPlanesReadyCallback(
    DrmOverlayPlaneList planes,
    base::OnceCallback<void(DrmOverlayPlaneList planes)> callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(planes)));
}

bool HardwareDisplayPlaneManagerAtomic::SetPlaneData(
    HardwareDisplayPlaneList* plane_list,
    HardwareDisplayPlane* hw_plane,
    const DrmOverlayPlane& overlay,
    uint32_t crtc_id,
    const gfx::Rect& src_rect,
    CrtcController* crtc) {
  HardwareDisplayPlaneAtomic* atomic_plane =
      static_cast<HardwareDisplayPlaneAtomic*>(hw_plane);
  uint32_t framebuffer_id = overlay.enable_blend
                                ? overlay.buffer->framebuffer_id()
                                : overlay.buffer->opaque_framebuffer_id();
  int fence_fd = base::kInvalidPlatformFile;

  if (overlay.gpu_fence) {
    const auto& gpu_fence_handle = overlay.gpu_fence->GetGpuFenceHandle();
    if (gpu_fence_handle.type !=
        gfx::GpuFenceHandleType::kAndroidNativeFenceSync) {
      LOG(ERROR) << "Received invalid gpu fence";
      return false;
    }
    fence_fd = gpu_fence_handle.native_fd.fd;
  }

  if (!atomic_plane->SetPlaneData(plane_list->atomic_property_set.get(),
                                  crtc_id, framebuffer_id,
                                  overlay.display_bounds, src_rect,
                                  overlay.plane_transform, fence_fd)) {
    LOG(ERROR) << "Failed to set plane properties";
    return false;
  }
  atomic_plane->set_crtc(crtc);
  return true;
}

bool HardwareDisplayPlaneManagerAtomic::InitializePlanes() {
  ScopedDrmPlaneResPtr plane_resources = drm_->GetPlaneResources();
  if (!plane_resources) {
    PLOG(ERROR) << "Failed to get plane resources.";
    return false;
  }

  for (uint32_t i = 0; i < plane_resources->count_planes; ++i) {
    std::unique_ptr<HardwareDisplayPlane> plane(
        CreatePlane(plane_resources->planes[i]));

    if (plane->Initialize(drm_))
      planes_.push_back(std::move(plane));
  }

  return true;
}

std::unique_ptr<HardwareDisplayPlane>
HardwareDisplayPlaneManagerAtomic::CreatePlane(uint32_t plane_id) {
  return std::make_unique<HardwareDisplayPlaneAtomic>(plane_id);
}

bool HardwareDisplayPlaneManagerAtomic::CommitColorMatrix(
    const CrtcProperties& crtc_props) {
  DCHECK(crtc_props.ctm.id);
#if !defined(COMMIT_PROPERTIES_ON_PAGE_FLIP)
  ScopedDrmAtomicReqPtr property_set(drmModeAtomicAlloc());
  int ret = drmModeAtomicAddProperty(property_set.get(), crtc_props.id,
                                     crtc_props.ctm.id, crtc_props.ctm.value);
  if (ret < 0) {
    LOG(ERROR) << "Failed to set CTM property for crtc=" << crtc_props.id;
    return false;
  }

  // If we try to do this in a non-blocking fashion this can return EBUSY since
  // there is a pending page flip. Do a blocking commit (the same as the legacy
  // API) to ensure the properties are applied.
  // TODO(dnicoara): Should cache these values locally and aggregate them with
  // the page flip event otherwise this "steals" a vsync to apply the property.
  return drm_->CommitProperties(property_set.get(), 0, 0, nullptr);
#else
  return true;
#endif
}

bool HardwareDisplayPlaneManagerAtomic::CommitGammaCorrection(
    const CrtcProperties& crtc_props) {
  DCHECK(crtc_props.degamma_lut.id || crtc_props.gamma_lut.id);
#if !defined(COMMIT_PROPERTIES_ON_PAGE_FLIP)
  ScopedDrmAtomicReqPtr property_set(drmModeAtomicAlloc());
  if (crtc_props.degamma_lut.id) {
    int ret = drmModeAtomicAddProperty(property_set.get(), crtc_props.id,
                                       crtc_props.degamma_lut.id,
                                       crtc_props.degamma_lut.value);
    if (ret < 0) {
      LOG(ERROR) << "Failed to set DEGAMMA_LUT property for crtc="
                 << crtc_props.id;
      return false;
    }
  }

  if (crtc_props.gamma_lut.id) {
    int ret = drmModeAtomicAddProperty(property_set.get(), crtc_props.id,
                                       crtc_props.gamma_lut.id,
                                       crtc_props.gamma_lut.value);
    if (ret < 0) {
      LOG(ERROR) << "Failed to set GAMMA_LUT property for crtc="
                 << crtc_props.id;
      return false;
    }
  }

  // If we try to do this in a non-blocking fashion this can return EBUSY since
  // there is a pending page flip. Do a blocking commit (the same as the legacy
  // API) to ensure the properties are applied.
  // TODO(dnicoara): Should cache these values locally and aggregate them with
  // the page flip event otherwise this "steals" a vsync to apply the property.
  return drm_->CommitProperties(property_set.get(), 0, 0, nullptr);
#else
  return true;
#endif
}

bool HardwareDisplayPlaneManagerAtomic::AddOutFencePtrProperties(
    drmModeAtomicReqPtr property_set,
    const std::vector<CrtcController*>& crtcs,
    std::vector<base::ScopedFD>* out_fence_fds,
    std::vector<base::ScopedFD::Receiver>* out_fence_fd_receivers) {
  // Reserve space in vector to ensure no reallocation will take place
  // and thus all pointers to elements will remain valid
  DCHECK(out_fence_fds->empty());
  DCHECK(out_fence_fd_receivers->empty());
  out_fence_fds->reserve(crtcs.size());
  out_fence_fd_receivers->reserve(crtcs.size());

  for (auto* crtc : crtcs) {
    const auto crtc_index = LookupCrtcIndex(crtc->crtc());
    DCHECK_GE(crtc_index, 0);
    const auto out_fence_ptr_id =
        crtc_state_[crtc_index].properties.out_fence_ptr.id;

    if (out_fence_ptr_id > 0) {
      out_fence_fds->push_back(base::ScopedFD());
      out_fence_fd_receivers->emplace_back(out_fence_fds->back());
      // Add the OUT_FENCE_PTR property pointing to the memory location
      // to save the out-fence fd into for this crtc. Note that
      // the out-fence fd is produced only after we perform the atomic
      // commit, so we need to ensure that the pointer remains valid
      // until then.
      int ret = drmModeAtomicAddProperty(
          property_set, crtc->crtc(), out_fence_ptr_id,
          reinterpret_cast<uint64_t>(out_fence_fd_receivers->back().get()));
      if (ret < 0) {
        LOG(ERROR) << "Failed to set OUT_FENCE_PTR property for crtc="
                   << crtc->crtc() << " error=" << -ret;
        out_fence_fd_receivers->pop_back();
        out_fence_fds->pop_back();
        return false;
      }
    }
  }

  return true;
}

}  // namespace ui
