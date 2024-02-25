// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
class TimeDelta;
}

namespace ui {
class BitmapCursor;
class DrmWindowHostManager;

// DrmCursor manages all cursor state but is dependent on an injected
// proxy for how it communicates state changes to other threads or
// processes. The proxy implementation must satisfy DrmCursorProxy.
class DrmCursorProxy {
 public:
  virtual ~DrmCursorProxy() = default;

  // Sets the cursor |bitmaps| on |window| at |point| with |frame_delay|.
  virtual void CursorSet(gfx::AcceleratedWidget window,
                         const std::vector<SkBitmap>& bitmaps,
                         const std::optional<gfx::Point>& point,
                         base::TimeDelta frame_delay) = 0;
  // Moves the cursor in |window| to |point|.
  virtual void Move(gfx::AcceleratedWidget window, const gfx::Point& point) = 0;

  // Initialize EvdevThread-specific state.
  virtual void InitializeOnEvdevIfNecessary() = 0;
};

// DrmCursor manages all cursor state and semantics.
class DrmCursor : public CursorDelegateEvdev {
 public:
  explicit DrmCursor(DrmWindowHostManager* window_manager);

  DrmCursor(const DrmCursor&) = delete;
  DrmCursor& operator=(const DrmCursor&) = delete;

  ~DrmCursor() override;

  // Sets or the DrmProxy |proxy|. If |proxy| is set, the DrmCursor uses
  // it to communicate to the GPU process or thread. Returns the previous
  // value.
  void SetDrmCursorProxy(std::unique_ptr<DrmCursorProxy> proxy);
  void ResetDrmCursorProxy();

  // Change the cursor over the specified window.
  void SetCursor(gfx::AcceleratedWidget window,
                 scoped_refptr<BitmapCursor> platform_cursor);

  // Handle window lifecycle.
  void OnWindowAdded(gfx::AcceleratedWidget window,
                     const gfx::Rect& bounds_in_screen,
                     const gfx::Rect& cursor_confined_bounds);
  void OnWindowRemoved(gfx::AcceleratedWidget window);

  // Handle window bounds changes.
  void CommitBoundsChange(gfx::AcceleratedWidget window,
                          const gfx::Rect& new_display_bounds_in_screen,
                          const gfx::Rect& new_confined_bounds);

  // CursorDelegateEvdev:
  void MoveCursorTo(gfx::AcceleratedWidget window,
                    const gfx::PointF& location) override;
  void MoveCursorTo(const gfx::PointF& screen_location) override;
  void MoveCursor(const gfx::Vector2dF& delta) override;
  bool IsCursorVisible() override;
  gfx::PointF GetLocation() override;
  gfx::Rect GetCursorConfinedBounds() override;
  void InitializeOnEvdev() override;

 private:
  void SetCursorLocationLocked(const gfx::PointF& location);
  void SendCursorShowLocked();
  void SendCursorHideLocked();
  void SendCursorMoveLocked();

  void MoveCursorToOnEvdevThread(const gfx::PointF& screen_location);
  void MoveCursorToOnUiThread(const gfx::PointF& screen_location);

  // Lock-testing helpers.
  void CursorSetLockTested(gfx::AcceleratedWidget window,
                           const std::vector<SkBitmap>& bitmaps,
                           const std::optional<gfx::Point>& point,
                           base::TimeDelta frame_delay);
  void MoveLockTested(gfx::AcceleratedWidget window, const gfx::Point& point);
  // The mutex synchronizing this object.
  base::Lock lock_;

  // Enforce our threading constraints.
  THREAD_CHECKER(ui_thread_checker_);
  THREAD_CHECKER(evdev_thread_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;

  // The location of the bitmap (the cursor location is the hotspot location).
  gfx::Point GetBitmapLocationLocked();

  // The current cursor bitmap (immutable).
  scoped_refptr<BitmapCursor> cursor_ GUARDED_BY(lock_);

  // The window under the cursor.
  gfx::AcceleratedWidget window_ GUARDED_BY(lock_);

  // The location of the cursor within the window.
  gfx::PointF location_ GUARDED_BY(lock_);

  // The bounds of the display under the cursor.
  gfx::Rect display_bounds_in_screen_ GUARDED_BY(lock_);

  // The bounds that the cursor is confined to in |window|.
  gfx::Rect confined_bounds_ GUARDED_BY(lock_);

  const raw_ptr<DrmWindowHostManager> window_manager_
      GUARDED_BY_CONTEXT(ui_thread_checker_);  // Not owned.

  std::unique_ptr<DrmCursorProxy> proxy_ GUARDED_BY(lock_);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_H_
