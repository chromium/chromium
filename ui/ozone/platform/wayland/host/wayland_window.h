// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_

#include <list>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/containers/linked_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event_target.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_surface.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_drag_handler.h"

struct zwp_keyboard_shortcuts_inhibitor_v1;

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
class WaylandToplevelWindow;

using WidgetSubsurfaceSet = base::flat_set<std::unique_ptr<WaylandSubsurface>>;

class WaylandWindow : public PlatformWindow,
                      public PlatformEventDispatcher,
                      public WmDragHandler,
                      public EventTarget,
                      public WaylandZAuraSurface::Delegate {
 public:
  WaylandWindow(const WaylandWindow&) = delete;
  WaylandWindow& operator=(const WaylandWindow&) = delete;

  ~WaylandWindow() override;

  // A factory method that can create any of the derived types of WaylandWindow
  // (WaylandToplevelWindow, WaylandPopup and WaylandAuxiliaryWindow).
  static std::unique_ptr<WaylandWindow> Create(
      PlatformWindowDelegate* delegate,
      WaylandConnection* connection,
      PlatformWindowInitProperties properties);

  void OnWindowLostCapture();

  // Updates the surface scale of the window.  Top level windows take
  // scale from the output attached to either their current display or the
  // primary one if their widget is not yet created, children inherit scale from
  // their parent.  The method recalculates window bounds appropriately if asked
  // to do so (this is not needed upon window initialization).
  virtual void UpdateWindowScale(bool update_bounds);

  // Propagates the buffer scale of the next commit to exo.
  virtual void PropagateBufferScale(float new_scale) = 0;

  WaylandSurface* root_surface() const { return root_surface_.get(); }
  WaylandSubsurface* primary_subsurface() const {
    return primary_subsurface_.get();
  }
  const WidgetSubsurfaceSet& wayland_subsurfaces() const {
    return wayland_subsurfaces_;
  }
  WaylandZAuraSurface* GetZAuraSurface();

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
                      int64_t seq,
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
  // is located at. This determines how events can be translated and how pixel
  // size of the surface is treated. This is called as a result of the window
  // moving to a new display (output), or if the scale factor of its current
  // display changes. This is not sent via a configure.
  void SetWindowScale(float new_scale);

  float ui_scale() const { return ui_scale_; }

  // Returns the preferred entered output id, if any. The preferred output is
  // the one with the largest scale. This is needed to properly render contents
  // as it seems like an expectation of Wayland. However, if all the entered
  // outputs have the same scale factor, the very first entered output is chosen
  // as there is no way to figure out what output the window occupies the most.
  absl::optional<WaylandOutput::Id> GetPreferredEnteredOutputId();

  // Returns current type of the window.
  PlatformWindowType type() const { return type_; }

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
  void UpdateDragImage(const gfx::ImageSkia& image,
                       const gfx::Vector2d& offset) override;

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

  // Handles the configuration events coming from the shell objects.
  // The width and height come in DIP of the output that the surface is
  // currently bound to.
  virtual void HandleSurfaceConfigure(uint32_t serial);

  struct WindowStates {
    bool is_maximized = false;
    bool is_fullscreen = false;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    bool is_immersive_fullscreen = false;
#endif
    bool is_activated = false;
    bool is_minimized = false;
    bool is_snapped_primary = false;
    bool is_snapped_secondary = false;
    bool is_floated = false;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    WindowTiledEdges tiled_edges;
#endif
  };

  // Configure related:
  virtual void HandleToplevelConfigure(int32_t width,
                                       int32_t height,
                                       const WindowStates& window_states);
  virtual void HandleAuraToplevelConfigure(int32_t x,
                                           int32_t y,
                                           int32_t width,
                                           int32_t height,
                                           const WindowStates& window_states);
  virtual void HandlePopupConfigure(const gfx::Rect& bounds);

  // Call when we get a new frame produced from viz with |seq| sequence ID.
  // This is used to determine which requests have been fulfilled,
  // and sends the appropriate acks back to the wayland server.
  virtual void OnSequencePoint(int64_t seq) = 0;

  // Called by shell surfaces to indicate that this window can start submitting
  // frames. Updating state based on configure is handled separately to this.
  void OnSurfaceConfigureEvent();

  // Sets the raster scale to be applied on the next configure.
  void SetPendingRasterScale(float scale) {
    pending_configure_state_.raster_scale = scale;
  }

  // See comments on the member variable for an explanation of this.
  const PlatformWindowDelegate::State& applied_state() const {
    return applied_state_;
  }

  // See comments on the member variable for an explanation of this.
  const PlatformWindowDelegate::State& latched_state() const {
    return latched_state_;
  }

  // Tells if the surface has already been configured. This will be true after
  // the first set of configure event and ack request, meaning that wl_surface
  // can attach buffers.
  virtual bool IsSurfaceConfigured() = 0;

  // Sends configure acknowledgement to the wayland server.
  virtual void AckConfigure(uint32_t serial) = 0;

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

  // Sets the window geometry.
  virtual void SetWindowGeometry(gfx::Size size_dip);

  // Returns the offset of the window geometry within the window surface.
  gfx::Vector2d GetWindowGeometryOffsetInDIP() const;

  // Returns the effective decoration insets.
  gfx::Insets GetDecorationInsetsInDIP() const;

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
  // only applies to toplevel surfaces (surfaces such as popups, subsurfaces
  // do not support that).
  virtual bool IsActive() const;

  // WaylandWindow can be any type of object - WaylandToplevelWindow,
  // WaylandPopup. The following methods cast itself to WaylandPopup or
  // WaylandToplevelWindow, if |this| is of that type.
  virtual WaylandPopup* AsWaylandPopup();
  virtual WaylandToplevelWindow* AsWaylandToplevelWindow();

  // Returns true if the window's bounds is in screen coordinates.
  virtual bool IsScreenCoordinatesEnabled() const;

  // Returns true if this window's configure state supports the minimized state.
  virtual bool SupportsConfigureMinimizedState() const;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner() {
    return ui_task_runner_;
  }

  base::WeakPtr<WaylandWindow> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Clears the state of the |frame_manager_| when the GPU channel is
  // destroyed.
  void OnChannelDestroyed();

#if DCHECK_IS_ON()
  void disable_null_target_dcheck_for_testing() {
    disable_null_target_dcheck_for_test_ = true;
  }
#endif

 protected:
  WaylandWindow(PlatformWindowDelegate* delegate,
                WaylandConnection* connection);

  WaylandConnection* connection() { return connection_; }
  const WaylandConnection* connection() const { return connection_; }
  PlatformWindowDelegate* delegate() { return delegate_; }
  zaura_surface* aura_surface() {
    return aura_surface_ ? aura_surface_.get() : nullptr;
  }

  // Update the bounds of the window in DIP. Unlike SetBoundInDIP, it will not
  // send a request to the compositor even if the screen coordinate is enabled.
  void UpdateBoundsInDIP(const gfx::Rect& bounds_dip);

  void set_ui_scale(float ui_scale) { ui_scale_ = ui_scale; }

  // Updates mask for this window.
  virtual void UpdateWindowMask() = 0;

  // [Deprecated]
  // If the given |bounds_px| violates size constraints set for this window,
  // fixes them so they don't.
  gfx::Rect AdjustBoundsToConstraintsPx(const gfx::Rect& bounds_px);

  // If the given |bounds_dip| violates size constraints set for this window,
  // fixes them so they don't.
  gfx::Rect AdjustBoundsToConstraintsDIP(const gfx::Rect& bounds_dip);

  const gfx::Rect& restored_bounds_dip() const { return restored_bounds_dip_; }

  // Configure related:

  // Processes the currently pending State. This may generate a new in-flight
  // StateRequest, or apply and ack the request immediately. This should be
  // called after the server has finished sending a configure request. The
  // serial number comes from the server and needs to be acked when the changes
  // from the configure have been applied.
  void ProcessPendingConfigureState(uint32_t serial);

  // Requests the given state via RequestState, given that this was a server
  // initiated change (e.g. configure).
  void RequestStateFromServer(PlatformWindowDelegate::State state,
                              int64_t serial);

  // Requests the given state via RequestState, given that this was a client
  // initiated change.
  void RequestStateFromClient(PlatformWindowDelegate::State state);

  // Requests the given state. If this request originates from a configure from
  // the server, specify |serial|. If |force| is true, the state will always be
  // applied, even if requests are being throttled.
  void RequestState(PlatformWindowDelegate::State state,
                    int64_t serial,
                    bool force);

  // Processes the given sequence point number. It will also latch and ack
  // the latest fulfilled in-flight request if it exists.
  void ProcessSequencePoint(int64_t viz_seq);

  // Applies the latest in-flight StateRequest, if it exists. In-flight
  // StateRequests need to wait for a frame generated after we inserted a
  // sequence point for their changes. If |force| is true, the state will always
  // be applied, even if requests are being throttled. This is used for client
  // requested changes (server requested changes may be throttled).
  void MaybeApplyLatestStateRequest(bool force);

  // PendingConfigureState describes the content of a configure sent from the
  // wayland server.
  struct PendingConfigureState {
    absl::optional<gfx::Rect> bounds_dip;
    absl::optional<gfx::Size> size_px;
    absl::optional<float> raster_scale;
  };

  // This holds the requested state for the next configure from the server.
  // The window may get several configuration events that update the pending
  // bounds or other state. When the configure is fully received, we may
  // create a StateRequest for this pending State.
  PendingConfigureState pending_configure_state_;

 private:
  friend class WaylandBufferManagerViewportTest;
  friend class BlockableWaylandToplevelWindow;
  friend class WaylandWindowManager;

  FRIEND_TEST_ALL_PREFIXES(WaylandScreenTest, SetWindowScale);
  FRIEND_TEST_ALL_PREFIXES(WaylandBufferManagerTest, CanSubmitOverlayPriority);
  FRIEND_TEST_ALL_PREFIXES(WaylandBufferManagerTest, CanSetRoundedCorners);
  FRIEND_TEST_ALL_PREFIXES(WaylandBufferManagerTest,
                           CommitOverlaysNonsensicalBoundsRect);
  FRIEND_TEST_ALL_PREFIXES(WaylandWindowTest,
                           ServerInitiatedRestoreFromMinimizedState);

  // Initializes the WaylandWindow with supplied properties.
  bool Initialize(PlatformWindowInitProperties properties);

  uint32_t DispatchEventToDelegate(const PlatformEvent& native_event);

  // Additional initialization of derived classes.
  virtual bool OnInitialize(PlatformWindowInitProperties properties,
                            PlatformWindowDelegate::State* state) = 0;

  // WaylandWindowDragController might need to take ownership of the wayland
  // surface whether the window that originated the DND session gets destroyed
  // in the middle of that session (e.g: when it is snapped into a tab strip).
  // Surface ownership is allowed to be taken only when the window is under
  // destruction, i.e: |shutting_down_| is set. This can be done, for example,
  // by implementing |WaylandWindowObserver::OnWindowRemoved|.
  friend WaylandWindowDragController;
  std::unique_ptr<WaylandSurface> TakeWaylandSurface();

  void UpdateCursorShape(scoped_refptr<BitmapCursor> cursor);

  // StateRequest describes a State that we are applying to the window, and the
  // metadata about that State, such as what serial number to use for ack (if it
  // came from a configure), or the viz sequence number.
  struct StateRequest {
    // State that has been requested.
    PlatformWindowDelegate::State state;

    // Wayland serial number for acking a configure. This is -1 if there is no
    // serial number (e.g. from client initiated change).
    int64_t serial = -1;

    // Viz sequence number at the time of this request. We are looking for a
    // frame with a number greater than this to latch this request.
    int64_t viz_seq = -1;

    // Whether this request has been applied.
    bool applied = false;
  };

  // Latches the given request. This must be called after the frame
  // corresponding to the request is received. This acks the request and updates
  // any window state that should be based on the currently latched state.
  void LatchStateRequest(const StateRequest& req);

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

  wl::Object<zaura_surface> aura_surface_;

  // The current cursor bitmap (immutable).
  scoped_refptr<BitmapCursor> cursor_;

  // Margins between edges of the surface and the window geometry (i.e., the
  // area of the window that is visible to the user as the actual window).  The
  // areas outside the geometry are used to draw client-side window decorations.
  // TODO(crbug.com/1306688): Use DIP for frame insets.
  absl::optional<gfx::Insets> frame_insets_px_;

  bool has_touch_focus_ = false;
  // The UI scale may be forced through the command line, which means that it
  // replaces the default value that is equal to the natural device scale.
  // We need it to place and size the menus properly.
  float ui_scale_ = 1.0f;

  // Stores current opacity of the window. Set on ::Initialize call.
  ui::PlatformWindowOpacity opacity_;

  // The type of the current WaylandWindow object.
  ui::PlatformWindowType type_ = ui::PlatformWindowType::kWindow;

  // Set when the window enters in shutdown process.
  bool shutting_down_ = false;

  // The bounds of the platform window before it went maximized or fullscreen in
  // dip.
  gfx::Rect restored_bounds_dip_;

  // This holds the currently applied state. When in doubt, use this as the
  // source of truth for this window's state. Whenever applied_state_ is
  // changed, that change should be applied and a new in-flight request and
  // sequence point should be created. Note that changes can be applied via
  // other means than configures from the Wayland server. For example,
  // PlatformWindow::SetBoundsInDIP can change the bounds without the server
  // doing anything. This is separated from pending_configure_state_ to support
  // these two different sources (server and PlatformWindow/etc) of control of
  // the state.
  //
  // Here is an explanation of the State system:
  //
  // After applying some state changes (e.g. setting Chrome's bounds), we ask
  // PlatformWindowDelegate for a sequence ID, which will be used to identify
  // the correct buffer that has content corresponding to these changes. It is
  // not sufficient to use the buffer size to identify this frame, because not
  // all state changes change the buffer size. Usually these state changes are
  // caused by configures from the wayland server, but not always. The client
  // (us) can also set state (e.g. client side bounds change), and this needs to
  // be managed along with changes via configure.
  //
  // Once the sequence ID reaches ozone/wayland GPU from viz, it will pass it
  // over mojo back to WaylandBufferManagerHost where the whole round trip
  // started. WaylandWindow will match it up with pending configures, which are
  // now identified by the sequence ID at the original time of that configure.
  //
  // Once we have the sequence ID from viz back, we need to make sure the right
  // configure is acked. Let's explicitly classify all configure related state
  // into stages:
  //
  // Pending (pending_configure_state_): Accumulates configure data passed by
  // the server.
  //
  // Requested (in_flight_requests_): On configure, we request the configure
  // state to be applied. Not all configure state will be applied, due to
  // throttling. Also, any client side changes (e.g.
  // PlatformWindow::SetBoundsInDIP) should go through requested state to make
  // sure it takes the same code path.
  //
  // Applied (applied_state_): A configure state which we have asked the browser
  // to apply, e.g. by calling delegate()->OnBoundsChanged.
  //
  // Latched (latched_state_): When we receive the frame back from ozone/wayland
  // GPU, we use the viz sequence ID to match it up with a configure. That state
  // is now "latched".
  //
  // State changes go through this flow:
  // 1. Pending - if via configure
  // 2. Requested - in a queue to be applied (unless throttled)
  // 3. Applied - we asked the browser to apply these state changes, waiting for
  //    the frame to come back
  // 4. Latched - the frame corresponding to this state came back, we can ack
  //    the configure if there was one
  PlatformWindowDelegate::State applied_state_;

  // The current configuration state of the window. This is initially set to
  // values provided by the client, until we get an actual configure from the
  // server. See the comments on applied_state_ for further explanation.
  PlatformWindowDelegate::State latched_state_;

  // In-flight state requests. Once a frame comes from the GPU
  // process with the appropriate viz sequence number, ack_configure request
  // with |serial| will be sent to the Wayland compositor if needed.
  base::circular_deque<StateRequest> in_flight_requests_;

  // AcceleratedWidget for this window. This will be unique even over time.
  gfx::AcceleratedWidget accelerated_widget_;

  WmDragHandler::DragFinishedCallback drag_finished_callback_;

  base::OnceClosure drag_loop_quit_closure_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  wl::Object<zwp_keyboard_shortcuts_inhibitor_v1>
      permanent_keyboard_shortcuts_inhibitor_;
#endif

#if DCHECK_IS_ON()
  bool disable_null_target_dcheck_for_test_ = false;
#endif

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  base::WeakPtrFactory<WaylandWindow> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_H_
