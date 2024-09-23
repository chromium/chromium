// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/drm/gpu/drm_display.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <algorithm>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_color_management.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/common/hardware_display_controller_info.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

std::vector<drmModeModeInfo> GetDrmModeVector(drmModeConnector* connector) {
  std::vector<drmModeModeInfo> modes;
  for (int i = 0; i < connector->count_modes; ++i)
    modes.push_back(connector->modes[i]);

  return modes;
}

}  // namespace

DrmDisplay::PrivacyScreenProperty::PrivacyScreenProperty(
    const scoped_refptr<DrmDevice>& drm,
    drmModeConnector* connector)
    : drm_(drm), connector_(connector) {
  privacy_screen_hw_state_ =
      drm_->GetProperty(connector_, kPrivacyScreenHwStatePropertyName);
  privacy_screen_sw_state_ =
      drm_->GetProperty(connector_, kPrivacyScreenSwStatePropertyName);

  if (!privacy_screen_hw_state_ || !privacy_screen_sw_state_) {
    privacy_screen_hw_state_.reset();
    privacy_screen_sw_state_.reset();

    property_last_ = display::kPrivacyScreenLegacyStateLast;
    privacy_screen_legacy_ =
        drm_->GetProperty(connector_, kPrivacyScreenPropertyNameLegacy);
  }
}

DrmDisplay::PrivacyScreenProperty::~PrivacyScreenProperty() = default;

bool DrmDisplay::PrivacyScreenProperty::SetPrivacyScreenProperty(bool enabled) {
  drmModePropertyRes* property = GetWritePrivacyScreenProperty();
  if (!property) {
    LOG(ERROR)
        << "Privacy screen is not supported but an attempt to set it was made.";
    return false;
  }

  const display::PrivacyScreenState state_to_set =
      enabled ? display::kEnabled : display::kDisabled;
  if (!drm_->SetProperty(connector_->connector_id, property->prop_id,
                         GetDrmValueForInternalType(state_to_set, *property,
                                                    kPrivacyScreenStates))) {
    LOG(ERROR) << (enabled ? "Enabling" : "Disabling") << " property '"
               << property->name << "' failed!";
    return false;
  }

  return ValidateCurrentStateAgainst(enabled);
}

display::PrivacyScreenState
DrmDisplay::PrivacyScreenProperty::GetPrivacyScreenState() const {
  drmModePropertyRes* property = GetReadPrivacyScreenProperty();
  if (!property) {
    LOG(ERROR) << "Privacy screen is not supported but an attempt to read its "
                  "state was made.";
    return display::kNotSupported;
  }

  ScopedDrmObjectPropertyPtr property_values(drm_->GetObjectProperties(
      connector_->connector_id, DRM_MODE_OBJECT_CONNECTOR));
  if (!property_values) {
    PLOG(INFO) << "Properties no longer valid for connector "
               << connector_->connector_id << ".";
    return display::kNotSupported;
  }

  const std::string privacy_screen_state_name =
      GetEnumNameForProperty(*property, *property_values);
  const display::PrivacyScreenState* state = GetInternalTypeValueFromDrmEnum(
      privacy_screen_state_name, kPrivacyScreenStates);
  return state ? *state : display::kNotSupported;
}

bool DrmDisplay::PrivacyScreenProperty::ValidateCurrentStateAgainst(
    bool enabled) const {
  display::PrivacyScreenState current_state = GetPrivacyScreenState();
  if (current_state == display::kNotSupported)
    return false;

  bool currently_on = false;
  if (current_state == display::kEnabled ||
      current_state == display::kEnabledLocked) {
    currently_on = true;
  }
  return currently_on == enabled;
}

drmModePropertyRes*
DrmDisplay::PrivacyScreenProperty::GetReadPrivacyScreenProperty() const {
  if (privacy_screen_hw_state_ && privacy_screen_sw_state_)
    return privacy_screen_hw_state_.get();
  return privacy_screen_legacy_.get();
}

drmModePropertyRes*
DrmDisplay::PrivacyScreenProperty::GetWritePrivacyScreenProperty() const {
  if (privacy_screen_hw_state_ && privacy_screen_sw_state_)
    return privacy_screen_sw_state_.get();
  return privacy_screen_legacy_.get();
}

DrmDisplay::CrtcConnectorPair::CrtcConnectorPair(
    uint32_t crtc_id,
    ScopedDrmConnectorPtr drm_connector,
    std::optional<gfx::Point> tile_location)
    : crtc_id(crtc_id),
      connector(std::move(drm_connector)),
      tile_location(tile_location) {
  CHECK(connector != nullptr);
}

