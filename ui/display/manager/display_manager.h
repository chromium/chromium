// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_MANAGER_H_
#define UI_DISPLAY_MANAGER_DISPLAY_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/display/manager/util/display_manager_util.h"
#include "ui/display/tablet_state.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/unified_desktop_utils.h"

namespace gfx {
class Insets;
class Rect;
}  // namespace gfx

namespace display {

class DisplayChangeObserver;
class DisplayLayoutStore;
class DisplayObserver;
class NativeDisplayDelegate;
class Screen;

namespace test {
class DisplayManagerTestApi;
}  // namespace test

// DisplayManager maintains the current display configurations,
// and notifies observers when configuration changes.
class DISPLAY_MANAGER_EXPORT DisplayManager
    : public DisplayConfigurator::SoftwareMirroringController {
 public:
  class DISPLAY_MANAGER_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void CreateDisplay(const Display& display) = 0;
    virtual void RemoveDisplay(const Display& display) = 0;
    virtual void UpdateDisplayMetrics(const Display& display,
                                      uint32_t metrics) = 0;

    // Create or updates the mirroring window with |display_info_list|.
    virtual void CreateOrUpdateMirroringDisplay(
        const DisplayInfoList& display_info_list) = 0;

    // Closes the mirror window if not necessary.
    virtual void CloseMirroringDisplayIfNotNecessary() = 0;

    // Sets the primary display by display id.
    virtual void SetPrimaryDisplayId(int64_t id) = 0;

    // Called before and after the display configuration changes.  When
    // |clear_focus| is true, the implementation should deactivate the active
    // window and set the focus window to NULL.
    virtual void PreDisplayConfigurationChange(bool clear_focus) = 0;
    virtual void PostDisplayConfigurationChange() = 0;
  };

  // How secondary displays will be used.
  // 1) EXTENDED mode extends the desktop onto additional displays, creating one
  //    root window for each display. Each display has a shelf and status tray,
  //    and each user window is only rendered on a single display.
  // 2) MIRRORING mode copies the content of the primary display to the second
  //    display via software mirroring. This only supports 2 displays for now.
  // 3) UNIFIED mode creates a virtual desktop with a *single* root window that
  //    spans multiple physical displays via software mirroring. The primary
  //    physical display has a shelf and status tray, and user windows may
  //    render spanning across multiple displays.
  //
  // WARNING: These values are persisted to logs. Entries should not be
  //          renumbered and numeric values should never be reused.
  enum MultiDisplayMode {
    EXTENDED = 0,
    MIRRORING = 1,
    UNIFIED = 2,

    // Always keep this the last item.
    MULTI_DISPLAY_MODE_LAST = UNIFIED,
  };

  explicit DisplayManager(std::unique_ptr<Screen> screen);

  DisplayManager(const DisplayManager&) = delete;
  DisplayManager& operator=(const DisplayManager&) = delete;

  ~DisplayManager() override;

  DisplayLayoutStore* layout_store() { return layout_store_.get(); }

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // When set to true, the DisplayManager calls OnDisplayMetricsChanged even if
  // the display's bounds didn't change. Used to swap primary display.
  void set_force_bounds_changed(bool force_bounds_changed) {
    force_bounds_changed_ = force_bounds_changed;
  }

  void set_internal_display_has_accelerometer(bool has_accelerometer) {
    internal_display_has_accelerometer_ = has_accelerometer;
  }

  // Returns the display id of the first display in the output list.
  int64_t first_display_id() const { return first_display_id_; }

  TouchDeviceManager* touch_device_manager() const {
    return touch_device_manager_.get();
  }

  DisplayConfigurator* configurator() { return display_configurator_.get(); }

  const UnifiedDesktopLayoutMatrix& current_unified_desktop_matrix() const {
    return current_unified_desktop_matrix_;
  }

  void SetConfigureDisplays(bool configure_displays);

  // Initializes displays using command line flag. Returns false if no command
  // line flag was provided.
  bool InitFromCommandLine();

