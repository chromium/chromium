// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_WINDOW_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_handler/wm_drag_handler.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"

namespace gfx {
class PointF;
}

namespace ui {

class BitmapCursorOzone;
class OSExchangeData;
class PlatformWindowDelegate;
class WaylandConnection;
class XDGPopupWrapper;
class XDGSurfaceWrapper;

struct PlatformWindowInitProperties;

namespace {
class XDGShellObjectFactory;
}  // namespace

class WaylandWindow : public PlatformWindow,
                      public PlatformEventDispatcher,
                      public WmMoveResizeHandler,
                      public WmDragHandler {
 public:
  WaylandWindow(PlatformWindowDelegate* delegate,
                WaylandConnection* connection);
  ~WaylandWindow() override;

  static WaylandWindow* FromSurface(wl_surface* surface);

  bool Initialize(PlatformWindowInitProperties properties);

  wl_surface* surface() const { return surface_.get(); }
  XDGSurfaceWrapper* xdg_surface() const { return xdg_surface_.get(); }
  XDGPopupWrapper* xdg_popup() const { return xdg_popup_.get(); }

  // Apply the bounds specified in the most recent configure event. This should
  // be called after processing all pending events in the wayland connection.
  void ApplyPendingBounds();

  // Set whether this window has pointer focus and should dispatch mouse events.
  void set_pointer_focus(bool focus) { has_pointer_focus_ = focus; }
  bool has_pointer_focus() const { return has_pointer_focus_; }

  // Set whether this window has keyboard focus and should dispatch key events.
  void set_keyboard_focus(bool focus) { has_keyboard_focus_ = focus; }

  bool has_keyboard_focus() const { return has_keyboard_focus_; }

  // Set whether this window has touch focus and should dispatch touch events.
  void set_touch_focus(bool focus) { has_touch_focus_ = focus; }

  // Set a child of this window. It is very important in case of nested
  // xdg_popups as long as they must be destroyed in the back order.
  void set_child_window(WaylandWindow* window) { child_window_ = window; }

  // Set whether this window has an implicit grab (often referred to as capture
  // in Chrome code). Implicit grabs happen while a pointer is down.
  void set_has_implicit_grab(bool value) { has_implicit_grab_ = value; }
  bool has_implicit_grab() const { return has_implicit_grab_; }

  bool is_active() const { return is_active_; }

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location) override;

  // WmDragHandler
  void StartDrag(const ui::OSExchangeData& data,
                 int operation,
                 gfx::NativeCursor cursor,
                 base::OnceCallback<void(int)> callback) override;

  // PlatformWindow
  void Show() override;
  void Hide() override;
  void Close() override;
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
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  PlatformImeController* GetPlatformImeController() override;
  void SetRestoredBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInPixels() const override;

  // PlatformEventDispatcher
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  void HandleSurfaceConfigure(int32_t widht,
                              int32_t height,
                              bool is_maximized,
                              bool is_fullscreen,
                              bool is_activated);

  void OnCloseRequest();

  void OnDragEnter(const gfx::PointF& point,
                   std::unique_ptr<OSExchangeData> data,
                   int operation);
  int OnDragMotion(const gfx::PointF& point, uint32_t time, int operation);
  void OnDragDrop(std::unique_ptr<OSExchangeData> data);
  void OnDragLeave();
  void OnDragSessionClose(uint32_t dnd_action);

 private:
  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsFullscreen() const;

  void MaybeTriggerPendingStateChange();

  // Creates a popup window, which is visible as a menu window.
  void CreateXdgPopup();
  // Creates a surface window, which is visible as a main window.
  void CreateXdgSurface();
  // Creates a subsurface window, to host tooltip's content.
  void CreateTooltipSubSurface();

  // Gets a parent window for this window.
  WaylandWindow* GetParentWindow(gfx::AcceleratedWidget parent_widget);

  WmMoveResizeHandler* AsWmMoveResizeHandler();

  PlatformWindowDelegate* delegate_;
  WaylandConnection* connection_;
  WaylandWindow* parent_window_ = nullptr;
  WaylandWindow* child_window_ = nullptr;

  // Creates xdg objects based on xdg shell version.
  std::unique_ptr<XDGShellObjectFactory> xdg_shell_objects_factory_;

  wl::Object<wl_surface> surface_;
  wl::Object<wl_subsurface> tooltip_subsurface_;

  // Wrappers around xdg v5 and xdg v6 objects. WaylandWindow doesn't
  // know anything about the version.
  std::unique_ptr<XDGSurfaceWrapper> xdg_surface_;
  std::unique_ptr<XDGPopupWrapper> xdg_popup_;

  // The current cursor bitmap (immutable).
  scoped_refptr<BitmapCursorOzone> bitmap_;

  base::OnceCallback<void(int)> drag_closed_callback_;

  gfx::Rect bounds_;
  gfx::Rect pending_bounds_;
  // The bounds of our window before we were maximized or fullscreen.
  gfx::Rect restored_bounds_;
  bool has_pointer_focus_ = false;
  bool has_keyboard_focus_ = false;
  bool has_touch_focus_ = false;
  bool has_implicit_grab_ = false;

  // Stores current states of the window.
  ui::PlatformWindowState state_;
  // Stores a pending state of the window, which is used before the surface is
  // activated.
  ui::PlatformWindowState pending_state_;

  bool is_active_ = false;
  bool is_minimizing_ = false;

  bool is_tooltip_ = false;

  DISALLOW_COPY_AND_ASSIGN(WaylandWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_WINDOW_H_