DrmDisplay::CrtcConnectorPair::CrtcConnectorPair(
    DrmDisplay::CrtcConnectorPair&& other) noexcept = default;
DrmDisplay::CrtcConnectorPair& DrmDisplay::CrtcConnectorPair::operator=(
    DrmDisplay::CrtcConnectorPair&& other) noexcept = default;

DrmDisplay::CrtcConnectorPair::~CrtcConnectorPair() = default;

DrmDisplay::CrtcConnectorPair CreateCrtcConnectorPair(
    HardwareDisplayControllerInfo& info) {
  std::optional<gfx::Point> tile_location = std::nullopt;
  if (info.tile_property().has_value()) {
    tile_location = info.tile_property()->location;
  }

  return DrmDisplay::CrtcConnectorPair(info.crtc()->crtc_id,
                                       info.ReleaseConnector(), tile_location);
}

std::vector<DrmDisplay::CrtcConnectorPair> CreateCrtcConnectorPairs(
    HardwareDisplayControllerInfo* info) {
  CHECK(info != nullptr);

  std::vector<DrmDisplay::CrtcConnectorPair> crtc_connector_pairs;
  // If the display is tiled, then |info| is always the primary info.
  crtc_connector_pairs.push_back(CreateCrtcConnectorPair(*info));

  for (auto& tile_info : info->nonprimary_tile_infos()) {
    crtc_connector_pairs.push_back(CreateCrtcConnectorPair(*tile_info));
  }

  return crtc_connector_pairs;
}

DrmDisplay::DrmDisplay(const scoped_refptr<DrmDevice>& drm,
                       HardwareDisplayControllerInfo* info,
                       const display::DisplaySnapshot& display_snapshot)
    : display_id_(display_snapshot.display_id()),
      base_connector_id_(display_snapshot.base_connector_id()),
      drm_(drm),
      crtc_connector_pairs_(CreateCrtcConnectorPairs(info)),
      primary_crtc_connector_pair_(crtc_connector_pairs_.front()) {
  modes_ = GetDrmModeVector(primary_crtc_connector_pair_->connector.get());
  origin_ = display_snapshot.origin();
  hdr_static_metadata_ = display_snapshot.hdr_static_metadata();
  privacy_screen_property_ = std::make_unique<PrivacyScreenProperty>(
      drm_, primary_crtc_connector_pair_->connector.get());
  tile_property_ = info->tile_property();

  SkColorSpacePrimaries output_primaries =
      display_snapshot.color_info().edid_primaries;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Do not allow display_snapshot and connector property state to go out of
  // sync. HDR capability is determined in
  // gfx::DisplayUtil::GetColorSpaceFromEdid
  if (display_snapshot.color_space() == gfx::ColorSpace::CreateHDR10()) {
    output_primaries = SkNamedPrimariesExt::kRec2020;
    SetColorspaceProperty(display_snapshot.color_space());
    SetHdrOutputMetadata(display_snapshot.color_space());
  } else {
    SetColorspaceProperty(gfx::ColorSpace::CreateSRGB());
    ClearHdrOutputMetadata();
  }
#endif
  for (const auto& crtc_connector_pair : crtc_connector_pairs_) {
    drm_->plane_manager()->SetOutputColorSpace(crtc_connector_pair.crtc_id,
                                               output_primaries);
  }

  vsync_rate_min_from_edid_ = info->edid_parser().has_value()
                                  ? info->edid_parser()->vsync_rate_min()
                                  : std::nullopt;
}

DrmDisplay::~DrmDisplay() = default;

uint32_t DrmDisplay::GetPrimaryConnectorId() const {
  DCHECK(primary_crtc_connector_pair_->connector);
  return primary_crtc_connector_pair_->connector->connector_id;
}

const std::vector<DrmDisplay::CrtcConnectorPair>&
DrmDisplay::crtc_connector_pairs() const {
  return crtc_connector_pairs_;
}

