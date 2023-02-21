// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"

#include <stddef.h>
#include <cstring>

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_display.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

constexpr char kMultipleDisplayIdsCollisionDetected[] =
    "Display.MultipleDisplays.GenerateId.CollisionDetection";

// A list of property names that are blocked from issuing a full display
// configuration (modeset) via a udev display CHANGE event.
const char* kBlockedEventsByTriggerProperty[] = {"Content Protection"};

class DisplayComparator {
 public:
  explicit DisplayComparator(const DrmDisplay* display)
      : drm_(display->drm()),
        crtc_(display->crtc()),
        connector_(display->connector()) {}

  DisplayComparator(const scoped_refptr<DrmDevice>& drm,
                    uint32_t crtc,
                    uint32_t connector)
      : drm_(drm), crtc_(crtc), connector_(connector) {}

  bool operator()(const std::unique_ptr<DrmDisplay>& other) const {
    return drm_ == other->drm() && connector_ == other->connector() &&
           crtc_ == other->crtc();
  }

 private:
  scoped_refptr<DrmDevice> drm_;
  uint32_t crtc_;
  uint32_t connector_;
};

bool MatchMode(const display::DisplayMode& display_mode,
               const drmModeModeInfo& m) {
  return display_mode.size() == ModeSize(m) &&
         display_mode.refresh_rate() == ModeRefreshRate(m) &&
         display_mode.is_interlaced() == ModeIsInterlaced(m);
}

bool FindMatchingMode(const std::vector<drmModeModeInfo> modes,
                      const display::DisplayMode& display_mode,
                      drmModeModeInfo* mode) {
  for (const drmModeModeInfo& m : modes) {
    if (MatchMode(display_mode, m)) {
      *mode = m;
      return true;
    }
  }
  return false;
}

bool FindModeForDisplay(
    drmModeModeInfo* mode_ptr,
    const display::DisplayMode& display_mode,
    const std::vector<drmModeModeInfo>& modes,
    const std::vector<std::unique_ptr<DrmDisplay>>& all_displays) {
  bool mode_found = FindMatchingMode(modes, display_mode, mode_ptr);
  if (!mode_found) {
    // If the display doesn't have the mode natively, then lookup the mode
    // from other displays and try using it on the current display (some
    // displays support panel fitting and they can use different modes even
    // if the mode isn't explicitly declared).
    for (const auto& other_display : all_displays) {
      mode_found =
          FindMatchingMode(other_display->modes(), display_mode, mode_ptr);
      if (mode_found)
        break;
    }
    if (!mode_found) {
      LOG(ERROR) << "Failed to find mode: size="
                 << display_mode.size().ToString()
                 << " is_interlaced=" << display_mode.is_interlaced()
                 << " refresh_rate=" << display_mode.refresh_rate();
    }
  }
  return mode_found;
}

std::string GetEventPropertyByKey(const std::string& key,
                                  const EventPropertyMap event_props) {
  const auto it = event_props.find(key);
  if (it == event_props.end())
    return std::string();

  return std::string(it->second);
}

}  // namespace

DrmGpuDisplayManager::DrmGpuDisplayManager(ScreenManager* screen_manager,
                                           DrmDeviceManager* drm_device_manager)
    : screen_manager_(screen_manager),
      drm_device_manager_(drm_device_manager) {}

DrmGpuDisplayManager::~DrmGpuDisplayManager() = default;

void DrmGpuDisplayManager::SetDisplaysConfiguredCallback(
    base::RepeatingClosure callback) {
  displays_configured_callback_ = std::move(callback);
}

