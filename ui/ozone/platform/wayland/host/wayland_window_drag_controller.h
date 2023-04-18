// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_DRAG_CONTROLLER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_DRAG_CONTROLLER_H_

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/events/event.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_touch.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

namespace ui {

class ScopedEventDispatcher;
class WaylandConnection;
class WaylandDataDeviceManager;
class WaylandDataOffer;
class WaylandWindow;
class WaylandWindowManager;
class WaylandSurface;

// Drag controller implementation that drives window moving sessions (aka: tab
// dragging). Wayland Drag and Drop protocol is used, under the hood, to keep
// track of cursor location and surface focus.
//
// TODO(crbug.com/896640): Use drag icon to emulate window moving.
class WaylandWindowDragController : public WaylandDataDevice::DragDelegate,
                                    public WaylandDataSource::Delegate,
                                    public PlatformEventDispatcher,
                                    public WaylandWindowObserver {
 public:
  // Constants used to keep track of the drag controller state.
  enum class State {
    kIdle,       // No DnD session nor drag loop running.
    kAttached,   // DnD session ongoing but no drag loop running.
    kDetached,   // Drag loop running. ie: blocked in a Drag() call.
    kDropped,    // Drop event was just received.
    kCancelled,  // Drag cancel event was just received.
    kAttaching,  // About to transition back to |kAttached|.
  };

  WaylandWindowDragController(WaylandConnection* connection,
                              WaylandDataDeviceManager* device_manager,
                              WaylandPointer::Delegate* pointer_delegate,
                              WaylandTouch::Delegate* touch_delegate);
  WaylandWindowDragController(const WaylandWindowDragController&) = delete;
  WaylandWindowDragController& operator=(const WaylandWindowDragController&) =
      delete;
  ~WaylandWindowDragController() override;

  // Starts a new Wayland DND session for window dragging, if not done yet.
  // Whereas `origin` is used as the origin drag surface and `event_source` as
  // the event type that is triggering the drag session, ie: mouse or touch. See
  // https://wayland.app/protocols/wayland#wl_data_device:request:start_drag for
  // more protocol-related information.
  bool StartDragSession(WaylandToplevelWindow* origin,
                        mojom::DragEventSource event_source);

  bool Drag(WaylandToplevelWindow* window, const gfx::Vector2d& offset);
  void StopDragging();

  State state() const { return state_; }

  void OnToplevelWindowCreated(WaylandToplevelWindow* window);

  // Tells if "extended drag" extension is available.
  bool IsExtendedDragAvailable() const;

  // Makes IsExtendedDragAvailable() always return true.
  void set_extended_drag_available_for_testing(bool available) {
    extended_drag_available_for_testing_ = available;
  }

  WaylandWindow* origin_window_for_testing() { return origin_window_; }

  absl::optional<mojom::DragEventSource> drag_source() { return drag_source_; }

 private:
  class ExtendedDragSource;

  FRIEND_TEST_ALL_PREFIXES(WaylandWindowDragControllerTest,
                           HandleDraggedWindowDestructionAfterMoveLoop);
  FRIEND_TEST_ALL_PREFIXES(WaylandWindowDragControllerTest, GetSerial);

  // WaylandDataDevice::DragDelegate:
  bool IsDragSource() const override;
  void DrawIcon() override;
  void OnDragOffer(std::unique_ptr<WaylandDataOffer> offer) override;
  void OnDragEnter(WaylandWindow* window,
                   const gfx::PointF& location,
                   uint32_t serial) override;
  void OnDragMotion(const gfx::PointF& location) override;
  void OnDragLeave() override;
  void OnDragDrop() override;
  const WaylandWindow* GetDragTarget() const override;

  // WaylandDataSource::Delegate
  void OnDataSourceFinish(bool completed) override;
  void OnDataSourceSend(const std::string& mime_type,
                        std::string* contents) override;

  // PlatformEventDispatcher
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  // WaylandWindowObserver:
  void OnWindowRemoved(WaylandWindow* window) override;

  // Handles drag/move mouse |event|, while in |kDetached| mode, forwarding it
  // as a bounds change event to the upper layer handlers.
  void HandleMotionEvent(LocatedEvent* event);
  // Handles the mouse button release (i.e: drop). Dispatches the required
  // events and resets the internal state.
  void HandleDropAndResetState();
  // Registers as the top level PlatformEvent dispatcher and runs a nested
  // RunLoop, which blocks until the DnD session finishes.
  void RunLoop();
  // Unregisters the internal event dispatcher and asks to quit the nested loop.
  void QuitLoop();
  // Set |window| as the current dragged window and |offset| as the drag offset,
  // which makes |window| to appear anchored to the pointer cursor, if
  // extended-drag extension is available.
  void SetDraggedWindow(WaylandToplevelWindow* window,
                        const gfx::Vector2d& offset);
  // Tells if "extended drag" extension is available, ignoring
  // |extended_drag_available_for_testing_|.
  bool IsExtendedDragAvailableInternal() const;

  // Returns the serial for the given |drag_source| if |origin| has the
  // corresponding focus, otherwise return null.
  absl::optional<wl::Serial> GetSerial(mojom::DragEventSource drag_source,
                                       WaylandToplevelWindow* origin);

  const raw_ptr<WaylandConnection> connection_;
  const raw_ptr<WaylandDataDeviceManager> data_device_manager_;
  const raw_ptr<WaylandDataDevice> data_device_;
  const raw_ptr<WaylandWindowManager> window_manager_;
  const raw_ptr<WaylandPointer::Delegate> pointer_delegate_;
  const raw_ptr<WaylandTouch::Delegate> touch_delegate_;

  State state_ = State::kIdle;
  absl::optional<mojom::DragEventSource> drag_source_;

  gfx::Vector2d drag_offset_;

  // The last known pointer location in DIP.
  gfx::PointF pointer_location_;

  std::unique_ptr<WaylandDataSource> data_source_;
  std::unique_ptr<WaylandDataOffer> data_offer_;

  std::unique_ptr<ExtendedDragSource> extended_drag_source_;

  // The current toplevel window being dragged, when in detached mode.
  raw_ptr<WaylandToplevelWindow> dragged_window_ = nullptr;

  // Keeps track of the window that holds the pointer grab. i.e: the owner of
  // the surface that must receive the mouse release event upon drop.
  raw_ptr<WaylandWindow> pointer_grab_owner_ = nullptr;

  // The window where the DND session originated from. i.e: which had the
  // pointer focus when the session was initiated.
  raw_ptr<WaylandWindow> origin_window_ = nullptr;

  raw_ptr<WaylandWindow> drag_target_window_ = nullptr;

  // The |origin_window_| can be destroyed during the DND session. If this
  // happens, |origin_surface_| takes ownership of its surface and ensure it
  // is kept alive until the end of the session.
  std::unique_ptr<WaylandSurface> origin_surface_;

  std::unique_ptr<ScopedEventDispatcher> nested_dispatcher_;
  base::OnceClosure quit_loop_closure_;

  // Tells if the current drag motion events should be processed.
  //
  // The processing of drag motion events should be suspended when:
  //
  // 1/ Buggy compositors send wl_pointer::motion events, for example, while
  // a DND session is still in progress, which leads to issues in window
  // dragging sessions.
  //
  // 2/ `Screen coordinates` feature is enabled, given that in this mode
  // the surface location during a window drag operation is updated
  // via `zaura_shell::origin_change` request.
  //
  // This flag is used to make window drag controller resistant to such
  // scenarios.
  bool should_process_drag_motion_events_ = false;

  bool extended_drag_available_for_testing_ = false;

  base::WeakPtrFactory<WaylandWindowDragController> weak_factory_{this};
};

// Stream operator so WaylandWindowDragController::State can be used in
// log/assertion statements.
std::ostream& operator<<(std::ostream& out,
                         WaylandWindowDragController::State state);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_DRAG_CONTROLLER_H_
