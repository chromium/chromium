// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"

#include <sync/sync.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <memory>
#include <sstream>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/platform_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
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

gfx::GpuFenceHandle CreateMergedGpuFenceFromFDs(
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

  gfx::GpuFenceHandle handle;
  if (merged_fd.is_valid())
    handle.Adopt(std::move(merged_fd));

  return handle;
}

std::vector<uint32_t> GetCrtcIdsOfPlanes(
    const HardwareDisplayPlaneList& plane_list) {
  std::vector<uint32_t> crtcs;
  for (HardwareDisplayPlane* plane : plane_list.plane_list) {
    HardwareDisplayPlaneAtomic* atomic_plane =
        static_cast<HardwareDisplayPlaneAtomic*>(plane);
    if (crtcs.empty() || crtcs.back() != atomic_plane->AssignedCrtcId())
      crtcs.push_back(atomic_plane->AssignedCrtcId());
  }
  return crtcs;
}

bool AddPendingCrtcProperty(drmModeAtomicReq* property_set,
                            uint32_t crtc_id,
                            DrmWrapper::Property& prop,
                            std::optional<ScopedDrmPropertyBlob>& pending_blob,
                            std::vector<ScopedDrmPropertyBlob>& pending_blobs) {
  // If `pending_blob` is std::nullopt then don't change the property.
  if (!pending_blob.has_value()) {
    return true;
  }

  // Take the pending blob. If we successfully set it, we'll add it to
  // `pending_blobs`.
  ScopedDrmPropertyBlob blob = std::move(pending_blob.value());
  pending_blob = std::nullopt;
  if (!prop.id) {
    return true;
  }

  // Update the CRTC property and add the change to the commit.
  if (blob) {
    prop.value = blob->id();
  } else {
    // If the blob was nullptr, then un-set the property.
    prop.value = 0;
  }
  int ret =
      drmModeAtomicAddProperty(property_set, crtc_id, prop.id, prop.value);
  if (ret < 0) {
    LOG(ERROR) << "Failed to set CTM property for crtc=" << crtc_id;
    return false;
  }

  // Save the pending blob in `pending_blobs` so that it can be freed after
  // `property_set` is committed.
  pending_blobs.push_back(std::move(blob));
  return true;
}

bool AddAllPendingCrtcProperties(
    drmModeAtomicReq* property_set,
    HardwareDisplayPlaneManager::CrtcState& crtc_state,
    std::vector<ScopedDrmPropertyBlob>& pending_blobs) {
  bool result = true;
  auto& crtc_props = crtc_state.properties;

  if (!AddPendingCrtcProperty(property_set, crtc_props.id, crtc_props.ctm,
                              crtc_state.pending_ctm_blob, pending_blobs)) {
    LOG(ERROR) << "Failed to set CTM property for crtc=" << crtc_props.id;
    result = false;
  }

  if (!AddPendingCrtcProperty(
          property_set, crtc_props.id, crtc_props.degamma_lut,
          crtc_state.pending_degamma_lut_blob, pending_blobs)) {
    LOG(ERROR) << "Failed to set DEGAMMA_LUT property for crtc="
               << crtc_props.id;
    result = false;
  }

  if (!AddPendingCrtcProperty(property_set, crtc_props.id, crtc_props.gamma_lut,
                              crtc_state.pending_gamma_lut_blob,
                              pending_blobs)) {
    LOG(ERROR) << "Failed to set GAMMA_LUT property for crtc=" << crtc_props.id;
    result = false;
  }

  return result;
}

// Resets |plane|'s properties if it is not found in any
// HardwareDisplayPlaneList::plane_list of |plane_lists|, as it is not currently
// being used. Returns true if the properties were reset.
bool ResetPlanePropsIfUnused(
    const base::flat_set<HardwareDisplayPlaneList*>& plane_lists,
    HardwareDisplayPlane* plane) {
  // When checking if old plane is being used for the current frame, all
  // current frames must be checked, not just the current |plane_list| as
  // the plane might have migrated to another CRTC.
  for (HardwareDisplayPlaneList* plane_list : plane_lists) {
    if (base::Contains(plane_list->plane_list, plane)) {
      return false;
    }
  }

  // |plane| is shared state between |old_plane_list| and |plane_list|.
  // When we call BeginFrame(), we reset in_use since we need to be able to
  // allocate the planes as needed. The current frame might not need to use
  // |plane|, thus |plane->in_use()| would be false even though the previous
  // frame used it. It's existence in |old_plane_list| is sufficient to
  // signal that |plane| was in use previously.
  HardwareDisplayPlaneAtomic* atomic_plane =
      static_cast<HardwareDisplayPlaneAtomic*>(plane);
  atomic_plane->AssignDisableProps();
  return true;
}

}  // namespace

