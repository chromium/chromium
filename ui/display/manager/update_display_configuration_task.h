// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_UPDATE_DISPLAY_CONFIGURATION_TASK_H_
#define UI_DISPLAY_MANAGER_UPDATE_DISPLAY_CONFIGURATION_TASK_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/manager/configure_displays_task.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/types/native_display_observer.h"

namespace display {

class DisplaySnapshot;
class NativeDisplayDelegate;

class DISPLAY_MANAGER_EXPORT UpdateDisplayConfigurationTask
    : public NativeDisplayObserver {
 public:
  using ResponseCallback = base::OnceCallback<void(
      /*success=*/bool,
      /*displays=*/const std::vector<DisplaySnapshot*>&,
      /*unassociated_displays=*/const std::vector<DisplaySnapshot*>&,
      /*new_display_state=*/MultipleDisplayState,
      /*new_power_state=*/chromeos::DisplayPowerState)>;

  UpdateDisplayConfigurationTask(
      NativeDisplayDelegate* delegate,
      DisplayLayoutManager* layout_manager,
      MultipleDisplayState new_display_state,
      chromeos::DisplayPowerState new_power_state,
      int power_flags,
      RefreshRateThrottleState refresh_rate_throttle_state,
      bool force_configure,
      ConfigurationType configuration_type,
      ResponseCallback callback);

  UpdateDisplayConfigurationTask(const UpdateDisplayConfigurationTask&) =
      delete;
  UpdateDisplayConfigurationTask& operator=(
      const UpdateDisplayConfigurationTask&) = delete;

  ~UpdateDisplayConfigurationTask() override;

  void Run();

  // display::NativeDisplayObserver:
  void OnConfigurationChanged() override;
  void OnDisplaySnapshotsInvalidated() override;

 private:
  // Callback to NativeDisplayDelegate::GetDisplays().
  void OnDisplaysUpdated(const std::vector<DisplaySnapshot*>& displays);

  // Callback to ConfigureDisplaysTask used to process the result of a display
  // configuration run.
  void OnStateEntered(ConfigureDisplaysTask::Status status);

  // If the initial display configuration run failed due to errors entering
  // mirror more, another configuration run is executed to enter software
  // mirroring. This is the callback used to process the result of that
  // configuration.
  void OnEnableSoftwareMirroring(ConfigureDisplaysTask::Status status);

  // Starts the configuration process. |callback| is used to continue the task
  // after |configure_taks_| finishes executing.
  void EnterState(ConfigureDisplaysTask::ResponseCallback callback);

  // Finishes display configuration and runs |callback_|.
  void FinishConfiguration(bool success);

  // Returns true if the DPMS state should be force to on.
  bool ShouldForceDpms() const;

  // Returns true if a display configuration is required.
  bool ShouldConfigure() const;

  // Returns a display state based on the power state.
  MultipleDisplayState ChooseDisplayState() const;

  NativeDisplayDelegate* delegate_;       // Not owned.
  DisplayLayoutManager* layout_manager_;  // Not owned.

  // Requested display state.
  MultipleDisplayState new_display_state_;

  // Requested power state.
  chromeos::DisplayPowerState new_power_state_;

  // Bitwise-or-ed values for the kSetDisplayPower* values defined in
  // DisplayConfigurator.
  int power_flags_;

  // Whether the configuration task should select a low refresh rate
  // for the internal display.
  RefreshRateThrottleState refresh_rate_throttle_state_;

  bool force_configure_;

  // Whether the configuration task should be done without blanking the
  // displays.
  const ConfigurationType configuration_type_;

  // Used to signal that the task has finished.
  ResponseCallback callback_;

  bool requesting_displays_;

  // List of updated displays.
  std::vector<DisplaySnapshot*> cached_displays_;

  // List of updated displays which have no associated crtc. It can happen
  // when the device is connected with so many displays that has no available
  // crtc to assign.
  std::vector<DisplaySnapshot*> cached_unassociated_displays_;

  std::unique_ptr<ConfigureDisplaysTask> configure_task_;

  // The timestamp when Run() was called. Null if the task is not running.
  absl::optional<base::TimeTicks> start_timestamp_;

  base::WeakPtrFactory<UpdateDisplayConfigurationTask> weak_ptr_factory_{this};
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_UPDATE_DISPLAY_CONFIGURATION_TASK_H_
