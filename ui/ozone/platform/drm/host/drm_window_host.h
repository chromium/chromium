// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_WINDOW_HOST_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_WINDOW_HOST_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/host/gpu_thread_observer.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

class DrmCursor;
class DrmDisplayHostManager;
class DrmWindowHostManager;
class EventFactoryEvdev;
class GpuThreadAdapter;

// Implementation of the platform window. This object and its handle |widget_|
// uniquely identify a window. Since the DRI/GBM platform is split into 2
// pieces (Browser process and GPU process), internally we need to make sure the
// state is synchronized between the 2 processes.
//
// |widget_| is used in both processes to uniquely identify the window. This
// means that any state on the browser side needs to be propagated to the GPU.
// State propagation needs to happen before the state change is acknowledged to
// |delegate_| as |delegate_| is responsible for initializing the surface
// associated with the window (the surface is created on the GPU process).
class DrmWindowHost : public PlatformWindow,
                      public PlatformEventDispatcher,
                      public GpuThreadObserver {
 public:
  DrmWindowHost(PlatformWindowDelegate* delegate,
                const gfx::Rect& bounds,
                GpuThreadAdapter* sender,
                EventFactoryEvdev* event_factory,
                DrmCursor* cursor,
                DrmWindowHostManager* window_manager,
                DrmDisplayHostManager* display_manager);

  DrmWindowHost(const DrmWindowHost&) = delete;
  DrmWindowHost& operator=(const DrmWindowHost&) = delete;

  ~DrmWindowHost() override;

  void Initialize();

  gfx::AcceleratedWidget GetAcceleratedWidget() const;

  gfx::Rect GetCursorConfinedBounds() const;

  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInDIP() const override;
  void SetTitle(const std::u16string& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  void SetCursor(scoped_refptr<PlatformCursor> cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInDIP() const override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;

  void OnMouseEnter();

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // GpuThreadObserver:
  void OnGpuProcessLaunched() override;
  void OnGpuThreadReady() override;
  void OnGpuThreadRetired() override;

 private:
  void SendBoundsChange();

  const raw_ptr<PlatformWindowDelegate> delegate_;        // Not owned.
  const raw_ptr<GpuThreadAdapter> sender_;                // Not owned.
  const raw_ptr<EventFactoryEvdev> event_factory_;        // Not owned.
  const raw_ptr<DrmCursor> cursor_;                       // Not owned.
  const raw_ptr<DrmWindowHostManager> window_manager_;    // Not owned.
  const raw_ptr<DrmDisplayHostManager> display_manager_;  // Not owned.

  gfx::Rect bounds_;
  const gfx::AcceleratedWidget widget_;

  gfx::Rect cursor_confined_bounds_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_WINDOW_HOST_H_