HardwareDisplayPlaneManagerAtomic::HardwareDisplayPlaneManagerAtomic(
    DrmDevice* drm)
    : HardwareDisplayPlaneManager(drm) {}

HardwareDisplayPlaneManagerAtomic::~HardwareDisplayPlaneManagerAtomic() =
    default;

bool HardwareDisplayPlaneManagerAtomic::SetCrtcProps(
    drmModeAtomicReq* atomic_request,
    uint32_t crtc_id,
    bool set_active,
    uint32_t mode_id,
    bool enable_vrr) {
  // Only making a copy here to retrieve the the props IDs. The state will be
  // updated only after a successful modeset.
  CrtcProperties modeset_props = GetCrtcStateForCrtcId(crtc_id).properties;
  modeset_props.active.value = static_cast<uint64_t>(set_active);
  modeset_props.mode_id.value = mode_id;
  modeset_props.vrr_enabled.value = enable_vrr;

  bool status =
      AddPropertyIfValid(atomic_request, crtc_id, modeset_props.active);
  status &= AddPropertyIfValid(atomic_request, crtc_id, modeset_props.mode_id);
  status &=
      AddPropertyIfValid(atomic_request, crtc_id, modeset_props.vrr_enabled);
  return status;
}

bool HardwareDisplayPlaneManagerAtomic::SetConnectorProps(
    drmModeAtomicReq* atomic_request,
    uint32_t connector_id,
    uint32_t crtc_id) {
  auto connector_index = LookupConnectorIndex(connector_id);
  DCHECK(connector_index.has_value());
  // Only making a copy here to retrieve the the props IDs. The state will be
  // updated only after a successful modeset.
  ConnectorProperties connector_props = connectors_props_[*connector_index];
  connector_props.crtc_id.value = crtc_id;
  // Set link-status to DRM_MODE_LINK_STATUS_GOOD when a connector is connected
  // and has modes. In case a link training has failed and link-status is now
  // BAD, the kernel expects the userspace to reset it to GOOD; otherwise, it
  // will ignore modeset requests which have the same mode as the reported bad
  // status. However, if a connector is marked connected but has no modes, it
  // effectively has a bandwidth of 0Gbps and failed all link training
  // attempts. Leave it in link_status bad, since resetting the connector's
  // resources in DRM should not be affected by this state.
  // https://www.kernel.org/doc/html/latest/gpu/drm-kms.html#standard-connector-properties
  if (connector_props.connection == DRM_MODE_CONNECTED &&
      connector_props.count_modes != 0) {
    connector_props.link_status.value = DRM_MODE_LINK_STATUS_GOOD;
  }

  bool status =
      AddPropertyIfValid(atomic_request, connector_id, connector_props.crtc_id);
  status &= AddPropertyIfValid(atomic_request, connector_id,
                               connector_props.link_status);
  return status;
}

