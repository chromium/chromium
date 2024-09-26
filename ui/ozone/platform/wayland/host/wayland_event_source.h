// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_

#include <deque>
#include <memory>
#include <optional>
#include <ostream>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
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
  static void ConvertEventToTarget(EventTarget* new_target,
                                   LocatedEvent* event);

  WaylandEventSource(wl_display* display,
                     wl_event_queue* event_queue,
                     WaylandWindowManager* window_manager,
                     WaylandConnection* connection,
                     bool use_threaded_polling = false);
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
  // This method assumes connection is already established and input objects
  // are already bound and properly initialized.
  void StartProcessingEvents();

  // Forwards the call to WaylandEventWatcher, which calls
  // wl_display_roundtrip_queue.
  void RoundTripQueue();

  void DumpState(std::ostream& out) const;

  void ResetStateForTesting() override;

  // WaylandKeyboard::Delegate
  void OnKeyboardFocusChanged(WaylandWindow* window, bool focused) override;
  void OnKeyboardModifiersChanged(int modifiers) override;
  uint32_t OnKeyboardKeyEvent(EventType type,
                              DomCode dom_code,
                              bool repeat,
                              std::optional<uint32_t> serial,
                              base::TimeTicks timestamp,
                              int device_id,
                              WaylandKeyboard::KeyEventKind kind) override;
  void OnSynthesizedKeyPressEvent(DomCode dom_code,
                                  base::TimeTicks timestamp) override;

  // WaylandPointer::Delegate
  void OnPointerFocusChanged(WaylandWindow* window,
                             const gfx::PointF& location,
                             base::TimeTicks timestamp,
                             wl::EventDispatchPolicy dispatch_policy) override;
  void OnPointerButtonEvent(EventType evtype,
                            int changed_button,
                            base::TimeTicks timestamp,
                            WaylandWindow* window,
                            wl::EventDispatchPolicy dispatch_policy,
                            bool allow_release_of_unpressed_button,
                            bool is_synthesized) override;
  void OnPointerMotionEvent(const gfx::PointF& location,
                            base::TimeTicks timestamp,
                            wl::EventDispatchPolicy dispatch_policy,
                            bool is_synthesized) override;
  void OnPointerAxisEvent(const gfx::Vector2dF& offset,
                          base::TimeTicks timestamp) override;
  void OnPointerFrameEvent() override;
  void OnPointerAxisSourceEvent(uint32_t axis_source) override;
  void OnPointerAxisStopEvent(uint32_t axis,
                              base::TimeTicks timestamp) override;
  const gfx::PointF& GetPointerLocation() const override;
  bool IsPointerButtonPressed(EventFlags button) const override;
  void ReleasePressedPointerButtons(WaylandWindow* window,
                                    base::TimeTicks timestamp) override;
  void OnPointerStylusToolChanged(EventPointerType pointer_type) override;
  void OnPointerStylusForceChanged(float force) override;
  void OnPointerStylusTiltChanged(const gfx::Vector2dF& tilt) override;
  const WaylandWindow* GetPointerTarget() const override;

  // WaylandTouch::Delegate
  void OnTouchPressEvent(WaylandWindow* window,
                         const gfx::PointF& location,
                         base::TimeTicks timestamp,
                         PointerId id,
                         wl::EventDispatchPolicy dispatch_policy) override;
  void OnTouchReleaseEvent(base::TimeTicks timestamp,
                           PointerId id,
                           wl::EventDispatchPolicy dispatch_policy,
                           bool is_synthesized) override;
  void OnTouchMotionEvent(const gfx::PointF& location,
                          base::TimeTicks timestamp,
                          PointerId id,
                          wl::EventDispatchPolicy dispatch_policy,
                          bool is_synthesized) override;
  void OnTouchCancelEvent() override;
  void OnTouchFrame() override;
  void OnTouchFocusChanged(WaylandWindow* window) override;
  std::vector<PointerId> GetActiveTouchPointIds() override;
  const WaylandWindow* GetTouchTarget(PointerId id) const override;
  void OnTouchStylusToolChanged(PointerId pointer_id,
                                EventPointerType pointer_type) override;
  void OnTouchStylusForceChanged(PointerId pointer_id, float force) override;
  void OnTouchStylusTiltChanged(PointerId pointer_id,
                                const gfx::Vector2dF& tilt) override;

  // WaylandZwpPointerGesture::Delegate:
  void OnPinchEvent(EventType event_type,
                    const gfx::Vector2dF& delta,
                    base::TimeTicks timestamp,
                    int device_id,
                    std::optional<float> scale_delta) override;
  void OnHoldEvent(EventType event_type,
                   uint32_t finger_count,
                   base::TimeTicks timestamp,
                   int device_id,
                   wl::EventDispatchPolicy dispatch_policy) override;

  // WaylandZwpRelativePointerManager::Delegate:
  void SetRelativePointerMotionEnabled(bool enabled) override;
  void OnRelativePointerMotion(const gfx::Vector2dF& delta,
                               base::TimeTicks timestamp) override;

 private:
  struct PointerScrollData {
    PointerScrollData();
    PointerScrollData(const PointerScrollData& other);
    PointerScrollData(PointerScrollData&&);
    ~PointerScrollData();

    PointerScrollData& operator=(const PointerScrollData&);
    PointerScrollData& operator=(PointerScrollData&&);

    std::optional<uint32_t> axis_source;
    float dx = 0.0f;
    float dy = 0.0f;
    base::TimeDelta dt;
    bool is_axis_stop = false;
    std::optional<base::TimeTicks> timestamp;

    void DumpState(std::ostream& out) const;
  };

  struct FrameData {
    FrameData(const Event& event, base::OnceCallback<void()> completion_cb);
    FrameData(const FrameData& other) = delete;
    FrameData(FrameData&&) = delete;
    ~FrameData();

    std::unique_ptr<Event> event;
    base::OnceCallback<void()> completion_cb;

    void DumpState(std::ostream& out) const;
  };

  // PlatformEventSource:
  void OnDispatcherListChanged() override;

  // WaylandWindowObserver:
  void OnWindowRemoved(WaylandWindow* window) override;

  void HandleTouchFocusChange(WaylandWindow* window,
                              bool focused,
                              std::optional<PointerId> id = std::nullopt);
  bool ShouldUnsetTouchFocus(WaylandWindow* window, PointerId id);

  // Computes initial velocity of fling scroll based on recent frames.
  // The fling velocity is computed the same way as in libgestures.
  gfx::Vector2dF ComputeFlingVelocity();

  // For pointer events.
  std::optional<PointerDetails> AmendStylusData() const;

  // For touch events.
  std::optional<PointerDetails> AmendStylusData(PointerId pointer_id) const;

  // Wrap up method to support async pointer down/up event processing.
  void OnPointerButtonEventInternal(WaylandWindow* window, EventType type);

  // Wrap up method to support async touch release processing.
  void OnTouchReleaseInternal(PointerId id);

  // Ensure a valid instance of the PointerScrollData class member.
  void EnsurePointerScrollData(const std::optional<base::TimeTicks>& timestamp);

  void ProcessPointerScrollData();

  // Set the target to the event, then dispatch the event.
  void SetTargetAndDispatchEvent(Event* event, EventTarget* target);

  // Find and set the target for the touch event, then dispatch the event.
  void SetTouchTargetAndDispatchTouchEvent(TouchEvent* event);

  const raw_ptr<WaylandWindowManager> window_manager_;

  const raw_ptr<WaylandConnection> connection_;

  // Bitmask of EventFlags used to keep track of the the pointer state.
  int pointer_flags_ = 0;

  // Bitmask of EventFlags used to keep track of the last changed button.
  int last_pointer_button_pressed_ = 0;

  // Bitmask of EventFlags used to keep track of the the keyboard state.
  // See ui/events/event_constants.h for examples and details.
  int keyboard_modifiers_ = 0;

  // Last known pointer location.
  gfx::PointF pointer_location_;

  // Last known relative pointer location (used for pointer lock).
  std::optional<gfx::PointF> relative_pointer_location_;

  // Accumulates the scroll data within a pointer frame internal.
  std::optional<PointerScrollData> pointer_scroll_data_;

  // Latest set of pointer scroll data to compute fling scroll.
  // Front is newer, and back is older.
  std::deque<PointerScrollData> pointer_scroll_data_set_;

  // Time of the last pointer frame event.
  base::TimeTicks last_pointer_frame_time_;

  struct StylusData {
    EventPointerType type = EventPointerType::kUnknown;
    gfx::Vector2dF tilt;
    float force = std::numeric_limits<float>::quiet_NaN();
  };

  // Last known pointer stylus data (eg {mouse, pen, eraser or touch}, tilt and
  // force).
  std::optional<StylusData> last_pointer_stylus_data_;

  // Last known touch stylus data (eg {touch, pen or eraser}, tilt and force).
  base::flat_map<PointerId, std::optional<StylusData>> last_touch_stylus_data_;

  // Order set of touch events to be dispatching on the next
  // wl_touch::frame event.
  std::deque<std::unique_ptr<FrameData>> touch_frames_;

  // Order set of pointer events to be dispatching on the next
  // wl_pointer::frame event.
  std::deque<std::unique_ptr<FrameData>> pointer_frames_;

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Status of fling.
  bool is_fling_active_ = false;
#endif

  // Map that keeps track of the current touch points, associating touch IDs to
  // to the surface/location where they happened.
  struct TouchPoint;
  base::flat_map<PointerId, std::unique_ptr<TouchPoint>> touch_points_;

  std::unique_ptr<WaylandEventWatcher> event_watcher_;
};

}  // namespace ui
#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_
