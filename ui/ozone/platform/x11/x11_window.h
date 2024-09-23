// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_WINDOW_H_
#define UI_OZONE_PLATFORM_X11_X11_WINDOW_H_

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/x/x11_desktop_window_move_client.h"
#include "ui/base/x/x11_drag_drop_client.h"
#include "ui/base/x/x11_move_loop_delegate.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/sync.h"
#include "ui/gfx/x/xfixes.h"
#include "ui/gfx/x/xproto.h"
#include "ui/platform_window/extensions/workspace_extension.h"
#include "ui/platform_window/extensions/x11_extension.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_drag_handler.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"

class SkPath;

namespace ui {

class PlatformWindowDelegate;
class X11ExtensionDelegate;
class X11MoveLoop;
class WorkspaceExtensionDelegate;

// PlatformWindow implementation for X11.
class X11Window : public PlatformWindow,
                  public WmMoveResizeHandler,
                  public PlatformEventDispatcher,
                  public x11::EventObserver,
                  public WorkspaceExtension,
                  public X11Extension,
                  public WmDragHandler,
                  public XDragDropClient::Delegate,
                  public X11MoveLoopDelegate,
                  public WmMoveLoopHandler,
                  public X11DesktopWindowMoveClient::Delegate {
 public:
  explicit X11Window(PlatformWindowDelegate* platform_window_delegate);

  X11Window(const X11Window&) = delete;
  X11Window& operator=(const X11Window&) = delete;

  ~X11Window() override;

  virtual void Initialize(PlatformWindowInitProperties properties);

  // X11WindowManager calls this.
  void OnXWindowLostCapture();

  void OnMouseEnter();

  gfx::AcceleratedWidget GetWidget() const;
  gfx::Rect GetOuterBounds() const;
  void SetTransientWindow(x11::Window window);

  bool has_pointer() const { return has_pointer_; }

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
  void SetRestoredBoundsInDIP(const gfx::Rect& bounds) final;
  gfx::Rect GetRestoredBoundsInDIP() const final;
  bool ShouldWindowContentsBeTransparent() const override;
  void SetZOrderLevel(ZOrderLevel order) override;
  ZOrderLevel GetZOrderLevel() const override;
  void StackAbove(gfx::AcceleratedWidget widget) override;
  void StackAtTop() override;
  void FlashFrame(bool flash_frame) override;
  void SetShape(std::unique_ptr<ShapeRects> native_shape,
                const gfx::Transform& transform) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;
  void SetOpacity(float opacity) override;
  bool CanSetDecorationInsets() const override;
  void SetOpaqueRegion(
      std::optional<std::vector<gfx::Rect>> region_px) override;
  void SetInputRegion(std::optional<std::vector<gfx::Rect>> region_px) override;
  void NotifyStartupComplete(const std::string& startup_id) override;

  // WorkspaceExtension:
  std::string GetWorkspace() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  void SetWorkspaceExtensionDelegate(
      WorkspaceExtensionDelegate* delegate) override;

  // X11Extension:
  bool IsSyncExtensionAvailable() const override;
  bool IsWmTiling() const override;
  void OnCompleteSwapAfterResize() override;
  gfx::Rect GetXRootWindowOuterBounds() const override;
  void LowerXWindow() override;
  void SetOverrideRedirect(bool override_redirect) override;
  bool CanResetOverrideRedirect() const override;
  void SetX11ExtensionDelegate(X11ExtensionDelegate* delegate) override;

  // x11::EventObserver:
  void OnEvent(const x11::Event& event) override;

 protected:
  PlatformWindowDelegate* platform_window_delegate() const {
    return platform_window_delegate_;
  }

  void OnXWindowStateChanged();
  void OnXWindowDamageEvent(const gfx::Rect& damage_rect);
  void OnXWindowCloseRequested();
  void OnXWindowIsActiveChanged(bool active);
  void OnXWindowWorkspaceChanged();
  void OnXWindowLostPointerGrab();
  void OnXWindowSelectionEvent(const x11::SelectionNotifyEvent& xev);
  void OnXWindowDragDropEvent(const x11::ClientMessageEvent& xev);
  std::optional<gfx::Size> GetMinimumSizeForXWindow();
  std::optional<gfx::Size> GetMaximumSizeForXWindow();
  SkPath GetWindowMaskForXWindow();

 private:
  FRIEND_TEST_ALL_PREFIXES(X11WindowTest, Shape);
  FRIEND_TEST_ALL_PREFIXES(X11WindowTest, WindowManagerTogglesFullscreen);
  FRIEND_TEST_ALL_PREFIXES(X11WindowTest,
                           ToggleMinimizePropogateToPlatformWindowDelegate);

  void UpdateDecorationInsets();

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  void DispatchUiEvent(ui::Event* event, const x11::Event& xev);

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) override;

  // WmMoveLoopHandler:
  bool RunMoveLoop(const gfx::Vector2d& drag_offset) override;
  void EndMoveLoop() override;

