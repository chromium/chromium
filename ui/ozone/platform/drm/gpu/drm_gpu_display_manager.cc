// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"

#include <stddef.h>

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_display.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

constexpr char kMultipleDisplayIdsCollisionDetected[] =
    "Display.MultipleDisplays.GenerateId.CollisionDetection";

// A list of property names that are blocked from issuing a full display
// configuration (modeset) via a udev display CHANGE event.
const char* kBlockedEventsByTriggerProperty[] = {"Content Protection"};

struct DrmDisplayParams {
  scoped_refptr<DrmDevice> drm;
  std::unique_ptr<HardwareDisplayControllerInfo> display_info;
  raw_ptr<display::DisplaySnapshot> snapshot;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TestOnlyModesetOutcome {
  kSuccess = 0,
  kFallbackSuccess = 1,
  kFailure = 2,
  kMaxValue = kFailure,
};

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

// Finds a mode from |modes| that matches the size and timing specified by
// |request_mode| and writes the results to |out_mode|. Returns true if a
// matching mode was found.
bool FindMatchingMode(const display::DisplayMode& request_mode,
                      const std::vector<drmModeModeInfo>& modes,
                      drmModeModeInfo* out_mode) {
  for (const drmModeModeInfo& m : modes) {
    if (MatchMode(request_mode, m)) {
      *out_mode = m;
      return true;
    }
  }
  return false;
}

// Finds a mode that matches the size and timing specified by |request_mode| and
// writes the results to |out_mode|. Prioritizes choosing modes natively
// belonging to |display|, and attempts panel-fitting from |all_displays| if
// needed. Returns true if a matching mode was found.
bool FindModeForDisplay(
    const display::DisplayMode& request_mode,
    const DrmDisplay& display,
    const std::vector<std::unique_ptr<DrmDisplay>>& all_displays,
    drmModeModeInfo* out_mode) {
  if (FindMatchingMode(request_mode, display.modes(), out_mode)) {
    return true;
  }

  // If the display doesn't have the mode natively, then lookup the mode
  // from other displays and try using it on the current display (some
  // displays support panel fitting and they can use different modes even
  // if the mode isn't explicitly declared).
  for (const auto& other_display : all_displays) {
    if (FindMatchingMode(request_mode, other_display->modes(), out_mode)) {
      VLOG(3) << "Found matching mode from another display. Attempting to "
                 "apply via panel fitting.";
      return true;
    }
  }

  LOG(ERROR) << "Failed to find mode: size=" << request_mode.size().ToString()
             << " is_interlaced=" << request_mode.is_interlaced()
             << " refresh_rate=" << request_mode.refresh_rate();
  return false;
}

std::string GetEventPropertyByKey(const std::string& key,
                                  const EventPropertyMap event_props) {
  const auto it = event_props.find(key);
  if (it == event_props.end())
    return std::string();

  return std::string(it->second);
}

ControllerConfigParams* FindConfigParamsForConnector(
    std::vector<ControllerConfigParams>& config_list,
    uint32_t connector_id) {
  for (auto& config : config_list) {
    if (config.connector == connector_id) {
      return &config;
    }
  }
  return nullptr;
}

std::string ConfigRequestToString(
    const std::vector<display::DisplayConfigurationParams>& config_requests) {
  std::string signature;
  for (const auto& config : config_requests) {
    if (config.id <= 0) {
      LOG(WARNING) << __func__
                   << ": potentially invalid display ID: " << config.id;
    }

    signature += base::NumberToString(config.id) + ":" +
                 config.origin.ToString() + ":" +
                 (config.mode ? config.mode->ToString() : "Disabled") + ":" +
                 (config.enable_vrr ? "vrr" : "no_vrr") + ";";
  }
  if (signature.empty()) {
    LOG(WARNING) << __func__ << ": empty return value with request of size: "
                 << config_requests.size();
  }
  return signature;
}

TestOnlyModesetOutcome GetTestOnlyModesetOutcome(
    bool config_success,
    bool did_test_modeset_with_fallback) {
  if (!config_success) {
    return TestOnlyModesetOutcome::kFailure;
  }
  return did_test_modeset_with_fallback
             ? TestOnlyModesetOutcome::kFallbackSuccess
             : TestOnlyModesetOutcome::kSuccess;
}

std::string NumDisplaysToHistogramString(int num_displays) {
  DCHECK(num_displays >= 0)
      << __func__ << ": " << num_displays << " displays detected.";
  switch (num_displays) {
    case 1:
      return "OneDisplay";
    case 2:
      return "TwoDisplays";
    case 3:
      return "ThreeDisplays";
    default:
      return "FourOrMoreDisplays";
  }
}

std::string GetNumFallbackHistogramName(int num_displays) {
  return base::StrCat({"ConfigureDisplays.Modeset.Test.DynamicCRTCs.",
                       NumDisplaysToHistogramString(num_displays),
                       ".PermutationsAttempted"});
}
std::string GetTestOnlyModesetOutcomeName(int num_displays) {
  return base::StrCat({"ConfigureDisplays.Modeset.Test.",
                       NumDisplaysToHistogramString(num_displays), ".Outcome"});
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
  successful_test_config_params_.clear();

  std::vector<std::unique_ptr<DrmDisplay>> old_displays;
  old_displays.swap(displays_);
  std::vector<DrmDisplayParams> displays_to_create;
  MovableDisplaySnapshots display_snapshots;

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
    base::flat_set<uint32_t> valid_connector_ids =
        drm->plane_manager()->ResetConnectorsCacheAndGetValidIds(
            drm->GetResources());

    // TODO: b/327011965 - Move assigning CRTCs to connectors from
    // RefreshNativeDisplays() to before test modeset.
    // Create new DisplaySnapshots and resolve display ID collisions.
    auto display_infos = GetDisplayInfosAndUpdateCrtcs(*drm);

    // Make sure that the display infos we got have valid connector IDs.
    // If not, we need to remove the display info from the list. This removes
    // any zombie connectors.
    std::erase_if(
        display_infos, [&valid_connector_ids](const auto& display_info) {
          return !base::Contains(valid_connector_ids,
                                 display_info->connector()->connector_id);
        });

    // Consolidate all display infos that belong to the same tiled display into
    // one.
    ConsolidateTiledDisplayInfo(display_infos);

    for (auto& display_info : display_infos) {
      display_snapshots.emplace_back(CreateDisplaySnapshot(
          *drm, display_info.get(), static_cast<uint8_t>(device_index)));

      display::DisplaySnapshot* current_display_snapshot =
          display_snapshots.back().get();
      const auto colliding_display_snapshot_iter = edid_id_collision_map.find(
          current_display_snapshot->edid_display_id());
      if (colliding_display_snapshot_iter != edid_id_collision_map.end()) {
        collision_detected = true;

        current_display_snapshot->AddIndexToDisplayId();

        display::DisplaySnapshot* colliding_display_snapshot =
            colliding_display_snapshot_iter->second;
        colliding_display_snapshot->AddIndexToDisplayId();
        edid_id_collision_map[colliding_display_snapshot->edid_display_id()] =
            colliding_display_snapshot;
      }
      edid_id_collision_map[current_display_snapshot->edid_display_id()] =
          current_display_snapshot;

      // Ownership of |display_info| is handed over.
      displays_to_create.push_back(
          {drm, std::move(display_info), current_display_snapshot});
    }
    device_index++;
  }

