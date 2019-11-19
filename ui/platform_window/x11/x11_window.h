// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_
#define UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_

#include "base/macros.h"
#include "ui/base/x/x11_window.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/platform_window_linux.h"
#include "ui/platform_window/x11/x11_window_export.h"

namespace ui {

class LocatedEvent;

// Delegate interface used to communicate the X11PlatformWindow API client about
// XEvents of interest.
class X11_WINDOW_EXPORT XEventDelegate {
 public:
  virtual ~XEventDelegate() {}

  // TODO(crbug.com/990756): We need to implement/reuse ozone interface for
  // these.
  virtual void OnXWindowSelectionEvent(XEvent* xev) = 0;
  virtual void OnXWindowDragDropEvent(XEvent* xev) = 0;
};

// PlatformWindow implementation for X11. PlatformEvents are XEvents.
class X11_WINDOW_EXPORT X11Window : public PlatformWindowLinux,
                                    public WmMoveResizeHandler,
                                    public XWindow,
                                    public PlatformEventDispatcher {
 public:
  explicit X11Window(PlatformWindowDelegateLinux* platform_window_delegate);
  ~X11Window() override;

  void Initialize(PlatformWindowInitProperties properties);

  void SetXEventDelegate(XEventDelegate* delegate);

  // X11WindowManager calls this.
  // XWindow override:
  void OnXWindowLostCapture() override;

  void OnMouseEnter();

  gfx::AcceleratedWidget GetWidget() const;

  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  bool IsSyncExtensionAvailable() const override;
  void OnCompleteSwapAfterResize() override;
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInPixels() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void SetZOrderLevel(ZOrderLevel order) override;
  ZOrderLevel GetZOrderLevel() const override;
  void StackAbove(gfx::AcceleratedWidget widget) override;
  void StackAtTop() override;
  base::Optional<int> GetWorkspace() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  void FlashFrame(bool flash_frame) override;
  gfx::Rect GetXRootWindowOuterBounds() const override;
  bool ContainsPointInXRegion(const gfx::Point& point) const override;
  void SetShape(std::unique_ptr<ShapeRects> native_shape,
                const gfx::Transform& transform) override;
  void SetOpacityForXWindow(float opacity) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;
  bool IsTranslucentWindowOpacitySupported() const override;
  void LowerXWindow() override;

 protected:
  PlatformWindowDelegateLinux* platform_window_delegate() const {
    return platform_window_delegate_;
  }

  bool is_shutting_down() const { return is_shutting_down_; }

  // XWindow:
  void OnXWindowCreated() override;

 private:
  bool HandleAsAtkEvent(XEvent* xev);

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // XWindow:
  void OnXWindowStateChanged() override;
  void OnXWindowDamageEvent(const gfx::Rect& damage_rect) override;
  void OnXWindowBoundsChanged(const gfx::Rect& size) override;
  void OnXWindowCloseRequested() override;
  void OnXWindowIsActiveChanged(bool active) override;
  void OnXWindowMapped() override;
  void OnXWindowUnmapped() override;
  void OnXWindowWorkspaceChanged() override;
  void OnXWindowLostPointerGrab() override;
  void OnXWindowEvent(ui::Event* event) override;
  void OnXWindowSelectionEvent(XEvent* xev) override;
  void OnXWindowDragDropEvent(XEvent* xev) override;
  base::Optional<gfx::Size> GetMinimumSizeForXWindow() override;
  base::Optional<gfx::Size> GetMaximumSizeForXWindow() override;
  void GetWindowMaskForXWindow(const gfx::Size& size,
                               SkPath* window_mask) override;

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) override;

  // X11WindowOzone sets own event dispatcher now.
  virtual void SetPlatformEventDispatcher();

  // Adjusts |requested_size_in_pixels| to avoid the WM "feature" where setting
  // the window size to the monitor size causes the WM to set the EWMH for
  // fullscreen.
  gfx::Size AdjustSizeForDisplay(const gfx::Size& requested_size_in_pixels);

  // Converts the location of the |located_event| from the
  // |current_window_bounds| to the |target_window_bounds|.
  void ConvertEventLocationToTargetLocation(
      const gfx::Rect& target_window_bounds,
      const gfx::Rect& current_window_bounds,
      ui::LocatedEvent* located_event);

  // Stores current state of this window.
  PlatformWindowState state_ = PlatformWindowState::kUnknown;

  PlatformWindowDelegateLinux* const platform_window_delegate_;

  XEventDelegate* x_event_delegate_ = nullptr;

  // Tells if the window got a ::Close call.
  bool is_shutting_down_ = false;

  // The z-order level of the window; the window exhibits "always on top"
  // behavior if > 0.
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;

  // The bounds of our window before the window was maximized.
  gfx::Rect restored_bounds_in_pixels_;

  DISALLOW_COPY_AND_ASSIGN(X11Window);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_
