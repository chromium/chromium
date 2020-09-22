// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_

#include <deque>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/host/wayland_event_watcher.h"
#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_touch.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

struct wl_display;

namespace gfx {
class Vector2d;
}

namespace ui {

class WaylandWindow;
class WaylandWindowManager;

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
                           public WaylandTouch::Delegate {
 public:
  WaylandEventSource(wl_display* display, WaylandWindowManager* window_manager);
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
  bool StartProcessingEvents();
  // Stops polling for events from input devices.
  bool StopProcessingEvents();

  // Tells if pointer |button| is currently pressed.
  bool IsPointerButtonPressed(EventFlags button) const;

  // Allow to explicitly reset pointer flags. Required in cases where the
  // pointer state is modified by a button pressed event, but the respective
  // button released event is not delivered (e.g: window moving, drag and drop).
  void ResetPointerFlags();

 protected:
  // WaylandKeyboard::Delegate
  void OnKeyboardCreated(WaylandKeyboard* keyboard) override;
  void OnKeyboardDestroyed(WaylandKeyboard* keyboard) override;
  void OnKeyboardFocusChanged(WaylandWindow* window, bool focused) override;
  void OnKeyboardModifiersChanged(int modifiers) override;
  uint32_t OnKeyboardKeyEvent(EventType type,
                              DomCode dom_code,
                              bool repeat,
                              base::TimeTicks timestamp) override;

  // WaylandPointer::Delegate
  void OnPointerCreated(WaylandPointer* pointer) override;
  void OnPointerDestroyed(WaylandPointer* pointer) override;
  void OnPointerFocusChanged(WaylandWindow* window,
                             const gfx::PointF& location) override;
  void OnPointerButtonEvent(EventType evtype,
                            int changed_button,
                            WaylandWindow* window = nullptr) override;
  void OnPointerMotionEvent(const gfx::PointF& location) override;
  void OnPointerAxisEvent(const gfx::Vector2d& offset) override;
  void OnPointerFrameEvent() override;
  void OnPointerAxisSourceEvent(uint32_t axis_source) override;
  void OnPointerAxisStopEvent(uint32_t axis) override;

  // WaylandTouch::Delegate
  void OnTouchCreated(WaylandTouch* touch) override;
  void OnTouchDestroyed(WaylandTouch* touch) override;
  void OnTouchPressEvent(WaylandWindow* window,
                         const gfx::PointF& location,
                         base::TimeTicks timestamp,
                         PointerId id) override;
  void OnTouchReleaseEvent(base::TimeTicks timestamp, PointerId id) override;
  void OnTouchMotionEvent(const gfx::PointF& location,
                          base::TimeTicks timestamp,
                          PointerId id) override;
  void OnTouchCancelEvent() override;

 private:
  struct PointerFrame {
    uint32_t axis_source = WL_POINTER_AXIS_SOURCE_WHEEL;
    float dx = 0.0f;
    float dy = 0.0f;
    base::TimeDelta dt;
    bool is_axis_stop = false;
  };
  struct TouchPoint;

  // PlatformEventSource:
  void OnDispatcherListChanged() override;

  // WaylandWindowObserver
  void OnWindowRemoved(WaylandWindow* window) override;

  void UpdateKeyboardModifiers(int modifier, bool down);
  void HandleKeyboardFocusChange(WaylandWindow* window, bool focused);
  void HandlePointerFocusChange(WaylandWindow* window);
  void HandleTouchFocusChange(WaylandWindow* window,
                              bool focused,
                              base::Optional<PointerId> id = base::nullopt);
  bool ShouldUnsetTouchFocus(WaylandWindow* window, PointerId id);

  // Computes initial velocity of fling scroll based on recent frames.
  gfx::Vector2dF ComputeFlingVelocity();

  WaylandWindowManager* const window_manager_;

  // Input device objects. Owned by WaylandConnection.
  WaylandKeyboard* keyboard_ = nullptr;
  WaylandPointer* pointer_ = nullptr;
  WaylandTouch* touch_ = nullptr;

  // Bitmask of EventFlags used to keep track of the the pointer state.
  int pointer_flags_ = 0;

  // Bitmask of EventFlags used to keep track of the last changed button.
  int last_pointer_button_pressed_ = 0;

  // Bitmask of EventFlags used to keep track of the the keyboard state.
  int keyboard_modifiers_ = 0;

  // Last known pointer location.
  gfx::PointF pointer_location_;

  // Current frame
  PointerFrame current_pointer_frame_;

  // Time of the last pointer frame event.
  base::TimeTicks last_pointer_frame_time_;

  // Recent pointer frames to compute fling scroll.
  // Front is newer, and back is older.
  std::deque<PointerFrame> recent_pointer_frames_;

  // The window the pointer is over.
  WaylandWindow* window_with_pointer_focus_ = nullptr;

  // Map that keeps track of the current touch points, associating touch IDs to
  // to the surface/location where they happened.
  base::flat_map<PointerId, std::unique_ptr<TouchPoint>> touch_points_;

  std::unique_ptr<WaylandEventWatcher> event_watcher_;
};

}  // namespace ui
#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_SOURCE_H_