  // Initialize default display.
  void InitDefaultDisplay();

  // Update the internal display's display info.
  void UpdateInternalDisplay(const ManagedDisplayInfo& display_info);

  // Initializes font related params that depends on display configuration.
  void RefreshFontParams();

  // Returns the display layout used for current displays.
  const DisplayLayout& GetCurrentDisplayLayout() const;

  // Returns the actual display layout after it has been resolved and applied.
  const DisplayLayout& GetCurrentResolvedDisplayLayout() const;

  // Returns the currently connected display list.
  DisplayIdList GetConnectedDisplayIdList() const;

  // Test if the `connected_display_id_list_` matches the internal state of
  // DisplayManager, which is a combination of
  // `hardware_mirroring_display_id_list_`, `software_mirroring_display_list_`
  // and the `display_id_list` argument which is the list of displays that host
  // the desktop environment. In unified desktop mode, the `active_id_list` will
  // be ignored because it is not a real display but virtual (i.e. not a
  // physical display).
  bool IsConnectedDisplayIdListInSyncWithCurrentState(
      const DisplayIdList& display_id_list) const;

  // Sets the layout for the current display pair. The |layout| specifies the
  // location of the displays relative to their parents.
  void SetLayoutForCurrentDisplays(std::unique_ptr<DisplayLayout> layout);

  // Returns display for given |display_id|.
  const Display& GetDisplayForId(int64_t display_id) const;

  // Checks the validity of given |display_id|.
  bool IsDisplayIdValid(int64_t display_id) const;

  void OnScreenBrightnessChanged(float brightness);

  // Finds the display that contains |point| in screen coordinates.  Returns
  // invalid display if there is no display that can satisfy the condition.
  const Display& FindDisplayContainingPoint(
      const gfx::Point& point_in_screen) const;

  // Sets the work area's |insets| to the display given by |display_id|.
  bool UpdateWorkAreaOfDisplay(int64_t display_id, const gfx::Insets& insets);

  // Registers the overscan insets for the display of the specified ID. Note
  // that the insets size should be specified in DIP size. It also triggers the
  // display's bounds change.
  void SetOverscanInsets(int64_t display_id, const gfx::Insets& insets_in_dip);

  // Sets the display's rotation for the given |source|. The new |rotation| will
  // also become active.
  void SetDisplayRotation(int64_t display_id,
                          Display::Rotation rotation,
                          Display::RotationSource source);

  // Sets the external display's configuration, including resolution change and
  // device scale factor change. Returns true if it changes the display
  // resolution so that the caller needs to show a notification in case the new
  // resolution actually doesn't work.
  bool SetDisplayMode(int64_t display_id,
                      const ManagedDisplayMode& display_mode);

  // Register per display properties.
  // |overscan_insets| is null if the display has no custom overscan insets.
  // |touch_calibration_data| is null if the display has no touch calibration
  // associated data.
  void RegisterDisplayProperty(
      int64_t display_id,
      Display::Rotation rotation,
      const gfx::Insets* overscan_insets,
      const gfx::Size& resolution_in_pixels,
      float device_scale_factor,
      float display_zoom_factor,
      const DisplaySizeToZoomFactorMap& display_zoom_factor_map,
      float refresh_rate,
      bool is_interlaced,
      VariableRefreshRateState variable_refresh_rate_state,
      const std::optional<float>& vsync_rate_min);

  // Register stored rotation properties for the internal display.
  void RegisterDisplayRotationProperties(bool rotation_lock,
                                         Display::Rotation rotation);

  // Returns the stored rotation lock preference if it has been loaded,
  // otherwise false.
  bool registered_internal_display_rotation_lock() const {
    return registered_internal_display_rotation_lock_;
  }

  // Returns the stored rotation preference for the internal display if it has
  // been loaded, otherwise |Display::Rotate_0|.
  Display::Rotation registered_internal_display_rotation() const {
    return registered_internal_display_rotation_;
  }

