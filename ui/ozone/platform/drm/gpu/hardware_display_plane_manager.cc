// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

#include <drm_fourcc.h>
#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"

namespace ui {

HardwareDisplayPlaneList::HardwareDisplayPlaneList() {
  atomic_property_set.reset(drmModeAtomicAlloc());
}

HardwareDisplayPlaneList::~HardwareDisplayPlaneList() = default;

HardwareDisplayPlaneList::PageFlipInfo::PageFlipInfo(uint32_t crtc_id,
                                                     uint32_t framebuffer)
    : crtc_id(crtc_id), framebuffer(framebuffer) {}

HardwareDisplayPlaneList::PageFlipInfo::PageFlipInfo(
    const PageFlipInfo& other) = default;

HardwareDisplayPlaneList::PageFlipInfo::~PageFlipInfo() = default;

HardwareDisplayPlaneManager::CrtcState::CrtcState() = default;

HardwareDisplayPlaneManager::CrtcState::~CrtcState() = default;

HardwareDisplayPlaneManager::CrtcState::CrtcState(CrtcState&&) = default;

HardwareDisplayPlaneManager::HardwareDisplayPlaneManager(DrmDevice* drm)
    : drm_(drm) {}

HardwareDisplayPlaneManager::~HardwareDisplayPlaneManager() = default;

bool HardwareDisplayPlaneManager::Initialize() {
  // Try to get all of the planes if possible, so we don't have to try to
  // discover hidden primary planes.
  has_universal_planes_ =
      drm_->SetCapability(DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

  // This is to test whether or not it is safe to remove non-universal planes
  // supporting code in a following CL. See crbug.com/1129546 for more details.
  CHECK(has_universal_planes_);

  if (!InitializeCrtcState())
    return false;

  if (!InitializePlanes())
    return false;

  std::sort(planes_.begin(), planes_.end(),
            [](const std::unique_ptr<HardwareDisplayPlane>& l,
               const std::unique_ptr<HardwareDisplayPlane>& r) {
              return l->id() < r->id();
            });

  PopulateSupportedFormats();
  return true;
}

std::unique_ptr<HardwareDisplayPlane> HardwareDisplayPlaneManager::CreatePlane(
    uint32_t id) {
  return std::make_unique<HardwareDisplayPlane>(id);
}

HardwareDisplayPlane* HardwareDisplayPlaneManager::FindNextUnusedPlane(
    size_t* index,
    uint32_t crtc_index,
    const DrmOverlayPlane& overlay) const {
  for (size_t i = *index; i < planes_.size(); ++i) {
    auto* plane = planes_[i].get();
    if (!plane->in_use() && IsCompatible(plane, overlay, crtc_index)) {
      *index = i + 1;
      return plane;
    }
  }
  return nullptr;
}

int HardwareDisplayPlaneManager::LookupCrtcIndex(uint32_t crtc_id) const {
  for (size_t i = 0; i < crtc_state_.size(); ++i) {
    if (crtc_state_[i].properties.id == crtc_id)
      return i;
  }
  return -1;
}

int HardwareDisplayPlaneManager::LookupConnectorIndex(
    uint32_t connector_id) const {
  for (size_t i = 0; i < connectors_props_.size(); ++i) {
    if (connectors_props_[i].id == connector_id)
      return i;
  }
  return -1;
}

bool HardwareDisplayPlaneManager::IsCompatible(HardwareDisplayPlane* plane,
                                               const DrmOverlayPlane& overlay,
                                               uint32_t crtc_index) const {
  if (plane->type() == DRM_PLANE_TYPE_CURSOR ||
      !plane->CanUseForCrtc(crtc_index))
    return false;

  const uint32_t format =
      overlay.enable_blend ? overlay.buffer->framebuffer_pixel_format()
                           : overlay.buffer->opaque_framebuffer_pixel_format();
  if (!plane->IsSupportedFormat(format))
    return false;

  // TODO(kalyank): We should check for z-order and any needed transformation
  // support. Driver doesn't expose any property to check for z-order, can we
  // rely on the sorting we do based on plane ids ?

  return true;
}

void HardwareDisplayPlaneManager::PopulateSupportedFormats() {
  std::set<uint32_t> supported_formats;

  for (const auto& plane : planes_) {
    const std::vector<uint32_t>& formats = plane->supported_formats();
    supported_formats.insert(formats.begin(), formats.end());
  }

  supported_formats_.reserve(supported_formats.size());
  supported_formats_.assign(supported_formats.begin(), supported_formats.end());
}

void HardwareDisplayPlaneManager::ResetCurrentPlaneList(
    HardwareDisplayPlaneList* plane_list) const {
  for (auto* hardware_plane : plane_list->plane_list) {
    hardware_plane->set_in_use(false);
    hardware_plane->set_owning_crtc(0);
  }

  plane_list->plane_list.clear();
  plane_list->legacy_page_flips.clear();
  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
}

void HardwareDisplayPlaneManager::RestoreCurrentPlaneList(
    HardwareDisplayPlaneList* plane_list) const {
  for (auto* plane : plane_list->plane_list) {
    plane->set_in_use(false);
  }
  for (auto* plane : plane_list->old_plane_list) {
    plane->set_in_use(true);
  }
  plane_list->plane_list.clear();
  plane_list->legacy_page_flips.clear();
  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
}

void HardwareDisplayPlaneManager::BeginFrame(
    HardwareDisplayPlaneList* plane_list) {
  for (auto* plane : plane_list->old_plane_list) {
    plane->set_in_use(false);
  }
}

bool HardwareDisplayPlaneManager::AssignOverlayPlanes(
    HardwareDisplayPlaneList* plane_list,
    const DrmOverlayPlaneList& overlay_list,
    uint32_t crtc_id) {
  int crtc_index = LookupCrtcIndex(crtc_id);
  if (crtc_index < 0) {
    LOG(ERROR) << "Cannot find crtc " << crtc_id;
    return false;
  }

  size_t plane_idx = 0;
  for (const auto& plane : overlay_list) {
    HardwareDisplayPlane* hw_plane =
        FindNextUnusedPlane(&plane_idx, crtc_index, plane);
    if (!hw_plane) {
      RestoreCurrentPlaneList(plane_list);
      return false;
    }

    gfx::Rect fixed_point_rect;
    const gfx::Size& size = plane.buffer->size();
    gfx::RectF crop_rectf = plane.crop_rect;
    crop_rectf.Scale(size.width(), size.height());
    // DrmOverlayManager::CanHandleCandidate guarantees this is safe.
    gfx::Rect crop_rect = gfx::ToNearestRect(crop_rectf);
    // Convert to 16.16 fixed point required by the DRM overlay APIs.
    fixed_point_rect =
        gfx::Rect(crop_rect.x() << 16, crop_rect.y() << 16,
                  crop_rect.width() << 16, crop_rect.height() << 16);

    if (!SetPlaneData(plane_list, hw_plane, plane, crtc_id, fixed_point_rect)) {
      RestoreCurrentPlaneList(plane_list);
      return false;
    }

    plane_list->plane_list.push_back(hw_plane);
    hw_plane->set_owning_crtc(crtc_id);
    hw_plane->set_in_use(true);
  }
  return true;
}

const std::vector<uint32_t>& HardwareDisplayPlaneManager::GetSupportedFormats()
    const {
  return supported_formats_;
}

std::vector<uint64_t> HardwareDisplayPlaneManager::GetFormatModifiers(
    uint32_t crtc_id,
    uint32_t format) const {
  int crtc_index = LookupCrtcIndex(crtc_id);

  for (const auto& plane : planes_) {
    if (plane->CanUseForCrtc(crtc_index) &&
        plane->type() == DRM_PLANE_TYPE_PRIMARY) {
      return plane->ModifiersForFormat(format);
    }
  }

  return std::vector<uint64_t>();
}

void HardwareDisplayPlaneManager::ResetConnectorsCache(
    const ScopedDrmResourcesPtr& resources) {
  connectors_props_.clear();

  for (int i = 0; i < resources->count_connectors; ++i) {
    ConnectorProperties state_props;
    state_props.id = resources->connectors[i];

    ScopedDrmObjectPropertyPtr props(drm_->GetObjectProperties(
        resources->connectors[i], DRM_MODE_OBJECT_CONNECTOR));
    if (!props) {
      PLOG(ERROR) << "Failed to get Connector properties for connector="
                  << state_props.id;
      continue;
    }
    GetDrmPropertyForName(drm_, props.get(), "CRTC_ID", &state_props.crtc_id);
    DCHECK(!drm_->is_atomic() || state_props.crtc_id.id);

    connectors_props_.emplace_back(std::move(state_props));
  }
}

bool HardwareDisplayPlaneManager::SetColorMatrix(
    uint32_t crtc_id,
    const std::vector<float>& color_matrix) {
  if (color_matrix.empty()) {
    // TODO: Consider allowing an empty matrix to disable the color transform
    // matrix.
    LOG(ERROR) << "CTM is empty. Expected a 3x3 matrix.";
    return false;
  }

  const int crtc_index = LookupCrtcIndex(crtc_id);
  DCHECK_GE(crtc_index, 0);
  CrtcState* crtc_state = &crtc_state_[crtc_index];

  ScopedDrmColorCtmPtr ctm_blob_data = CreateCTMBlob(color_matrix);
  if (!crtc_state->properties.ctm.id)
    return SetColorCorrectionOnAllCrtcPlanes(crtc_id, std::move(ctm_blob_data));

  crtc_state->ctm_blob =
      drm_->CreatePropertyBlob(ctm_blob_data.get(), sizeof(drm_color_ctm));
  crtc_state->properties.ctm.value = crtc_state->ctm_blob->id();
  return CommitColorMatrix(crtc_state->properties);
}

void HardwareDisplayPlaneManager::SetBackgroundColor(
    uint32_t crtc_id,
    const uint64_t background_color) {
  const int crtc_index = LookupCrtcIndex(crtc_id);
  DCHECK_GE(crtc_index, 0);
  CrtcState* crtc_state = &crtc_state_[crtc_index];

  crtc_state->properties.background_color.value = background_color;
}

bool HardwareDisplayPlaneManager::SetGammaCorrection(
    uint32_t crtc_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  const int crtc_index = LookupCrtcIndex(crtc_id);
  if (crtc_index < 0) {
    LOG(ERROR) << "Unknown CRTC ID=" << crtc_id;
    return false;
  }

  CrtcState* crtc_state = &crtc_state_[crtc_index];
  CrtcProperties* crtc_props = &crtc_state->properties;

  if (!degamma_lut.empty() &&
      (!crtc_props->degamma_lut.id || !crtc_props->degamma_lut_size.id))
    return false;

  if (!crtc_props->gamma_lut.id || !crtc_props->gamma_lut_size.id) {
    if (degamma_lut.empty())
      return drm_->SetGammaRamp(crtc_id, gamma_lut);

    // We're missing either degamma or gamma lut properties. We shouldn't try to
    // set just one of them.
    return false;
  }

  ScopedDrmColorLutPtr degamma_blob_data = CreateLutBlob(
      ResampleLut(degamma_lut, crtc_props->degamma_lut_size.value));
  ScopedDrmColorLutPtr gamma_blob_data =
      CreateLutBlob(ResampleLut(gamma_lut, crtc_props->gamma_lut_size.value));

  if (degamma_blob_data) {
    crtc_state->degamma_lut_blob = drm_->CreatePropertyBlob(
        degamma_blob_data.get(),
        sizeof(drm_color_lut) * crtc_props->degamma_lut_size.value);
    crtc_props->degamma_lut.value = crtc_state->degamma_lut_blob->id();
  } else {
    crtc_props->degamma_lut.value = 0;
  }

  if (gamma_blob_data) {
    crtc_state->gamma_lut_blob = drm_->CreatePropertyBlob(
        gamma_blob_data.get(),
        sizeof(drm_color_lut) * crtc_props->gamma_lut_size.value);
    crtc_props->gamma_lut.value = crtc_state->gamma_lut_blob->id();
  } else {
    crtc_props->gamma_lut.value = 0;
  }

  return CommitGammaCorrection(*crtc_props);
}

bool HardwareDisplayPlaneManager::InitializeCrtcState() {
  ScopedDrmResourcesPtr resources(drm_->GetResources());
  if (!resources) {
    PLOG(ERROR) << "Failed to get resources.";
    return false;
  }

  DisableConnectedConnectorsToCrtcs(resources);
  ResetConnectorsCache(resources);

  unsigned int num_crtcs_with_out_fence_ptr = 0;

  for (int i = 0; i < resources->count_crtcs; ++i) {
    CrtcState state;
    state.properties.id = resources->crtcs[i];

    ScopedDrmObjectPropertyPtr props(
        drm_->GetObjectProperties(resources->crtcs[i], DRM_MODE_OBJECT_CRTC));
    if (!props) {
      PLOG(ERROR) << "Failed to get CRTC properties for crtc_id="
                  << state.properties.id;
      continue;
    }

    GetDrmPropertyForName(drm_, props.get(), "ACTIVE",
                          &state.properties.active);
    DCHECK(!drm_->is_atomic() || state.properties.active.id);
    GetDrmPropertyForName(drm_, props.get(), "MODE_ID",
                          &state.properties.mode_id);
    DCHECK(!drm_->is_atomic() || state.properties.mode_id.id);
    // These properties are optional. If they don't exist we can tell by the
    // invalid ID.
    GetDrmPropertyForName(drm_, props.get(), "CTM", &state.properties.ctm);
    GetDrmPropertyForName(drm_, props.get(), "GAMMA_LUT",
                          &state.properties.gamma_lut);
    GetDrmPropertyForName(drm_, props.get(), "GAMMA_LUT_SIZE",
                          &state.properties.gamma_lut_size);
    GetDrmPropertyForName(drm_, props.get(), "DEGAMMA_LUT",
                          &state.properties.degamma_lut);
    GetDrmPropertyForName(drm_, props.get(), "DEGAMMA_LUT_SIZE",
                          &state.properties.degamma_lut_size);
    GetDrmPropertyForName(drm_, props.get(), "OUT_FENCE_PTR",
                          &state.properties.out_fence_ptr);
    GetDrmPropertyForName(drm_, props.get(), "BACKGROUND_COLOR",
                          &state.properties.background_color);

    num_crtcs_with_out_fence_ptr += (state.properties.out_fence_ptr.id != 0);

    crtc_state_.emplace_back(std::move(state));
  }

  // Check that either all or none of the crtcs support the OUT_FENCE_PTR
  // property. Otherwise we will get an incomplete, and thus not useful,
  // out-fence set when we perform a commit involving the problematic
  // crtcs.
  if (num_crtcs_with_out_fence_ptr != 0 &&
      num_crtcs_with_out_fence_ptr != crtc_state_.size()) {
    LOG(ERROR) << "Only some of the crtcs support the OUT_FENCE_PTR property";
    return false;
  }

  return true;
}

void HardwareDisplayPlaneManager::DisableConnectedConnectorsToCrtcs(
    const ScopedDrmResourcesPtr& resources) {
  // Should only be called when no CRTC state has been set yet because we
  // hard-disable CRTCs.
  DCHECK(crtc_state_.empty());

  for (int i = 0; i < resources->count_connectors; ++i) {
    ScopedDrmConnectorPtr connector =
        drm_->GetConnector(resources->connectors[i]);
    if (!connector)
      continue;
    // Disable Zombie connectors (disconnected connectors but holding to an
    // encoder).
    if (connector->encoder_id &&
        connector->connection == DRM_MODE_DISCONNECTED) {
      ScopedDrmEncoderPtr encoder(
          drmModeGetEncoder(drm_->get_fd(), connector->encoder_id));
      if (encoder)
        drm_->DisableCrtc(encoder->crtc_id);
    }
  }
}

const HardwareDisplayPlaneManager::CrtcState&
HardwareDisplayPlaneManager::GetCrtcStateForCrtcId(uint32_t crtc_id) {
  return CrtcStateForCrtcId(crtc_id);
}

HardwareDisplayPlaneManager::CrtcState&
HardwareDisplayPlaneManager::CrtcStateForCrtcId(uint32_t crtc_id) {
  int crtc_index = LookupCrtcIndex(crtc_id);
  DCHECK_GE(crtc_index, 0);
  return crtc_state_[crtc_index];
}

void HardwareDisplayPlaneManager::UpdateCrtcAndPlaneStatesAfterModeset(
    const CommitRequest& commit_request) {
  base::flat_set<HardwareDisplayPlaneList*> disable_planes_lists;

  for (const auto& crtc_request : commit_request) {
    bool is_enabled = crtc_request.should_enable();

    int connector_index = LookupConnectorIndex(crtc_request.connector_id());
    DCHECK_GE(connector_index, 0);
    ConnectorProperties& connector_props = connectors_props_[connector_index];
    connector_props.crtc_id.value = is_enabled ? crtc_request.crtc_id() : 0;

    CrtcState& crtc_state = CrtcStateForCrtcId(crtc_request.crtc_id());
    crtc_state.properties.active.value = static_cast<uint64_t>(is_enabled);

    if (is_enabled) {
      crtc_state.mode = crtc_request.mode();
      crtc_state.modeset_framebuffer =
          DrmOverlayPlane::GetPrimaryPlane(crtc_request.overlays())->buffer;
    } else {
      if (crtc_request.plane_list())
        disable_planes_lists.insert(crtc_request.plane_list());

      // TODO(crbug/1135291): Use atomic APIs to reset cursor plane.
      if (!drm_->SetCursor(crtc_request.crtc_id(), 0, gfx::Size())) {
        PLOG(ERROR) << "Failed to drmModeSetCursor: device:"
                    << drm_->device_path().value()
                    << " crtc:" << crtc_request.crtc_id();
      }
    }
  }

  // TODO(markyacoub): DisableOverlayPlanes should be part of the commit
  // request.
  for (HardwareDisplayPlaneList* list : disable_planes_lists) {
    bool status = DisableOverlayPlanes(list);
    LOG_IF(ERROR, !status) << "Can't disable overlays when disabling HDC.";
    list->plane_list.clear();
  }
}

void HardwareDisplayPlaneManager::ResetModesetBufferOfCrtc(uint32_t crtc_id) {
  CrtcState& crtc_state = CrtcStateForCrtcId(crtc_id);
  crtc_state.modeset_framebuffer = nullptr;
}

}  // namespace ui