  // Create a new DrmDisplay with each of the corresponding display info and
  // display snapshot. Note: do not use |display_infos| beyond this point,
  // since some of the objects' internal references will be surrendered.
  for (const DrmDisplayParams& params : displays_to_create) {
    // If the DrmDisplay was present previously, copy its origin to the
    // corresponding DisplaySnapshot before creating a new DrmDisplay.
    auto old_drm_display_it = base::ranges::find_if(
        old_displays,
        DisplayComparator(params.drm, params.display_info->crtc()->crtc_id,
                          params.display_info->connector()->connector_id));
    if (old_drm_display_it != old_displays.end()) {
      params.snapshot->set_origin(old_drm_display_it->get()->origin());
      old_displays.erase(old_drm_display_it);
    }

    displays_.emplace_back(std::make_unique<DrmDisplay>(
        params.drm, params.display_info.get(), *params.snapshot));
  }

  const bool multiple_connected_displays = display_snapshots.size() > 1;
  if (multiple_connected_displays) {
    base::UmaHistogramBoolean(kMultipleDisplayIdsCollisionDetected,
                              collision_detected);
  }

  NotifyScreenManager(displays_, old_displays);
  return display_snapshots;
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

    // Get the connector's ID and convert it to an int.
    const std::string connector_id_str =
        GetEventPropertyByKey("CONNECTOR", event_props);
    if (connector_id_str.empty()) {
      break;
    }
    uint32_t connector_id;
    {
      const bool conversion_success =
          base::StringToUint(connector_id_str, &connector_id);
      DCHECK(conversion_success);
    }

    // Get the trigger property's ID and convert to an int.
    const std::string trigger_prop_id_str =
        GetEventPropertyByKey("PROPERTY", event_props);
    if (trigger_prop_id_str.empty())
      break;