  // Fills in the display |mode| currently in use in |display_id| if found,
  // returning true in that case, otherwise false.
  bool GetActiveModeForDisplayId(int64_t display_id,
                                 ManagedDisplayMode* mode) const;

  // Returns true and fills in the display's selected |mode| if found, or false.
  bool GetSelectedModeForDisplayId(int64_t display_id,
                                   ManagedDisplayMode* mode) const;

  // Sets the selected mode of |display_id| to |display_mode| if it's a
  // supported mode. This doesn't trigger reconfiguration or observers
  // notifications. This is suitable to be used from within an observer
  // notification to prevent reentrance to UpdateDisplaysWith().
  void SetSelectedModeForDisplayId(int64_t display_id,
                                   const ManagedDisplayMode& display_mode);

  // Tells if the virtual resolution feature is enabled.
  bool IsDisplayUIScalingEnabled() const;

  // Returns the current overscan insets for the specified |display_id|.
  // Returns an empty insets (0, 0, 0, 0) if no insets are specified for the
  // display.
  gfx::Insets GetOverscanInsets(int64_t display_id) const;

  // Called when display configuration has changed. The new display
  // configurations is passed as a vector of Display object, which contains each
  // display's new information.
  void OnNativeDisplaysChanged(
      const std::vector<ManagedDisplayInfo>& display_info_list);

  // Updates current displays using current |display_info_|.
  void UpdateDisplays();

  // Returns the display at |index|. The display at 0 is no longer considered
  // "primary".
  const Display& GetDisplayAt(size_t index) const;

  const Display& GetPrimaryDisplayCandidate() const;

  // This is called by ScreenAsh when the primary display is requested, but
  // there is no valid display. It provides a display that
  // - has a non-empty screen rect
  // - has a valid gfx::BufferFormat
  // This exists to enable buggy observers assume that the primary display
  // will always have non-zero size and a valid gfx::BufferFormat. The right
  // solution to this problem is to fix those observers.
  // https://crbug.com/866714, https://crbug.com/1057501
  static const Display& GetFakePrimaryDisplay();

  // Returns the logical number of displays. This returns 1 when displays are
  // mirrored.
  size_t GetNumDisplays() const;

  // Returns only the currently active displays. This list does not include the
  // displays that will be removed if |UpdateDisplaysWith| is currently
  // executing.
  // See https://crbug.com/632755
  const Displays& active_only_display_list() const {
    return is_updating_display_list_ ? active_only_display_list_
                                     : active_display_list();
  }

  const Displays& active_display_list() const { return active_display_list_; }

  // Returns true if the display specified by |display_id| is currently
  // connected and active. (mirroring display isn't active, for example).
  bool IsActiveDisplayId(int64_t display_id) const;

  // Returns the number of connected displays. For example, this returns 2 in
  // mirror mode with one external display.
  size_t num_connected_displays() const {
    return connected_display_id_list_.size();
  }

  // Returns true if either software or hardware mirror mode is active.
  bool IsInMirrorMode() const;

  // Returns true if software mirror mode is active. Note that when
  // SoftwareMirroringEnabled() returns true, it only means software mirroring
  // mode is requested, but it does not guarantee that the mode is active. The
  // mode will be active after UpdateDisplaysWith() is called.
  bool IsInSoftwareMirrorMode() const;

  // Returns true if hardware mirror mode is active.
  bool IsInHardwareMirrorMode() const;

  int64_t mirroring_source_id() const { return mirroring_source_id_; }

  // Returns a list of mirroring destination display ids.
  DisplayIdList GetMirroringDestinationDisplayIdList() const;

  const Displays& software_mirroring_display_list() const {
    return software_mirroring_display_list_;
  }

  // Used in test to prevent previous mirror modes affecting current mode.
  void set_disable_restoring_mirror_mode_for_test(bool disabled) {
    disable_restoring_mirror_mode_for_test_ = disabled;
  }

  const std::set<int64_t>& external_display_mirror_info() const {
    return external_display_mirror_info_;
  }

  void set_external_display_mirror_info(
      const std::set<int64_t>& external_display_mirror_info) {
    external_display_mirror_info_ = external_display_mirror_info;
  }