bool HardwareDisplayPlaneManagerAtomic::Commit(CommitRequest commit_request,
                                               uint32_t flags) {
  bool is_testing = flags & DRM_MODE_ATOMIC_TEST_ONLY;
  bool status = true;

  std::vector<ScopedDrmPropertyBlob> scoped_blobs;

  base::flat_set<HardwareDisplayPlaneList*> enable_planes_lists;
  base::flat_set<HardwareDisplayPlaneList*> all_planes_lists;

  ScopedDrmAtomicReqPtr atomic_request(drmModeAtomicAlloc());

  for (const auto& crtc_request : commit_request) {
    if (crtc_request.plane_list())
      all_planes_lists.insert(crtc_request.plane_list());

    uint32_t mode_id = 0;
    if (crtc_request.should_enable_crtc()) {
      auto mode_blob = drm_->CreatePropertyBlob(&crtc_request.mode(),
                                                sizeof(crtc_request.mode()));
      status &= (mode_blob != nullptr);
      if (mode_blob) {
        scoped_blobs.push_back(std::move(mode_blob));
        mode_id = scoped_blobs.back()->id();
      }
    }

    uint32_t crtc_id = crtc_request.crtc_id();

    status &= SetCrtcProps(atomic_request.get(), crtc_id,
                           crtc_request.should_enable_crtc(), mode_id,
                           crtc_request.enable_vrr());
    status &=
        SetConnectorProps(atomic_request.get(), crtc_request.connector_id(),
                          crtc_request.should_enable_crtc() * crtc_id);

    if (crtc_request.should_enable_crtc()) {
      DCHECK(crtc_request.plane_list());
      if (!AssignOverlayPlanes(crtc_request.plane_list(),
                               crtc_request.overlays(), crtc_id)) {
        LOG_IF(ERROR, !is_testing) << "Failed to Assign Overlay Planes";
        status = false;
      }
      enable_planes_lists.insert(crtc_request.plane_list());
    }
  }

  // TODO(markyacoub): Ideally this doesn't need to be a separate step. It
  // should all be handled in Set{Crtc,Connector,Plane}Props() modulo some state
  // tracking changes that should be done post commit. Break it apart when both
  // Commit() are consolidated.
  std::vector<ScopedDrmPropertyBlob> pending_blobs;
  SetAtomicPropsForCommit(enable_planes_lists, atomic_request.get(),
                          pending_blobs, is_testing);

  // TODO(markyacoub): failed |status|'s should be made as DCHECKs. The only
  // reason some of these would be failing is OOM. If we OOM-ed there's no point
  // in trying to recover.
  if (!status || !drm_->CommitProperties(atomic_request.get(), flags,
                                         commit_request.size(), nullptr)) {
    if (is_testing)
      VPLOG(2) << "Modeset Test is rejected.";
    else
      PLOG(ERROR) << "Failed to commit properties for modeset.";

    for (HardwareDisplayPlaneList* list : all_planes_lists)
      ResetCurrentPlaneList(list);

    return false;
  }

  if (!is_testing)
    UpdateCrtcAndPlaneStatesAfterModeset(commit_request);

  for (HardwareDisplayPlaneList* list : enable_planes_lists) {
    if (!is_testing)
      list->plane_list.swap(list->old_plane_list);
    list->plane_list.clear();
  }

  return true;
}

void HardwareDisplayPlaneManagerAtomic::SetAtomicPropsForCommit(
    const base::flat_set<HardwareDisplayPlaneList*>& plane_lists,
    drmModeAtomicReq* atomic_request,
    std::vector<ScopedDrmPropertyBlob>& pending_blobs,
    bool test_only) {
  for (HardwareDisplayPlaneList* plane_list : plane_lists) {
    // Set properties of planes being used in this commit.
    for (HardwareDisplayPlane* plane : plane_list->plane_list) {
      HardwareDisplayPlaneAtomic* atomic_plane =
          static_cast<HardwareDisplayPlaneAtomic*>(plane);
      atomic_plane->SetPlaneProps(atomic_request);
    }

    // Reset properties of old planes not being used in this commit.
    for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
      if (ResetPlanePropsIfUnused(plane_lists, plane)) {
        HardwareDisplayPlaneAtomic* atomic_plane =
            static_cast<HardwareDisplayPlaneAtomic*>(plane);
        atomic_plane->SetPlaneProps(atomic_request);
      }
    }

    const std::vector<uint32_t>& crtcs = GetCrtcIdsOfPlanes(*plane_list);
    for (uint32_t crtc : crtcs) {
      // This is actually pretty important, since these CRTC lists are generated
      // from planes who may or may not have crtcs ids set to 0 when not in use
      // (or when waiting for vblank).
      // TODO(b/189073356): See if we can use a DCHECK after we clean things up
      auto idx = LookupCrtcIndex(crtc);
      if (!idx) {
        continue;
      }

      AddPropertyIfValid(atomic_request, crtc,
                         crtc_state_[*idx].properties.background_color);
      if (base::FeatureList::IsEnabled(
              display::features::kCtmColorManagement)) {
        AddAllPendingCrtcProperties(atomic_request, crtc_state_[*idx],
                                    pending_blobs);
      }
    }

    if (test_only) {
      for (auto* plane : plane_list->plane_list) {
        plane->set_in_use(false);
      }
      for (auto* plane : plane_list->old_plane_list) {
        plane->set_in_use(true);
      }
    }
  }
}