    uint32_t trigger_prop_id;
    {
      const bool conversion_success =
          base::StringToUint(trigger_prop_id_str, &trigger_prop_id);
      DCHECK(conversion_success);
    }

    ScopedDrmObjectPropertyPtr property_values(
        drm->GetObjectProperties(connector_id, DRM_MODE_OBJECT_CONNECTOR));
    DCHECK(property_values);

    // Fetch the name of the property from the device.
    ScopedDrmPropertyPtr drm_property(drm->GetProperty(trigger_prop_id));
    DCHECK(drm_property);
    const std::string enum_value =
        GetEnumNameForProperty(*drm_property, *property_values);
    DCHECK(!enum_value.empty());

    trigger_prop_log =
        "[CONNECTOR:" + connector_id_str +
        "] trigger property: " + std::string(drm_property->name) + "=" +
        enum_value + ", ";
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
    display::ModesetFlags modeset_flags) {
  const bool is_commit =
      modeset_flags.Has(display::ModesetFlag::kCommitModeset);
  std::vector<ControllerConfigParams> controllers_to_configure;
  if (is_commit) {
    controllers_to_configure = GetLatestModesetTestConfig(config_requests);
  }

  if (controllers_to_configure.empty()) {
    for (const auto& request : config_requests) {
      int64_t display_id = request.id;
      DrmDisplay* display = FindDisplay(display_id);
      if (!display) {
        LOG(WARNING) << __func__ << ": there is no display with ID "
                     << display_id;
        return false;
      }

      std::unique_ptr<drmModeModeInfo> mode_ptr =
          request.mode ? std::make_unique<drmModeModeInfo>() : nullptr;
      if (request.mode) {
        if (!FindModeForDisplay(*request.mode, *display, displays_,
                                mode_ptr.get())) {
          return false;
        }
      }

      scoped_refptr<DrmDevice> drm = display->drm();
      ControllerConfigParams params(display->display_id(), drm, display->crtc(),
                                    display->connector(), request.origin,
                                    std::move(mode_ptr), request.enable_vrr,
                                    display->base_connector_id());
      controllers_to_configure.push_back(std::move(params));
    }
  }

  bool config_success = screen_manager_->ConfigureDisplayControllers(
      controllers_to_configure, modeset_flags);

  // Only attempt to fallback on using different CRTC-connector pairings if
  // hardware mirroring is disabled as hardware mirroring has multiple
  // connectors assigned to one CRTC, and the fallback assumes 1:1 pairing.
  const bool should_try_test_fallback =
      !is_commit && !config_success &&
      !display::features::IsHardwareMirrorModeEnabled();
  bool did_test_modeset_with_fallback = false;
  if (should_try_test_fallback) {
    did_test_modeset_with_fallback = true;
    config_success = RetryTestConfigureDisplaysWithAlternateCrtcs(
        config_requests, controllers_to_configure);
  }

  if (displays_configured_callback_)
    displays_configured_callback_.Run();

  if (is_commit) {
    successful_test_config_params_.clear();

    if (config_success) {
      for (const auto& controller : controllers_to_configure) {
        FindDisplay(controller.display_id)->SetOrigin(controller.origin);
      }
    }
  } else {
    const std::string test_modest_outcome_histogram =
        GetTestOnlyModesetOutcomeName(config_requests.size());
    const TestOnlyModesetOutcome test_modeset_outcome =
        GetTestOnlyModesetOutcome(config_success,
                                  did_test_modeset_with_fallback);
    base::UmaHistogramEnumeration(test_modest_outcome_histogram,
                                  test_modeset_outcome);
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

void DrmGpuDisplayManager::SetColorTemperatureAdjustment(
    int64_t display_id,
    const display::ColorTemperatureAdjustment& cta) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(WARNING) << __func__ << ": there is no display with ID " << display_id;
    return;
  }
  display->SetColorTemperatureAdjustment(cta);
}

void DrmGpuDisplayManager::SetColorCalibration(
    int64_t display_id,
    const display::ColorCalibration& calibration) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(WARNING) << __func__ << ": there is no display with ID " << display_id;
    return;
  }
  display->SetColorCalibration(calibration);
}

void DrmGpuDisplayManager::SetGammaAdjustment(
    int64_t display_id,
    const display::GammaAdjustment& adjustment) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(WARNING) << __func__ << ": there is no display with ID " << display_id;
    return;
  }
  display->SetGammaAdjustment(adjustment);
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

bool DrmGpuDisplayManager::SetPrivacyScreen(int64_t display_id, bool enabled) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(WARNING) << __func__ << ": there is no display with ID " << display_id;
    return false;
  }

  return display->SetPrivacyScreen(enabled);
}