  void set_should_restore_mirror_mode_from_display_prefs(bool value) {
    should_restore_mirror_mode_from_display_prefs_ = value;
  }

  const std::optional<MixedMirrorModeParams>& mixed_mirror_mode_params() const {
    return mixed_mirror_mode_params_;
  }

  // Set mixed mirror mode parameters. The parameters will be used to restore
  // mixed mirror mode in the next display configuration. (Use SetMirrorMode()
  // to immediately switch to mixed mirror mode.)
  void set_mixed_mirror_mode_params(
      std::optional<MixedMirrorModeParams> mixed_params) {
    mixed_mirror_mode_params_ = std::move(mixed_params);
  }

  void dec_screen_capture_active_counter() {
    DCHECK_GT(screen_capture_active_counter_, 0);
    screen_capture_active_counter_--;
  }

  void inc_screen_capture_active_counter() { ++screen_capture_active_counter_; }

  bool screen_capture_is_active() const {
    return screen_capture_active_counter_ > 0;
  }

  // Remove mirroring source and destination displays, so that they will be
  // updated when UpdateDisplaysWith() is called.
  void ClearMirroringSourceAndDestination();

  // Sets/gets if the unified desktop feature is enabled.
  void SetUnifiedDesktopEnabled(bool enabled);
  bool unified_desktop_enabled() const { return unified_desktop_enabled_; }

  // Returns true if it's in unified desktop mode.
  bool IsInUnifiedMode() const;

  // Sets the Unified Desktop layout using the given |matrix| and sets the
  // current mode to Unified Desktop.
  void SetUnifiedDesktopMatrix(const UnifiedDesktopLayoutMatrix& matrix);

  // Returns the Unified Desktop mode mirroring display according to the
  // supplied |cell_position| in the matrix. Returns invalid display if we're
  // not in Unified mode.
  Display GetMirroringDisplayForUnifiedDesktop(
      DisplayPositionInUnifiedMatrix cell_position) const;

  // Returns the index of the row in the Unified Mode layout matrix which
  // contains the display with |display_id|.
  int GetMirroringDisplayRowIndexInUnifiedMatrix(int64_t display_id) const;

  // Returns the maximum display height of the row with |row_index| in the
  // Unified Mode layout matrix.
  int GetUnifiedDesktopRowMaxHeight(int row_index) const;

  // Returns the display used for software mirroring. Returns invalid display
  // if not found.
  const Display GetMirroringDisplayById(int64_t id) const;

  // Retuns the display info associated with |display_id|.
  const ManagedDisplayInfo& GetDisplayInfo(int64_t display_id) const;

  // Returns the human-readable name for the display |id|.
  std::string GetDisplayNameForId(int64_t id) const;

  // Returns true if mirror mode should be set on for the specified displays.
  // If |should_check_hardware_mirroring| is true, the state of
  // IsInHardwareMirroringMode() will also be taken into account.
  bool ShouldSetMirrorModeOn(const DisplayIdList& id_list,
                             bool should_check_hardware_mirroring);

  // Change the mirror mode. |mixed_params| will be ignored if mirror mode is
  // off or normal. When mirror mode is off, display mode will be set to default
  // mode (either extended mode or unified desktop mode). When mirror mode is
  // normal, the default source display will be mirrored to all other displays.
  // When mirror mode is mixed, the specified source display will be mirrored to
  // the specified destination displays and all other connected displays will be
  // extended.
  void SetMirrorMode(MirrorMode mode,
                     const std::optional<MixedMirrorModeParams>& mixed_params);

  // Used to emulate display change when run in a desktop environment instead
  // of on a device.
  void AddRemoveDisplay();
  void ToggleDisplayScaleFactor();

  void InitConfigurator(std::unique_ptr<NativeDisplayDelegate> delegate);
  void ForceInitialConfigureWithObservers(
      display::DisplayChangeObserver* display_change_observer,
      display::DisplayConfigurator::Observer* display_error_observer);