MovableDisplaySnapshots DrmGpuDisplayManager::GetDisplays() {
  std::vector<std::unique_ptr<DrmDisplay>> old_displays;
  old_displays.swap(displays_);
  MovableDisplaySnapshots params_list;

  const DrmDeviceVector& devices = drm_device_manager_->GetDrmDevices();
  size_t device_index = 0;
  MapEdidIdToDisplaySnapshot edid_id_collision_map;
  bool collision_detected = false;
  for (const auto& drm : devices) {
    if (device_index >= kMaxDrmCount) {
      LOG(WARNING) << "Reached the current limit of " << kMaxDrmCount
                   << " connected DRM devices. Ignoring the remaining "
                   << devices.size() - kMaxDrmCount << " connected devices.";
      break;
    }

    // Receiving a signal that DRM state was updated. Need to reset the plane
    // manager's resource cache since IDs may have changed.
    drm->plane_manager()->ResetConnectorsCache(drm->GetResources());
    auto display_infos = GetDisplayInfosAndUpdateCrtcs(*drm);
    for (const auto& display_info : display_infos) {
      auto it = base::ranges::find_if(
          old_displays,
          DisplayComparator(drm, display_info->crtc()->crtc_id,
                            display_info->connector()->connector_id));
      std::unique_ptr<DrmDisplay> current_drm_display;
      if (it != old_displays.end()) {
        current_drm_display = std::move(*it);
        old_displays.erase(it);
      } else {
        current_drm_display = std::make_unique<DrmDisplay>(drm);
      }

      // Create list of supported drm formats and modifiers
      display::DrmFormatsAndModifiers drm_formats_and_modifiers;
      for (uint32_t format : drm->plane_manager()->GetSupportedFormats()) {
        std::vector<uint64_t> modifiers =
            drm->plane_manager()->GetFormatModifiers(
                display_info->crtc()->crtc_id, format);
        drm_formats_and_modifiers.emplace(format, modifiers);
      }

      // Create the new DisplaySnapshot and resolve display ID collisions.
      std::unique_ptr<display::DisplaySnapshot> current_display_snapshot =
          CreateDisplaySnapshot(
              *drm, display_info.get(), static_cast<uint8_t>(device_index),
              current_drm_display->origin(), drm_formats_and_modifiers);

      const auto colliding_display_snapshot_iter = edid_id_collision_map.find(
          current_display_snapshot->edid_display_id());
      if (colliding_display_snapshot_iter != edid_id_collision_map.end()) {
        collision_detected = true;

        // Resolve collisions by adding each colliding display's connector index
        // to its display ID.
        current_display_snapshot->AddIndexToDisplayId();

        display::DisplaySnapshot* colliding_display_snapshot =
            colliding_display_snapshot_iter->second;
        colliding_display_snapshot->AddIndexToDisplayId();
        edid_id_collision_map[colliding_display_snapshot->edid_display_id()] =
            colliding_display_snapshot;
      }

      // Do not use |display_info| beyond this point, since some of its internal
      // references will be surrendered.
      current_drm_display->Update(display_info.get(),
                                  current_display_snapshot.get());

      // Update the map with the new (or potentially resolved) display snapshot.
      edid_id_collision_map[current_display_snapshot->edid_display_id()] =
          current_display_snapshot.get();
      params_list.push_back(std::move(current_display_snapshot));
      displays_.push_back(std::move(current_drm_display));
    }
    device_index++;
  }

  const bool multiple_connected_displays = params_list.size() > 1;
  if (multiple_connected_displays) {
    base::UmaHistogramBoolean(kMultipleDisplayIdsCollisionDetected,
                              collision_detected);
  }

  NotifyScreenManager(displays_, old_displays);
  return params_list;
}

bool DrmGpuDisplayManager::TakeDisplayControl() {
  const DrmDeviceVector& devices = drm_device_manager_->GetDrmDevices();
  bool status = true;
  for (const auto& drm : devices)
    status &= drm->SetMaster();

  // Roll-back any successful operation.
  if (!status) {
    LOG(ERROR) << "Failed to take control of the display";
    RelinquishDisplayControl();
  }

  return status;
}

void DrmGpuDisplayManager::RelinquishDisplayControl() {
  const DrmDeviceVector& devices = drm_device_manager_->GetDrmDevices();
  for (const auto& drm : devices)
    drm->DropMaster();
}

bool DrmGpuDisplayManager::ShouldDisplayEventTriggerConfiguration(
    const EventPropertyMap& event_props) {
  DCHECK(!event_props.empty());

  const std::string event_seq_num =
      GetEventPropertyByKey("SEQNUM", event_props);
  std::string log_prefix =
      "Display event CHANGE" +
      (event_seq_num.empty() ? "" : "(SEQNUM:" + event_seq_num + ") ");
  std::string trigger_prop_log;

  const std::string event_dev_path =
      GetEventPropertyByKey("DEVPATH", event_props);
  const DrmDeviceVector& devices = drm_device_manager_->GetDrmDevices();
  for (const auto& drm : devices) {
    if (drm->device_path().value().find(event_dev_path) == std::string::npos)
      continue;

    // Get the trigger property's ID and convert to an int.
    const std::string trigger_prop_id_str =
        GetEventPropertyByKey("PROPERTY", event_props);
    if (trigger_prop_id_str.empty())
      break;

    uint32_t trigger_prop_id;
    const bool conversion_success =
        base::StringToUint(trigger_prop_id_str, &trigger_prop_id);
    DCHECK(conversion_success);

    // Fetch the name of the property from the device.
    ScopedDrmPropertyPtr drm_property(drm->GetProperty(trigger_prop_id));
    DCHECK(drm_property);
    trigger_prop_log =
        "[trigger property: " + std::string(drm_property->name) + "] ";
    for (const char* blocked_prop : kBlockedEventsByTriggerProperty) {
      if (strcmp(drm_property->name, blocked_prop) == 0) {
        VLOG(1) << log_prefix << trigger_prop_log
                << "resolution: blocked; display configuration task "
                   "rejected.";
        return false;
      }
    }
  }

  VLOG(1) << log_prefix << trigger_prop_log
          << "resolution: allowed; display configuration task triggered.";
  return true;
}

