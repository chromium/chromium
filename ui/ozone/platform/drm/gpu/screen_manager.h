// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/common/tile_property.h"
#include "ui/ozone/platform/drm/gpu/drm_display.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/public/drm_modifiers_filter.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ui {

class DrmDevice;
class DrmWindow;

// Responsible for keeping track of active displays and configuring them.
class ScreenManager {
 public:
  using CrtcsWithDrmList =
      std::vector<std::pair<uint32_t, const scoped_refptr<DrmDevice>>>;

  ScreenManager();

  ScreenManager(const ScreenManager&) = delete;
  ScreenManager& operator=(const ScreenManager&) = delete;

  virtual ~ScreenManager();

  // Register a display controller. This must be called before trying to
  // configure it.
  void AddDisplayController(
      const scoped_refptr<DrmDevice>& drm,
      uint32_t crtc,
      uint32_t connector,
      std::optional<TileProperty> tile_property = std::nullopt);

  // Register all the display controllers corresponding to |display|.
  void AddDisplayControllersForDisplay(const DrmDisplay& display);

  // Remove display controllers from the list of active controllers. The
  // controllers are removed since they were disconnected.
  void RemoveDisplayControllers(const CrtcsWithDrmList& controllers_to_remove);

  // Enables/Disables the display controller based on if a mode exists. Adjusts
  // the behavior of the commit according to |modeset_flag| (see
  // display::ModesetFlag).
  bool ConfigureDisplayControllers(
      const std::vector<ControllerConfigParams>& controllers_params,
      display::ModesetFlags modeset_flags);

  // Returns a reference to the display controller configured to display within
  // |bounds|. If the caller caches the controller it must also register as an
  // observer to be notified when the controller goes out of scope.
  HardwareDisplayController* GetDisplayController(const gfx::Rect& bounds);

  // Returns a reference to the display controller associated with |crtc_id| on
  // |drm|. If the caller caches the controller it must also register as an
  // observer to be notified when the controller goes out of scope.
  HardwareDisplayController* GetDisplayController(
      const scoped_refptr<DrmDevice>& drm,
      int32_t crtc_id);

  // Adds a window for |widget|. Note: |widget| should not be associated with a
  // window when calling this function.
  void AddWindow(gfx::AcceleratedWidget widget,
                 std::unique_ptr<DrmWindow> window);

  // Removes the window for |widget|. Note: |widget| must have a window
  // associated with it when calling this function.
  std::unique_ptr<DrmWindow> RemoveWindow(gfx::AcceleratedWidget widget);

  // Returns the window associated with |widget|. Note: This function should be
  // called only if a valid window has been associated with |widget|.
  DrmWindow* GetWindow(gfx::AcceleratedWidget widget);

  // Updates the mapping between display controllers and windows such that a
  // controller will be associated with at most one window.
  void UpdateControllerToWindowMapping();

  // Adds trace records to |context|.
  void WriteIntoTrace(perfetto::TracedValue context) const;

  // Sets the DRM modifiers filter that removes modifiers incompatible with use
  // in raster and composite. This must be called during initialization before
  // any modeset happens.
  void SetDrmModifiersFilter(std::unique_ptr<DrmModifiersFilter> filter);

  // Replace CRTCs of HardwareDisplayControllers in |current_pairings| with
  // |new_pairings|, identified by their connectors.
  bool ReplaceDisplayControllersCrtcs(const scoped_refptr<DrmDevice>& drm,
                                      const ConnectorCrtcMap& current_pairings,
                                      const ConnectorCrtcMap& new_pairings);

 private:
  using HardwareDisplayControllers =
      std::vector<std::unique_ptr<HardwareDisplayController>>;
  using WidgetToWindowMap =
      std::unordered_map<gfx::AcceleratedWidget, std::unique_ptr<DrmWindow>>;
  using CrtcPreferredModifierMap = base::flat_map<
      uint32_t /*crtc_is*/,
      std::pair<bool /*modifiers_list.empty()*/, uint64_t /*picked_modifier*/>>;