bool DrmDisplay::ReplaceCrtcs(
    const base::flat_map<uint32_t /*current_crtc*/, uint32_t /*new_crtc*/>&
        current_to_new_crtc_ids) {
  std::vector<CrtcConnectorPair*> target_crtc_connector_pairs;
  // Check that there are no overlaps in new CRTC IDs
  base::flat_set<uint32_t> new_crtc_ids;
  for (auto [current_crtc_id, new_crtc_id] : current_to_new_crtc_ids) {
    std::pair<base::flat_set<uint32_t>::iterator, bool> insert_it =
        new_crtc_ids.insert(new_crtc_id);
    // Fail out if insertion failed due to existing key (duplication).
    if (!insert_it.second) {
      return false;
    }

    // Find CrtcConnectorPair corresponding to |current_crtc_id|.
    CrtcConnectorPair* target_crtc_connector_pair = nullptr;
    for (auto& crtc_connector_pair : crtc_connector_pairs_) {
      if (crtc_connector_pair.crtc_id == current_crtc_id) {
        target_crtc_connector_pair = &crtc_connector_pair;
        break;
      }
    }
    if (!target_crtc_connector_pair) {
      return false;
    }
    target_crtc_connector_pairs.push_back(target_crtc_connector_pair);
  }

  // Ensure that all CrtcConnectorPairs are getting replaced.
  if (target_crtc_connector_pairs.size() != crtc_connector_pairs_.size()) {
    return false;
  }

  for (auto& crtc_connector_pair : target_crtc_connector_pairs) {
    crtc_connector_pair->crtc_id =
        current_to_new_crtc_ids.at(crtc_connector_pair->crtc_id);
  }

  return true;
}

bool DrmDisplay::SetHdcpKeyProp(const std::string& key) {
  DCHECK(primary_crtc_connector_pair_->connector);

  TRACE_EVENT1("drm", "DrmDisplay::SetHdcpKeyProp", "connector",
               primary_crtc_connector_pair_->connector->connector_id);

  // The HDCP key is secret, we want to create it as write only so the user
  // space can't read it back. (i.e. through `modetest`)
  ScopedDrmPropertyBlob key_blob;
  // TODO(markyacoub): the flag requires being merged to libdrm then backported
  // to CrOS. Remove the #if once that happens.
#if defined(DRM_MODE_CREATE_BLOB_WRITE_ONLY)
  key_blob = drm_->CreatePropertyBlobWithFlags(key.data(), key.size(),
                                               DRM_MODE_CREATE_BLOB_WRITE_ONLY);
#endif

  if (!key_blob) {
    LOG(ERROR) << "Failed to create HDCP Key property blob";
    return false;
  }

  ScopedDrmPropertyPtr hdcp_key_property(drm_->GetProperty(
      primary_crtc_connector_pair_->connector.get(), kContentProtectionKey));
  DCHECK(hdcp_key_property);

  return drm_->SetProperty(
      primary_crtc_connector_pair_->connector->connector_id,
      hdcp_key_property->prop_id, key_blob->id());
}

// When reading DRM state always check that it's still valid. Any sort of events
// (such as disconnects) may invalidate the state.
bool DrmDisplay::GetHDCPState(
    display::HDCPState* hdcp_state,
    display::ContentProtectionMethod* protection_method) {
  DCHECK(primary_crtc_connector_pair_->connector);

  TRACE_EVENT1("drm", "DrmDisplay::GetHDCPState", "connector",
               primary_crtc_connector_pair_->connector->connector_id);
  ScopedDrmPropertyPtr hdcp_property(drm_->GetProperty(
      primary_crtc_connector_pair_->connector.get(), kContentProtection));
  if (!hdcp_property) {
    PLOG(INFO) << "'" << kContentProtection << "' property doesn't exist.";
    return false;
  }

  ScopedDrmObjectPropertyPtr property_values(drm_->GetObjectProperties(
      primary_crtc_connector_pair_->connector->connector_id,
      DRM_MODE_OBJECT_CONNECTOR));
  if (!property_values) {
    PLOG(INFO) << "Properties no longer valid for connector "
               << primary_crtc_connector_pair_->connector->connector_id << ".";
    return false;
  }

  const display::HDCPState* hw_hdcp_state =
      GetDrmPropertyCurrentValueAsInternalType(
          kContentProtectionStates, *hdcp_property, *property_values);
  if (hw_hdcp_state) {
    VLOG(3) << "HDCP state: " << *hw_hdcp_state << ".";
    *hdcp_state = *hw_hdcp_state;
  } else {
    LOG(ERROR) << "Unknown content protection value.";
    return false;
  }

  if (*hdcp_state == display::HDCP_STATE_UNDESIRED) {
    // ProtectionMethod doesn't matter if we don't have it desired/enabled.
    *protection_method = display::CONTENT_PROTECTION_METHOD_NONE;
    return true;
  }

  ScopedDrmPropertyPtr content_type_property(drm_->GetProperty(
      primary_crtc_connector_pair_->connector.get(), kHdcpContentType));
  if (!content_type_property) {
    // This won't exist if the driver doesn't support HDCP 2.2, so default it in
    // that case.
    VLOG(3) << "HDCP Content Type not supported, default to Type 0";
    *protection_method = display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0;
    return true;
  }

  const display::ContentProtectionMethod* hw_protection_method =
      GetDrmPropertyCurrentValueAsInternalType(
          kHdcpContentTypeStates, *content_type_property, *property_values);
  if (hw_protection_method) {
    VLOG(3) << "Content Protection Method: " << *protection_method << ".";
    *protection_method = *hw_protection_method;
  } else {
    LOG(ERROR) << "Unknown HDCP content type value.";
    return false;
  }

  return true;
}

