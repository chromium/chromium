// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_CONFIGURATOR_H_
#define UI_DISPLAY_MANAGER_DISPLAY_CONFIGURATOR_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace display {

class ContentProtectionManager;
class DisplayLayoutManager;
class DisplayMode;
class DisplaySnapshot;
class ManagedDisplayMode;
class NativeDisplayDelegate;
class UpdateDisplayConfigurationTask;

struct ColorTemperatureAdjustment;
struct ColorCalibration;

namespace test {
class DisplayManagerTestApi;
}  // namespace test

// This class interacts directly with the system display configurator.
class DISPLAY_MANAGER_EXPORT DisplayConfigurator
    : public NativeDisplayObserver {
 public:
  using ConfigurationCallback = base::OnceCallback<void(bool /* success */)>;
  using DisplayControlCallback = base::OnceCallback<void(bool success)>;
  using GetSeamlessRefreshRatesCallback =
      base::OnceCallback<void(const std::optional<std::vector<float>>&)>;

  using DisplayStateList =
      std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>;
  // Map of display id to a refresh rate override.
  using RefreshRateOverrideMap = std::unordered_map<int64_t, float>;

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called after the display configuration has been changed. |display|
    // contains the just-applied configuration. Note that the X server is no
    // longer grabbed when this method is called, so the actual configuration
    // could've changed already.
    virtual void OnDisplayConfigurationChanged(
        const DisplayStateList& displays) {}

    // Called after a display configuration change attempt failed. |displays|
    // contains displays that are detected when failed. |failed_new_state| is
    // the new state which the system failed to enter.
    virtual void OnDisplayConfigurationChangeFailed(
        const DisplayStateList& displays,
        MultipleDisplayState failed_new_state) {}

    // Called after the power state has been changed. |power_state| contains
    // the just-applied power state.
    virtual void OnPowerStateChanged(chromeos::DisplayPowerState power_state) {}

    // Called when the |cached_displays_| is cleared.
    virtual void OnDisplaySnapshotsInvalidated() {}
  };

  // Interface for classes that make decisions about which display state
  // should be used.
  class StateController {
   public:
    virtual ~StateController() {}

    // Called when displays are detected.
    virtual MultipleDisplayState GetStateForDisplayIds(
        const DisplayConfigurator::DisplayStateList& outputs) = 0;

    virtual bool GetSelectedModeForDisplayId(
        int64_t display_id,
        ManagedDisplayMode* out_mode) const = 0;
  };

  // Interface for classes that implement software based mirroring.
  class SoftwareMirroringController {
   public:
    virtual ~SoftwareMirroringController() {}

    // Called when the hardware mirroring failed.
    virtual void SetSoftwareMirroring(bool enabled) = 0;

    // Returns true when software mirroring mode is requested, but it does
    // not guarantee that the mode is active.
    virtual bool SoftwareMirroringEnabled() const = 0;

    // Returns true if hardware mirroring should not be used. (e.g. In mixed
    // mirror mode, the API caller specifies the mirroring source and
    // destination displays which do not exist in hardware mirroring.)
    virtual bool IsSoftwareMirroringEnforced() const = 0;
  };

  // Helper class used by tests.
  class TestApi {
   public:
    explicit TestApi(DisplayConfigurator* configurator)
        : configurator_(configurator) {}

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    ~TestApi() {}

    // If |configure_timer_| is started, stops the timer, runs
    // ConfigureDisplays(), and returns true; returns false otherwise.
    [[nodiscard]] bool TriggerConfigureTimeout();

    // Gets the current delay of the |configure_timer_| if it's running, or zero
    // time delta otherwise.
    base::TimeDelta GetConfigureDelay() const;

    DisplayLayoutManager* GetDisplayLayoutManager() const;

   private:
    raw_ptr<DisplayConfigurator, DanglingUntriaged> configurator_;  // not owned
  };

  // Flags that can be passed to SetDisplayPower().
  static const int kSetDisplayPowerNoFlags;
  // Configure displays even if the passed-in state matches |power_state_|.
  static const int kSetDisplayPowerForceProbe;
  // Do not change the state if multiple displays are connected or if the
  // only connected display is external.
  static const int kSetDisplayPowerOnlyIfSingleInternalDisplay;

  // Gap between screens so cursor at bottom of active display doesn't
  // partially appear on top of inactive display. Higher numbers guard
  // against larger cursors, but also waste more memory.
  // For simplicity, this is hard-coded to avoid the complexity of always
  // determining the DPI of the screen and rationalizing which screen we
  // need to use for the DPI calculation.
  // See crbug.com/130188 for initial discussion.
  static const int kVerticalGap = 60;

  // The delay to perform configuration after RRNotify. See the comment for
  // |configure_timer_|.
  static const int kConfigureDelayMs = 1000;

  // The delay to perform configuration after waking up from suspend when in
  // multi display mode. Should be bigger than |kConfigureDelayMs|. Generally
  // big enough for external displays to be detected and added.
  // crbug.com/614624.
  static const int kResumeConfigureMultiDisplayDelayMs = 2000;

  // Returns the mode within |display| that matches the given size with highest
  // refresh rate. Returns None if no matching display was found.
  static const DisplayMode* FindDisplayModeMatchingSize(
      const DisplaySnapshot& display,
      const gfx::Size& size);

  DisplayConfigurator();

  DisplayConfigurator(const DisplayConfigurator&) = delete;
  DisplayConfigurator& operator=(const DisplayConfigurator&) = delete;

  ~DisplayConfigurator() override;

  MultipleDisplayState display_state() const { return current_display_state_; }
  const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>&
  cached_displays() const {
    return cached_displays_;
  }
  void set_state_controller(StateController* controller) {
    state_controller_ = controller;
  }
  void set_mirroring_controller(SoftwareMirroringController* controller) {
    mirroring_controller_ = controller;
  }
  void SetConfigureDisplays(bool configure_displays);
  bool has_unassociated_display() const { return has_unassociated_display_; }
  chromeos::DisplayPowerState current_power_state() const {
    return current_power_state_;
  }
  ContentProtectionManager* content_protection_manager() const {
    return content_protection_manager_.get();
  }

  // Called when an external process no longer needs to control the display
  // and Chrome can take control.
  void TakeControl(DisplayControlCallback callback);

  // Called when an external process needs to control the display and thus
  // Chrome should relinquish it.
  void RelinquishControl(DisplayControlCallback callback);

  // Replaces |native_display_delegate_| with the delegate passed in and sets
  // |configure_display_| to true. Should be called before Init().
  void SetDelegateForTesting(
      std::unique_ptr<NativeDisplayDelegate> display_delegate);

  // Called asynchronously with the initial |power_state| loaded from prefs.
  // This may be called after ForceInitialConfigure triggers a call to
  // OnConfigured(), in which case UpdatePowerState() will be called with the
  // correct initial value. Does nothing if |requested_power_state_| is set,
  // e.g. via SetDisplayPower().
  void SetInitialDisplayPower(chromeos::DisplayPowerState power_state);

  // Initialize the display power state to DISPLAY_POWER_ALL_ON
  void InitializeDisplayPowerState();

  // Initialization, must be called right after constructor.
  // |is_panel_fitting_enabled| indicates hardware panel fitting support.
  void Init(std::unique_ptr<NativeDisplayDelegate> delegate,
            bool is_panel_fitting_enabled);

  // Does initial configuration of displays during startup.
  void ForceInitialConfigure();

  // Stop handling display configuration events/requests.
  void PrepareForExit();

  // Called when powerd notifies us that some set of displays should be turned
  // on or off.  This requires enabling or disabling the CRTC associated with
  // the display(s) in question so that the low power state is engaged.
  // |flags| contains bitwise-or-ed kSetDisplayPower* values. After the
  // configuration finishes |callback| is called with the status of the
  // operation.
  void SetDisplayPower(chromeos::DisplayPowerState power_state,
                       int flags,
                       ConfigurationCallback callback);

  // Force switching the display state to |new_state|. Returns false if
  // switching failed (possibly because |new_state| is invalid for the
  // current set of connected displays).
  void SetMultipleDisplayState(MultipleDisplayState new_state);

  // Request a description of the refresh rates to which the display can support
  // a configuration without a full modeset.
  // The supported refresh rates depend on the current configuration of the
  // display driver and hardware.
  //
  // It's possible that there could be some configuration change such that a
  // seamless modeset to a refresh rate returned from here succeeds at one time,
  // and fails at another due to some configuration change in the display
  // driver. The caller should re-query the supported refresh rates whenever
  // there is a full modeset, or when a seamless refresh rate change fails, to
  // ensure that the caller has an up-to-date picture of which refresh rates are
  // supported.
  //
  // A result of nullopt indicates that the request failed for some reason such
  // as an invalid display_id. An empty vector indicates that there
  // are no modes to which the display can be configured seamlessly. This could
  // happen if the display is currently turned off.
  void GetSeamlessRefreshRates(int64_t display_id,
                               GetSeamlessRefreshRatesCallback callback);

  // NativeDisplayObserver:
  void OnConfigurationChanged() override;
  void OnDisplaySnapshotsInvalidated() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserverForTesting(Observer* observer) const;

  // Sets all the displays into pre-suspend mode; usually this means
  // configure them for their resume state. This allows faster resume on
  // machines where display configuration is slow. On completion of the display
  // configuration |callback| is executed synchronously or asynchronously.
  void SuspendDisplays(ConfigurationCallback callback);

  // Reprobes displays to handle changes made while the system was
  // suspended.
  void ResumeDisplays();

  // Returns true if there is at least one display on.
  bool IsDisplayOn() const;

  // Sets the color temperature adjustment for the specified display.
  void SetColorTemperatureAdjustment(int64_t display_id,
                                     const ColorTemperatureAdjustment& cta);

  // Sets the color calibration for the specified display;
  void SetColorCalibration(int64_t display_id,
                           const ColorCalibration& calibration);

  // Enable/disable the privacy screen on display with |display_id|.
  // For this to succeed, privacy screen must be supported by the display.
  // After privacy screen is set, |callback| is called with the outcome
  // (success/failure) of the operation.
  void SetPrivacyScreen(int64_t display_id,
                        bool enabled,
                        ConfigurationCallback callback);

  // Returns the requested power state if set or the default power state.
  chromeos::DisplayPowerState GetRequestedPowerState() const;

  void reset_requested_power_state_for_test() {
    requested_power_state_ = std::nullopt;
  }

  std::optional<chromeos::DisplayPowerState> GetRequestedPowerStateForTest()
      const {
    return requested_power_state_;
  }

  // Requests to enable variable refresh rates on the specified displays and to
  // disable variable refresh rates on all other displays, and schedules a
  // seamless configuration change as needed.
  void SetVrrEnabled(const base::flat_set<int64_t>& display_ids);

  // Requests to override the refresh rate of the specified displays and
  // schedule a seamless configuration change if needed. If a display is not in
  // |overrides| then then the display may be configured back to its native
  // refresh rate, if the configuration can happen without a modeset. If the
  // affected displays are already configured according to |overrides|, then no
  // configuration will occur.
  void SetRefreshRateOverrides(const RefreshRateOverrideMap& overrides);

 private:
  friend class test::DisplayManagerTestApi;

  class DisplayLayoutManagerImpl;

  bool configurator_disabled() const {
    return !configure_displays_ || display_externally_controlled_;
  }

  // Updates |pending_*| members and applies the passed-in state. |callback| is
  // invoked (perhaps synchronously) on completion.
  void SetDisplayPowerInternal(chromeos::DisplayPowerState power_state,
                               int flags,
                               ConfigurationCallback callback);

  // Configures displays. Invoked by |configure_timer_|.
  void ConfigureDisplays();

  // Notifies observers about an attempted state change.
  void NotifyDisplayStateObservers(bool success,
                                   MultipleDisplayState attempted_state);

  // Notifies observers about a power state change.
  void NotifyPowerStateObservers();

  // Returns the display state that should be used with |cached_displays_| while
  // in |power_state|.
  MultipleDisplayState ChooseDisplayState(
      chromeos::DisplayPowerState power_state) const;

  // If |configuration_task_| isn't initialized, initializes it and starts the
  // configuration task.
  void RunPendingConfiguration();

  // Callback for |configuration_task_|. When the configuration process finishes
  // this is called with the result (|success|) and the updated display state.
  void OnConfigured(
      bool success,
      const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays,
      const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>&
          unassociated_displays,
      MultipleDisplayState new_display_state,
      chromeos::DisplayPowerState new_power_state);

  // Updates the current and pending power state and notifies observers.
  void UpdatePowerState(chromeos::DisplayPowerState new_power_state);

  // Updates the cached internal display. nullptr if one does not exists.
  void UpdateInternalDisplayCache();

  // Helps in identifying if a configuration task needs to be scheduled.
  // Return true if any of the |requested_*| parameters have been updated. False
  // otherwise.
  bool ShouldRunConfigurationTask() const;

  // Returns true if there are pending configuration changes that should be done
  // seamlessly.
  bool HasPendingSeamlessConfiguration() const;

  // Returns true if there are pending configuration changes that require a full
  // modeset.
  bool HasPendingFullConfiguration() const;

  // Helper functions which will call the callbacks in
  // |in_progress_configuration_callbacks_| and
  // |queued_configuration_callbacks_| and clear the lists after. |success| is
  // the configuration status used when calling the callbacks.
  void CallAndClearInProgressCallbacks(bool success);
  void CallAndClearQueuedCallbacks(bool success);

  // Callbacks used to signal when the native platform has released/taken
  // display control.
  void OnDisplayControlTaken(DisplayControlCallback callback, bool success);
  void OnDisplayControlRelinquished(DisplayControlCallback callback,
                                    bool success);

  // Helper function that sends the actual command.
  // |callback| is called upon completion of the relinquish command.
  // |success| is the result from calling SetDisplayPowerInternal() in
  // RelinquishDisplay().
  void SendRelinquishDisplayControl(DisplayControlCallback callback,
                                    bool success);

  // Returns the requested VRR state listing the display ids which should have
  // VRR enabled, defaulting to the current state as needed.
  const base::flat_set<int64_t> GetRequestedVrrState() const;

  // Returns whether a configuration should occur on account of a pending VRR
  // request.
  bool ShouldConfigureVrr() const;

  // Returns the per-display refresh rate overrides which should be used for
  // a configuration attempt. If there is a full configuration pending,
  // there will be no overrides set. If no new overrides have been requested,
  // this will return the current state.
  RefreshRateOverrideMap GetRequestedRefreshRateOverrides() const;

  // Returns the current state of refresh rate overrides. This is determined
  // by comparing the refresh rates of the currently configured mode and the
  // display's native mode.
  RefreshRateOverrideMap GetCurrentRefreshRateOverrideState() const;

  // Dangling in DemoIntegrationTest.NewTab on chromeos-amd64-generic-rel-gtest.
  raw_ptr<StateController, DanglingUntriaged> state_controller_;
  // Dangling in DemoIntegrationTest.NewTab on chromeos-amd64-generic-rel-gtest.
  raw_ptr<SoftwareMirroringController, DanglingUntriaged> mirroring_controller_;
  std::unique_ptr<NativeDisplayDelegate> native_display_delegate_;

  // Used to enable modes which rely on panel fitting.
  bool is_panel_fitting_enabled_;

  // This is detected by the constructor to determine whether or not we should
  // be enabled. If this flag is set to false, any attempts to change the
  // display configuration will immediately fail without changing the state.
  bool configure_displays_;

  // Current configuration state.
  MultipleDisplayState current_display_state_;
  chromeos::DisplayPowerState current_power_state_;

  // Pending requests. These values are used when triggering the next display
  // configuration.
  //
  // Stores the user requested state or INVALID if nothing was requested.
  MultipleDisplayState requested_display_state_;

  // Stores the requested power state.
  std::optional<chromeos::DisplayPowerState> requested_power_state_;

  // The power state used by RunPendingConfiguration(). May be
  // |requested_power_state_| or DISPLAY_POWER_ALL_OFF for suspend.
  chromeos::DisplayPowerState pending_power_state_;

  // True if |pending_power_state_| has been changed.
  bool has_pending_power_state_;

  // Bitwise-or value of the |kSetDisplayPower*| flags defined above.
  int pending_power_flags_;

  // Per-display pending refresh rate override requests. Displays not included
  // in this map will have their refresh rates set to their native refresh
  // rates.
  std::optional<RefreshRateOverrideMap> pending_refresh_rate_overrides_;

  // List of callbacks from callers waiting for the display configuration to
  // start/finish. Note these callbacks belong to the pending request, not a
  // request currently active.
  std::vector<ConfigurationCallback> queued_configuration_callbacks_;

  // List of callbacks belonging to the currently running display configuration
  // task.
  std::vector<ConfigurationCallback> in_progress_configuration_callbacks_;

  // True if the caller wants to force the display configuration process.
  bool force_configure_;

  // Most-recently-used display configuration. Note that the actual
  // configuration changes asynchronously.
  DisplayStateList cached_displays_;

  base::ObserverList<Observer>::Unchecked observers_;

  // The timer to delay configuring displays. This is used to aggregate multiple
  // display configuration events when they are reported in short time spans.
  base::OneShotTimer configure_timer_;

  // Display controlled by an external entity.
  bool display_externally_controlled_;

  // True if a TakeControl or RelinquishControl has been called but the response
  // hasn't arrived yet.
  bool display_control_changing_;

  // Whether the displays are currently suspended.
  bool displays_suspended_;

  std::unique_ptr<DisplayLayoutManagerImpl> layout_manager_;
  std::unique_ptr<ContentProtectionManager> content_protection_manager_;

  std::unique_ptr<UpdateDisplayConfigurationTask> configuration_task_;

  // Indicates whether there is any connected display having no associated crtc.
  // This can be caused by crtc shortage. When it is true, the corresponding
  // notification will be created to inform user.
  bool has_unassociated_display_;

  // Stores the requested variable refresh rate state as a set of display ids
  // for which VRR should be enabled. All omitted displays should have VRR
  // disabled. Absent if there is no pending state.
  std::optional<base::flat_set<int64_t>> pending_vrr_state_ = std::nullopt;

  // This must be the last variable.
  base::WeakPtrFactory<DisplayConfigurator> weak_ptr_factory_{this};
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_CONFIGURATOR_H_