  // SoftwareMirroringController override:
  void SetSoftwareMirroring(bool enabled) override;
  bool SoftwareMirroringEnabled() const override;
  bool IsSoftwareMirroringEnforced() const override;

  // Sets the touch calibration data via `TouchDeviceManager`, mapping
  // `touchdevice` to the given `display_id`. If `apply_spatial_calibration` is
  // true, the bounds and valid screen space of the target touch device are also
  // calibrated, otherwise this information is thrown out.
  void SetTouchCalibrationData(
      int64_t display_id,
      const TouchCalibrationData::CalibrationPointPairQuad& point_pair_quad,
      const gfx::Size& display_bounds,
      const ui::TouchscreenDevice& touchdevice,
      bool apply_spatial_calibration);
  void ClearTouchCalibrationData(
      int64_t display_id,
      std::optional<ui::TouchscreenDevice> touchdevice);
  void UpdateZoomFactor(int64_t display_id, float zoom_factor);
  bool HasUnassociatedDisplay() const;

  // Sets/gets default multi display mode.
  void SetDefaultMultiDisplayModeForCurrentDisplays(MultiDisplayMode mode);
  MultiDisplayMode current_default_multi_display_mode() const {
    return current_default_multi_display_mode_;
  }

  // Sets multi display mode.
  void SetMultiDisplayMode(MultiDisplayMode mode);

  // Reconfigure display configuration using the same physical display.
  // TODO(oshima): Refactor and move this impl to |SetDefaultMultiDisplayMode|.
  void ReconfigureDisplays();

  // Update the bounds of the display given by |display_id|.
  bool UpdateDisplayBounds(int64_t display_id, const gfx::Rect& new_bounds);

  // Creates mirror window asynchronously if the software mirror mode is
  // enabled.
  void CreateMirrorWindowAsyncIfAny();

  // A unit test may change the internal display id (which never happens on a
  // real device). This will update the mode list for internal display for this
  // test scenario.
  void UpdateInternalManagedDisplayModeListForTest();

  // Zooms the display identified by |display_id| by increasing or decreasing
  // its zoom factor value by 1 unit. Zooming in will have no effect on the
  // display if it is already at its maximum zoom. Vice versa for zooming out.
  bool ZoomDisplay(int64_t display_id, bool up);

  // Resets the zoom value to 1 for the display identified by |display_id|.
  void ResetDisplayZoom(int64_t display_id);

  // Sets `tablet_state_` and notifies observers of display.
  void SetTabletState(const TabletState& tablet_state);

  // DisplayObserver notification utilities.
  void NotifyMetricsChanged(const Display& display, uint32_t metrics);
  void NotifyDisplayAdded(const Display& display);
  void NotifyWillRemoveDisplays(const Displays& display);
  void NotifyDisplaysRemoved(const Displays& displays);

  // DisplayManagerObserver notification utilities.
  void NotifyDisplaysInitialized();
  void NotifyWillProcessDisplayChanges();
  void NotifyDidProcessDisplayChanges(
      const DisplayManagerObserver::DisplayConfigurationChange& config_change);
  void NotifyWillApplyDisplayChanges(bool clear_focus);
  void NotifyDidApplyDisplayChanges();

  // Delegated from the Screen implementation.
  void AddDisplayObserver(DisplayObserver* observer);
  void RemoveDisplayObserver(DisplayObserver* observer);

  // Add/Remove interface for DisplayManager::Obserevrs.
  void AddDisplayManagerObserver(DisplayManagerObserver* observer);
  void RemoveDisplayManagerObserver(DisplayManagerObserver* observer);

  display::TabletState GetTabletState() const;

 private:
  friend class test::DisplayManagerTestApi;
  friend class DisplayLayoutStore;

  // See description above |notify_depth_| for details.
  class BeginEndNotifier {
   public:
    explicit BeginEndNotifier(DisplayManager* display_manager,
                              bool notify_on_pending_change_only = false);

    BeginEndNotifier(const BeginEndNotifier&) = delete;
    BeginEndNotifier& operator=(const BeginEndNotifier&) = delete;

