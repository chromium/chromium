// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_WINDOW_H_
#define UI_BASE_X_X11_WINDOW_H_

#include <array>
#include <memory>
#include <string>

#include "base/cancelable_callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"

class SkPath;

namespace gfx {
class ImageSkia;
class Transform;
}  // namespace gfx

namespace ui {

class Event;
class XScopedEventSelector;

////////////////////////////////////////////////////////////////////////////////
// XWindow class
//
// Encapsulates a full featured Xlib-based X11 Window, intended mainly to be
// used in Linux desktop. Abstracts away most of X11 API interaction and
// communicates events (and ask some required information) through
// |XWindow::Delegate| interface.
//
// |XWindow::Configuration| holds parameters used in window initialization.
// Fields are equivalent and a sub-set of Widget::InitParams.
//
// All bounds and size values are assumed to be expressed in pixels.
class COMPONENT_EXPORT(UI_BASE_X) XWindow {
 public:
  class Delegate;

  using NativeShapeRects = std::vector<gfx::Rect>;

  enum class WindowType {
    kWindow,
    kPopup,
    kMenu,
    kTooltip,
    kDrag,
    kBubble,
  };

  enum class WindowOpacity {
    kInferOpacity,
    kOpaqueWindow,
    kTranslucentWindow,
  };

  struct Configuration final {
    Configuration();
    Configuration(const Configuration& config);
    ~Configuration();

    WindowType type;
    WindowOpacity opacity;
    gfx::Rect bounds;
    gfx::ImageSkia* icon;
    base::Optional<int> background_color;
    bool activatable;
    bool force_show_in_taskbar;
    bool keep_on_top;
    bool visible_on_all_workspaces;
    bool remove_standard_frame;
    bool prefer_dark_theme;
    std::string workspace;
    std::string wm_class_name;
    std::string wm_class_class;
    std::string wm_role_name;
    base::Optional<int> visual_id;
  };

  XWindow();
  virtual ~XWindow();

  void Init(const Configuration& config);
  void Map(bool inactive = false);
  void Close();
  void Maximize();
  void Minimize();
  void Unmaximize();
  bool Hide();
  void Unhide();
  void SetFullscreen(bool fullscreen);
  void Activate();
  void Deactivate();
  bool IsActive() const;
  void GrabPointer();
  void ReleasePointerGrab();
  void StackXWindowAbove(::Window window);
  void StackXWindowAtTop();
  bool IsTargetedBy(const XEvent& xev) const;
  void WmMoveResize(int hittest, const gfx::Point& location) const;
  void ProcessEvent(XEvent* xev);

  void SetSize(const gfx::Size& size_in_pixels);
  void SetBounds(const gfx::Rect& requested_bounds);
  bool IsXWindowVisible() const;
  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsFullscreen() const;
  gfx::Rect GetOutterBounds() const;

  void SetCursor(::Cursor cursor);
  bool SetTitle(base::string16 title);
  void SetXWindowOpacity(float opacity);
  void SetXWindowAspectRatio(const gfx::SizeF& aspect_ratio);
  void SetXWindowIcons(const gfx::ImageSkia& window_icon,
                       const gfx::ImageSkia& app_icon);
  void SetXWindowVisibleOnAllWorkspaces(bool visible);
  bool IsXWindowVisibleOnAllWorkspaces() const;
  void MoveCursorTo(const gfx::Point& location);
  void SetAlwaysOnTop(bool always_on_top);
  void SetFlashFrameHint(bool flash_frame);
  void UpdateMinAndMaxSize();
  void SetUseNativeFrame(bool use_native_frame);
  void DispatchResize();
  void CancelResize();
  void NotifySwapAfterResize();
  void ConfineCursorTo(const gfx::Rect& bounds);
  void LowerWindow();

  // Returns if the point is within XWindow shape. If shape is not set, always
  // returns true.
  bool ContainsPointInRegion(const gfx::Point& point) const;

  void SetXWindowShape(std::unique_ptr<NativeShapeRects> native_shape,
                       const gfx::Transform& transform);

  // Resets the window region for the current window bounds if necessary.
  void ResetWindowRegion();

  gfx::Rect bounds() const { return bounds_in_pixels_; }
  gfx::Rect previous_bounds() const { return previous_bounds_in_pixels_; }
  void set_bounds(gfx::Rect new_bounds) { bounds_in_pixels_ = new_bounds; }