bool HardwareDisplayPlaneManagerAtomic::Commit(
    HardwareDisplayPlaneList* plane_list,
    scoped_refptr<PageFlipRequest> page_flip_request,
    gfx::GpuFenceHandle* release_fence) {
  bool test_only = !page_flip_request;

  ScopedDrmAtomicReqPtr atomic_property_set(drmModeAtomicAlloc());
  std::vector<ScopedDrmPropertyBlob> pending_blobs;
  SetAtomicPropsForCommit(base::flat_set<HardwareDisplayPlaneList*>{plane_list},
                          atomic_property_set.get(), pending_blobs, test_only);

  // After we perform the atomic commit, and if the caller has requested an
  // out-fence, the out_fence_fds vector will contain any provided out-fence
  // fds for the crtcs, therefore the scope of out_fence_fds needs to outlive
  // the CommitProperties call. CommitProperties will write into the Receiver,
  // and the Receiver's destructor writes into ScopedFD, so we need to ensure
  // that the Receivers are destructed before we attempt to use their
  // corresponding ScopedFDs.
  std::vector<uint32_t> crtcs = GetCrtcIdsOfPlanes(*plane_list);
  std::vector<base::ScopedFD> out_fence_fds;
  {
    std::vector<base::ScopedFD::Receiver> out_fence_fd_receivers;
    if (release_fence) {
      if (!AddOutFencePtrProperties(atomic_property_set.get(),
                                    crtcs, &out_fence_fds,
                                    &out_fence_fd_receivers)) {
        ResetCurrentPlaneList(plane_list);
        return false;
      }
    }

    uint32_t flags =
        test_only ? DRM_MODE_ATOMIC_TEST_ONLY : DRM_MODE_ATOMIC_NONBLOCK;

    if (!drm_->CommitProperties(atomic_property_set.get(), flags,
                                crtcs.size(), page_flip_request)) {
      if (!test_only) {
        std::ostringstream ss;
        ss << "Failed to commit properties for page flip: [";
        for (const auto* plane : plane_list->plane_list) {
          plane->DumpProperties(ss);
          ss << (plane != plane_list->plane_list.back() ? ", " : "]");
        }
        ss << ", Current properties in scanout: [";
        for (const auto* plane : plane_list->old_plane_list) {
          plane->DumpProperties(ss);
          ss << (plane != plane_list->old_plane_list.back() ? ", " : "]");
        }
        PLOG(ERROR) << ss.str();
      } else {
        VPLOG(2) << "Failed to commit properties for MODE_ATOMIC_TEST_ONLY.";
      }

      ResetCurrentPlaneList(plane_list);
      return false;
    }
  }

  if (release_fence)
    *release_fence = CreateMergedGpuFenceFromFDs(std::move(out_fence_fds));

  if (!test_only)
    plane_list->plane_list.swap(plane_list->old_plane_list);

  plane_list->plane_list.clear();
  return true;
}

bool HardwareDisplayPlaneManagerAtomic::TestSeamlessMode(
    int32_t crtc_id,
    const drmModeModeInfo& mode) {
  TRACE_EVENT1("drm", "HardwareDisplayPlaneManagerAtomic::TestSeamlessMode",
               "crtc_id", crtc_id);
  ScopedDrmAtomicReqPtr atomic_request(drmModeAtomicAlloc());

  ScopedDrmPropertyBlob mode_blob =
      drm_->CreatePropertyBlob(&mode, sizeof(mode));
  if (mode_blob == nullptr) {
    LOG(WARNING) << "Failed to create PropertyBlob for mode.";
    return false;
  }

  // Get a copy of the modeset_props. The actual state will not be updated until
  // after a successful modeset.
  CrtcProperties modeset_props = GetCrtcStateForCrtcId(crtc_id).properties;
  modeset_props.mode_id.value = mode_blob->id();

  if (!AddPropertyIfValid(atomic_request.get(), crtc_id,
                          modeset_props.mode_id)) {
    LOG(WARNING) << "Failed to add mode_id property";
    return false;
  }

  const int num_crtcs = 1;
  const uint32_t seamless_test_flags = DRM_MODE_ATOMIC_TEST_ONLY;
  return drm_->CommitProperties(atomic_request.get(), seamless_test_flags,
                                num_crtcs, nullptr);
}

bool HardwareDisplayPlaneManagerAtomic::DisableOverlayPlanes(
    HardwareDisplayPlaneList* plane_list) {
  bool ret = true;

  if (!plane_list->old_plane_list.empty()) {
    ScopedDrmAtomicReqPtr atomic_property_set(drmModeAtomicAlloc());
    for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
      plane->set_in_use(false);
      plane->set_owning_crtc(0);

      HardwareDisplayPlaneAtomic* atomic_plane =
          static_cast<HardwareDisplayPlaneAtomic*>(plane);
      atomic_plane->AssignPlaneProps(
          nullptr, 0, 0, gfx::Rect(), gfx::Rect(), gfx::Rect(),
          gfx::OVERLAY_TRANSFORM_NONE, gfx::ColorSpace(),
          base::kInvalidPlatformFile, DRM_FORMAT_INVALID, false);
      atomic_plane->SetPlaneProps(atomic_property_set.get());
    }
    ret = drm_->CommitProperties(atomic_property_set.get(),
                                 /*flags=*/0, 0 /*unused*/, nullptr);
    PLOG_IF(ERROR, !ret) << "Failed to commit properties for page flip.";
  }
  return ret;
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(planes)));
}

