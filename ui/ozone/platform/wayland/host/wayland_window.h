// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_handler/wm_drag_handler.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/platform_window_linux.h"

namespace gfx {
class PointF;
}

namespace ui {

class BitmapCursorOzone;
class OSExchangeData;
class WaylandConnection;
class XDGPopupWrapper;
class XDGSurfaceWrapper;

namespace {
class XDGShellObjectFactory;
}  // namespace

class WaylandWindow : public PlatformWindowLinux,
                      public PlatformEventDispatcher,
                      public WmMoveResizeHandler,
                      public WmDragHandler {
 public:
  WaylandWindow(PlatformWindowDelegate* delegate,
                WaylandConnection* connection);
  ~WaylandWindow() override;

  static WaylandWindow* FromSurface(wl_surface* surface);

  bool Initialize(PlatformWindowInitProperties properties);

  // Updates the surface buffer scale of the window.  Top level windows take
  // scale from the output attached to either their current display or the
  // primary one if their widget is not yet created, children inherit scale from
  // their parent.  The method recalculates window bounds appropriately if asked
  // to do so (this is not needed upon window initialization).
  void UpdateBufferScale(bool update_bounds);

  wl_surface* surface() const { return surface_.get(); }
  XDGSurfaceWrapper* xdg_surface() const { return xdg_surface_.get(); }
  XDGPopupWrapper* xdg_popup() const { return xdg_popup_.get(); }

  WaylandWindow* parent_window() const { return parent_window_; }

  gfx::AcceleratedWidget GetWidget() const;

  // Apply the bounds specified in the most recent configure event. This should
  // be called after processing all pending events in the wayland connection.
  void ApplyPendingBounds();

  // Set whether this window has pointer focus and should dispatch mouse events.
  void set_pointer_focus(bool focus) { has_pointer_focus_ = focus; }
  bool has_pointer_focus() const { return has_pointer_focus_; }

  // Set whether this window has keyboard focus and should dispatch key events.
  void set_keyboard_focus(bool focus) { has_keyboard_focus_ = focus; }
  bool has_keyboard_focus() const { return has_keyboard_focus_; }

  // The methods set or return whether this window has touch focus and should
  // dispatch touch events.
  void set_touch_focus(bool focus) { has_touch_focus_ = focus; }
  bool has_touch_focus() const { return has_touch_focus_; }

  // Set a child of this window. It is very important in case of nested
  // xdg_popups as long as they must be destroyed in the back order.
  void set_child_window(WaylandWindow* window) { child_window_ = window; }

  // Set whether this window has an implicit grab (often referred to as capture
  // in Chrome code). Implicit grabs happen while a pointer is down.
  void set_has_implicit_grab(bool value) { has_implicit_grab_ = value; }
  bool has_implicit_grab() const { return has_implicit_grab_; }

  int32_t buffer_scale() const { return buffer_scale_; }

  bool is_active() const { return is_active_; }

  const base::flat_set<uint32_t>& entered_outputs_ids() const {
    return entered_outputs_ids_;
  }

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) override;

  // WmDragHandler
  void StartDrag(const ui::OSExchangeData& data,
                 int operation,
                 gfx::NativeCursor cursor,
                 base::OnceCallback<void(int)> callback) override;

  // PlatformWindow
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
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInPixels() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;

  // PlatformEventDispatcher
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // Handles the configuration events coming from the surface (see
  // |XDGSurfaceWrapperV5::Configure| and
  // |XDGSurfaceWrapperV6::ConfigureTopLevel|.  The width and height come in
  // DIP of the output that the surface is currently bound to.
  void HandleSurfaceConfigure(int32_t widht,
                              int32_t height,
                              bool is_maximized,
                              bool is_fullscreen,
                              bool is_activated);
  void HandlePopupConfigure(const gfx::Rect& bounds);

  void OnCloseRequest();

