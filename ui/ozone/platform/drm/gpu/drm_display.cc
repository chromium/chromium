// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_display.h"

#include <xf86drmMode.h>
#include <memory>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

const char kContentProtection[] = "Content Protection";
const char kHdcpContentType[] = "HDCP Content Type";

const char kPrivacyScreen[] = "privacy-screen";

struct ContentProtectionMapping {
  const char* name;
  display::HDCPState state;
};

struct HdcpContentTypeMapping {
  const char* name;
  display::ContentProtectionMethod content_type;
};

const ContentProtectionMapping kContentProtectionStates[] = {
    {"Undesired", display::HDCP_STATE_UNDESIRED},
    {"Desired", display::HDCP_STATE_DESIRED},
    {"Enabled", display::HDCP_STATE_ENABLED}};

const HdcpContentTypeMapping kHdcpContentTypeStates[] = {
    {"HDCP Type0", display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0},
    {"HDCP Type1", display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_1}};

// Converts |state| to the DRM value associated with the it.
uint32_t GetContentProtectionValue(drmModePropertyRes* property,
                                   display::HDCPState state) {
  std::string name;
  for (size_t i = 0; i < base::size(kContentProtectionStates); ++i) {
    if (kContentProtectionStates[i].state == state) {
      name = kContentProtectionStates[i].name;
      break;
    }
  }

  for (int i = 0; i < property->count_enums; ++i) {
    if (name == property->enums[i].name)
      return i;
  }

  NOTREACHED();
  return 0;
}

// Converts |content_type| to the DRM value associated with the it.
uint32_t GetHdcpContentTypeValue(
    drmModePropertyRes* property,
    display::ContentProtectionMethod content_type) {
  std::string name;
  for (size_t i = 0; i < base::size(kHdcpContentTypeStates); ++i) {
    if (kHdcpContentTypeStates[i].content_type == content_type) {
      name = kHdcpContentTypeStates[i].name;
      break;
    }
  }

  for (int i = 0; i < property->count_enums; ++i) {
    if (name == property->enums[i].name)
      return i;
  }

  NOTREACHED();
  return 0;
}

std::string GetEnumNameForProperty(drmModeObjectProperties* property_values,
                                   drmModePropertyRes* property) {
  for (uint32_t prop_idx = 0; prop_idx < property_values->count_props;
       ++prop_idx) {
    if (property_values->props[prop_idx] != property->prop_id)
      continue;

    for (int enum_idx = 0; enum_idx < property->count_enums; ++enum_idx) {
      const drm_mode_property_enum& property_enum = property->enums[enum_idx];
      if (property_enum.value == property_values->prop_values[prop_idx])
        return property_enum.name;
    }
  }

  NOTREACHED();
  return std::string();
}

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

DrmDisplay::DrmDisplay(const scoped_refptr<DrmDevice>& drm)
    : drm_(drm), current_color_space_(gfx::ColorSpace::CreateSRGB()) {}

DrmDisplay::~DrmDisplay() = default;

uint32_t DrmDisplay::connector() const {
  DCHECK(connector_);
  return connector_->connector_id;
}

std::unique_ptr<display::DisplaySnapshot> DrmDisplay::Update(
    HardwareDisplayControllerInfo* info,
    size_t device_index) {
  std::unique_ptr<display::DisplaySnapshot> params = CreateDisplaySnapshot(
      info, drm_->get_fd(), drm_->device_path(), device_index, origin_);
  crtc_ = info->crtc()->crtc_id;
  // TODO(crbug.com/1119499): consider taking ownership of |info->connector()|
  connector_ = ScopedDrmConnectorPtr(
      drm_->GetConnector(info->connector()->connector_id));
  if (!connector_) {
    PLOG(ERROR) << "Failed to get connector "
                << info->connector()->connector_id;
    return nullptr;
  }

  display_id_ = params->display_id();
  modes_ = GetDrmModeVector(info->connector());
  is_hdr_capable_ =
      params->bits_per_channel() > 8 && params->color_space().IsHDR();
#if defined(OS_CHROMEOS)
  is_hdr_capable_ =
      is_hdr_capable_ &&
      base::FeatureList::IsEnabled(display::features::kUseHDRTransferFunction);
#endif

  return params;
}

bool DrmDisplay::GetHDCPState(
    display::HDCPState* state,
    display::ContentProtectionMethod* protection_method) {
  if (!connector_)
    return false;

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
  std::string name =
      GetEnumNameForProperty(property_values.get(), hdcp_property.get());
  size_t i;
  for (i = 0; i < base::size(kContentProtectionStates); ++i) {
    if (name == kContentProtectionStates[i].name) {
      *state = kContentProtectionStates[i].state;
      VLOG(3) << "HDCP state: " << *state << " (" << name << ")";
      break;
    }
  }

  if (i == base::size(kContentProtectionStates)) {
    LOG(ERROR) << "Unknown content protection value '" << name << "'";
    return false;
  }

  if (*state == display::HDCP_STATE_UNDESIRED) {
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
  name = GetEnumNameForProperty(property_values.get(),
                                content_type_property.get());
  for (i = 0; i < base::size(kHdcpContentTypeStates); ++i) {
    if (name == kHdcpContentTypeStates[i].name) {
      *protection_method = kHdcpContentTypeStates[i].content_type;
      VLOG(3) << "Content Protection Method: " << *protection_method << " ("
              << name << ")";
      break;
    }
  }

  if (i == base::size(kHdcpContentTypeStates)) {
    LOG(ERROR) << "Unknown HDCP content type value '" << name << "'";
    return false;
  }
  return true;
}

bool DrmDisplay::SetHDCPState(
    display::HDCPState state,
    display::ContentProtectionMethod protection_method) {
  if (!connector_) {
    return false;
  }

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
    } else if (!drm_->SetProperty(
                   connector_->connector_id, content_type_property->prop_id,
                   GetHdcpContentTypeValue(content_type_property.get(),
                                           protection_method))) {
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
      GetContentProtectionValue(hdcp_property.get(), state));
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

// TODO(gildekel): consider reformatting this to use the new DRM API or cache
// |privacy_screen_property| after crrev.com/c/1715751 lands.
void DrmDisplay::SetPrivacyScreen(bool enabled) {
  if (!connector_)
    return;

  ScopedDrmPropertyPtr privacy_screen_property(
      drm_->GetProperty(connector_.get(), kPrivacyScreen));

  if (!privacy_screen_property) {
    LOG(ERROR) << "'" << kPrivacyScreen << "' property doesn't exist.";
    return;
  }

  if (!drm_->SetProperty(connector_->connector_id,
                         privacy_screen_property->prop_id, enabled)) {
    LOG(ERROR) << (enabled ? "Enabling" : "Disabling") << " property '"
               << kPrivacyScreen << "' failed!";
  }
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

void DrmDisplay::CommitGammaCorrection(
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  if (!drm_->plane_manager()->SetGammaCorrection(crtc_, degamma_lut, gamma_lut))
    LOG(ERROR) << "Failed to set gamma tables for display: crtc_id = " << crtc_;
}

}  // namespace ui