    ~BeginEndNotifier();

   private:
    // Uses the pending display change data in display manager to create the
    // config change object propagated to observers.
    DisplayManagerObserver::DisplayConfigurationChange CreateConfigChange()
        const;

    // Propagates change notifications only if `pending_display_changes_` is
    // non-empty. This is necessary to handle change notifications triggering
    // further changes and nested notifications.
    // TODO(crbug.com/328134509): Update DisplayManager to better handle display
    // changes during change propagation.
    bool notify_on_pending_change_only_ = false;

    raw_ptr<DisplayManager> display_manager_;
  };

  // Tracks the in-progress change to the current display configuration. This is
  // reported to observers when the last BeginEndNotifier goes out of scope.
  struct PendingDisplayChanges {
    PendingDisplayChanges();
    PendingDisplayChanges(const PendingDisplayChanges&) = delete;
    PendingDisplayChanges& operator=(const PendingDisplayChanges&) = delete;
    ~PendingDisplayChanges();

    // True if there are no stored pending changes.
    bool IsEmpty() const;

    // Store added display_ids to avoid copying potentially stale display
    // objects while update state is accumulated.
    DisplayIdList added_display_ids;

    // Store displays by value as removed displays are no longer persisted by
    // the manager once removed from the `active_display_list_`.
    Displays removed_displays;

    // Maps the display_id to its metrics change.
    base::flat_map<int64_t, uint32_t> display_metrics_changes;
  };

  void set_change_display_upon_host_resize(bool value) {
    change_display_upon_host_resize_ = value;
  }

  // Updates the internal display data using `updated_display_info_list` and
  // notifies observers about the changes.
  void UpdateDisplaysWith(
      const std::vector<ManagedDisplayInfo>& updated_display_info_list);

  // Creates software mirroring display related information. The display used to
  // mirror the content is removed from the |display_info_list|.
  void CreateSoftwareMirroringDisplayInfo(DisplayInfoList* display_info_list);

  // Same as above but for Unified Desktop.
  void CreateUnifiedDesktopDisplayInfo(DisplayInfoList* display_info_list);

  // Finds an display for given |display_id|. Returns nullptr if not found.
  Display* FindDisplayForId(int64_t display_id);

  // Add the mirror display's display info if the software based mirroring is in
  // use. This should only be called before UpdateDisplaysWith().
  void AddMirrorDisplayInfoIfAny(DisplayInfoList* display_info_list);

  // Inserts and update the ManagedDisplayInfo according to the overscan state.
  // Note that The ManagedDisplayInfo stored in the |internal_display_info_| can
  // be different from |new_info| (due to overscan state), so you must use
  // |GetDisplayInfo| to get the correct ManagedDisplayInfo for a display.
  void InsertAndUpdateDisplayInfo(const ManagedDisplayInfo& new_info);

  // Applies recommended zoom factor when necessary, only used when an external
  // display is connected for the first time. e.g. when a 4K native mode is used
  // when firstly connected, the content is almost certainly too small.
  void ApplyDefaultZoomFactorIfNecessary(ManagedDisplayInfo& info);

  // Creates a display object from the ManagedDisplayInfo for
  // |display_id|.
  Display CreateDisplayFromDisplayInfoById(int64_t display_id);

  // Creates a display object from the ManagedDisplayInfo for |display_id| for
  // mirroring. The size of the display will be scaled using |scale| with the
  // offset using |origin|.
  Display CreateMirroringDisplayFromDisplayInfoById(int64_t display_id,
                                                    const gfx::Point& origin,
                                                    float scale);

  // Updates the bounds of all non-primary displays in |display_list| and append
  // the indices of displays updated to |updated_indices|.  When the size of
  // |display_list| equals 2, the bounds are updated using the layout registered
  // for the display pair. For more than 2 displays, the bounds are updated
  // using horizontal layout.
  void UpdateNonPrimaryDisplayBoundsForLayout(
      Displays* display_list,
      std::vector<size_t>* updated_indices);