  bool mapped_in_client() const { return window_mapped_in_client_; }
  bool is_always_on_top() const { return is_always_on_top_; }
  bool use_native_frame() const { return use_native_frame_; }
  bool use_custom_shape() const { return custom_window_shape_; }
  bool was_minimized() const { return was_minimized_; }
  bool has_alpha() const { return visual_has_alpha_; }
  base::Optional<int> workspace() const { return workspace_; }

  XDisplay* display() const { return xdisplay_; }
  ::Window window() const { return xwindow_; }
  ::Window root_window() const { return x_root_window_; }
  ::Region shape() const { return window_shape_.get(); }
  XID update_counter() const { return update_counter_; }
  XID extended_update_counter() const { return extended_update_counter_; }

 private:
  // Called on an XFocusInEvent, XFocusOutEvent, XIFocusInEvent, or an
  // XIFocusOutEvent.
  void OnFocusEvent(bool focus_in, int mode, int detail);

  // Called on an XEnterWindowEvent, XLeaveWindowEvent, XIEnterEvent, or an
  // XILeaveEvent.
  void OnCrossingEvent(bool enter,
                       bool focus_in_window_or_ancestor,
                       int mode,
                       int detail);

  // Called when |xwindow_|'s _NET_WM_STATE property is updated.
  void OnWMStateUpdated();

  // Called when |xwindow_|'s _NET_FRAME_EXTENTS property is updated.
  void OnFrameExtentsUpdated();

  void OnConfigureEvent(XEvent* xev);

  void OnWorkspaceUpdated();

  void OnWindowMapped();

  // Record the activation state.
  void BeforeActivationStateChanged();

  // Handle the state change since BeforeActivationStateChanged().
  void AfterActivationStateChanged();

  void DelayedResize(const gfx::Rect& bounds_in_pixels);

  // Updates |xwindow_|'s _NET_WM_USER_TIME if |xwindow_| is active.
  void UpdateWMUserTime(XEvent* event);

  // If mapped, sends a message to the window manager to enable or disable the
  // states |state1| and |state2|.  Otherwise, the states will be enabled or
  // disabled on the next map.  It's the caller's responsibility to make sure
  // atoms are set and unset in the appropriate pairs.  For example, if a caller
  // sets (_NET_WM_STATE_MAXIMIZED_VERT, _NET_WM_STATE_MAXIMIZED_HORZ), it would
  // be invalid to unset the maximized state by making two calls like
  // (_NET_WM_STATE_MAXIMIZED_VERT, x11::None), (_NET_WM_STATE_MAXIMIZED_HORZ,
  // x11::None).
  void SetWMSpecState(bool enabled, XAtom state1, XAtom state2);

  // Updates |window_properties_| with |new_window_properties|.
  void UpdateWindowProperties(
      const base::flat_set<XAtom>& new_window_properties);

  void UnconfineCursor();

  void SetVisualId(base::Optional<int> visual_id);

  void UpdateWindowRegion(XRegion* xregion);

  void NotifyBoundsChanged(const gfx::Rect& new_bounds_in_px);

  // Interface that must be used by a class that inherits the XWindow to receive
  // different messages from X Server.
  virtual void OnXWindowCreated() = 0;
  virtual void OnXWindowStateChanged() = 0;
  virtual void OnXWindowDamageEvent(const gfx::Rect& damage_rect) = 0;
  virtual void OnXWindowBoundsChanged(const gfx::Rect& size) = 0;
  virtual void OnXWindowCloseRequested() = 0;
  virtual void OnXWindowIsActiveChanged(bool active) = 0;
  virtual void OnXWindowMapped() = 0;
  virtual void OnXWindowUnmapped() = 0;
  virtual void OnXWindowWorkspaceChanged() = 0;
  virtual void OnXWindowLostPointerGrab() = 0;
  virtual void OnXWindowLostCapture() = 0;
  virtual void OnXWindowEvent(ui::Event* event) = 0;
  virtual void OnXWindowSelectionEvent(XEvent* xev) = 0;
  virtual void OnXWindowDragDropEvent(XEvent* xev) = 0;
  virtual base::Optional<gfx::Size> GetMinimumSizeForXWindow() = 0;
  virtual base::Optional<gfx::Size> GetMaximumSizeForXWindow() = 0;
  virtual void GetWindowMaskForXWindow(const gfx::Size& size,
                                       SkPath* window_mask) = 0;

  // The display and the native X window hosting the root window.
  XDisplay* xdisplay_ = nullptr;
  ::Window xwindow_ = x11::None;
  ::Window x_root_window_ = x11::None;

  // Events selected on |xwindow_|.
  std::unique_ptr<ui::XScopedEventSelector> xwindow_events_;