  // WmDragHandler:
  bool StartDrag(const OSExchangeData& data,
                 int operations,
                 mojom::DragEventSource source,
                 gfx::NativeCursor cursor,
                 bool can_grab_pointer,
                 base::OnceClosure drag_started_callback,
                 WmDragHandler::DragFinishedCallback drag_finished_callback,
                 WmDragHandler::LocationDelegate* delegate) override;
  void CancelDrag() override;
  void UpdateDragImage(const gfx::ImageSkia& image,
                       const gfx::Vector2d& offset) override;

  // XDragDropClient::Delegate
  std::optional<gfx::AcceleratedWidget> GetDragWidget() override;
  int UpdateDrag(const gfx::Point& screen_point) override;
  void UpdateCursor(mojom::DragOperation negotiated_operation) override;
  void OnBeginForeignDrag(x11::Window window) override;
  void OnEndForeignDrag() override;
  void OnBeforeDragLeave() override;
  mojom::DragOperation PerformDrop() override;
  void EndDragLoop() override;

  // X11MoveLoopDelegate
  void OnMouseMovement(const gfx::Point& screen_point,
                       int flags,
                       base::TimeTicks event_time) override;
  void OnMouseReleased() override;
  void OnMoveLoopEnded() override;

  // X11DesktopWindowMoveClient::Delegate:
  void SetBoundsOnMove(const gfx::Rect& requested_bounds) override;
  scoped_refptr<X11Cursor> GetLastCursor() override;
  gfx::Size GetSize() override;

  void QuitDragLoop();

  // Handles `key_event` as an Atk Key Event
  bool HandleAsAtkEvent(const x11::KeyEvent& key_event,
                        bool send_event,
                        bool transient);

  // Adjusts |requested_size_in_pixels| to avoid the WM "feature" where setting
  // the window size to the monitor size causes the WM to set the EWMH for
  // fullscreen.
  gfx::Size AdjustSizeForDisplay(const gfx::Size& requested_size_in_pixels);

  // Creates the X window with the given properties.
  // Depending on presence of the compositing manager and window type, may
  // change the opacity, in which case returns the final opacity type through
  // |opacity|.
  void CreateXWindow(const PlatformWindowInitProperties& properties);
  void CloseXWindow();
  void Map(bool inactive = false);
  void SetFullscreen(bool fullscreen);
  bool IsActive() const;
  bool IsTargetedBy(const x11::Event& xev) const;
  void HandleEvent(const x11::Event& xev);

  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsFullscreen() const;

  void SetFlashFrameHint(bool flash_frame);
  void UpdateMinAndMaxSize();
  void DispatchResize(bool origin_changed);
  void CancelResize();

  // Resets the window region for the current window bounds if necessary.
  void ResetWindowRegion();

  x11::Window window() const { return xwindow_; }
  x11::Window root_window() const { return x_root_window_; }
  std::vector<x11::Rectangle>* shape() const { return window_shape_.get(); }

  // Updates |xwindow_|'s _NET_WM_USER_TIME if |xwindow_| is active.
  void UpdateWMUserTime(ui::Event* event);

  // Called on an XFocusInEvent, XFocusOutEvent, XIFocusInEvent, or an
  // XIFocusOutEvent.
  void OnFocusEvent(bool focus_in,
                    x11::NotifyMode mode,
                    x11::NotifyDetail detail);

  // Called on an XEnterWindowEvent, XLeaveWindowEvent, XIEnterEvent, or an
  // XILeaveEvent.
  void OnCrossingEvent(bool enter,
                       bool focus_in_window_or_ancestor,
                       x11::NotifyMode mode,
                       x11::NotifyDetail detail);

  // Called when |xwindow_|'s _NET_WM_STATE property is updated.
  void OnWMStateUpdated();

  WindowTiledEdges GetTiledState() const;

  // Called when |xwindow_|'s _NET_FRAME_EXTENTS property is updated.
  void OnFrameExtentsUpdated();

  void OnConfigureEvent(const x11::ConfigureNotifyEvent& event,
                        bool send_event);

  void OnWorkspaceUpdated();

  void OnWindowMapped();

  // Record the activation state.
  void BeforeActivationStateChanged();

  // Handle the state change since BeforeActivationStateChanged().
  void AfterActivationStateChanged();

  void MaybeUpdateOcclusionState();

  void DelayedResize(bool origin_changed);

  // If mapped, sends a message to the window manager to enable or disable the
  // states |state1| and |state2|.  Otherwise, the states will be enabled or
  // disabled on the next map.  It's the caller's responsibility to make sure
  // atoms are set and unset in the appropriate pairs.  For example, if a caller
  // sets (_NET_WM_STATE_MAXIMIZED_VERT, _NET_WM_STATE_MAXIMIZED_HORZ), it would
  // be invalid to unset the maximized state by making two calls like
  // (_NET_WM_STATE_MAXIMIZED_VERT, x11::None), (_NET_WM_STATE_MAXIMIZED_HORZ,
  // x11::None).
  void SetWMSpecState(bool enabled, x11::Atom state1, x11::Atom state2);