  void CreateMirrorWindowIfAny();

  void RunPendingTasksForTest();

  // Applies the |layout| and updates the bounds of displays in |display_list|.
  // |updated_ids| contains the ids for displays whose bounds have changed.
  void ApplyDisplayLayout(DisplayLayout* layout,
                          Displays* display_list,
                          std::vector<int64_t>* updated_ids);

  // Update the info used to restore mirror mode.
  void UpdateInfoForRestoringMirrorMode();

  void UpdatePrimaryDisplayIdIfNecessary();

  void UpdateLayoutForMixedMode();

  raw_ptr<Delegate> delegate_ = nullptr;  // not owned.

  // When set to true, DisplayManager will use DisplayConfigurator to configure
  // displays. By default, this is set to true when running on device and false
  // when running off device.
  bool configure_displays_ = false;

  std::unique_ptr<Screen> screen_;

  std::unique_ptr<DisplayLayoutStore> layout_store_;

  std::unique_ptr<DisplayLayout> current_resolved_layout_;

  // The matrix that's used to layout the displays in Unified Desktop mode.
  UnifiedDesktopLayoutMatrix current_unified_desktop_matrix_;

  std::map<int64_t, int> mirroring_display_id_to_unified_matrix_row_;

  std::vector<int> unified_display_rows_heights_;

  int64_t first_display_id_ = kInvalidDisplayId;

  // List of current active displays. Active displays are the ones used to host
  // the user's desktop environment and exclude displays that mirror other
  // displays. This may contain the off-screen, virtual display (e.g. in unified
  // desktop mode).
  Displays active_display_list_;
  // This list does not include the displays that will be removed if
  // |UpdateDisplaysWith| is under execution.
  // See https://crbug.com/632755
  Displays active_only_display_list_;

  // True if active_display_list is being modified and has displays that are not
  // presently active.
  // See https://crbug.com/632755
  bool is_updating_display_list_ = false;

  // True if the display manager is currently performing an update in
  // `UpdateDisplaysWith()`. Remove once root cause behind primary display
  // issue has been resolved (see crbug.com/330166338).
  bool is_updating_displays_ = false;

  DisplayIdList connected_display_id_list_;

  bool force_bounds_changed_ = false;

  // The mapping from the display ID to its internal data.
  std::map<int64_t, ManagedDisplayInfo> display_info_;

  // Selected display modes for displays. Key is the displays' ID.
  std::map<int64_t, ManagedDisplayMode> display_modes_;

  // When set to true, the host window's resize event updates the display's
  // size. This is set to true when running on desktop environment (for
  // debugging) so that resizing the host window will update the display
  // properly. This is set to false on device as well as during the unit tests.
  bool change_display_upon_host_resize_ = false;

  MultiDisplayMode multi_display_mode_ = EXTENDED;
  MultiDisplayMode current_default_multi_display_mode_ = EXTENDED;

  // This is used in two distinct ways:
  // 1. The source display id when software mirroring is active.
  // 2. There's no source and destination display in hardware mirroring, so we
  // treat the first mirroring display id as source id when hardware mirroring
  // is active.
  int64_t mirroring_source_id_ = kInvalidDisplayId;

  // This is used in two distinct ways:
  // 1. when software mirroring is active this contains the destination
  // displays.
  // 2. when unified mode is enabled this is the set of physical displays.
  Displays software_mirroring_display_list_;

  // There's no source and destination display in hardware mirroring, so we
  // treat the first mirroring display as source and store its id in
  // |mirroring_source_id_| and treat the rest of mirroring displays as
  // destination and store their ids in this list.
  DisplayIdList hardware_mirroring_display_id_list_;

  // Stores external displays that were in mirror mode before.
  // These are display ids without output index.
  std::set<int64_t> external_display_mirror_info_;

  // This is set to true when the display prefs have been loaded from local
  // state to signal that we should restore the mirror mode state from
  // |external_display_mirror_info_| in the upcoming display re-configuration.
  bool should_restore_mirror_mode_from_display_prefs_ = false;

