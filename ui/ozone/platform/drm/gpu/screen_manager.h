// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_

#include <stdint.h>
#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/gfx/native_widget_types.h"
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
  ScreenManager();
  virtual ~ScreenManager();

  // Register a display controller. This must be called before trying to
  // configure it.
  void AddDisplayController(const scoped_refptr<DrmDevice>& drm,
                            uint32_t crtc,
                            uint32_t connector);

  // Remove a display controller from the list of active controllers. The
  // controller is removed since it was disconnected.
  void RemoveDisplayController(const scoped_refptr<DrmDevice>& drm,
                               uint32_t crtc);

  // Configure a display controller. The display controller is identified by
  // (|crtc|, |connector|) and the controller is modeset using |mode|.
  bool ConfigureDisplayController(const scoped_refptr<DrmDevice>& drm,
                                  uint32_t crtc,
                                  uint32_t connector,
                                  const gfx::Point& origin,
                                  const drmModeModeInfo& mode);

  // Disable the display controller identified by |crtc|. Note, the controller
  // may still be connected, so this does not remove the controller.
  bool DisableDisplayController(const scoped_refptr<DrmDevice>& drm,
                                uint32_t crtc);

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

  bool ActualConfigureDisplayController(const scoped_refptr<DrmDevice>& drm,
                                        uint32_t crtc,
                                        uint32_t connector,
                                        const gfx::Point& origin,
                                        const drmModeModeInfo& mode);

  // Returns an iterator into |controllers_| for the controller located at
  // |origin|.
  HardwareDisplayControllers::iterator FindActiveDisplayControllerByLocation(
      const gfx::Rect& bounds);

  // Returns an iterator into |controllers_| for the controller located at
  // |origin| with matching DRM device.
  HardwareDisplayControllers::iterator FindActiveDisplayControllerByLocation(
      const scoped_refptr<DrmDevice>& drm,
      const gfx::Rect& bounds);

  // Tries to set the controller identified by (|crtc|, |connector|) to mirror
  // those in |mirror|. |original| is an iterator to the HDC where the
  // controller is currently present.
  bool HandleMirrorMode(HardwareDisplayControllers::iterator original,
                        HardwareDisplayControllers::iterator mirror,
                        const scoped_refptr<DrmDevice>& drm,
                        uint32_t crtc,
                        uint32_t connector,
                        const drmModeModeInfo& mode);

  DrmOverlayPlane GetModesetBuffer(HardwareDisplayController* controller,
                                   const gfx::Rect& bounds);

  bool EnableController(HardwareDisplayController* controller);

  // Modeset the |controller| using |origin| and |mode|. If there is a window at
  // the controller location, then we'll re-use the current buffer.
  bool ModesetController(HardwareDisplayController* controller,
                         const gfx::Point& origin,
                         const drmModeModeInfo& mode);

  DrmWindow* FindWindowAt(const gfx::Rect& bounds) const;

  // List of display controllers (active and disabled).
  HardwareDisplayControllers controllers_;

  WidgetToWindowMap window_map_;

  DISALLOW_COPY_AND_ASSIGN(ScreenManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_