  // The window manager state bits.
  base::flat_set<XAtom> window_properties_;

  // Is this window able to receive focus?
  bool activatable_ = true;

  // Was this window initialized with the override_redirect window attribute?
  bool override_redirect_ = false;

  base::string16 window_title_;

  // Whether the window is visible with respect to Aura.
  bool window_mapped_in_client_ = false;

  // Whether the window is mapped with respect to the X server.
  bool window_mapped_in_server_ = false;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_in_pixels_;

  VisualID visual_id_ = 0;

  // Whether we used an ARGB visual for our window.
  bool visual_has_alpha_ = false;

  // The workspace containing |xwindow_|.  This will be base::nullopt when
  // _NET_WM_DESKTOP is unset.
  base::Optional<int> workspace_;

  // True if the window should stay on top of most other windows.
  bool is_always_on_top_ = false;

  // Does |xwindow_| have the pointer grab (XI2 or normal)?
  bool has_pointer_grab_ = false;

  // The focus-tracking state variables are as described in
  // gtk/docs/focus_tracking.txt
  //
  // |xwindow_| is active iff:
  //     (|has_window_focus_| || |has_pointer_focus_|) &&
  //     !|ignore_keyboard_input_|

  // Is the pointer in |xwindow_| or one of its children?
  bool has_pointer_ = false;

  // Is |xwindow_| or one of its children focused?
  bool has_window_focus_ = false;

  // (An ancestor window or the PointerRoot is focused) && |has_pointer_|.
  // |has_pointer_focus_| == true is the odd case where we will receive keyboard
  // input when |has_window_focus_| == false.  |has_window_focus_| and
  // |has_pointer_focus_| are mutually exclusive.
  bool has_pointer_focus_ = false;

  // X11 does not support defocusing windows; you can only focus a different
  // window.  If we would like to be defocused, we just ignore keyboard input we
  // no longer care about.
  bool ignore_keyboard_input_ = false;

  // Used for tracking activation state in {Before|After}ActivationStateChanged.
  bool was_active_ = false;
  bool had_pointer_ = false;
  bool had_pointer_grab_ = false;
  bool had_window_focus_ = false;

  bool was_minimized_ = false;

  // Used for synchronizing between |xwindow_| and desktop compositor during
  // resizing.
  XID update_counter_ = x11::None;
  XID extended_update_counter_ = x11::None;

  // Whenever the bounds are set, we keep the previous set of bounds around so
  // we can have a better chance of getting the real
  // |restored_bounds_in_pixels_|. Window managers tend to send a Configure
  // message with the maximized bounds, and then set the window maximized
  // property. (We don't rely on this for when we request that the window be
  // maximized, only when we detect that some other process has requested that
  // we become the maximized window.)
  gfx::Rect previous_bounds_in_pixels_;

  // True if a Maximize() call should be done after mapping the window.
  bool should_maximize_after_map_ = false;

  // Whether we currently are flashing our frame. This feature is implemented
  // by setting the urgency hint with the window manager, which can draw
  // attention to the window or completely ignore the hint. We stop flashing
  // the frame when |xwindow_| gains focus or handles a mouse button event.
  bool urgency_hint_set_ = false;

  // |xwindow_|'s minimum size.
  gfx::Size min_size_in_pixels_;

  // |xwindow_|'s maximum size.
  gfx::Size max_size_in_pixels_;

  // The window shape if the window is non-rectangular.
  gfx::XScopedPtr<XRegion, gfx::XObjectDeleter<XRegion, int, XDestroyRegion>>
      window_shape_;

  // Whether |window_shape_| was set via SetShape().
  bool custom_window_shape_ = false;

  // True if the window has title-bar / borders provided by the window manager.
  bool use_native_frame_ = false;

  // The size of the window manager provided borders (if any).
  gfx::Insets native_window_frame_borders_in_pixels_;

  // Used for synchronizing between |xwindow_| between desktop compositor during
  // resizing.
  int64_t pending_counter_value_ = 0;
  int64_t configure_counter_value_ = 0;
  int64_t current_counter_value_ = 0;
  bool pending_counter_value_is_extended_ = false;
  bool configure_counter_value_is_extended_ = false;

  base::CancelableOnceCallback<void()> delayed_resize_task_;

  // Keep track of barriers to confine cursor.
  bool has_pointer_barriers_ = false;
  std::array<XID, 4> pointer_barriers_;

  base::WeakPtrFactory<XWindow> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(XWindow);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_WINDOW_H_