  // True if mirror mode should not be restored. Only used in test.
  bool disable_restoring_mirror_mode_for_test_ = false;

  // Cached mirror mode for metrics changed notification.
  bool mirror_mode_for_metrics_ = false;

  // User preference for rotation lock of the internal display.
  bool registered_internal_display_rotation_lock_ = false;

  // User preference for the rotation of the internal display.
  Display::Rotation registered_internal_display_rotation_ = Display::ROTATE_0;

  bool unified_desktop_enabled_ = false;

  bool internal_display_has_accelerometer_ = false;

  // Set during screen capture to enable software compositing of mouse cursor,
  // this is a counter to enable multiple active sessions at once.
  int screen_capture_active_counter_ = 0;

  // Holds a callback to help RunPendingTasksForTest() to exit at the correct
  // time.
  base::OnceClosure created_mirror_window_;

  // TODO(oshima): Make this non reentrant.
  base::ObserverList<DisplayObserver> display_observers_;

  base::ObserverList<DisplayManagerObserver> manager_observers_;

  // Not empty if mixed mirror mode should be turned on (the specified source
  // display is mirrored to the specified destination displays). Empty if mixed
  // mirror mode is disabled.
  std::optional<MixedMirrorModeParams> mixed_mirror_mode_params_;

  // This is incremented whenever a BeginEndNotifier is created and decremented
  // when destroyed. BeginEndNotifier uses this to track when it should call
  // OnWillProcessDisplayChanges() and OnDidProcessDisplayChanges().
  int notify_depth_ = 0;

  // State accumulated during a display configuration update. Created when
  // BeginEndNotifier is created and propagated in OnDidProcessDisplayChanges()
  // when the last BeginEndNotifier is destroyed.
  std::optional<PendingDisplayChanges> pending_display_changes_;

  std::unique_ptr<display::DisplayConfigurator> display_configurator_;

  std::unique_ptr<TouchDeviceManager> touch_device_manager_;

  // A cancelable callback to trigger sending UMA metrics when display zoom is
  // updated. The reason we need a cancelable callback is because we dont want
  // to record UMA metrics for changes to the display zoom that are temporary.
  // Temporary changes may include things like the user trying out different
  // zoom levels before making the final decision.
  base::CancelableOnceClosure on_display_zoom_modify_timeout_;

  // Stores the id of the display being added during creation process. This is
  // used to skip updating.
  // TODO(crbug.com/329003664): Consolidate this logic and BeginEndNotifier.
  std::optional<int64_t> in_creating_display_;

  display::TabletState tablet_state_ = display::TabletState::kInClamshellMode;

  base::WeakPtrFactory<DisplayManager> weak_ptr_factory_{this};
};

}  // namespace display

namespace base {

// Since DisplayManagerObserver and DisplayObserver has custom methods to add
// and remove observers, we need define new trait customizations to use
// `base::ScopedObservation` and `base::ScopedMultiSourceObservation`. See
// `base/scoped_observation_traits.h` for more details.
template <>
struct ScopedObservationTraits<display::DisplayManager,
                               display::DisplayManagerObserver> {
  static void AddObserver(display::DisplayManager* source,
                          display::DisplayManagerObserver* observer) {
    source->AddDisplayManagerObserver(observer);
  }
  static void RemoveObserver(display::DisplayManager* source,
                             display::DisplayManagerObserver* observer) {
    source->RemoveDisplayManagerObserver(observer);
  }
};

template <>
struct ScopedObservationTraits<display::DisplayManager,
                               display::DisplayObserver> {
  static void AddObserver(display::DisplayManager* source,
                          display::DisplayObserver* observer) {
    source->AddDisplayObserver(observer);
  }
  static void RemoveObserver(display::DisplayManager* source,
                             display::DisplayObserver* observer) {
    source->RemoveDisplayObserver(observer);
  }
};

}  // namespace base

#endif  // UI_DISPLAY_MANAGER_DISPLAY_MANAGER_H_
