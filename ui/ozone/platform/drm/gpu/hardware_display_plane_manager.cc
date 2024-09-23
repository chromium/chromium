// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

#include <drm_fourcc.h>

#include <cstdint>
#include <memory>
#include <set>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_color_management.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"

namespace ui {

namespace {

gfx::Rect OverlayPlaneToDrmSrcRect(const DrmOverlayPlane& plane) {
  const gfx::Size& size = plane.buffer->size();
  gfx::RectF crop_rectf = plane.crop_rect;
  crop_rectf.Scale(size.width(), size.height());
  // DrmOverlayManager::CanHandleCandidate guarantees this is safe.
  gfx::Rect crop_rect = gfx::ToNearestRect(crop_rectf);
  // Convert to 16.16 fixed point required by the DRM overlay APIs.
  return gfx::Rect(crop_rect.x() << 16, crop_rect.y() << 16,
                   crop_rect.width() << 16, crop_rect.height() << 16);
}

skcms_Matrix3x3 PlaneToOutputMatrix(
    const HardwareDisplayPlaneManager::CrtcState& crtc_state) {
  skcms_Matrix3x3 plane_to_xyzd50;
  crtc_state.planes_primaries.toXYZD50(&plane_to_xyzd50);

  skcms_Matrix3x3 output_to_xyzd50;
  crtc_state.output_primaries.toXYZD50(&output_to_xyzd50);

  skcms_Matrix3x3 xyzd50_to_output;
  skcms_Matrix3x3_invert(&output_to_xyzd50, &xyzd50_to_output);

  return skcms_Matrix3x3_concat(&xyzd50_to_output, &plane_to_xyzd50);
}

}  // namespace

HardwareDisplayPlaneList::HardwareDisplayPlaneList() = default;

HardwareDisplayPlaneList::~HardwareDisplayPlaneList() = default;

HardwareDisplayPlaneList::PageFlipInfo::PageFlipInfo(uint32_t crtc_id,
                                                     uint32_t framebuffer)
    : crtc_id(crtc_id), framebuffer(framebuffer) {}

HardwareDisplayPlaneList::PageFlipInfo::PageFlipInfo(
    const PageFlipInfo& other) = default;

HardwareDisplayPlaneList::PageFlipInfo::~PageFlipInfo() = default;

void HardwareDisplayPlaneList::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();

  dict.Add("plane_list", plane_list);
  dict.Add("old_plane_list", old_plane_list);
}

HardwareDisplayPlaneManager::CrtcProperties::CrtcProperties() = default;
HardwareDisplayPlaneManager::CrtcProperties::CrtcProperties(
    const CrtcProperties& other) = default;
HardwareDisplayPlaneManager::CrtcProperties::~CrtcProperties() = default;

HardwareDisplayPlaneManager::CrtcState::CrtcState() = default;

HardwareDisplayPlaneManager::CrtcState::~CrtcState() = default;

HardwareDisplayPlaneManager::CrtcState::CrtcState(CrtcState&&) = default;

HardwareDisplayPlaneManager::HardwareDisplayPlaneManager(DrmDevice* drm)
    : drm_(drm) {}

HardwareDisplayPlaneManager::~HardwareDisplayPlaneManager() = default;

