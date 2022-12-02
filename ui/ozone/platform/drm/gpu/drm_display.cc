// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_display.h"

#include <xf86drmMode.h>
#include <memory>

#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
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

DrmDisplay::DrmDisplay(const scoped_refptr<DrmDevice>& drm)
    : drm_(drm), current_color_space_(gfx::ColorSpace::CreateSRGB()) {}

DrmDisplay::~DrmDisplay() = default;

uint32_t DrmDisplay::connector() const {
  DCHECK(connector_);
  return connector_->connector_id;
}

void DrmDisplay::Update(HardwareDisplayControllerInfo* info,
                        const display::DisplaySnapshot* display_snapshot) {
  // We take ownership of |info|'s connector because it will not be used again
  // beyond this point. It is safe to assume that |connector_| is populated
  // since it was obtained from GetDisplayInfosAndInvalidCrtcs(), which discards
  // invalid (nullptr) connectors.
  connector_ = info->ReleaseConnector();
  DCHECK(connector_);

  crtc_ = info->crtc()->crtc_id;
  display_id_ = display_snapshot->display_id();
  base_connector_id_ = display_snapshot->base_connector_id();
  modes_ = GetDrmModeVector(connector_.get());
  is_hdr_capable_ = display_snapshot->bits_per_channel() > 8 &&
                    display_snapshot->color_space().IsHDR();
  privacy_screen_property_ =
      std::make_unique<PrivacyScreenProperty>(drm(), connector_.get());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  is_hdr_capable_ =
      is_hdr_capable_ &&
      base::FeatureList::IsEnabled(display::features::kUseHDRTransferFunction);
#endif
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

void DrmDisplay::SetGammaCorrection(
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  // When both |degamma_lut| and |gamma_lut| are empty they are interpreted as
  // "linear/pass-thru" [1]. If the display |is_hdr_capable_| we have to make
  // sure the |current_color_space_| is considered properly.
  // [1]
  // https://www.kernel.org/doc/html/v4.19/gpu/drm-kms.html#color-management-properties
  if (degamma_lut.empty() && gamma_lut.empty() && is_hdr_capable_)
    SetColorSpace(current_color_space_);
  else
    CommitGammaCorrection(degamma_lut, gamma_lut);
}

bool DrmDisplay::SetPrivacyScreen(bool enabled) {
  return privacy_screen_property_->SetPrivacyScreenProperty(enabled);
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
  std::vector<display::GammaRampRGBEntry> degamma;
  std::vector<display::GammaRampRGBEntry> gamma;
  if (current_color_space_.IsHDR())
    return CommitGammaCorrection(degamma, gamma);

  // TODO(mcasas) This should be the inverse value of DisplayChangeObservers's
  // FillDisplayColorSpaces's kHDRLevel, move to a common place.
  // TODO(b/165822222): adjust this level based on the display brightness.
  constexpr float kSDRLevel = 0.85;
  // TODO(mcasas): Retrieve this from the |drm_| HardwareDisplayPlaneManager.
  constexpr size_t kNumGammaSamples = 64ul;
  // Only using kSDRLevel of the available values shifts the contrast ratio, we
  // restore it via a smaller local gamma correction using this exponent.
  constexpr float kExponent = 1.2;
  FillPowerFunctionValues(&gamma, kNumGammaSamples, kSDRLevel, kExponent);
  CommitGammaCorrection(degamma, gamma);
}

bool DrmDisplay::SetVrrEnabled(bool vrr_enabled) {
  if (!drm_->plane_manager()->SetVrrEnabled(crtc_, vrr_enabled)) {
    LOG(ERROR) << "Failed to set vrr_enabled property for crtc_id = " << crtc_;
    return false;
  }

  return true;
}

void DrmDisplay::CommitGammaCorrection(
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  if (!drm_->plane_manager()->SetGammaCorrection(crtc_, degamma_lut, gamma_lut))
    LOG(ERROR) << "Failed to set gamma tables for display: crtc_id = " << crtc_;
}

}  // namespace ui