bool DrmDisplay::SetHDCPState(
    display::HDCPState state,
    display::ContentProtectionMethod protection_method) {
  DCHECK(primary_crtc_connector_pair_->connector);

  if (protection_method != display::CONTENT_PROTECTION_METHOD_NONE) {
    ScopedDrmPropertyPtr content_type_property(drm_->GetProperty(
        primary_crtc_connector_pair_->connector.get(), kHdcpContentType));
    if (!content_type_property) {
      // If the driver doesn't support HDCP 2.2, this won't exist.
      if (protection_method & display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_1) {
        // We can't do this, since we can't specify the content type.
        VLOG(3)
            << "Cannot set HDCP Content Type 1 since driver doesn't support it";
        return false;
      }
      VLOG(3) << "HDCP Content Type not supported, default to Type 0";
    } else {
      if (!drm_->SetProperty(
              primary_crtc_connector_pair_->connector->connector_id,
              content_type_property->prop_id,
              GetDrmValueForInternalType(protection_method,
                                         *content_type_property,
                                         kHdcpContentTypeStates))) {
        // Failed setting HDCP Content Type.
        return false;
      }
    }
  }

  ScopedDrmPropertyPtr hdcp_property(drm_->GetProperty(
      primary_crtc_connector_pair_->connector.get(), kContentProtection));
  if (!hdcp_property) {
    PLOG(INFO) << "'" << kContentProtection << "' property doesn't exist.";
    return false;
  }

  return drm_->SetProperty(
      primary_crtc_connector_pair_->connector->connector_id,
      hdcp_property->prop_id,
      GetDrmValueForInternalType(state, *hdcp_property,
                                 kContentProtectionStates));
}

void DrmDisplay::SetColorTemperatureAdjustment(
    const display::ColorTemperatureAdjustment& cta) {
  for (const auto& crtc_connector_pair : crtc_connector_pairs_) {
    drm_->plane_manager()->SetColorTemperatureAdjustment(
        crtc_connector_pair.crtc_id, cta);
  }
}

void DrmDisplay::SetColorCalibration(
    const display::ColorCalibration& calibration) {
  for (const auto& crtc_connector_pair : crtc_connector_pairs_) {
    drm_->plane_manager()->SetColorCalibration(crtc_connector_pair.crtc_id,
                                               calibration);
  }
}

void DrmDisplay::SetGammaAdjustment(
    const display::GammaAdjustment& adjustment) {
  for (const auto& crtc_connector_pair : crtc_connector_pairs_) {
    drm_->plane_manager()->SetGammaAdjustment(crtc_connector_pair.crtc_id,
                                              adjustment);
  }
}

void DrmDisplay::SetBackgroundColor(const uint64_t background_color) {
  for (const auto& crtc_connector_pair : crtc_connector_pairs_) {
    drm_->plane_manager()->SetBackgroundColor(crtc_connector_pair.crtc_id,
                                              background_color);
  }
}

bool DrmDisplay::SetPrivacyScreen(bool enabled) {
  return privacy_screen_property_->SetPrivacyScreenProperty(enabled);
}