bool HardwareDisplayPlaneManager::Initialize() {
  // Try to get all of the planes if possible, so we don't have to try to
  // discover hidden primary planes.
  uint64_t value = 0;
  has_universal_planes_ =
      drm_->GetCapability(DRM_CLIENT_CAP_UNIVERSAL_PLANES, &value) && value;

  // Mediatek drivers produce broken results when given negative values. It
  // is suspected that this is due to incorrect parsing of the CTM blob.
  // TODO(b/324594144): Address clamping in the driver/kernel
  ctm_negative_values_broken_ = drm_->GetDriverName() == "mediatek";

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

std::optional<int> HardwareDisplayPlaneManager::LookupCrtcIndex(
    uint32_t crtc_id) const {
  for (size_t i = 0; i < crtc_state_.size(); ++i) {
    if (crtc_state_[i].properties.id == crtc_id)
      return i;
  }
  return {};
}

std::optional<int> HardwareDisplayPlaneManager::LookupConnectorIndex(
    uint32_t connector_id) const {
  for (size_t i = 0; i < connectors_props_.size(); ++i) {
    if (connectors_props_[i].id == connector_id)
      return i;
  }
  return {};
}

base::flat_set<uint32_t> HardwareDisplayPlaneManager::CrtcMaskToCrtcIds(
    uint32_t crtc_mask) const {
  base::flat_set<uint32_t> crtc_ids;
  for (uint32_t idx = 0; idx < crtc_state_.size(); idx++) {
    if (crtc_mask & (1 << idx))
      crtc_ids.insert(crtc_state_[idx].properties.id);
  }

  return crtc_ids;
}

bool HardwareDisplayPlaneManager::IsCompatible(HardwareDisplayPlane* plane,
                                               const DrmOverlayPlane& overlay,
                                               uint32_t crtc_id) const {
  bool ownership_compatible =
      plane->owning_crtc() == 0 || plane->owning_crtc() == crtc_id;
  if (plane->in_use() || !ownership_compatible ||
      plane->type() == DRM_PLANE_TYPE_CURSOR ||
      !plane->CanUseForCrtcId(crtc_id)) {
    return false;
  }

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
    uint32_t crtc_id,
    std::optional<gfx::Point> crtc_offset) {
  auto hw_planes_iter = planes_.begin();
  for (const auto& plane : overlay_list) {
    HardwareDisplayPlane* hw_plane = nullptr;
    for (; hw_planes_iter != planes_.end(); ++hw_planes_iter) {
      auto* current = hw_planes_iter->get();
      if (IsCompatible(current, plane, crtc_id)) {
        hw_plane = current;
        ++hw_planes_iter;  // bump so we don't assign the same plane twice
        break;
      }
    }

    if (!hw_plane) {
      RestoreCurrentPlaneList(plane_list);
      return false;
    }

    if (!SetPlaneData(plane_list, hw_plane, plane, crtc_id, crtc_offset,
                      OverlayPlaneToDrmSrcRect(plane))) {
      RestoreCurrentPlaneList(plane_list);
      return false;
    }

    // Set the color space for all planes based on the color space of the plane
    // with z-index 0. This assumes that all planes have the same primaries.
    // This assumption will need to be enforced in the compositor's overlay
    // processor.
    if (plane.z_order == 0 && plane.color_space.IsValid()) {
      SetColorSpaceForAllPlanes(crtc_id, plane.color_space.GetPrimaries());
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
  for (const auto& plane : planes_) {
    if (plane->CanUseForCrtcId(crtc_id) &&
        plane->type() == DRM_PLANE_TYPE_PRIMARY) {
      return plane->ModifiersForFormat(format);
    }
  }

  return {};
}

base::flat_set<uint32_t>
HardwareDisplayPlaneManager::ResetConnectorsCacheAndGetValidIds(
    const ScopedDrmResourcesPtr& resources) {
  connectors_props_.clear();
  base::flat_set<uint32_t> valid_ids;

  for (int i = 0; i < resources->count_connectors; ++i) {
    const uint32_t connector_id = resources->connectors[i];

    ScopedDrmObjectPropertyPtr props(
        drm_->GetObjectProperties(connector_id, DRM_MODE_OBJECT_CONNECTOR));
    if (!props) {
      PLOG(ERROR) << "Failed to get Connector properties for connector="
                  << connector_id;
      continue;
    }
    // Getting the connector is guaranteed if we survived getting the
    // connector's properties.
    ScopedDrmConnectorPtr connector = drm_->GetConnector(connector_id);
    DCHECK(connector);

    ConnectorProperties state_props;
    state_props.id = connector_id;
    state_props.connection = connector->connection;
    state_props.count_modes = connector->count_modes;
    GetDrmPropertyForName(drm_, props.get(), "CRTC_ID", &state_props.crtc_id);
    DCHECK(!drm_->is_atomic() || state_props.crtc_id.id);
    GetDrmPropertyForName(drm_, props.get(), "link-status",
                          &state_props.link_status);

    const std::vector<uint32_t> possible_encoder_ids(
        connector->encoders, connector->encoders + connector->count_encoders);
    state_props.possible_crtcs_bitmask =
        GetPossibleCrtcsBitmaskFromEncoders(*drm_, possible_encoder_ids);

    connectors_props_.emplace_back(std::move(state_props));
    valid_ids.emplace(connector_id);
  }

  return valid_ids;
}

void HardwareDisplayPlaneManager::SetOutputColorSpace(
    uint32_t crtc_id,
    const SkColorSpacePrimaries& primaries) {
  if (primaries == SkNamedPrimariesExt::kInvalid) {
    LOG(ERROR) << "Invalid output primaries for CRTC " << crtc_id;
    return;
  }
  CrtcState& crtc_state = CrtcStateForCrtcId(crtc_id);
  if (crtc_state.output_primaries == primaries) {
    return;
  }
  crtc_state.output_primaries = primaries;
  if (base::FeatureList::IsEnabled(display::features::kCtmColorManagement)) {
    UpdatePendingCrtcState(crtc_state);
  }
}

void HardwareDisplayPlaneManager::SetColorSpaceForAllPlanes(
    uint32_t crtc_id,
    const SkColorSpacePrimaries& primaries) {
  if (primaries == SkNamedPrimariesExt::kInvalid) {
    LOG(ERROR) << "Invalid plane primaries for CRTC " << crtc_id;
    return;
  }
  CrtcState& crtc_state = CrtcStateForCrtcId(crtc_id);
  if (crtc_state.planes_primaries == primaries) {
    return;
  }
  CHECK(primaries != SkNamedPrimariesExt::kInvalid);
  crtc_state.planes_primaries = primaries;
  if (base::FeatureList::IsEnabled(display::features::kCtmColorManagement)) {
    UpdatePendingCrtcState(crtc_state);
  }
}

void HardwareDisplayPlaneManager::SetColorTemperatureAdjustment(
    uint32_t crtc_id,
    const display::ColorTemperatureAdjustment& cta) {
  CrtcState& crtc_state = CrtcStateForCrtcId(crtc_id);
  crtc_state.color_temperature_adjustment = cta;
  UpdatePendingCrtcState(crtc_state);
  CommitPendingCrtcState(crtc_state);
}

void HardwareDisplayPlaneManager::SetColorCalibration(
    uint32_t crtc_id,
    const display::ColorCalibration& calibration) {
  CrtcState& crtc_state = CrtcStateForCrtcId(crtc_id);
  crtc_state.color_calibration = calibration;
  UpdatePendingCrtcState(crtc_state);
  CommitPendingCrtcState(crtc_state);
}

void HardwareDisplayPlaneManager::SetGammaAdjustment(
    uint32_t crtc_id,
    const display::GammaAdjustment& adjustment) {
  CrtcState& crtc_state = CrtcStateForCrtcId(crtc_id);
  crtc_state.gamma_adjustment = adjustment;
  UpdatePendingCrtcState(crtc_state);
  CommitPendingCrtcState(crtc_state);
}

void HardwareDisplayPlaneManager::SetBackgroundColor(
    uint32_t crtc_id,
    const uint64_t background_color) {
  CrtcState& crtc_state = CrtcStateForCrtcId(crtc_id);
  crtc_state.properties.background_color.value = background_color;
}

bool HardwareDisplayPlaneManager::InitializeCrtcState() {
  ScopedDrmResourcesPtr resources(drm_->GetResources());
  if (!resources) {
    PLOG(ERROR) << "Failed to get resources.";
    return false;
  }

  DisableConnectedConnectorsToCrtcs(resources);
  ResetConnectorsCacheAndGetValidIds(resources);

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
    GetDrmPropertyForName(drm_, props.get(), kVrrEnabledPropertyName,
                          &state.properties.vrr_enabled);

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
      ScopedDrmEncoderPtr encoder = drm_->GetEncoder(connector->encoder_id);
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
  auto crtc_index = LookupCrtcIndex(crtc_id);
  DCHECK(crtc_index.has_value());
  return crtc_state_[*crtc_index];
}

void HardwareDisplayPlaneManager::UpdateCrtcAndPlaneStatesAfterModeset(
    const CommitRequest& commit_request) {
  base::flat_set<HardwareDisplayPlaneList*> disable_planes_lists;

  for (const auto& crtc_request : commit_request) {
    bool is_enabled = crtc_request.should_enable_crtc();

    auto connector_index = LookupConnectorIndex(crtc_request.connector_id());
    DCHECK(connector_index.has_value());
    ConnectorProperties& connector_props = connectors_props_[*connector_index];
    connector_props.crtc_id.value = is_enabled ? crtc_request.crtc_id() : 0;

    CrtcState& crtc_state = CrtcStateForCrtcId(crtc_request.crtc_id());
    crtc_state.properties.active.value = static_cast<uint64_t>(is_enabled);
    crtc_state.properties.vrr_enabled.value = crtc_request.enable_vrr();

    if (is_enabled) {
      crtc_state.mode = crtc_request.mode();
      crtc_state.modeset_framebuffers.clear();
      for (const auto& overlay : crtc_request.overlays())
        crtc_state.modeset_framebuffers.push_back(overlay.buffer);

    } else {
      if (crtc_request.plane_list())
        disable_planes_lists.insert(crtc_request.plane_list());

      // TODO(crbug.com/40151802): Use atomic APIs to reset cursor plane.
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

void HardwareDisplayPlaneManager::ResetModesetStateForCrtc(uint32_t crtc_id) {
  CrtcState& crtc_state = CrtcStateForCrtcId(crtc_id);
  crtc_state.modeset_framebuffers.clear();
}

HardwareCapabilities HardwareDisplayPlaneManager::GetHardwareCapabilities(
    uint32_t crtc_id) {
  std::optional<std::string> driver = drm_->GetDriverName();
  HardwareCapabilities hc;
  if (!driver.has_value()) {
    hc.is_valid = false;
    return hc;
  }

  base::ranges::for_each(
      planes_, [crtc_id, &num_overlay_planes = hc.num_overlay_capable_planes,
                &buffer_formats = hc.supported_buffer_formats](
                   const std::unique_ptr<HardwareDisplayPlane>& plane) {
        if (plane->type() != DRM_PLANE_TYPE_CURSOR &&
            plane->CanUseForCrtcId(crtc_id)) {
          num_overlay_planes++;
          for (const auto& format : plane->supported_formats()) {
            if (ui::IsValidBufferFormat(format)) {
              buffer_formats.emplace(GetBufferFormatFromFourCCFormat(format));
            }
          }
        }
      });

  // While AMD advertises a cursor plane, it's actually a "fake" plane that the
  // display hardware blits to the topmost plane at presentation time. If that
  // topmost plane is scaled/translated (e.g. video), the cursor will then be
  // transformed along with it, leading to an incorrect cursor location in the
  // final presentation. For more info, see b/194335274.
  hc.has_independent_cursor_plane = *driver != "amdgpu" && *driver != "radeon";
  hc.is_valid = true;
  return hc;
}

uint32_t HardwareDisplayPlaneManager::GetPossibleCrtcsBitmaskForConnector(
    uint32_t connector_id) const {
  const auto& connector_prop =
      std::find_if(connectors_props_.begin(), connectors_props_.end(),
                   [connector_id](const ConnectorProperties& prop) {
                     return prop.id == connector_id;
                   });
  if (connector_prop == connectors_props_.end()) {
    LOG(WARNING) << __func__
                 << ": Failed to retrieve connector property for id "
                 << connector_id;
    return {};
  }
  return connector_prop->possible_crtcs_bitmask;
}

void HardwareDisplayPlaneManager::UpdatePendingCrtcState(
    CrtcState& crtc_state) {
  const auto& crtc_props = crtc_state.properties;

  // Set the CTM to convert from the planes' color space primaries to the
  // output color space primaries, followed by application of the color
  // temperature adjustment matrix. This is not the correct math to perform
  // color conversion in the following ways:
  //   * The primary conversion should be done in linear space. This can only
  //     be done if both DEGAMMA and GAMMA are functional, but DEGAMMA is
  //     very often broken.
  //   * The color temperature adjustment matrix is computed to be applied in
  //     sRGB space, not the output space.
  // This is being done as a trade-off sacrificing precise correctness in
  // color conversion for power savings.
  const skcms_Matrix3x3 plane_to_device_matrix =
      base::FeatureList::IsEnabled(display::features::kCtmColorManagement)
          ? PlaneToOutputMatrix(crtc_state)
          : crtc_state.color_calibration.srgb_to_device_matrix;
  const skcms_Matrix3x3 ctm = skcms_Matrix3x3_concat(
      &plane_to_device_matrix,
      &crtc_state.color_temperature_adjustment.srgb_matrix);
  if (crtc_state.properties.ctm.id) {
    ScopedDrmColorCtmPtr ctm_blob_data =
        CreateCTMBlob(ctm, ctm_negative_values_broken_);
    crtc_state.pending_ctm_blob =
        drm_->CreatePropertyBlob(ctm_blob_data.get(), sizeof(drm_color_ctm));
  }

  // Set the DEGAMMA curve to the one specified in the color profile, only if
  // we will also be setting the GAMMA curve.
  // TODO(crbug.com/40945652): This always has to be the identity because
  // many devices have broken implementations. Identitify devices where this
  // functionality is not broken.
  if (crtc_props.gamma_lut.id && crtc_props.gamma_lut_size.id &&
      crtc_props.degamma_lut.id && crtc_props.degamma_lut_size.id) {
    const auto& degamma_curve = crtc_state.color_calibration.srgb_to_linear;
    if (degamma_curve.IsDefaultIdentity()) {
      crtc_state.pending_degamma_lut_blob = nullptr;
    } else {
      ScopedDrmColorLutPtr degamma_blob_data =
          CreateLutBlob(degamma_curve, crtc_props.degamma_lut_size.value);
      crtc_state.pending_degamma_lut_blob = drm_->CreatePropertyBlob(
          degamma_blob_data.get(),
          sizeof(drm_color_lut) * crtc_props.degamma_lut_size.value);
    }
  }

  // Set the GAMMA curve to the concatenation of the color profile with the
  // gamma adjustment.
  // TODO(crbug.com/40945652): Identify devices where this functionality
  // is reliable.
  const auto gamma_curve = display::GammaCurve::MakeConcat(
      crtc_state.color_calibration.linear_to_device,
      crtc_state.gamma_adjustment.curve);
  if (crtc_props.gamma_lut.id && crtc_props.gamma_lut_size.id) {
    if (gamma_curve.IsDefaultIdentity()) {
      crtc_state.pending_gamma_lut_blob = nullptr;
    } else {
      ScopedDrmColorLutPtr gamma_blob_data =
          CreateLutBlob(gamma_curve, crtc_props.gamma_lut_size.value);
      crtc_state.pending_gamma_lut_blob = drm_->CreatePropertyBlob(
          gamma_blob_data.get(),
          sizeof(drm_color_lut) * crtc_props.gamma_lut_size.value);
    }
  } else {
    // Fall back to legacy gamma if needed.
    drm_->SetGammaRamp(crtc_props.id, gamma_curve);
  }
}

}  // namespace ui