bool HardwareDisplayPlaneManagerAtomic::SetPlaneData(
    HardwareDisplayPlaneList*,
    HardwareDisplayPlane* hw_plane,
    const DrmOverlayPlane& overlay,
    uint32_t crtc_id,
    std::optional<gfx::Point> crtc_offset,
    const gfx::Rect& src_rect) {
  HardwareDisplayPlaneAtomic* atomic_plane =
      static_cast<HardwareDisplayPlaneAtomic*>(hw_plane);
  uint32_t framebuffer_id = overlay.enable_blend
                                ? overlay.buffer->framebuffer_id()
                                : overlay.buffer->opaque_framebuffer_id();
  int fence_fd = base::kInvalidPlatformFile;

  if (overlay.gpu_fence) {
    const auto& gpu_fence_handle = overlay.gpu_fence->GetGpuFenceHandle();
    fence_fd = gpu_fence_handle.Peek();
  }

  gfx::Rect crtc_rect = overlay.display_bounds;
  if (crtc_offset.has_value()) {
    // Move the CRTC offset by |crtc_offset|. Note that |crtc_offset.origin()|
    // may not be (0,0).
    crtc_rect += {crtc_offset->x(), crtc_offset->y()};
  }

  if (!atomic_plane->AssignPlaneProps(
          drm_, crtc_id, framebuffer_id, crtc_rect, src_rect,
          overlay.damage_rect, overlay.plane_transform, overlay.color_space,
          fence_fd, overlay.buffer->framebuffer_pixel_format(),
          overlay.buffer->is_original_buffer())) {
    return false;
  }
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

bool HardwareDisplayPlaneManagerAtomic::CommitPendingCrtcState(
    CrtcState& crtc_state) {
  std::vector<ScopedDrmPropertyBlob> pending_blobs;
  ScopedDrmAtomicReqPtr property_set(drmModeAtomicAlloc());

  bool result = AddAllPendingCrtcProperties(property_set.get(), crtc_state,
                                            pending_blobs);

  // If we aren't committing any new blobs, early-out.
  if (pending_blobs.empty()) {
    return result;
  }

  // If we try to do this in a non-blocking fashion this can return EBUSY since
  // there is a pending page flip. Do a blocking commit (the same as the legacy
  // API) to ensure the properties are applied.
  // TODO(dnicoara): Should cache these values locally and aggregate them with
  // the page flip event otherwise this "steals" a vsync to apply the property.
  if (!drm_->CommitProperties(property_set.get(), 0, 0, nullptr)) {
    LOG(ERROR) << "Failed to commit properties for crtc="
               << crtc_state.properties.id;
    result = false;
  }

  return result;
}

bool HardwareDisplayPlaneManagerAtomic::AddOutFencePtrProperties(
    drmModeAtomicReq* property_set,
    const std::vector<uint32_t>& crtcs,
    std::vector<base::ScopedFD>* out_fence_fds,
    std::vector<base::ScopedFD::Receiver>* out_fence_fd_receivers) {
  // Reserve space in vector to ensure no reallocation will take place
  // and thus all pointers to elements will remain valid
  DCHECK(out_fence_fds->empty());
  DCHECK(out_fence_fd_receivers->empty());
  out_fence_fds->reserve(crtcs.size());
  out_fence_fd_receivers->reserve(crtcs.size());

  for (uint32_t crtc : crtcs) {
    const auto crtc_index = LookupCrtcIndex(crtc);
    DCHECK(crtc_index.has_value());
    const auto out_fence_ptr_id =
        crtc_state_[*crtc_index].properties.out_fence_ptr.id;

    if (out_fence_ptr_id > 0) {
      out_fence_fds->push_back(base::ScopedFD());
      out_fence_fd_receivers->emplace_back(out_fence_fds->back());
      // Add the OUT_FENCE_PTR property pointing to the memory location
      // to save the out-fence fd into for this crtc. Note that
      // the out-fence fd is produced only after we perform the atomic
      // commit, so we need to ensure that the pointer remains valid
      // until then.
      int ret = drmModeAtomicAddProperty(
          property_set, crtc, out_fence_ptr_id,
          reinterpret_cast<uint64_t>(out_fence_fd_receivers->back().get()));
      if (ret < 0) {
        LOG(ERROR) << "Failed to set OUT_FENCE_PTR property for crtc=" << crtc
                   << " error=" << -ret;
        out_fence_fd_receivers->pop_back();
        out_fence_fds->pop_back();
        return false;
      }
    }
  }

  return true;
}

}  // namespace ui
