// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_

#include <deque>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_touch.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_pointer_gestures.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_relative_pointer_manager.h"

struct wl_display;

namespace gfx {
class Vector2dF;
}

namespace ui {

class WaylandConnection;
class WaylandWindow;
class WaylandWindowManager;
class WaylandEventWatcher;

// Wayland implementation of ui::PlatformEventSource. It polls for events
// through WaylandEventWatcher and centralizes the input and focus handling
// logic within Ozone Wayland backend. In order to do so, it also implements the
// input objects' delegate interfaces, which are the entry point of event data
// coming from input devices, e.g: wl_{keyboard,pointer,touch}, which are then
// pre-processed, translated into ui::Event instances and dispatched to the
// PlatformEvent system.
class WaylandEventSource : public PlatformEventSource,
                           public WaylandWindowObserver,
                           public WaylandKeyboard::Delegate,
                           public WaylandPointer::Delegate,
                           public WaylandTouch::Delegate,
                           public WaylandZwpPointerGestures::Delegate,
                           public WaylandZwpRelativePointerManager::Delegate {
 public:
  WaylandEventSource(wl_display* display,
                     wl_event_queue* event_queue,
                     WaylandWindowManager* window_manager,
                     WaylandConnection* connection);
  WaylandEventSource(const WaylandEventSource&) = delete;
  WaylandEventSource& operator=(const WaylandEventSource&) = delete;
  ~WaylandEventSource() override;

  int last_pointer_button_pressed() const {
    return last_pointer_button_pressed_;
  }

  int keyboard_modifiers() const { return keyboard_modifiers_; }

  // Sets a callback that that shutdowns the browser in case of unrecoverable
  // error. Called by WaylandEventWatcher.
  void SetShutdownCb(base::OnceCallback<void()> shutdown_cb);

  // Starts polling for events from the wayland connection file descriptor.
  // This method assumes connection is already estabilished and input objects
  // are already bound and properly initialized.
  void StartProcessingEvents();

  // Allow to explicitly reset pointer flags. Required in cases where the
  // pointer state is modified by a button pressed event, but the respective
  // button released event is not delivered (e.g: window moving, drag and drop).
  void ResetPointerFlags();

  // Forwards the call to WaylandEventWatcher, which calls
  // wl_display_roundtrip_queue.
  void RoundTripQueue();

 protected:
  // WaylandKeyboard::Delegate
  void OnKeyboardFocusChanged(WaylandWindow* window, bool focused) override;
  void OnKeyboardModifiersChanged(int modifiers) override;
  uint32_t OnKeyboardKeyEvent(EventType type,
                              DomCode dom_code,
                              bool repeat,
                              absl::optional<uint32_t> serial,
                              base::TimeTicks timestamp,
                              int device_id,
                              WaylandKeyboard::KeyEventKind kind) override;

  // WaylandPointer::Delegate
  void OnPointerFocusChanged(WaylandWindow* window,
                             const gfx::PointF& location) override;
  void OnPointerButtonEvent(EventType evtype,
                            int changed_button,
                            WaylandWindow* window = nullptr) override;
  void OnPointerMotionEvent(const gfx::PointF& location) override;
  void OnPointerAxisEvent(const gfx::Vector2dF& offset) override;
  void OnPointerFrameEvent() override;
  void OnPointerAxisSourceEvent(uint32_t axis_source) override;
  void OnPointerAxisStopEvent(uint32_t axis) override;
  void OnResetPointerFlags() override;
  const gfx::PointF& GetPointerLocation() const override;
  bool IsPointerButtonPressed(EventFlags button) const override;
  void OnPointerStylusToolChanged(EventPointerType pointer_type) override;
  const WaylandWindow* GetPointerTarget() const override;

  // WaylandTouch::Delegate
  using DispatchPolicy = WaylandTouch::Delegate::EventDispatchPolicy;
  void OnTouchPressEvent(WaylandWindow* window,
                         const gfx::PointF& location,
                         base::TimeTicks timestamp,
                         PointerId id,
                         EventDispatchPolicy dispatch_policy) override;
  void OnTouchReleaseEvent(base::TimeTicks timestamp,
                           PointerId id,
                           EventDispatchPolicy dispatch_policy) override;
  void OnTouchMotionEvent(const gfx::PointF& location,
                          base::TimeTicks timestamp,
                          PointerId id,
                          EventDispatchPolicy dispatch_policy) override;
  void OnTouchCancelEvent() override;
  void OnTouchFrame() override;
  void OnTouchFocusChanged(WaylandWindow* window) override;
  std::vector<PointerId> GetActiveTouchPointIds() override;
  const WaylandWindow* GetTouchTarget(PointerId id) const override;
  void OnTouchStylusToolChanged(PointerId pointer_id,
                                EventPointerType pointer_type) override;

  // WaylandZwpPointerGesture::Delegate:
  void OnPinchEvent(EventType event_type,
                    const gfx::Vector2dF& delta,
                    base::TimeTicks timestamp,
                    int device_id,
                    absl::optional<float> scale_delta) override;

  // WaylandZwpRelativePointerManager::Delegate:
  void SetRelativePointerMotionEnabled(bool enabled) override;
  void OnRelativePointerMotion(const gfx::Vector2dF& delta) override;

 private:
  struct PointerFrame {
    PointerFrame();
    PointerFrame(const PointerFrame& other);
    PointerFrame(PointerFrame&&);
    ~PointerFrame();

    PointerFrame& operator=(const PointerFrame&);
    PointerFrame& operator=(PointerFrame&&);

    absl::optional<uint32_t> axis_source;
    float dx = 0.0f;
    float dy = 0.0f;
    base::TimeDelta dt;
    bool is_axis_stop = false;
  };

  struct TouchFrame {
    TouchFrame(const TouchEvent& event,
               base::OnceCallback<void()> completion_cb);
    TouchFrame(const TouchFrame& other) = delete;
    TouchFrame(TouchFrame&&) = delete;
    ~TouchFrame();

    TouchEvent event;
    base::OnceCallback<void()> completion_cb;
  };

  // PlatformEventSource:
  void OnDispatcherListChanged() override;

  // WaylandWindowObserver:
  void OnWindowRemoved(WaylandWindow* window) override;

  void UpdateKeyboardModifiers(int modifier, bool down);
  void HandleTouchFocusChange(WaylandWindow* window,
                              bool focused,
                              absl::optional<PointerId> id = absl::nullopt);
  bool ShouldUnsetTouchFocus(WaylandWindow* window, PointerId id);

  // Computes initial velocity of fling scroll based on recent frames.
  gfx::Vector2dF ComputeFlingVelocity();

  bool SurfaceSubmissionInPixelCoordinates() const;

  // For pointer events.
  PointerDetails PointerDetailsForDispatching() const;

  // For touch events.
  PointerDetails PointerDetailsForDispatching(PointerId pointer_id) const;

  // Wrap up method to support async touch release processing.
  void OnTouchReleaseInternal(PointerId id);

  WaylandWindowManager* const window_manager_;

  WaylandConnection* const connection_;

  // Bitmask of EventFlags used to keep track of the the pointer state.
  int pointer_flags_ = 0;

  // Bitmask of EventFlags used to keep track of the last changed button.
  int last_pointer_button_pressed_ = 0;

  // Bitmask of EventFlags used to keep track of the the keyboard state.
  int keyboard_modifiers_ = 0;

  // Last known pointer location.
  gfx::PointF pointer_location_;

  // Last known relative pointer location (used for pointer lock).
  absl::optional<gfx::PointF> relative_pointer_location_;

  // Current frame
  PointerFrame current_pointer_frame_;

  // Time of the last pointer frame event.
  base::TimeTicks last_pointer_frame_time_;

  // Last known pointer stylus type (eg mouse, pen, eraser or touch).
  absl::optional<EventPointerType> last_pointer_stylus_tool_;

  // Last known touch stylus type (eg touch, pen or eraser).
  // absl::optional<PointerId, EventPointerType> last_touch_stylus_tool_;
  base::flat_map<PointerId, absl::optional<EventPointerType>>
      last_touch_stylus_tool_;

  // Recent pointer frames to compute fling scroll.
  // Front is newer, and back is older.
  std::deque<PointerFrame> recent_pointer_frames_;

  // Order set of touch events to be dispatching on the next
  // wl_touch::frame event.
  std::deque<std::unique_ptr<TouchFrame>> touch_frames_;

  // Map that keeps track of the current touch points, associating touch IDs to
  // to the surface/location where they happened.
  struct TouchPoint;
  base::flat_map<PointerId, std::unique_ptr<TouchPoint>> touch_points_;

  std::unique_ptr<WaylandEventWatcher> event_watcher_;
};

}  // namespace ui
#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_
