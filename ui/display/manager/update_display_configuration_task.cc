// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/update_display_configuration_task.h"

#include "base/bind.h"
#include "ui/display/manager/configure_displays_task.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/display_util.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace display {

UpdateDisplayConfigurationTask::UpdateDisplayConfigurationTask(
    NativeDisplayDelegate* delegate,
    DisplayLayoutManager* layout_manager,
    MultipleDisplayState new_display_state,
    chromeos::DisplayPowerState new_power_state,
    int power_flags,
    bool force_configure,
    ResponseCallback callback)
    : delegate_(delegate),
      layout_manager_(layout_manager),
      new_display_state_(new_display_state),
      new_power_state_(new_power_state),
      power_flags_(power_flags),
      force_configure_(force_configure),
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
    const std::vector<DisplaySnapshot*>& displays) {
  cached_displays_ = displays;
  requesting_displays_ = false;

  // If the user hasn't requested a display state, update it using the requested
  // power state.
  if (new_display_state_ == MULTIPLE_DISPLAY_STATE_INVALID)
    new_display_state_ = ChooseDisplayState();

  VLOG(1) << "OnDisplaysUpdated: new_display_state="
          << MultipleDisplayStateToString(new_display_state_)
          << " new_power_state=" << DisplayPowerStateToString(new_power_state_)
          << " flags=" << power_flags_
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
                                         new_power_state_, &requests)) {
    std::move(callback).Run(ConfigureDisplaysTask::ERROR);
    return;
  }
  if (!requests.empty()) {
    configure_task_.reset(
        new ConfigureDisplaysTask(delegate_, requests, std::move(callback)));
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

}  // namespace display