bool DrmGpuDisplayManager::ConfigureDisplays(
    const std::vector<display::DisplayConfigurationParams>& config_requests,
    uint32_t modeset_flag) {
  ScreenManager::ControllerConfigsList controllers_to_configure;
  for (const auto& config : config_requests) {
    int64_t display_id = config.id;
    DrmDisplay* display = FindDisplay(display_id);
    if (!display) {
      LOG(WARNING) << __func__ << ": there is no display with ID "
                   << display_id;
      return false;
    }

    std::unique_ptr<drmModeModeInfo> mode_ptr =
        config.mode ? std::make_unique<drmModeModeInfo>() : nullptr;
    if (config.mode) {
      if (!FindModeForDisplay(mode_ptr.get(), *config.mode.value(),
                              display->modes(), displays_)) {
        return false;
      }
    }

    scoped_refptr<DrmDevice> drm = display->drm();
    ScreenManager::ControllerConfigParams params(
        display->display_id(), drm, display->crtc(), display->connector(),
        config.origin, std::move(mode_ptr), config.enable_vrr,
        display->base_connector_id());
    controllers_to_configure.push_back(std::move(params));
  }

  bool config_success = screen_manager_->ConfigureDisplayControllers(
      controllers_to_configure, modeset_flag);

  if (displays_configured_callback_)
    displays_configured_callback_.Run();

  const bool test_only = modeset_flag == display::kTestModeset;
  if (!test_only && config_success) {
    for (const auto& controller : controllers_to_configure)
      FindDisplay(controller.display_id)->SetOrigin(controller.origin);
  }

  return config_success;
}

bool DrmGpuDisplayManager::SetHdcpKeyProp(int64_t display_id,
                                          const std::string& key) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(ERROR) << "SetHdcpKeyProp: There is no display with ID " << display_id;
    return false;
  }

  return display->SetHdcpKeyProp(key);
}

bool DrmGpuDisplayManager::GetHDCPState(
    int64_t display_id,
    display::HDCPState* state,
    display::ContentProtectionMethod* protection_method) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(WARNING) << __func__ << ": there is no display with ID " << display_id;
    return false;
  }

  return display->GetHDCPState(state, protection_method);
}

bool DrmGpuDisplayManager::SetHDCPState(
    int64_t display_id,
    display::HDCPState state,
    display::ContentProtectionMethod protection_method) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(WARNING) << __func__ << ": there is no display with ID " << display_id;
    return false;
  }

  return display->SetHDCPState(state, protection_method);
}

void DrmGpuDisplayManager::SetColorMatrix(
    int64_t display_id,
    const std::vector<float>& color_matrix) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(WARNING) << __func__ << ": there is no display with ID " << display_id;
    return;
  }

  display->SetColorMatrix(color_matrix);
}

void DrmGpuDisplayManager::SetBackgroundColor(int64_t display_id,
                                              const uint64_t background_color) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(WARNING) << __func__ << ": there is no display with ID" << display_id;
    return;
  }

  display->SetBackgroundColor(background_color);
}

void DrmGpuDisplayManager::SetGammaCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(WARNING) << __func__ << ": there is no display with ID " << display_id;
    return;
  }
  display->SetGammaCorrection(degamma_lut, gamma_lut);
}

bool DrmGpuDisplayManager::SetPrivacyScreen(int64_t display_id, bool enabled) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(WARNING) << __func__ << ": there is no display with ID " << display_id;
    return false;
  }

  return display->SetPrivacyScreen(enabled);
}

void DrmGpuDisplayManager::SetColorSpace(int64_t crtc_id,
                                         const gfx::ColorSpace& color_space) {
  for (const auto& display : displays_) {
    if (display->crtc() == crtc_id) {
      display->SetColorSpace(color_space);
      return;
    }
  }
  LOG(WARNING) << __func__ << ": there is no display with CRTC ID " << crtc_id;
}

DrmDisplay* DrmGpuDisplayManager::FindDisplay(int64_t display_id) {
  for (const auto& display : displays_) {
    if (display->display_id() == display_id)
      return display.get();
  }

  return nullptr;
}

void DrmGpuDisplayManager::NotifyScreenManager(
    const std::vector<std::unique_ptr<DrmDisplay>>& new_displays,
    const std::vector<std::unique_ptr<DrmDisplay>>& old_displays) const {
  ScreenManager::CrtcsWithDrmList controllers_to_remove;
  for (const auto& old_display : old_displays) {
    if (base::ranges::none_of(new_displays,
                              DisplayComparator(old_display.get()))) {
      controllers_to_remove.emplace_back(old_display->crtc(),
                                         old_display->drm());
    }
  }
  if (!controllers_to_remove.empty())
    screen_manager_->RemoveDisplayControllers(controllers_to_remove);

  for (const auto& new_display : new_displays) {
    if (base::ranges::none_of(old_displays,
                              DisplayComparator(new_display.get()))) {
      screen_manager_->AddDisplayController(
          new_display->drm(), new_display->crtc(), new_display->connector());
    }
  }
}

}  // namespace ui
