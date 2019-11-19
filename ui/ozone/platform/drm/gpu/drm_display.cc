// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_display.h"

#include <xf86drmMode.h>
#include <memory>

#include "base/stl_util.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

const char kContentProtection[] = "Content Protection";

struct ContentProtectionMapping {
  const char* name;
  display::HDCPState state;
};

const ContentProtectionMapping kContentProtectionStates[] = {
    {"Undesired", display::HDCP_STATE_UNDESIRED},
    {"Desired", display::HDCP_STATE_DESIRED},
    {"Enabled", display::HDCP_STATE_ENABLED}};

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

  for (int i = 0; i < property->count_enums; ++i)
    if (name == property->enums[i].name)
      return i;

  NOTREACHED();
  return 0;
}

std::string GetEnumNameForProperty(drmModeConnector* connector,
                                   drmModePropertyRes* property) {
  for (int prop_idx = 0; prop_idx < connector->count_props; ++prop_idx) {
    if (connector->props[prop_idx] != property->prop_id)
      continue;

    for (int enum_idx = 0; enum_idx < property->count_enums; ++enum_idx) {
      const drm_mode_property_enum& property_enum = property->enums[enum_idx];
      if (property_enum.value == connector->prop_values[prop_idx])
        return property_enum.name;
    }
  }

  NOTREACHED();
  return std::string();
}

gfx::Size GetDrmModeSize(const drmModeModeInfo& mode) {
  return gfx::Size(mode.hdisplay, mode.vdisplay);
}

std::vector<drmModeModeInfo> GetDrmModeVector(drmModeConnector* connector) {
  std::vector<drmModeModeInfo> modes;
  for (int i = 0; i < connector->count_modes; ++i)
    modes.push_back(connector->modes[i]);

  return modes;
}

}  // namespace

DrmDisplay::DrmDisplay(ScreenManager* screen_manager,
                       const scoped_refptr<DrmDevice>& drm)
    : screen_manager_(screen_manager), drm_(drm) {
}

DrmDisplay::~DrmDisplay() {
}

uint32_t DrmDisplay::connector() const {
  return connector_->connector_id;
}

std::unique_ptr<display::DisplaySnapshot> DrmDisplay::Update(
    HardwareDisplayControllerInfo* info,
    size_t device_index) {
  std::unique_ptr<display::DisplaySnapshot> params = CreateDisplaySnapshot(
      info, drm_->get_fd(), drm_->device_path(), device_index, origin_);
  crtc_ = info->crtc()->crtc_id;
  // TODO(dcastagna): consider taking ownership of |info->connector()|
  connector_ = ScopedDrmConnectorPtr(
      drm_->GetConnector(info->connector()->connector_id));
  if (!connector_) {
    PLOG(ERROR) << "Failed to get connector "
                << info->connector()->connector_id;
  }

  display_id_ = params->display_id();
  modes_ = GetDrmModeVector(info->connector());
  return params;
}

bool DrmDisplay::Configure(const drmModeModeInfo* mode,
                           const gfx::Point& origin) {
  VLOG(1) << "DRM configuring: device=" << drm_->device_path().value()
          << " crtc=" << crtc_ << " connector=" << connector_
          << " origin=" << origin.ToString()
          << " size=" << (mode ? GetDrmModeSize(*mode).ToString() : "0x0")
          << " refresh_rate=" << (mode ? mode->vrefresh : 0) << "Hz";

  if (mode) {
    if (!screen_manager_->ConfigureDisplayController(
            drm_, crtc_, connector_->connector_id, origin, *mode)) {
      VLOG(1) << "Failed to configure: device=" << drm_->device_path().value()
              << " crtc=" << crtc_ << " connector=" << connector_;
      return false;
    }
  } else {
    if (!screen_manager_->DisableDisplayController(drm_, crtc_)) {
      VLOG(1) << "Failed to disable device=" << drm_->device_path().value()
              << " crtc=" << crtc_;
      return false;
    }
  }

  origin_ = origin;
  return true;
}

bool DrmDisplay::GetHDCPState(display::HDCPState* state) {
  if (!connector_)
    return false;

  ScopedDrmPropertyPtr hdcp_property(
      drm_->GetProperty(connector_.get(), kContentProtection));
  if (!hdcp_property) {
    PLOG(INFO) << "'" << kContentProtection << "' property doesn't exist.";
    return false;
  }

  std::string name =
      GetEnumNameForProperty(connector_.get(), hdcp_property.get());
  for (size_t i = 0; i < base::size(kContentProtectionStates); ++i) {
    if (name == kContentProtectionStates[i].name) {
      *state = kContentProtectionStates[i].state;
      VLOG(3) << "HDCP state: " << *state << " (" << name << ")";
      return true;
    }
  }

  LOG(ERROR) << "Unknown content protection value '" << name << "'";
  return false;
}

bool DrmDisplay::SetHDCPState(display::HDCPState state) {
  if (!connector_) {
    return false;
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
  if (!drm_->plane_manager()->SetGammaCorrection(crtc_, degamma_lut,
                                                 gamma_lut)) {
    LOG(ERROR) << "Failed to set gamma tables for display: crtc_id = " << crtc_;
  }
}

}  // namespace ui
