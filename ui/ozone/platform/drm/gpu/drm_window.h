// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/swap_result.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"
#include "ui/ozone/public/overlay_surface_candidate.h"
#include "ui/ozone/public/swap_completion_callback.h"

class SkBitmap;

namespace base {
class TimeDelta;
}  // namespace base

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ui {

class DrmDeviceManager;
class DrmOverlayValidator;
class HardwareDisplayController;
class ScreenManager;

// The GPU object representing a window.
//
// The main purpose of this object is to associate drawing surfaces with
// displays. A surface created with the same id as the window (from
// GetAcceleratedWidget()) will paint onto that window. A window with
// the same bounds as a display will paint onto that display.
//
// If there's no display whose bounds match the window's, the window is
// disconnected and its contents will not be visible to the user.
class DrmWindow {
 public:
  DrmWindow(gfx::AcceleratedWidget widget,
            DrmDeviceManager* device_manager,
            ScreenManager* screen_manager);

  DrmWindow(const DrmWindow&) = delete;
  DrmWindow& operator=(const DrmWindow&) = delete;

  ~DrmWindow();

  gfx::Rect bounds() const { return bounds_; }

  void Initialize();

  void Shutdown();

  // Returns the accelerated widget associated with the window.
  gfx::AcceleratedWidget GetAcceleratedWidget() const;

  // Returns the current controller the window is displaying on. Callers should
  // not cache the result as the controller may change as the window is moved.
  HardwareDisplayController* GetController() const;

  void SetController(HardwareDisplayController* controller);

  // Called when the window is resized/moved.
  void SetBounds(const gfx::Rect& bounds);

  // Update the HW cursor bitmap & move to the location if specified.
  // If the bitmap is empty, the cursor is hidden.
  void SetCursor(const std::vector<SkBitmap>& bitmaps,
                 const std::optional<gfx::Point>& location,
                 base::TimeDelta frame_delay);

  // Move the HW cursor to the specified location.
  void MoveCursor(const gfx::Point& location);

  void SchedulePageFlip(std::vector<DrmOverlayPlane> planes,
                        SwapCompletionOnceCallback submission_callback,
                        PresentationOnceCallback presentation_callback);
  OverlayStatusList TestPageFlip(
      const OverlaySurfaceCandidateList& overlay_params);

  const DrmOverlayPlaneList& last_submitted_planes() const {
    return last_submitted_planes_;
  }

 private:
  // Draw next frame in an animated cursor.
  void OnCursorAnimationTimeout();

  void UpdateCursorImage();
  void UpdateCursorLocation();

  // Draw the last set cursor & update the cursor plane.
  void ResetCursor();

  const gfx::AcceleratedWidget widget_;

  const raw_ptr<DrmDeviceManager> device_manager_;  // Not owned.
  const raw_ptr<ScreenManager> screen_manager_;     // Not owned.

  // The current bounds of the window.
  gfx::Rect bounds_;

  // The controller associated with the current window. This may be nullptr if
  // the window isn't over an active display.
  raw_ptr<HardwareDisplayController, DanglingUntriaged> controller_ = nullptr;
  std::unique_ptr<DrmOverlayValidator> overlay_validator_;

  base::RepeatingTimer cursor_timer_;
  std::vector<SkBitmap> cursor_bitmaps_;
  gfx::Point cursor_location_;
  int cursor_frame_ = 0;

  DrmOverlayPlaneList last_submitted_planes_;

  bool force_buffer_reallocation_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_H_