std::optional<std::vector<float>> DrmGpuDisplayManager::GetSeamlessRefreshRates(
    int64_t display_id) const {
  TRACE_EVENT1("drm", "DrmGpuDisplayManager::GetSeamlessRefreshRates",
               "display_id", display_id);

  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(WARNING) << __func__ << ": there is no display with ID " << display_id;
    return std::nullopt;
  }

  HardwareDisplayController* controller =
      screen_manager_->GetDisplayController(display->drm(), display->crtc());
  if (!controller) {
    LOG(ERROR) << "Could not find HardwareDisplayController for display_id: "
               << display_id;
    return std::nullopt;
  }

  // TODO: b/323362145: Support continuity logic.
  const gfx::Size current_mode_size = controller->GetModeSize();
  std::vector<float> range;
  for (const drmModeModeInfo& mode : display->modes()) {
    if (ui::ModeSize(mode) != current_mode_size) {
      continue;
    }

    // Do a test commit to check if this mode can be configured without
    // a modeset.
    if (controller->TestSeamlessRefreshRate(display->crtc(), mode)) {
      range.push_back(ModeRefreshRate(mode));
    }
  }
  return range;
}

DrmDisplay* DrmGpuDisplayManager::FindDisplay(int64_t display_id) const {
  for (const auto& display : displays_) {
    if (display->display_id() == display_id)
      return display.get();
  }

  return nullptr;
}

