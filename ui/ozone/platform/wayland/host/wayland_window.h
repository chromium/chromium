// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_

#include <list>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/containers/linked_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/events/event_target.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_drag_handler.h"

namespace wl {

struct WaylandOverlayConfig;

}  // namespace wl

namespace ui {

class BitmapCursor;
class OSExchangeData;
class WaylandConnection;
class WaylandSubsurface;
class WaylandWindowDragController;
class WaylandFrameManager;
class WaylandPopup;

using WidgetSubsurfaceSet = base::flat_set<std::unique_ptr<WaylandSubsurface>>;

class WaylandWindow : public PlatformWindow,
                      public PlatformEventDispatcher,
                      public WmDragHandler,
                      public EventTarget {
 public:
  WaylandWindow(const WaylandWindow&) = delete;
  WaylandWindow& operator=(const WaylandWindow&) = delete;

  ~WaylandWindow() override;

  // A factory method that can create any of the derived types of WaylandWindow
  // (WaylandToplevelWindow, WaylandPopup and WaylandAuxiliaryWindow).
  static std::unique_ptr<WaylandWindow> Create(
      PlatformWindowDelegate* delegate,
      WaylandConnection* connection,
      PlatformWindowInitProperties properties,
      bool update_visual_size_immediately = false,
      bool apply_pending_state_on_update_visual_size = false);

  void OnWindowLostCapture();

  // Updates the surface scale of the window.  Top level windows take
  // scale from the output attached to either their current display or the
  // primary one if their widget is not yet created, children inherit scale from
  // their parent.  The method recalculates window bounds appropriately if asked
  // to do so (this is not needed upon window initialization).
  virtual void UpdateWindowScale(bool update_bounds);

  WaylandSurface* root_surface() const { return root_surface_.get(); }
  WaylandSubsurface* primary_subsurface() const {
    return primary_subsurface_.get();
  }
  const WidgetSubsurfaceSet& wayland_subsurfaces() const {
    return wayland_subsurfaces_;
  }

  base::LinkedList<WaylandSubsurface>* subsurface_stack_committed() {
    return &subsurface_stack_committed_;
  }

  void set_parent_window(WaylandWindow* parent_window) {
    parent_window_ = parent_window;
  }
  WaylandWindow* parent_window() const { return parent_window_; }

  gfx::AcceleratedWidget GetWidget() const;

  // Creates a WaylandSubsurface to put into |wayland_subsurfaces_|. Called if
  // more subsurfaces are needed when a frame arrives.
  bool RequestSubsurface();
  // Re-arrange the |subsurface_stack_above_| and |subsurface_stack_below_| s.t.
  // subsurface_stack_above_.size() >= above and
  // subsurface_stack_below_.size() >= below.
  bool ArrangeSubsurfaceStack(size_t above, size_t below);
  bool CommitOverlays(uint32_t frame_id,
                      std::vector<wl::WaylandOverlayConfig>& overlays);

  // Called when the focus changed on this window.
  void OnPointerFocusChanged(bool focused);

  // Returns the focus status of this window.
  bool HasPointerFocus() const;
  bool HasKeyboardFocus() const;

  // The methods set or return whether this window has touch focus and should
  // dispatch touch events.
  void set_touch_focus(bool focus) { has_touch_focus_ = focus; }
  bool has_touch_focus() const { return has_touch_focus_; }

  // Set a child of this window. It is very important in case of nested
  // shell_popups as long as they must be destroyed in the back order.
  void set_child_window(WaylandWindow* window) { child_window_ = window; }
  WaylandWindow* child_window() const { return child_window_; }

  // Sets the window_scale for this window with respect to a display this window
  // is located at. This determines how events can be translated and how size of
  // the surface is treated (px to DIP conversion and vice versa.)
  void SetWindowScale(float new_scale);
  float window_scale() const { return window_scale_; }
  float ui_scale() const { return ui_scale_; }

  // A preferred output is the one with the largest scale. This is needed to
  // properly render contents as it seems like an expectation of Wayland.
  // However, if all the outputs the window is located at have the same scale
  // factor, the very first entered output is chosen as there is no way to
  // figure out what output the window occupies the most.
  uint32_t GetPreferredEnteredOutputId();

  // Returns current type of the window.
  PlatformWindowType type() const { return type_; }

  gfx::Size visual_size_px() const { return visual_size_px_; }

  absl::optional<gfx::Insets> frame_insets_px() const {
    return frame_insets_px_;
  }
  void set_frame_insets_px(gfx::Insets insets) { frame_insets_px_ = insets; }

  bool received_configure_event() const { return received_configure_event_; }

  // Remove WaylandOutput associated with WaylandSurface of this window.
  void RemoveEnteredOutput(uint32_t output_id);

  // WmDragHandler
  bool StartDrag(const ui::OSExchangeData& data,
                 int operations,
                 mojom::DragEventSource source,
                 gfx::NativeCursor cursor,
                 bool can_grab_pointer,
                 WmDragHandler::DragFinishedCallback drag_finished_callback,
                 WmDragHandler::LocationDelegate* delegate) override;
  void CancelDrag() override;

  // PlatformWindow
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
  void ToggleFullscreen() override;
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
  bool ShouldWindowContentsBeTransparent() const override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  bool IsTranslucentWindowOpacitySupported() const override;
  void SetDecorationInsets(const gfx::Insets* insets_px) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;
  bool ShouldUpdateWindowShape() const override;

  // PlatformEventDispatcher
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // EventTarget:
  bool CanAcceptEvent(const Event& event) override;
  EventTarget* GetParentTarget() override;
  std::unique_ptr<EventTargetIterator> GetChildIterator() const override;
  EventTargeter* GetEventTargeter() override;
  void ConvertEventToTarget(const EventTarget* target,
                            LocatedEvent* event) const override;

  // Handles the configuration events coming from the shell objects.
  // The width and height come in DIP of the output that the surface is
  // currently bound to.
  virtual void HandleSurfaceConfigure(uint32_t serial);
  virtual void HandleToplevelConfigure(int32_t width,
                                       int32_t height,
                                       bool is_maximized,
                                       bool is_fullscreen,
                                       bool is_activated);
  virtual void HandleAuraToplevelConfigure(int32_t x,
                                           int32_t y,
                                           int32_t width,
                                           int32_t height,
                                           bool is_maximized,
                                           bool is_fullscreen,
                                           bool is_activated);
  virtual void HandlePopupConfigure(const gfx::Rect& bounds);
  // The final size of the Wayland surface is determined by the buffer size in
  // px * scale that the Chromium compositor renders at. If the window changes a
  // display (and scale changes from 1 to 2), the buffers are recreated with
  // some delays. Thus, applying a visual size using window_scale (which is the
  // current scale of a wl_output where the window is located at) is wrong, as
  // it may result in a smaller visual size than needed. For example, buffers'
  // size in px is 100x100, the buffer scale and window scale is 1. The window
  // is moved to another display and window scale changes to 2. The window's
  // bounds also change are multiplied by the scale factor. It takes time until
  // buffers are recreated for a larger size in px and submitted. However, there
  // might be an in flight frame that submits buffers with old size. Thus,
  // applying scale factor immediately will result in a visual size in dip to be
  // smaller than needed. This results in a bouncing window size in some
  // scenarios like starting Chrome on a secondary display with larger scale
  // factor than the primary display's one. Thus, this method gets a scale
  // factor that helps to determine size of the surface in dip respecting
  // size that GPU renders at.
  virtual void UpdateVisualSize(const gfx::Size& size_px, float scale_factor);

  // Handles close requests.
  virtual void OnCloseRequest();

  // Notifies about drag/drop session events. |point| is in DIP as wayland
  // sends coordinates in "surface-local" coordinates.
  virtual void OnDragEnter(const gfx::PointF& point,
                           std::unique_ptr<OSExchangeData> data,
                           int operation);
  virtual int OnDragMotion(const gfx::PointF& point, int operation);
  virtual void OnDragDrop();
  virtual void OnDragLeave();
  virtual void OnDragSessionClose(ui::mojom::DragOperation operation);

  virtual absl::optional<std::vector<gfx::Rect>> GetWindowShape() const;

  // Tells if the surface has already been configured.
  virtual bool IsSurfaceConfigured() = 0;

  // Called by shell surfaces to indicate that this window can start submitting
  // frames.
  void OnSurfaceConfigureEvent();

  // Sets the window geometry.
  virtual void SetWindowGeometry(gfx::Rect bounds);

  // Sends configure acknowledgement to the wayland server.
  virtual void AckConfigure(uint32_t serial) = 0;

  // Updates the window decorations, if possible at the moment.
  virtual void UpdateDecorations();

  // Returns a root parent window within the same hierarchy.
  WaylandWindow* GetRootParentWindow();

  // Returns a top most child window within the same hierarchy.
  WaylandWindow* GetTopMostChildWindow();

  // Called by the WaylandSurface attached to this window when that surface
  // becomes partially or fully within the scanout region of an output that it
  // wasn't before.
  void OnEnteredOutput();

  // Called by the WaylandSurface attached to this window when that surface
  // becomes fully outside of one of outputs that it previously resided on.
  void OnLeftOutput();

  // Returns true iff this window is opaque.
  bool IsOpaqueWindow() const;

  // Says if the current window is set as active by the Wayland server. This
  // only applies to toplevel surfaces (surfaces such as popups, subsurfaces do
  // not support that).
  virtual bool IsActive() const;

  // WaylandWindow can be any type of object - WaylandToplevelWindow,
  // WaylandPopup, WaylandAuxiliaryWindow. This method casts itself to
  // WaylandPopup, if |this| has type of WaylandPopup.
  virtual WaylandPopup* AsWaylandPopup();

  // Returns true if the window's bounds is in screen coordinates.
  virtual bool IsScreenCoordinatesEnabled() const;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() {
    return ui_task_runner_;
  }

  base::WeakPtr<WaylandWindow> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Clears the state of the |frame_manager_| when the GPU channel is destroyed.
  void OnChannelDestroyed();

  // These are never intended to be used except in unit tests.
  void set_update_visual_size_immediately_for_testing(bool update) {
    update_visual_size_immediately_for_testing_ = update;
  }

  void set_apply_pending_state_on_update_visual_size_for_testing(bool apply) {
    apply_pending_state_on_update_visual_size_for_testing_ = apply;
  }

#if DCHECK_IS_ON()
  void disable_null_target_dcheck_for_testing() {
    disable_null_target_dcheck_for_test_ = true;
  }
#endif

  bool has_pending_configures() const { return !pending_configures_.empty(); }

 protected:
  WaylandWindow(PlatformWindowDelegate* delegate,
                WaylandConnection* connection);

  WaylandConnection* connection() { return connection_; }
  const WaylandConnection* connection() const { return connection_; }
  PlatformWindowDelegate* delegate() { return delegate_; }

  // Update the bounds of the window in DIP. Unlike SetBoundInDIP, it will not
  // send a request to the compositor even if the screen coordinate is enabled.
  void UpdateBoundsInDIP(const gfx::Rect& bounds_dip);

  void set_ui_scale(float ui_scale) { ui_scale_ = ui_scale; }

  // Updates mask for this window.
  virtual void UpdateWindowMask() = 0;

  // Processes the pending bounds in dip.
  void ProcessPendingBoundsDip(uint32_t serial);

  // [Deprecated]
  // If the given |bounds_px| violates size constraints set for this window,
  // fixes them so they don't.
  gfx::Rect AdjustBoundsToConstraintsPx(const gfx::Rect& bounds_px);

  // If the given |bounds_dip| violates size constraints set for this window,
  // fixes them so they don't.
  gfx::Rect AdjustBoundsToConstraintsDIP(const gfx::Rect& bounds_dip);

  // Processes the size information form visual size update and returns true if
  // any pending configure is fulfilled.
  bool ProcessVisualSizeUpdate(const gfx::Size& size_px, float scale_factor);

  // Applies pending bounds.
  virtual void ApplyPendingBounds();

  gfx::Rect pending_bounds_dip() const { return pending_bounds_dip_; }
  void set_pending_bounds_dip(const gfx::Rect rect) {
    pending_bounds_dip_ = rect;
  }

  const gfx::Size& restored_size_dip() const { return restored_size_dip_; }

 private:
  friend class WaylandBufferManagerViewportTest;
  friend class BlockableWaylandToplevelWindow;
  friend class WaylandWindowManager;

  FRIEND_TEST_ALL_PREFIXES(WaylandScreenTest, SetWindowScale);
  FRIEND_TEST_ALL_PREFIXES(WaylandBufferManagerTest, CanSubmitOverlayPriority);
  FRIEND_TEST_ALL_PREFIXES(WaylandBufferManagerTest, CanSetRoundedCorners);

  // Initializes the WaylandWindow with supplied properties.
  bool Initialize(PlatformWindowInitProperties properties);

  uint32_t DispatchEventToDelegate(const PlatformEvent& native_event);

  // Additional initialization of derived classes.
  virtual bool OnInitialize(PlatformWindowInitProperties properties) = 0;

  // WaylandWindowDragController might need to take ownership of the wayland
  // surface whether the window that originated the DND session gets destroyed
  // in the middle of that session (e.g: when it is snapped into a tab strip).
  // Surface ownership is allowed to be taken only when the window is under
  // destruction, i.e: |shutting_down_| is set. This can be done, for example,
  // by implementing |WaylandWindowObserver::OnWindowRemoved|.
  friend WaylandWindowDragController;
  std::unique_ptr<WaylandSurface> TakeWaylandSurface();

  void UpdateCursorShape(scoped_refptr<BitmapCursor> cursor);

  raw_ptr<PlatformWindowDelegate> delegate_;
  raw_ptr<WaylandConnection> connection_;
  raw_ptr<WaylandWindow> parent_window_ = nullptr;
  raw_ptr<WaylandWindow> child_window_ = nullptr;

  std::unique_ptr<WaylandFrameManager> frame_manager_;
  bool received_configure_event_ = false;

  // |root_surface_| is a surface for the opaque background. Its z-order is
  // INT32_MIN.
  std::unique_ptr<WaylandSurface> root_surface_;
  // |primary_subsurface| is the primary that shows the widget content.
  std::unique_ptr<WaylandSubsurface> primary_subsurface_;
  // Subsurfaces excluding the primary_subsurface
  WidgetSubsurfaceSet wayland_subsurfaces_;
  bool wayland_overlay_delegation_enabled_;

  // The stack of sub-surfaces to take effect when Commit() is called.
  // |subsurface_stack_above_| refers to subsurfaces that are stacked above the
  // primary. These include the subsurfaces to be hidden as well.
  // Subsurface at the front of the list is the closest to the primary.
  std::list<WaylandSubsurface*> subsurface_stack_above_;
  std::list<WaylandSubsurface*> subsurface_stack_below_;

  // The stack of sub-surfaces currently committed. This list is altered when
  // the subsurface arrangement are played back by WaylandFrameManager.
  base::LinkedList<WaylandSubsurface> subsurface_stack_committed_;

  // The current cursor bitmap (immutable).
  scoped_refptr<BitmapCursor> cursor_;

  // Current bounds of the platform window. This is either initialized, or the
  // requested size by the Wayland compositor. When this is set in SetBounds(),
  // delegate_->OnBoundsChanged() is called and updates current_surface_size in
  // Viz. However, it is not guaranteed that the next arriving frame will match
  // |bounds_px_|.
  gfx::Rect bounds_px_;

  // The size presented by the gpu process. This is the visible size of the
  // window, which can be different from |bounds_px_| due to renderers taking
  // time to produce a compositor frame.
  // The rough flow of size changes:
  //   Wayland compositor -> xdg_surface.configure()
  //   -> WaylandWindow::SetBounds() -> IPC -> DisplayPrivate::Resize()
  //   -> OutputSurface::SwapBuffers() -> WaylandWindow::UpdateVisualSize()
  //   -> xdg_surface.ack_configure() -> Wayland compositor.
  gfx::Size visual_size_px_;
  // Margins between edges of the surface and the window geometry (i.e., the
  // area of the window that is visible to the user as the actual window).  The
  // areas outside the geometry are used to draw client-side window decorations.
  absl::optional<gfx::Insets> frame_insets_px_;

  bool has_touch_focus_ = false;
  // The UI scale may be forced through the command line, which means that it
  // replaces the default value that is equal to the natural device scale.
  // We need it to place and size the menus properly.
  float ui_scale_ = 1.0f;
  // Current scale factor of the output where the window is located at.
  float window_scale_ = 1.f;

  // Stores current opacity of the window. Set on ::Initialize call.
  ui::PlatformWindowOpacity opacity_;

  // The type of the current WaylandWindow object.
  ui::PlatformWindowType type_ = ui::PlatformWindowType::kWindow;

  // Set when the window enters in shutdown process.
  bool shutting_down_ = false;

  // In a non-test environment, a frame update makes a SetBounds() change
  // visible in |visual_size_px_|, but in some unit tests there will never be
  // any frame updates. This flag causes UpdateVisualSize() to be invoked during
  // SetBounds() in unit tests.
  bool update_visual_size_immediately_for_testing_ = false;

  // In a non-test environment, root_surface_->ApplyPendingBounds() is called to
  // send Wayland protocol requests, but in some unit tests there will never be
  // any frame updates. This flag causes root_surface_->ApplyPendingBounds() to
  // be invoked during UpdateVisualSize() in unit tests.
  bool apply_pending_state_on_update_visual_size_for_testing_ = false;

  // These bounds attributes below have suffixes that indicate units used.
  // Wayland operates in DIP but the platform operates in physical pixels so
  // our WaylandWindow is the link that has to translate the units. See also
  // comments in the implementation.
  //
  // Bounds that will be applied when the window state is finalized. The window
  // may get several configuration events that update the pending bounds, and
  // only upon finalizing the state is the latest value stored as the current
  // bounds via |ApplyPendingBounds|. Measured in DIP because updated in the
  // handler that receives DIP from Wayland.
  gfx::Rect pending_bounds_dip_;

  // The size of the platform window before it went maximized or fullscreen in
  // dip.
  gfx::Size restored_size_dip_;

  // Pending xdg-shell configures. Once this window is drawn to |bounds_dip|,
  // ack_configure request with |serial| will be sent to the Wayland compositor.
  struct PendingConfigure {
    gfx::Rect bounds_dip;
    uint32_t serial;
    // True if this configure has been passed to the compositor for rendering.
    bool set = false;
  };
  base::circular_deque<PendingConfigure> pending_configures_;

  // AcceleratedWidget for this window. This will be unique even over time.
  gfx::AcceleratedWidget accelerated_widget_;

  WmDragHandler::DragFinishedCallback drag_finished_callback_;

  base::OnceClosure drag_loop_quit_closure_;

#if DCHECK_IS_ON()
  bool disable_null_target_dcheck_for_test_ = false;
#endif

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  base::WeakPtrFactory<WaylandWindow> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_