  void OnDragEnter(const gfx::PointF& point,
                   std::unique_ptr<OSExchangeData> data,
                   int operation);
  int OnDragMotion(const gfx::PointF& point, uint32_t time, int operation);
  void OnDragDrop(std::unique_ptr<OSExchangeData> data);
  void OnDragLeave();
  void OnDragSessionClose(uint32_t dnd_action);

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandScreenTest, SetBufferScale);

  void SetBoundsDip(const gfx::Rect& bounds_dip);
  void SetBufferScale(int32_t scale, bool update_bounds);

  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsFullscreen() const;

  void MaybeTriggerPendingStateChange();

  // Creates a popup window, which is visible as a menu window.
  void CreateXdgPopup();
  // Creates a surface window, which is visible as a main window.
  void CreateXdgSurface();
  // Creates (if necessary) and show subsurface window, to host
  // tooltip's content.
  void CreateAndShowTooltipSubSurface();

  // Gets a parent window for this window.
  WaylandWindow* GetParentWindow(gfx::AcceleratedWidget parent_widget);

  WmMoveResizeHandler* AsWmMoveResizeHandler();

  // Install a surface listener and start getting wl_output enter/leave events.
  void AddSurfaceListener();

  void AddEnteredOutputId(struct wl_output* output);
  void RemoveEnteredOutputId(struct wl_output* output);

  void UpdateCursorPositionFromEvent(std::unique_ptr<Event> event);

  // Returns bounds with origin relative to parent window's origin.
  gfx::Rect AdjustPopupWindowPosition() const;

  WaylandWindow* GetTopLevelWindow();

  // It's important to set opaque region for opaque windows (provides
  // optimization hint for the Wayland compositor).
  void MaybeUpdateOpaqueRegion();

  bool IsOpaqueWindow() const;

  // wl_surface_listener
  static void Enter(void* data,
                    struct wl_surface* wl_surface,
                    struct wl_output* output);
  static void Leave(void* data,
                    struct wl_surface* wl_surface,
                    struct wl_output* output);

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

  // These bounds attributes below have suffices that indicate units used.
  // Wayland operates in DIP but the platform operates in physical pixels so
  // our WaylandWindow is the link that has to translate the units.  See also
  // comments in the implementation.
  //
  // Bounds that will be applied when the window state is finalized.  The window
  // may get several configuration events that update the pending bounds, and
  // only upon finalizing the state is the latest value stored as the current
  // bounds via |ApplyPendingBounds|.  Measured in DIP because updated in the
  // handler that receives DIP from Wayland.
  gfx::Rect pending_bounds_dip_;
  // Current bounds of the platform window.
  gfx::Rect bounds_px_;
  // The bounds of the platform window before it went maximized or fullscreen.
  gfx::Rect restored_bounds_px_;

  bool has_pointer_focus_ = false;
  bool has_keyboard_focus_ = false;
  bool has_touch_focus_ = false;
  bool has_implicit_grab_ = false;
  // Wayland's scale factor for the output that this window currently belongs
  // to.
  int32_t buffer_scale_ = 1;
  // The UI scale may be forced through the command line, which means that it
  // replaces the default value that is equal to the natural device scale.
  // We need it to place and size the menus properly.
  float ui_scale_ = 1.0;

  // Stores current states of the window.
  PlatformWindowState state_;
  // Stores a pending state of the window, which is used before the surface is
  // activated.
  PlatformWindowState pending_state_;

  // Stores current opacity of the window. Set on ::Initialize call.
  ui::PlatformWindowOpacity opacity_;

  bool is_active_ = false;
  bool is_minimizing_ = false;

  bool is_tooltip_ = false;

  // For top level window, stores IDs of outputs that the window is currently
  // rendered at.
  //
  // Not used by popups.  When sub-menus are hidden and shown again, Wayland
  // 'repositions' them to wrong outputs by sending them leave and enter
  // events so their list of entered outputs becomes meaningless after they have
  // been hidden at least once.  To determine which output the popup belongs to,
  // we ask its parent.
  base::flat_set<uint32_t> entered_outputs_ids_;

  DISALLOW_COPY_AND_ASSIGN(WaylandWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_
