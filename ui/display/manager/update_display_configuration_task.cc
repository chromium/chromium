// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/update_display_configuration_task.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "display_configurator.h"
#include "ui/display/manager/configure_displays_task.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/util/display_manager_util.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace display {

namespace {
// Move all internal panel displays to the front of the display list. Otherwise,
// the list remains in order.
void MoveInternalDisplaysToTheFront(
    std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays) {
  DisplayConfigurator::DisplayStateList sorted_displays;

  // First pass for internal panels.
  for (DisplaySnapshot* display : displays) {
    if (display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL)
      sorted_displays.push_back(display);
  }

  // Second pass for the rest.
  for (DisplaySnapshot* display : displays) {
    if (display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL)
      continue;

    sorted_displays.push_back(display);
  }

  displays.swap(sorted_displays);
}

bool ResolveOverrides(
    const DisplayConfigurator::RefreshRateOverrideMap& refresh_rate_overrides,
    std::vector<DisplayConfigureRequest>& requests) {
  for (auto& request : requests) {
    if (!request.mode) {
      continue;
    }

    auto override_it =
        refresh_rate_overrides.find(request.display->display_id());
    if (override_it == refresh_rate_overrides.end()) {
      continue;
    }

    request.mode = std::make_unique<const DisplayMode>(
        request.mode->size(), request.mode->is_interlaced(),
        override_it->second, /*vsync_rate_min=*/std::nullopt);
  }
  return true;
}

}  // namespace

UpdateDisplayConfigurationTask::UpdateDisplayConfigurationTask(
    NativeDisplayDelegate* delegate,
    DisplayLayoutManager* layout_manager,
    MultipleDisplayState new_display_state,
    chromeos::DisplayPowerState new_power_state,
    int power_flags,
    const base::flat_set<int64_t>& new_vrr_state,
    const DisplayConfigurator::RefreshRateOverrideMap& refresh_rate_overrides,
    bool force_configure,
    ConfigurationType configuration_type,
    ResponseCallback callback)
    : delegate_(delegate),
      layout_manager_(layout_manager),
      new_display_state_(new_display_state),
      new_power_state_(new_power_state),
      power_flags_(power_flags),
      new_vrr_state_(new_vrr_state),
      refresh_rate_overrides_(refresh_rate_overrides),
      force_configure_(force_configure),
      configuration_type_(configuration_type),
      callback_(std::move(callback)),
      requesting_displays_(false) {
  delegate_->AddObserver(this);
}

UpdateDisplayConfigurationTask::~UpdateDisplayConfigurationTask() {
  delegate_->RemoveObserver(this);
}