gfx::HDRStaticMetadata::Eotf DrmDisplay::GetEotf(
    const gfx::ColorSpace::TransferID transfer_id) {
  switch (transfer_id) {
    case gfx::ColorSpace::TransferID::SRGB:
      return gfx::HDRStaticMetadata::Eotf::kGammaSdrRange;
    case gfx::ColorSpace::TransferID::PQ:
      return gfx::HDRStaticMetadata::Eotf::kPq;
    case gfx::ColorSpace::TransferID::HLG:
      return gfx::HDRStaticMetadata::Eotf::kHlg;
    case gfx::ColorSpace::TransferID::SRGB_HDR:
    case gfx::ColorSpace::TransferID::LINEAR_HDR:
    case gfx::ColorSpace::TransferID::CUSTOM_HDR:
    case gfx::ColorSpace::TransferID::PIECEWISE_HDR:
    case gfx::ColorSpace::TransferID::SCRGB_LINEAR_80_NITS:
      return gfx::HDRStaticMetadata::Eotf::kGammaHdrRange;
    default:
      NOTREACHED_IN_MIGRATION();
      return gfx::HDRStaticMetadata::Eotf::kGammaSdrRange;
  }
}

bool DrmDisplay::ClearHdrOutputMetadata() {
  for (const auto& crtc_connector_pair : crtc_connector_pairs_) {
    DCHECK(crtc_connector_pair.connector);

    ScopedDrmPropertyPtr hdr_output_metadata_property(drm_->GetProperty(
        crtc_connector_pair.connector.get(), kHdrOutputMetadata));
    if (!hdr_output_metadata_property) {
      PLOG(INFO) << "'" << kHdrOutputMetadata
                 << "' property doesn't exist for connector "
                 << crtc_connector_pair.connector->connector_id;
      return false;
    }

    // TODO(b/342617770): Atomically set connector properties across all
    // connectors owned by DrmDisplay to prevent scenarios where SetProperty()
    // succeeds for a subset of the connectors and creates inconsistencies.
    if (!drm_->SetProperty(crtc_connector_pair.connector->connector_id,
                           hdr_output_metadata_property->prop_id, 0)) {
      PLOG(INFO) << "Cannot set '" << kHdrOutputMetadata
                 << "' property on connector "
                 << crtc_connector_pair.connector->connector_id;
      return false;
    }
  }

  return true;
}

bool DrmDisplay::SetHdrOutputMetadata(const gfx::ColorSpace color_space) {
  DCHECK(hdr_static_metadata_.has_value());
  DCHECK(color_space.IsValid());

  ScopedDrmHdrOutputMetadataPtr hdr_output_metadata(
      static_cast<drm_hdr_output_metadata*>(
          drmMalloc(sizeof(drm_hdr_output_metadata))));
  hdr_output_metadata->metadata_type = 0;
  hdr_output_metadata->hdmi_metadata_type1.metadata_type = 0;

  gfx::HDRStaticMetadata::Eotf eotf = GetEotf(color_space.GetTransferID());
  DCHECK(hdr_static_metadata_->IsEotfSupported(eotf));
  hdr_output_metadata->hdmi_metadata_type1.eotf = static_cast<uint8_t>(eotf);

  hdr_output_metadata->hdmi_metadata_type1.max_cll = 0;
  hdr_output_metadata->hdmi_metadata_type1.max_fall =
      hdr_static_metadata_->max_avg;
  // This value is coded as an unsigned 16-bit value in units of 1 cd/m2,
  // where 0x0001 represents 1 cd/m2 and 0xFFFF represents 65535 cd/m2.
  hdr_output_metadata->hdmi_metadata_type1.max_display_mastering_luminance =
      hdr_static_metadata_->max;
  // This value is coded as an unsigned 16-bit value in units of 0.0001 cd/m2,
  // where 0x0001 represents 0.0001 cd/m2 and 0xFFFF represents 6.5535 cd/m2.
  hdr_output_metadata->hdmi_metadata_type1.min_display_mastering_luminance =
      hdr_static_metadata_->min * 10000.0;

  SkColorSpacePrimaries primaries = color_space.GetPrimaries();
  constexpr int kPrimariesFixedPoint = 50000;
  hdr_output_metadata->hdmi_metadata_type1.display_primaries[0].x =
      primaries.fRX * kPrimariesFixedPoint;
  hdr_output_metadata->hdmi_metadata_type1.display_primaries[0].y =
      primaries.fRY * kPrimariesFixedPoint;
  hdr_output_metadata->hdmi_metadata_type1.display_primaries[1].x =
      primaries.fGX * kPrimariesFixedPoint;
  hdr_output_metadata->hdmi_metadata_type1.display_primaries[1].y =
      primaries.fGY * kPrimariesFixedPoint;
  hdr_output_metadata->hdmi_metadata_type1.display_primaries[2].x =
      primaries.fBX * kPrimariesFixedPoint;
  hdr_output_metadata->hdmi_metadata_type1.display_primaries[2].y =
      primaries.fBY * kPrimariesFixedPoint;
  hdr_output_metadata->hdmi_metadata_type1.white_point.x =
      primaries.fWX * kPrimariesFixedPoint;
  hdr_output_metadata->hdmi_metadata_type1.white_point.y =
      primaries.fWY * kPrimariesFixedPoint;

  for (const auto& crtc_connector_pair : crtc_connector_pairs_) {
    DCHECK(crtc_connector_pair.connector);

    ScopedDrmPropertyBlob hdr_output_metadata_property_blob =
        drm_->CreatePropertyBlob(hdr_output_metadata.get(),
                                 sizeof(drm_hdr_output_metadata));
    if (!hdr_output_metadata_property_blob) {
      PLOG(INFO) << "Cannot create '" << kHdrOutputMetadata
                 << "' property blob.";
      return false;
    }

    ScopedDrmPropertyPtr hdr_output_metadata_property(drm_->GetProperty(
        crtc_connector_pair.connector.get(), kHdrOutputMetadata));
    if (!hdr_output_metadata_property) {
      PLOG(INFO) << "'" << kHdrOutputMetadata
                 << "' property doesn't exist for connector "
                 << crtc_connector_pair.connector->connector_id;
      return false;
    }

    // TODO(b/342617770): Atomically set connector properties across all
    // connectors owned by DrmDisplay to prevent scenarios where SetProperty()
    // succeeds for a subset of the connectors and creates inconsistencies.
    if (!hdr_output_metadata_property->prop_id ||
        !drm_->SetProperty(crtc_connector_pair.connector->connector_id,
                           hdr_output_metadata_property->prop_id,
                           hdr_output_metadata_property_blob->id())) {
      PLOG(ERROR) << "Cannot set '" << kHdrOutputMetadata << "' property.";
      return false;
    }
  }

  return true;
}