  // Returns an iterator into |controllers_| for the controller identified by
  // (|crtc|, |connector|).
  HardwareDisplayControllers::iterator FindDisplayController(
      const scoped_refptr<DrmDevice>& drm,
      uint32_t crtc);

  bool TestAndSetPreferredModifiers(
      const std::vector<ControllerConfigParams>& controllers_params,
      bool is_seamless_modeset);
  bool TestAndSetLinearModifier(
      const std::vector<ControllerConfigParams>& controllers_params,
      bool is_seamless_modeset);
  // Setting the Preferred modifiers that passed from one of the Modeset Test
  // functions. The preferred modifiers are used in Modeset.
  void SetPreferredModifiers(
      const std::vector<ControllerConfigParams>& controllers_params,
      const CrtcPreferredModifierMap& crtcs_preferred_modifier);
  // The planes used for modesetting can have overlays beside the primary, test
  // if we can modeset with them. If not, return false to indicate that we must
  // only use the primary plane.
  bool TestModesetWithOverlays(
      const std::vector<ControllerConfigParams>& controllers_params,
      bool is_seamless_modeset);
  bool Modeset(const std::vector<ControllerConfigParams>& controllers_params,
               bool can_modeset_with_overlays,
               bool is_seamless_modeset);

  // Configures a display controller to be enabled. The display controller is
  // identified by (|crtc|, |connector|) and the controller is to be modeset
  // using |mode|. Controller modeset props are added into |commit_request|.
  void SetDisplayControllerForEnableAndGetProps(
      CommitRequest* commit_request,
      const scoped_refptr<DrmDevice>& drm,
      uint32_t crtc,
      uint32_t connector,
      const gfx::Point& origin,
      const drmModeModeInfo& mode,
      const DrmOverlayPlaneList& modeset_planes,
      bool enable_vrr);

  // Configures a display controller to be disabled. The display controller is
  // identified by |crtc|. Controller modeset props are added into
  // |commit_request|.
  // Note: the controller may still be connected, so this does not remove the
  // controller.
  bool SetDisableDisplayControllerForDisableAndGetProps(
      CommitRequest* commit_request,
      const scoped_refptr<DrmDevice>& drm,
      uint32_t crtc);

  void UpdateControllerStateAfterModeset(const scoped_refptr<DrmDevice>& drm,
                                         const CommitRequest& commit_request,
                                         bool did_succeed);

  void HandleMirrorIfExists(
      const scoped_refptr<DrmDevice>& drm,
      const CrtcCommitRequest& crtc_request,
      const HardwareDisplayControllers::iterator& controller);

  // Returns an iterator into |controllers_| for the controller located at
  // |origin|.
  HardwareDisplayControllers::iterator FindActiveDisplayControllerByLocation(
      const gfx::Rect& bounds);

  // Returns an iterator into |controllers_| for the controller located at
  // |origin| with matching DRM device.
  HardwareDisplayControllers::iterator FindActiveDisplayControllerByLocation(
      const scoped_refptr<DrmDevice>& drm,
      const gfx::Rect& bounds);

  DrmOverlayPlaneList GetModesetPlanes(HardwareDisplayController* controller,
                                       const gfx::Rect& bounds,
                                       const std::vector<uint64_t>& modifiers,
                                       bool include_overlays,
                                       bool is_testing);

  // Gets props for modesetting the |controller| using |origin| and |mode|.
  void GetModesetControllerProps(CommitRequest* commit_request,
                                 HardwareDisplayController* controller,
                                 const gfx::Point& origin,
                                 const drmModeModeInfo& mode,
                                 const DrmOverlayPlaneList& modeset_planes,
                                 bool enable_vrr);
  void GetEnableControllerProps(CommitRequest* commit_request,
                                HardwareDisplayController* controller,
                                const DrmOverlayPlaneList& modeset_planes);

  DrmWindow* FindWindowAt(const gfx::Rect& bounds) const;

  // This must be destructed before |controllers_|.
  std::unique_ptr<DrmModifiersFilter> drm_modifiers_filter_;

  // List of display controllers (active and disabled).
  HardwareDisplayControllers controllers_;

  WidgetToWindowMap window_map_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_
