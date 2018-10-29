// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_

#include <stddef.h>
#include <stdint.h>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor_loader_x11.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/x11.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

namespace gfx {
class ImageSkia;
class ImageSkiaRep;
}

namespace ui {
enum class DomCode;
class EventHandler;
class KeyboardHook;
class XScopedEventSelector;
}

namespace views {
class DesktopDragDropClientAuraX11;
class DesktopWindowTreeHostObserverX11;
class NonClientFrameView;
class X11DesktopWindowMoveClient;

class VIEWS_EXPORT DesktopWindowTreeHostX11
    : public DesktopWindowTreeHost,
      public aura::WindowTreeHost,
      public ui::PlatformEventDispatcher {
 public:
  DesktopWindowTreeHostX11(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~DesktopWindowTreeHostX11() override;

  // A way of converting an X11 |xid| host window into the content_window()
  // of the associated DesktopNativeWidgetAura.
  static aura::Window* GetContentWindowForXID(XID xid);

  // A way of converting an X11 |xid| host window into this object.
  static DesktopWindowTreeHostX11* GetHostForXID(XID xid);

  // Get all open top-level windows. This includes windows that may not be
  // visible. This list is sorted in their stacking order, i.e. the first window
  // is the topmost window.
  static std::vector<aura::Window*> GetAllOpenWindows();

  // Returns the current bounds in terms of the X11 Root Window.
  gfx::Rect GetX11RootWindowBounds() const;

  // Returns the current bounds in terms of the X11 Root Window including the
  // borders provided by the window manager (if any).
  gfx::Rect GetX11RootWindowOuterBounds() const;

  // Returns the window shape if the window is not rectangular. Returns NULL
  // otherwise.
  ::Region GetWindowShape() const;

  void AddObserver(DesktopWindowTreeHostObserverX11* observer);
  void RemoveObserver(DesktopWindowTreeHostObserverX11* observer);

  // Swaps the current handler for events in the non client view with |handler|.
  void SwapNonClientEventHandler(std::unique_ptr<ui::EventHandler> handler);

  // Runs the |func| callback for each content-window, and deallocates the
  // internal list of open windows.
  static void CleanUpWindowList(void (*func)(aura::Window* window));

  // Disables event listening to make |dialog| modal.
  std::unique_ptr<base::OnceClosure> DisableEventListening();

  // Returns a map of KeyboardEvent code to KeyboardEvent key values.
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;

 protected:
  // Overridden from DesktopWindowTreeHost:
  void Init(const Widget::InitParams& params) override;
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  void OnWidgetInitDone() override;
  void OnActiveWindowChanged(bool active) override;
  std::unique_ptr<corewm::Tooltip> CreateTooltip() override;
  std::unique_ptr<aura::client::DragDropClient> CreateDragDropClient(
      DesktopNativeCursorManager* cursor_manager) override;
  void Close() override;
  void CloseNow() override;
  aura::WindowTreeHost* AsWindowTreeHost() override;
  void Show(ui::WindowShowState show_state,
            const gfx::Rect& restore_bounds) override;
  bool IsVisible() const override;
  void SetSize(const gfx::Size& requested_size) override;
  void StackAbove(aura::Window* window) override;
  void StackAtTop() override;
  void CenterWindow(const gfx::Size& size) override;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const override;
  gfx::Rect GetWindowBoundsInScreen() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  gfx::Rect GetRestoredBounds() const override;
  std::string GetWorkspace() const override;
  gfx::Rect GetWorkAreaBoundsInScreen() const override;
  void SetShape(std::unique_ptr<Widget::ShapeRects> native_shape) override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool HasCapture() const override;
  void SetAlwaysOnTop(bool always_on_top) override;
  bool IsAlwaysOnTop() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  bool SetWindowTitle(const base::string16& title) override;
  void ClearNativeFocus() override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;
  void EndMoveLoop() override;
  void SetVisibilityChangedAnimationsEnabled(bool value) override;
  NonClientFrameView* CreateNonClientFrameView() override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void FrameTypeChanged() override;
  void SetFullscreen(bool fullscreen) override;
  bool IsFullscreen() const override;
  void SetOpacity(float opacity) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void InitModalType(ui::ModalType modal_type) override;
  void FlashFrame(bool flash_frame) override;
  bool IsAnimatingClosed() const override;
  bool IsTranslucentWindowOpacitySupported() const override;
  void SizeConstraintsChanged() override;
  bool ShouldUpdateWindowTransparency() const override;
  bool ShouldUseDesktopNativeCursorManager() const override;
  bool ShouldCreateVisibilityController() const override;

  // Overridden from aura::WindowTreeHost:
  gfx::Transform GetRootTransform() const override;
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInPixels(
      const gfx::Rect& requested_bounds_in_pixels,
      const viz::LocalSurfaceId& local_surface_id = viz::LocalSurfaceId(),
      base::TimeTicks local_surface_id_allocation_time =
          base::TimeTicks()) override;
  gfx::Point GetLocationOnScreenInPixels() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool CaptureSystemKeyEventsImpl(
      base::Optional<base::flat_set<ui::DomCode>> dom_codes) override;
  void ReleaseSystemKeyEventCapture() override;
  bool IsKeyLocked(ui::DomCode dom_code) override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void MoveCursorToScreenLocationInPixels(
      const gfx::Point& location_in_pixels) override;
  void OnCursorVisibilityChangedNative(bool show) override;

  // Overridden from display::DisplayObserver via aura::WindowTreeHost:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  friend class DesktopWindowTreeHostX11HighDPITest;

  // Initializes our X11 surface to draw on. This method performs all
  // initialization related to talking to the X11 server.
  void InitX11Window(const Widget::InitParams& params);

  // Creates an aura::WindowEventDispatcher to contain the content_window()
  // along with all aura client objects that direct behavior.
  aura::WindowEventDispatcher* InitDispatcher(const Widget::InitParams& params);

  // Adjusts |requested_size| to avoid the WM "feature" where setting the
  // window size to the monitor size causes the WM to set the EWMH for
  // fullscreen.
  gfx::Size AdjustSize(const gfx::Size& requested_size);

  // If mapped, sends a message to the window manager to enable or disable the
  // states |state1| and |state2|.  Otherwise, the states will be enabled or
  // disabled on the next map.  It's the caller's responsibility to make sure
  // atoms are set and unset in the appropriate pairs.  For example, if a caller
  // sets (_NET_WM_STATE_MAXIMIZED_VERT, _NET_WM_STATE_MAXIMIZED_HORZ), it would
  // be invalid to unset the maximized state by making two calls like
  // (_NET_WM_STATE_MAXIMIZED_VERT, x11::None), (_NET_WM_STATE_MAXIMIZED_HORZ,
  // x11::None).
  void SetWMSpecState(bool enabled, XAtom state1, XAtom state2);

  // Called when |xwindow_|'s _NET_WM_STATE property is updated.
  void OnWMStateUpdated();

  // Updates |window_properties_| with |new_window_properties|.
  void UpdateWindowProperties(
      const base::flat_set<XAtom>& new_window_properties);

  // Called when |xwindow_|'s _NET_FRAME_EXTENTS property is updated.
  void OnFrameExtentsUpdated();

  // Record the activation state.
  void BeforeActivationStateChanged();

  // Handle the state change since BeforeActivationStateChanged().
  void AfterActivationStateChanged();

  // Called on an XEnterWindowEvent, XLeaveWindowEvent, XIEnterEvent, or an
  // XILeaveEvent.
  void OnCrossingEvent(bool enter,
                       bool focus_in_window_or_ancestor,
                       int mode,
                       int detail);

  // Called on an XFocusInEvent, XFocusOutEvent, XIFocusInEvent, or an
  // XIFocusOutEvent.
  void OnFocusEvent(bool focus_in, int mode, int detail);

  // Makes a round trip to the X server to get the enclosing workspace for this
  // window.
  void UpdateWorkspace();

  // Updates |xwindow_|'s minimum and maximum size.
  void UpdateMinAndMaxSize();

  // Updates |xwindow_|'s _NET_WM_USER_TIME if |xwindow_| is active.
  void UpdateWMUserTime(const ui::PlatformEvent& event);

  // Sets whether the window's borders are provided by the window manager.
  void SetUseNativeFrame(bool use_native_frame);

  // Dispatches a mouse event, taking mouse capture into account. If a
  // different host has capture, we translate the event to its coordinate space
  // and dispatch it to that host instead.
  void DispatchMouseEvent(ui::MouseEvent* event);

  // Dispatches a touch event, taking capture into account. If a different host
  // has capture, then touch-press events are translated to its coordinate space
  // and dispatched to that host instead.
  void DispatchTouchEvent(ui::TouchEvent* event);

  // Dispatches a key event.
  void DispatchKeyEvent(ui::KeyEvent* event);

  // Resets the window region for the current widget bounds if necessary.
  void ResetWindowRegion();

  // Serializes an image to the format used by _NET_WM_ICON.
  void SerializeImageRepresentation(const gfx::ImageSkiaRep& rep,
                                    std::vector<unsigned long>* data);

  // See comment for variable open_windows_.
  static std::list<XID>& open_windows();

  // Map the window (shows it) taking into account the given |show_state|.
  void MapWindow(ui::WindowShowState show_state);

  void SetWindowTransparency();

  // Relayout the widget's client and non-client views.
  void Relayout();

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  void DelayedResize(const gfx::Size& size_in_pixels);
  void DelayedChangeFrameType(Widget::FrameType new_type);

  gfx::Rect ToDIPRect(const gfx::Rect& rect_in_pixels) const;
  gfx::Rect ToPixelRect(const gfx::Rect& rect_in_dip) const;

  // Enables event listening after closing |dialog|.
  void EnableEventListening();

  // Removes |delayed_resize_task_| from the task queue (if it's in
  // the queue) and adds it back at the end of the queue.
  void RestartDelayedResizeTask();

  // Set visibility and fire OnNativeWidgetVisibilityChanged() if it changed.
  void SetVisible(bool visible);

  // Accessor for DesktopNativeWidgetAura::content_window().
  aura::Window* content_window();

  // X11 things
  // The display and the native X window hosting the root window.
  XDisplay* xdisplay_;
  ::Window xwindow_;

  // Events selected on |xwindow_|.
  std::unique_ptr<ui::XScopedEventSelector> xwindow_events_;

  // The native root window.
  ::Window x_root_window_;

  // Whether the window is mapped with respect to the X server.
  bool window_mapped_in_server_;

  // Whether the window is visible with respect to Aura.
  bool window_mapped_in_client_;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_in_pixels_;

  // Whenever the bounds are set, we keep the previous set of bounds around so
  // we can have a better chance of getting the real
  // |restored_bounds_in_pixels_|. Window managers tend to send a Configure
  // message with the maximized bounds, and then set the window maximized
  // property. (We don't rely on this for when we request that the window be
  // maximized, only when we detect that some other process has requested that
  // we become the maximized window.)
  gfx::Rect previous_bounds_in_pixels_;

  // The bounds of our window before we were maximized.
  gfx::Rect restored_bounds_in_pixels_;

  // |xwindow_|'s minimum size.
  gfx::Size min_size_in_pixels_;

  // |xwindow_|'s maximum size.
  gfx::Size max_size_in_pixels_;

  // The workspace containing |xwindow_|.  This will be base::nullopt when
  // _NET_WM_DESKTOP is unset.
  base::Optional<int> workspace_;

  // The window manager state bits.
  base::flat_set<XAtom> window_properties_;

  // Whether |xwindow_| was requested to be fullscreen via SetFullscreen().
  bool is_fullscreen_;

  // True if the window should stay on top of most other windows.
  bool is_always_on_top_;

  // True if the window has title-bar / borders provided by the window manager.
  bool use_native_frame_;

  // True if a Maximize() call should be done after mapping the window.
  bool should_maximize_after_map_;

  // Whether we used an ARGB visual for our window.
  bool use_argb_visual_;

  DesktopDragDropClientAuraX11* drag_drop_client_;

  std::unique_ptr<ui::EventHandler> x11_non_client_event_filter_;
  std::unique_ptr<X11DesktopWindowMoveClient> x11_window_move_client_;

  // TODO(beng): Consider providing an interface to DesktopNativeWidgetAura
  //             instead of providing this route back to Widget.
  internal::NativeWidgetDelegate* native_widget_delegate_;

  DesktopNativeWidgetAura* desktop_native_widget_aura_;

  // We can optionally have a parent which can order us to close, or own
  // children who we're responsible for closing when we CloseNow().
  DesktopWindowTreeHostX11* window_parent_;
  std::set<DesktopWindowTreeHostX11*> window_children_;

  base::ObserverList<DesktopWindowTreeHostObserverX11>::Unchecked
      observer_list_;

  // The window shape if the window is non-rectangular.
  gfx::XScopedPtr<_XRegion, gfx::XObjectDeleter<_XRegion, int, XDestroyRegion>>
      window_shape_;

  // Whether |window_shape_| was set via SetShape().
  bool custom_window_shape_;

  // The size of the window manager provided borders (if any).
  gfx::Insets native_window_frame_borders_in_pixels_;

  // The current DesktopWindowTreeHostX11 which has capture. Set synchronously
  // when capture is requested via SetCapture().
  static DesktopWindowTreeHostX11* g_current_capture;

  // A list of all (top-level) windows that have been created but not yet
  // destroyed.
  static std::list<XID>* open_windows_;

  base::string16 window_title_;

  // Whether we currently are flashing our frame. This feature is implemented
  // by setting the urgency hint with the window manager, which can draw
  // attention to the window or completely ignore the hint. We stop flashing
  // the frame when |xwindow_| gains focus or handles a mouse button event.
  bool urgency_hint_set_;

  // Does |xwindow_| have the pointer grab (XI2 or normal)?
  bool has_pointer_grab_;

  bool activatable_;

  // The focus-tracking state variables are as described in
  // gtk/docs/focus_tracking.txt
  //
  // |xwindow_| is active iff:
  //     (|has_window_focus_| || |has_pointer_focus_|) &&
  //     !|ignore_keyboard_input_|

  // Is the pointer in |xwindow_| or one of its children?
  bool has_pointer_;

  // Is |xwindow_| or one of its children focused?
  bool has_window_focus_;

  // (An ancestor window or the PointerRoot is focused) && |has_pointer_|.
  // |has_pointer_focus_| == true is the odd case where we will receive keyboard
  // input when |has_window_focus_| == false.  |has_window_focus_| and
  // |has_pointer_focus_| are mutually exclusive.
  bool has_pointer_focus_;

  // X11 does not support defocusing windows; you can only focus a different
  // window.  If we would like to be defocused, we just ignore keyboard input we
  // no longer care about.
  bool ignore_keyboard_input_;

  // Used for tracking activation state in {Before|After}ActivationStateChanged.
  bool was_active_;
  bool had_pointer_;
  bool had_pointer_grab_;
  bool had_window_focus_;

  // Captures system key events when keyboard lock is requested.
  std::unique_ptr<ui::KeyboardHook> keyboard_hook_;

  base::CancelableCallback<void()> delayed_resize_task_;

  std::unique_ptr<aura::ScopedWindowTargeter> targeter_for_modal_;

  uint32_t modal_dialog_counter_;

  base::WeakPtrFactory<DesktopWindowTreeHostX11> close_widget_factory_;
  base::WeakPtrFactory<DesktopWindowTreeHostX11> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_
