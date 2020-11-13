// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_

#include <stdint.h>
#include <memory>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/gpu/drm_display.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

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

  struct ControllerConfigParams {
    ControllerConfigParams(int64_t display_id,
                           scoped_refptr<DrmDevice> drm,
                           uint32_t crtc,
                           uint32_t connector,
                           gfx::Point origin,
                           std::unique_ptr<drmModeModeInfo> pmode);
    ControllerConfigParams(const ControllerConfigParams& other);
    ControllerConfigParams(ControllerConfigParams&& other);
    ~ControllerConfigParams();

    const int64_t display_id;
    const scoped_refptr<DrmDevice> drm;
    const uint32_t crtc;
    const uint32_t connector;
    const gfx::Point origin;
    std::unique_ptr<drmModeModeInfo> mode = nullptr;
  };

  ScreenManager();
  virtual ~ScreenManager();

  // Register a display controller. This must be called before trying to
  // configure it.
  void AddDisplayController(const scoped_refptr<DrmDevice>& drm,
                            uint32_t crtc,
                            uint32_t connector);

  // Remove display controllers from the list of active controllers. The
  // controllers are removed since they were disconnected.
  void RemoveDisplayControllers(const CrtcsWithDrmList& controllers_to_remove);

  // Enables/Disables the display controller based on if a mode exists.
  base::flat_map<int64_t, bool> ConfigureDisplayControllers(
      const std::vector<ScreenManager::ControllerConfigParams>&
          controllers_params);

  // Returns a reference to the display controller configured to display within
  // |bounds|. If the caller caches the controller it must also register as an
  // observer to be notified when the controller goes out of scope.
  HardwareDisplayController* GetDisplayController(const gfx::Rect& bounds);

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

 private:
  using HardwareDisplayControllers =
      std::vector<std::unique_ptr<HardwareDisplayController>>;
  using WidgetToWindowMap =
      std::unordered_map<gfx::AcceleratedWidget, std::unique_ptr<DrmWindow>>;

  // Returns an iterator into |controllers_| for the controller identified by
  // (|crtc|, |connector|).
  HardwareDisplayControllers::iterator FindDisplayController(
      const scoped_refptr<DrmDevice>& drm,
      uint32_t crtc);

  base::flat_map<int64_t, bool> TestAndModeset(
      const std::vector<ControllerConfigParams>& controllers_params);

  bool TestModeset(
      const std::vector<ControllerConfigParams>& controllers_params);

  base::flat_map<int64_t, bool> Modeset(
      const std::vector<ControllerConfigParams>& controllers_params);

  // Configures a display controller to be enabled. The display controller is
  // identified by (|crtc|, |connector|) and the controller is to be modeset
  // using |mode|. Controller modeset props are added into |commit_request|.
  bool SetDisplayControllerForEnableAndGetProps(
      CommitRequest* commit_request,
      const scoped_refptr<DrmDevice>& drm,
      uint32_t crtc,
      uint32_t connector,
      const gfx::Point& origin,
      const drmModeModeInfo& mode);

  // Configures a display controller to be disabled. The display controller is
  // identified by |crtc|. Controller modeset props are added into
  // |commit_request|.
  // Note: the controller may still be connected, so this does not remove the
  // controller.
  bool SetDisableDisplayControllerForDisableAndGetProps(
      CommitRequest* commit_request,
      const scoped_refptr<DrmDevice>& drm,
      uint32_t crtc);

  void UpdateControllerStateAfterModeset(const ControllerConfigParams& config,
                                         const CommitRequest& commit_request,
                                         bool did_succeed);

  // Returns an iterator into |controllers_| for the controller located at
  // |origin|.
  HardwareDisplayControllers::iterator FindActiveDisplayControllerByLocation(
      const gfx::Rect& bounds);

  // Returns an iterator into |controllers_| for the controller located at
  // |origin| with matching DRM device.
  HardwareDisplayControllers::iterator FindActiveDisplayControllerByLocation(
      const scoped_refptr<DrmDevice>& drm,
      const gfx::Rect& bounds);

  // Tries to set the |original| controller to mirror those in |mirror|.
  // |original| is an iterator to the HDC where the controller is currently
  // present.
  bool HandleMirrorMode(CommitRequest* commit_request,
                        HardwareDisplayControllers::iterator original,
                        HardwareDisplayControllers::iterator mirror,
                        const drmModeModeInfo& mode);

  DrmOverlayPlane GetModesetBuffer(HardwareDisplayController* controller,
                                   const gfx::Rect& bounds);

  // Gets props for modesetting the |controller| using |origin| and |mode|. If
  // there is a window at the controller location, then we'll re-use the current
  // buffer.
  bool GetModesetControllerProps(CommitRequest* commit_request,
                                 HardwareDisplayController* controller,
                                 const gfx::Point& origin,
                                 const drmModeModeInfo& mode);
  bool GetEnableControllerProps(CommitRequest* commit_request,
                                HardwareDisplayController* controller);

  DrmWindow* FindWindowAt(const gfx::Rect& bounds) const;

  // List of display controllers (active and disabled).
  HardwareDisplayControllers controllers_;

  WidgetToWindowMap window_map_;

  DISALLOW_COPY_AND_ASSIGN(ScreenManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_