bool DrmDisplay::SetColorspaceProperty(const gfx::ColorSpace color_space) {
  for (const auto& crtc_connector_pair : crtc_connector_pairs_) {
    DCHECK(crtc_connector_pair.connector);

    ScopedDrmPropertyPtr color_space_property(
        drm_->GetProperty(crtc_connector_pair.connector.get(), kColorSpace));
    if (!color_space_property) {
      PLOG(INFO) << "'" << kColorSpace << "' property doesn't exist.";
      return false;
    }

    // TODO(b/342617770): Atomically set connector properties across all
    // connectors owned by DrmDisplay to prevent scenarios where SetProperty()
    // succeeds for a subset of the connectors and creates inconsistencies.
    if (!color_space_property->prop_id ||
        !drm_->SetProperty(
            crtc_connector_pair.connector->connector_id,
            color_space_property->prop_id,
            GetEnumValueForName(*drm_, color_space_property->prop_id,
                                GetNameForColorspace(color_space)))) {
      PLOG(ERROR) << "Cannot set '" << GetNameForColorspace(color_space)
                  << "' to '" << kColorSpace << "' property for connector "
                  << crtc_connector_pair.connector->connector_id;
      return false;
    }
  }
  return true;
}

bool DrmDisplay::IsVrrCapable() const {
  const bool is_vsync_rate_min_positive =
      vsync_rate_min_from_edid_.has_value() && *vsync_rate_min_from_edid_ > 0;
  if (!is_vsync_rate_min_positive) {
    return false;
  }

  for (const auto& crtc_connector_pair : crtc_connector_pairs_) {
    drmModeConnector* connector = crtc_connector_pair.connector.get();
    if (!connector || !ui::IsVrrCapable(*drm_, connector)) {
      return false;
    }
  }

  return true;
}

std::optional<TileProperty> DrmDisplay::GetTileProperty() const {
  return tile_property_;
}

const DrmDisplay::CrtcConnectorPair*
DrmDisplay::GetCrtcConnectorPairForConnectorId(uint32_t connector_id) const {
  for (const auto& crtc_connector_pair : crtc_connector_pairs_) {
    DCHECK(crtc_connector_pair.connector);

    if (crtc_connector_pair.connector->connector_id == connector_id) {
      return &crtc_connector_pair;
    }
  }
  return nullptr;
}

bool DrmDisplay::ContainsCrtc(uint32_t crtc_id) const {
  for (const auto& crtc_connector_pair : crtc_connector_pairs_) {
    if (crtc_connector_pair.crtc_id == crtc_id) {
      return true;
    }
  }
  return false;
}

}  // namespace ui