void UpdateDisplayConfigurationTask::Run() {
  requesting_displays_ = true;
  delegate_->GetDisplays(
      base::BindOnce(&UpdateDisplayConfigurationTask::OnDisplaysUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UpdateDisplayConfigurationTask::OnConfigurationChanged() {}

void UpdateDisplayConfigurationTask::OnDisplaySnapshotsInvalidated() {
  cached_displays_.clear();
  if (!requesting_displays_ && weak_ptr_factory_.HasWeakPtrs()) {
    // This task has already been run and getting the displays request is not in
    // flight. We need to re-run it to get updated displays snapshots.
    weak_ptr_factory_.InvalidateWeakPtrs();
    Run();
  }
}

void UpdateDisplayConfigurationTask::OnDisplaysUpdated(
    const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays) {
  cached_displays_ = displays;
  MoveInternalDisplaysToTheFront(cached_displays_);
  requesting_displays_ = false;

  // If the user hasn't requested a display state, update it using the requested
  // power state.
  if (new_display_state_ == MULTIPLE_DISPLAY_STATE_INVALID)
    new_display_state_ = ChooseDisplayState();

  VLOG(1) << "OnDisplaysUpdated: new_display_state="
          << MultipleDisplayStateToString(new_display_state_)
          << " new_power_state=" << DisplayPowerStateToString(new_power_state_)
          << " flags=" << power_flags_
          << " new_vrr_state=" << VrrStateToString(new_vrr_state_)
          << " refresh_rate_overrides="
          << RefreshRateOverrideToString(refresh_rate_overrides_)
          << " force_configure=" << force_configure_
          << " display_count=" << cached_displays_.size();
  if (ShouldConfigure()) {
    EnterState(base::BindOnce(&UpdateDisplayConfigurationTask::OnStateEntered,
                              weak_ptr_factory_.GetWeakPtr()));
  } else {
    // If we don't have to configure then we're sticking with the old
    // configuration. Update it such that it reflects in the reported value.
    new_power_state_ = layout_manager_->GetPowerState();
    FinishConfiguration(true);
  }
}

void UpdateDisplayConfigurationTask::EnterState(
    ConfigureDisplaysTask::ResponseCallback callback) {
  VLOG(2) << "EnterState";
  std::vector<DisplayConfigureRequest> requests;
  if (!layout_manager_->GetDisplayLayout(cached_displays_, new_display_state_,
                                         new_power_state_, new_vrr_state_,
                                         &requests)) {
    std::move(callback).Run(ConfigureDisplaysTask::ERROR);
    return;
  }
  if (!ResolveOverrides(refresh_rate_overrides_, requests)) {
    std::move(callback).Run(ConfigureDisplaysTask::ERROR);
    return;
  }

  if (!requests.empty()) {
    configure_task_ = std::make_unique<ConfigureDisplaysTask>(
        delegate_, requests, std::move(callback), configuration_type_);
    configure_task_->Run();
  } else {
    VLOG(2) << "No displays";
    std::move(callback).Run(ConfigureDisplaysTask::SUCCESS);
  }
}

void UpdateDisplayConfigurationTask::OnStateEntered(
    ConfigureDisplaysTask::Status status) {
  bool success = status != ConfigureDisplaysTask::ERROR;
  if (new_display_state_ == MULTIPLE_DISPLAY_STATE_MULTI_MIRROR &&
      status == ConfigureDisplaysTask::PARTIAL_SUCCESS)
    success = false;

  if (layout_manager_->GetSoftwareMirroringController()) {
    bool enable_software_mirroring = false;
    if (!success && new_display_state_ == MULTIPLE_DISPLAY_STATE_MULTI_MIRROR) {
      if (layout_manager_->GetDisplayState() !=
              MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED ||
          layout_manager_->GetPowerState() != new_power_state_ ||
          force_configure_) {
        new_display_state_ = MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED;
        EnterState(base::BindOnce(
            &UpdateDisplayConfigurationTask::OnEnableSoftwareMirroring,
            weak_ptr_factory_.GetWeakPtr()));
        return;
      }

      success = layout_manager_->GetDisplayState() ==
                MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED;
      enable_software_mirroring = success;
      if (success)
        new_display_state_ = MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED;
    }

    layout_manager_->GetSoftwareMirroringController()->SetSoftwareMirroring(
        enable_software_mirroring);
  }

  FinishConfiguration(success);
}

void UpdateDisplayConfigurationTask::OnEnableSoftwareMirroring(
    ConfigureDisplaysTask::Status status) {
  bool success = status != ConfigureDisplaysTask::ERROR;
  layout_manager_->GetSoftwareMirroringController()->SetSoftwareMirroring(
      success);
  FinishConfiguration(success);
}

void UpdateDisplayConfigurationTask::FinishConfiguration(bool success) {
  base::UmaHistogramBoolean(
      "DisplayManager.UpdateDisplayConfigurationTask.Success", success);

  std::move(callback_).Run(success, cached_displays_,
                           cached_unassociated_displays_, new_display_state_,
                           new_power_state_);
}

bool UpdateDisplayConfigurationTask::ShouldForceDpms() const {
  return new_power_state_ != chromeos::DISPLAY_POWER_ALL_OFF &&
         (layout_manager_->GetPowerState() != new_power_state_ ||
          (power_flags_ & DisplayConfigurator::kSetDisplayPowerForceProbe));
}

bool UpdateDisplayConfigurationTask::ShouldConfigure() const {
  if (force_configure_)
    return true;

  if (cached_displays_.size() == 1 &&
      cached_displays_[0]->type() == DISPLAY_CONNECTION_TYPE_INTERNAL)
    return true;

  if (!(power_flags_ &
        DisplayConfigurator::kSetDisplayPowerOnlyIfSingleInternalDisplay))
    return true;

  if (new_display_state_ != layout_manager_->GetDisplayState())
    return true;

  // Compare refresh rate overrides with current states.
  if (ShouldConfigureRefreshRate()) {
    return true;
  }

  if (ShouldConfigureVrr()) {
    return true;
  }

  return false;
}

MultipleDisplayState UpdateDisplayConfigurationTask::ChooseDisplayState()
    const {
  int num_displays = cached_displays_.size();
  int num_on_displays =
      GetDisplayPower(cached_displays_, new_power_state_, nullptr);

  if (num_displays == 0)
    return MULTIPLE_DISPLAY_STATE_HEADLESS;

  if (num_displays == 1 || num_on_displays == 1) {
    // If only one display is currently turned on, return the "single" state
    // so that its native mode will be used.
    return MULTIPLE_DISPLAY_STATE_SINGLE;
  }

  // Try to use the saved configuration; otherwise, default to extended.
  DisplayConfigurator::StateController* state_controller =
      layout_manager_->GetStateController();

  if (!state_controller)
    return MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED;
  return state_controller->GetStateForDisplayIds(cached_displays_);
}

bool UpdateDisplayConfigurationTask::ShouldConfigureVrr() const {
  for (const DisplaySnapshot* display : cached_displays_) {
    if (!display->IsVrrCapable()) {
      continue;
    }

    if (new_vrr_state_.contains(display->display_id()) !=
        display->IsVrrEnabled()) {
      return true;
    }
  }

  return false;
}

bool UpdateDisplayConfigurationTask::ShouldConfigureRefreshRate() const {
  for (const DisplaySnapshot* display : cached_displays_) {
    // TODO b/334104991: Refresh rate override is only enabled for internal
    // displays.
    if (display->type() != DISPLAY_CONNECTION_TYPE_INTERNAL) {
      continue;
    }

    // No mode means display isn't turned on. Refresh rate override should
    // not affect whether a display is enabled.
    if (!display->current_mode() || !display->native_mode()) {
      continue;
    }

    // Target refresh rate is the native mode's refresh rate, unless an override
    // is specified.
    float target_refresh_rate = display->native_mode()->refresh_rate();
    auto it = refresh_rate_overrides_.find(display->display_id());
    if (it != refresh_rate_overrides_.end()) {
      target_refresh_rate = it->second;
    }

    // If the target refresh rate doesn't match the current refresh rate, then
    // a configuration is needed.
    if (display->current_mode()->refresh_rate() != target_refresh_rate) {
      return true;
    }
  }

  // Checked all displays, and none of them require a refresh rate override.
  return false;
}

}  // namespace display