DrmDisplay* DrmGpuDisplayManager::FindDisplayByConnectorId(
    uint32_t connector_id) const {
  for (const auto& display : displays_) {
    if (display->connector() == connector_id) {
      return display.get();
    }
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

// TODO: b/327015722 - Move test modeset fallback with alternate CRTCs
// from DrmGpuDisplayManager to ScreenManager.
// The kernel can sometimes silently reallocate the resources of one CRTC to
// another, making the other ineffective. One such case is when i915's Bigjoiner
// takes the underlying pipe of a secondary CRTC for high bandwidth displays
// (DP 2.1+). Attempting modeset with a stolen CRTC will result in failure. The
// only way for userspace to overcome a stolen CRTC is to dynamically assign
// other CRTC configurations via test modesets.
bool DrmGpuDisplayManager::RetryTestConfigureDisplaysWithAlternateCrtcs(
    const std::vector<display::DisplayConfigurationParams>& config_requests,
    const std::vector<ControllerConfigParams>& controllers_to_configure) {
  // Separate individual params of |controllers_to_configure| into multiple
  // std::vector<ControllerConfigParams> by their DrmDevice.
  base::flat_map<scoped_refptr<DrmDevice>, std::vector<ControllerConfigParams>>
      drm_device_controllers_to_configure;
  for (const auto& config : controllers_to_configure) {
    scoped_refptr<DrmDevice> drm = config.drm;
    drm_device_controllers_to_configure[drm].emplace_back(config);
  }

  // For each DrmDevice, try test modeset with all possible CRTC-connector
  // combinations. Use the first successful one.
  int num_permutations_attempted = 0;
  bool fallback_successful_for_all_devices = true;
  std::vector<ControllerConfigParams> successful_config_list;
  for (auto& [drm, configs_list] : drm_device_controllers_to_configure) {
    std::vector<CrtcConnectorPairs> crtc_connector_permutations =
        GetAllCrtcConnectorPermutations(*drm, configs_list);

    VLOG(1) << "Number of possible fallback CRTC-connector permutations: "
            << crtc_connector_permutations.size();

    bool has_successful_permutation = false;
    for (const auto& permutation : crtc_connector_permutations) {
      // Set up the display abstractions according to the current |permutation|.
      for (const auto& crtc_connector_pair : permutation) {
        uint32_t crtc_id = crtc_connector_pair.crtc_id;
        uint32_t connector_id = crtc_connector_pair.connector_id;

        ControllerConfigParams* param =
            FindConfigParamsForConnector(configs_list, connector_id);
        if (!param) {
          LOG(ERROR) << __func__
                     << ": Could not find ControllerConfigParams for connector "
                        "with ID: "
                     << connector_id;
          continue;
        }
        param->crtc = crtc_id;
      }

      if (!UpdateDisplaysWithNewCrtcs(configs_list)) {
        continue;
      }

      ++num_permutations_attempted;
      if (screen_manager_->ConfigureDisplayControllers(
              configs_list, {display::ModesetFlag::kTestModeset})) {
        has_successful_permutation = true;
        for (auto& config : configs_list) {
          successful_config_list.push_back(config);
        }
        // No need to try other permutations for the device if one is
        // successful.
        break;
      }
    }

    fallback_successful_for_all_devices &= has_successful_permutation;
    if (!fallback_successful_for_all_devices) {
      LOG(WARNING) << __func__
                   << ": No successful CRTC-connector pairing permutation "
                      "found or DRM device: "
                   << drm->device_path().value();

      // TODO: b/329078793 - Stop reverting to the original config once
      // pageflips are deferred/skipped during configuration.
      // Revert ozone abstractions back to the original CRTC-controller pairings
      // before the fallback attempt. The original CRTC-connector pairings are
      // usually stable across display changes, and has better chances for a
      // successful pageflip if one manages to happen between
      // ConfigureDisplays() calls.
      if (!UpdateDisplaysWithNewCrtcs(controllers_to_configure)) {
        LOG(ERROR)
            << __func__
            << ": Failed to revert to the original CRTC-connector pairings.";
      }

      const std::string num_fallback_histogram =
          GetNumFallbackHistogramName(config_requests.size());
      base::UmaHistogramCounts1000(num_fallback_histogram,
                                   num_permutations_attempted);
      return false;
    }
  }

  if (fallback_successful_for_all_devices) {
    const std::string config_request_string =
        ConfigRequestToString(config_requests);
    successful_test_config_params_.insert(
        {config_request_string, successful_config_list});
  }

  // TODO: b/329078793 - Stop reverting to the original config once
  // pageflips are deferred/skipped during configuration.
  // Revert ozone abstractions back to the original CRTC-controller pairings
  // before the fallback attempt. The original CRTC-connector pairings are
  // usually stable across display changes, and has better chances for a
  // successful pageflip if one manages to happen between
  // ConfigureDisplays() calls.
  if (!UpdateDisplaysWithNewCrtcs(controllers_to_configure)) {
    LOG(ERROR) << __func__
               << ": Failed to revert to the original CRTC-connector pairings.";
  }

  const std::string num_fallback_histogram =
      GetNumFallbackHistogramName(config_requests.size());
  base::UmaHistogramCounts1000(num_fallback_histogram,
                               num_permutations_attempted);

  return fallback_successful_for_all_devices;
}

bool DrmGpuDisplayManager::UpdateDisplaysWithNewCrtcs(
    const std::vector<ControllerConfigParams>& controllers_to_configure) {
  base::flat_map<scoped_refptr<DrmDevice>, std::vector<ControllerConfigParams>>
      drm_device_to_configs;
  for (const auto& config : controllers_to_configure) {
    scoped_refptr<DrmDevice> drm = config.drm;
    drm_device_to_configs[drm].emplace_back(config);
  }

  // TODO: b/327015722 - handle ReplaceDisplayControllersCrtcs() inside
  // ScreenManager.
  std::vector<std::pair<DrmDisplay*, uint32_t /*new_crtc_id*/>>
      display_to_new_crtcs_pairs;
  for (const auto& [drm, config_list] : drm_device_to_configs) {
    ConnectorCrtcMap current_connector_to_crtc_pairings;
    ConnectorCrtcMap new_connector_to_crtc_pairings;
    for (const auto& config_param : config_list) {
      const uint32_t connector_id = config_param.connector;
      DrmDisplay* display = FindDisplayByConnectorId(connector_id);
      if (!display) {
        LOG(DFATAL) << "DrmDisplay with connector ID " << connector_id
                    << " not found.";
        return false;
      }

      display_to_new_crtcs_pairs.push_back({display, config_param.crtc});
      current_connector_to_crtc_pairings[connector_id] = display->crtc();
      new_connector_to_crtc_pairings[connector_id] = config_param.crtc;
    }

    if (!screen_manager_->ReplaceDisplayControllersCrtcs(
            drm, current_connector_to_crtc_pairings,
            new_connector_to_crtc_pairings)) {
      return false;
    }
  }

  for (auto& [display, crtc_id] : display_to_new_crtcs_pairs) {
    display->set_crtc(crtc_id);
  }

  return true;
}

std::vector<ControllerConfigParams>
DrmGpuDisplayManager::GetLatestModesetTestConfig(
    const std::vector<display::DisplayConfigurationParams>& config_requests) {
  const std::string config_request_string =
      ConfigRequestToString(config_requests);
  const auto& config_param_it =
      successful_test_config_params_.find(config_request_string);

  if (config_param_it == successful_test_config_params_.end()) {
    return {};
  }

  if (!UpdateDisplaysWithNewCrtcs(config_param_it->second)) {
    LOG(ERROR) << __func__ << ": Unable to restore CRTC-connector pairings.";
  }

  return config_param_it->second;
}

}  // namespace ui
