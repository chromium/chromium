// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_display.h"

#include <xf86drmMode.h>

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_color_management.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
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

void FillPowerFunctionValues(std::vector<display::GammaRampRGBEntry>* table,
                             size_t table_size,
                             float max_value,
                             float exponent) {
  for (size_t i = 0; i < table_size; i++) {
    const uint16_t v = max_value * std::numeric_limits<uint16_t>::max() *
                       pow((static_cast<float>(i) + 1) / table_size, exponent);
    struct display::GammaRampRGBEntry gamma_entry = {v, v, v};
    table->push_back(gamma_entry);
  }
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

DrmDisplay::DrmDisplay(const scoped_refptr<DrmDevice>& drm,
                       HardwareDisplayControllerInfo* info,
                       const display::DisplaySnapshot& display_snapshot)
    : display_id_(display_snapshot.display_id()),
      base_connector_id_(display_snapshot.base_connector_id()),
      drm_(drm),
      crtc_(info->crtc()->crtc_id),
      connector_(info->ReleaseConnector()) {
  modes_ = GetDrmModeVector(connector_.get());
  origin_ = display_snapshot.origin();
  is_hdr_capable_ = display_snapshot.bits_per_channel() > 8 &&
                    display_snapshot.color_space().IsHDR();
  hdr_static_metadata_ = display_snapshot.hdr_static_metadata();
  current_color_space_ = gfx::ColorSpace::CreateSRGB();
  privacy_screen_property_ =
      std::make_unique<PrivacyScreenProperty>(drm_, connector_.get());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  is_hdr_capable_ =
      is_hdr_capable_ &&
      base::FeatureList::IsEnabled(display::features::kUseHDRTransferFunction);

  if (is_hdr_capable_ &&
      base::FeatureList::IsEnabled(
          display::features::kEnableExternalDisplayHDR10Mode)) {
    current_color_space_ = display_snapshot.color_space();
    SetColorspaceProperty(display_snapshot.color_space());
    SetHdrOutputMetadata(display_snapshot.color_space());
  }
#endif
}

DrmDisplay::~DrmDisplay() = default;

uint32_t DrmDisplay::connector() const {
  DCHECK(connector_);
  return connector_->connector_id;
}

bool DrmDisplay::SetHdcpKeyProp(const std::string& key) {
  DCHECK(connector_);

  TRACE_EVENT1("drm", "DrmDisplay::SetHdcpKeyProp", "connector",
               connector_->connector_id);

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

  ScopedDrmPropertyPtr hdcp_key_property(
      drm_->GetProperty(connector_.get(), kContentProtectionKey));
  DCHECK(hdcp_key_property);

  return drm_->SetProperty(connector_->connector_id, hdcp_key_property->prop_id,
                           key_blob->id());
}

// When reading DRM state always check that it's still valid. Any sort of events
// (such as disconnects) may invalidate the state.
bool DrmDisplay::GetHDCPState(
    display::HDCPState* hdcp_state,
    display::ContentProtectionMethod* protection_method) {
  DCHECK(connector_);

  TRACE_EVENT1("drm", "DrmDisplay::GetHDCPState", "connector",
               connector_->connector_id);
  ScopedDrmPropertyPtr hdcp_property(
      drm_->GetProperty(connector_.get(), kContentProtection));
  if (!hdcp_property) {
    PLOG(INFO) << "'" << kContentProtection << "' property doesn't exist.";
    return false;
  }

  ScopedDrmObjectPropertyPtr property_values(drm_->GetObjectProperties(
      connector_->connector_id, DRM_MODE_OBJECT_CONNECTOR));
  if (!property_values) {
    PLOG(INFO) << "Properties no longer valid for connector "
               << connector_->connector_id << ".";
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

  ScopedDrmPropertyPtr content_type_property(
      drm_->GetProperty(connector_.get(), kHdcpContentType));
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
  DCHECK(connector_);

  if (protection_method != display::CONTENT_PROTECTION_METHOD_NONE) {
    ScopedDrmPropertyPtr content_type_property(
        drm_->GetProperty(connector_.get(), kHdcpContentType));
    if (!content_type_property) {
      // If the driver doesn't support HDCP 2.2, this won't exist.
      if (protection_method & display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_1) {
        // We can't do this, since we can't specify the content type.
        VLOG(3)
            << "Cannot set HDCP Content Type 1 since driver doesn't support it";
        return false;
      }
      VLOG(3) << "HDCP Content Type not supported, default to Type 0";
    } else if (!drm_->SetProperty(connector_->connector_id,
                                  content_type_property->prop_id,
                                  GetDrmValueForInternalType(
                                      protection_method, *content_type_property,
                                      kHdcpContentTypeStates))) {
      // Failed setting HDCP Content Type.
      return false;
    }
  }

  ScopedDrmPropertyPtr hdcp_property(
      drm_->GetProperty(connector_.get(), kContentProtection));
  if (!hdcp_property) {
    PLOG(INFO) << "'" << kContentProtection << "' property doesn't exist.";
    return false;
  }

  return drm_->SetProperty(
      connector_->connector_id, hdcp_property->prop_id,
      GetDrmValueForInternalType(state, *hdcp_property,
                                 kContentProtectionStates));
}

void DrmDisplay::SetColorMatrix(const std::vector<float>& color_matrix) {
  if (!drm_->plane_manager()->SetColorMatrix(crtc_, color_matrix)) {
    LOG(ERROR) << "Failed to set color matrix for display: crtc_id = " << crtc_;
  }
}

void DrmDisplay::SetBackgroundColor(const uint64_t background_color) {
  drm_->plane_manager()->SetBackgroundColor(crtc_, background_color);
}

void DrmDisplay::SetGammaCorrection(const display::GammaCurve& degamma,
                                    const display::GammaCurve& gamma) {
  // When both |degamma| and |gamma| are empty they are interpreted as
  // "linear/pass-thru" [1]. If the display |is_hdr_capable_| we have to make
  // sure the |current_color_space_| is considered properly.
  // [1]
  // https://www.kernel.org/doc/html/v4.19/gpu/drm-kms.html#color-management-properties
  if (degamma.IsDefaultIdentity() && gamma.IsDefaultIdentity() &&
      is_hdr_capable_) {
    SetColorSpace(current_color_space_);
  } else {
    CommitGammaCorrection(degamma, gamma);
  }
}

bool DrmDisplay::SetPrivacyScreen(bool enabled) {
  return privacy_screen_property_->SetPrivacyScreenProperty(enabled);
}

gfx::HDRStaticMetadata::Eotf DrmDisplay::GetEotf(
    const gfx::ColorSpace::TransferID transfer_id) {
  if (!is_hdr_capable_) {
    return gfx::HDRStaticMetadata::Eotf::kGammaSdrRange;
  }

  switch (transfer_id) {
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
      NOTREACHED();
      return gfx::HDRStaticMetadata::Eotf::kGammaSdrRange;
  }
}

bool DrmDisplay::SetHdrOutputMetadata(const gfx::ColorSpace color_space) {
  DCHECK(connector_);
  DCHECK(hdr_static_metadata_.has_value());
  DCHECK(color_space.IsValid());

  drm_hdr_output_metadata* hdr_output_metadata =
      static_cast<drm_hdr_output_metadata*>(
          malloc(sizeof(drm_hdr_output_metadata)));
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

  ScopedDrmHdrOutputMetadataPtr hdr_output_metadata_blob(hdr_output_metadata);

  ScopedDrmPropertyBlob hdr_output_metadata_property_blob =
      drm_->CreatePropertyBlob(hdr_output_metadata_blob.get(),
                               sizeof(drm_hdr_output_metadata));
  ScopedDrmPropertyPtr hdr_output_metadata_property(
      drm_->GetProperty(connector_.get(), kHdrOutputMetadata));
  if (!hdr_output_metadata_property) {
    PLOG(INFO) << "'" << kHdrOutputMetadata << "' property doesn't exist.";
    return false;
  }

  if (!drm_->SetProperty(connector_->connector_id,
                         hdr_output_metadata_property->prop_id,
                         hdr_output_metadata_property_blob->id())) {
    PLOG(INFO) << "Cannot set '" << kHdrOutputMetadata << "' property.";
    return false;
  }
  return true;
}

bool DrmDisplay::SetColorspaceProperty(const gfx::ColorSpace color_space) {
  DCHECK(connector_);
  DCHECK(hdr_static_metadata_.has_value());
  ScopedDrmPropertyPtr color_space_property(
      drm_->GetProperty(connector_.get(), kColorSpace));
  if (!color_space_property) {
    PLOG(INFO) << "'" << kColorSpace << "' property doesn't exist.";
    return false;
  }
  if (!drm_->SetProperty(
          connector_->connector_id, color_space_property->prop_id,
          GetEnumValueForName(*drm_, color_space_property->prop_id,
                              GetNameForColorspace(color_space)))) {
    PLOG(INFO) << "Cannot set '" << GetNameForColorspace(color_space)
               << "' to 'Colorspace' property.";
    return false;
  }
  return true;
}

void DrmDisplay::SetColorSpace(const gfx::ColorSpace& color_space) {
  // There's only something to do if the display supports HDR.
  if (!is_hdr_capable_)
    return;
  current_color_space_ = color_space;

  // When |color_space| is HDR we can simply leave the gamma tables empty, which
  // is interpreted as "linear/pass-thru", see [1]. However when we have an SDR
  // |color_space|, we need to write a scaled down |gamma| function to prevent
  // the mode change brightness to be visible.
  if (current_color_space_.IsHDR())
    return CommitGammaCorrection({}, {});

  // TODO(mcasas) This should be the inverse value of DisplayChangeObservers's
  // FillDisplayColorSpaces's kHDRLevel, move to a common place.
  // TODO(b/165822222): adjust this level based on the display brightness.
  constexpr float kSDRLevel = 0.85;
  // TODO(mcasas): Retrieve this from the |drm_| HardwareDisplayPlaneManager.
  constexpr size_t kNumGammaSamples = 64ul;
  // Only using kSDRLevel of the available values shifts the contrast ratio, we
  // restore it via a smaller local gamma correction using this exponent.
  constexpr float kExponent = 1.2;
  std::vector<display::GammaRampRGBEntry> gamma_entries;
  FillPowerFunctionValues(&gamma_entries, kNumGammaSamples, kSDRLevel,
                          kExponent);
  CommitGammaCorrection({}, display::GammaCurve(gamma_entries));
}

void DrmDisplay::CommitGammaCorrection(const display::GammaCurve& degamma,
                                       const display::GammaCurve& gamma) {
  if (!drm_->plane_manager()->SetGammaCorrection(crtc_, degamma, gamma)) {
    LOG(ERROR) << "Failed to set gamma tables for display: crtc_id = " << crtc_;
  }
}

}  // namespace ui