  // Updates |window_properties_| with |new_window_properties|.
  void UpdateWindowProperties(
      const base::flat_set<x11::Atom>& new_window_properties);

  void UnconfineCursor();

  void UpdateWindowRegion(std::unique_ptr<std::vector<x11::Rectangle>> region);

  void NotifyBoundsChanged(bool origin_changed);

  // Initializes as a status icon window.
  bool InitializeAsStatusIcon();

  // Stores current state of this window.
  PlatformWindowState state_ = PlatformWindowState::kUnknown;

  WindowTiledEdges tiled_state_;

  const raw_ptr<PlatformWindowDelegate> platform_window_delegate_;

  raw_ptr<WorkspaceExtensionDelegate, DanglingUntriaged>
      workspace_extension_delegate_ = nullptr;

  raw_ptr<X11ExtensionDelegate, DanglingUntriaged> x11_extension_delegate_ =
      nullptr;

  // Tells if the window got a ::Close call.
  bool is_shutting_down_ = false;

  // The z-order level of the window; the window exhibits "always on top"
  // behavior if > 0.
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;

  // The bounds of our window before the window was maximized.
  gfx::Rect restored_bounds_in_pixels_;

  std::unique_ptr<X11DesktopWindowMoveClient> x11_window_move_client_;

  // Whether the drop handler has notified that the drag has entered.
  bool notified_enter_ = false;
  // Keeps the last negotiated operations returned by the drop handler.
  int allowed_drag_operations_ = 0;

  // Handles XDND events going through this window.
  std::unique_ptr<XDragDropClient> drag_drop_client_;
  WmDragHandler::DragFinishedCallback drag_finished_callback_;
  raw_ptr<WmDragHandler::LocationDelegate, DanglingUntriaged>
      drag_location_delegate_ = nullptr;

  // Run loop used while dragging from this window.
  std::unique_ptr<X11MoveLoop> drag_loop_;

  // Events that we have selected on the source window of the incoming drag.
  x11::ScopedEventSelector source_window_events_;

  // The display and the native X window hosting the root window.
  const raw_ref<x11::Connection> connection_;
  x11::Window xwindow_ = x11::Window::None;
  x11::Window x_root_window_ = x11::Window::None;

  // Any native, modal dialog hanging from this window.
  x11::Window transient_window_ = x11::Window::None;

  // Events selected on |xwindow_|.
  x11::ScopedEventSelector xwindow_events_;

  // The window manager state bits.
  base::flat_set<x11::Atom> window_properties_;

  // Is this window able to receive focus?
  bool activatable_ = true;

  // Was this window initialized with the override_redirect window attribute?
  bool override_redirect_ = false;

  std::optional<std::u16string> window_title_;

  // Whether the window is visible with respect to Aura.
  bool window_mapped_in_client_ = false;

  // Whether the window is mapped with respect to the X server.
  bool window_mapped_in_server_ = false;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_in_pixels_;

  x11::VisualId visual_id_{};

  // Whether we used an ARGB visual for our window.
  bool visual_has_alpha_ = false;

  // The workspace containing |xwindow_|.  This will be std::nullopt when
  // _NET_WM_DESKTOP is unset.
  std::optional<int> workspace_;

  // True if the window should stay on top of most other windows.
  bool is_always_on_top_ = false;

  // True if the window is security-sensitive. Implies |is_always_on_top_|.
  bool is_security_surface_ = false;

  // True if the window is fully obscured by another window.
  bool is_occluded_ = false;

  PlatformWindowOcclusionState occlusion_state_ =
      PlatformWindowOcclusionState::kUnknown;

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

  // Used for synchronizing between |xwindow_| and desktop compositor during
  // resizing.
  x11::Sync::Counter update_counter_{};
  x11::Sync::Counter extended_update_counter_{};

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

  // True if GrabPointer() should be called after mapping the window.
  bool should_grab_pointer_after_map_ = false;

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
  std::unique_ptr<std::vector<x11::Rectangle>> window_shape_;

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

  // Used for ignoring bounds changes during the fullscreening process.  For
  // cross-display fullscreening, there is a Restore() (called by BrowserView)
  // that may cause configuration bounds updates that make this window appear to
  // temporarily be on a different screen than its destination screen.  This
  // restore only happens if the window is maximized. The integer represents how
  // many events to ignore.
  int ignore_next_configures_ = 0;
  // True between Restore() and the next OnXWindowStateChanged().
  bool restore_in_flight_ = false;
  // True between SetBoundsInPixels (when the bounds actually change) and the
  // next OnConfigureEvent.
  bool bounds_change_in_flight_ = false;

  base::CancelableOnceClosure delayed_resize_task_;

  // Keep track of barriers to confine cursor.
  bool has_pointer_barriers_ = false;
  std::array<x11::XFixes::Barrier, 4> pointer_barriers_;

  scoped_refptr<X11Cursor> last_cursor_;

  base::CancelableOnceCallback<void(x11::Cursor)> on_cursor_loaded_;

  base::WeakPtrFactory<X11Window> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_WINDOW_H_
