// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_H_
#define UI_EVENTS_EVENT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform_event.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/latency/latency_info.h"

namespace gfx {
class Transform;
}

namespace ui {

class CancelModeEvent;
class Event;
class EventRewriter;
class EventTarget;
class KeyEvent;
class LocatedEvent;
class MouseEvent;
class MouseWheelEvent;
class ScrollEvent;
class TouchEvent;

enum class DomCode : uint32_t;

// Note: In order for Clone() to work properly, every concrete class
// transitively inheriting Event must implement Clone() explicitly, even if any
// ancestors have provided an implementation.
class EVENTS_EXPORT Event {
 public:
  using Properties = base::flat_map<std::string, std::vector<uint8_t>>;

  virtual ~Event();

  class DispatcherApi {
   public:
    explicit DispatcherApi(Event* event) : event_(event) {}

    DispatcherApi(const DispatcherApi&) = delete;
    DispatcherApi& operator=(const DispatcherApi&) = delete;

    void set_target(EventTarget* target) { event_->target_ = target; }

    void set_phase(EventPhase phase) { event_->phase_ = phase; }
    void set_result(int result) {
      event_->result_ = static_cast<EventResult>(result);
    }
    void set_time_stamp(base::TimeTicks time) { event_->time_stamp_ = time; }

   private:
    raw_ptr<Event, AcrossTasksDanglingUntriaged> event_;
  };

  void SetNativeEvent(const PlatformEvent& event);
  const PlatformEvent& native_event() const { return native_event_; }
  EventType type() const { return type_; }
  // time_stamp represents time since machine was booted.
  const base::TimeTicks time_stamp() const { return time_stamp_; }
  int flags() const { return flags_; }

  // Returns a name for the event, typically used in logging/debugging. This is
  // a convenience for EventTypeName(type()) (EventTypeName() is in
  // event_utils).
  const char* GetName() const;

  // This is only intended to be used externally by classes that are modifying
  // events in an EventRewriter.
  void SetFlags(int flags);

  EventTarget* target() const { return target_; }
  EventPhase phase() const { return phase_; }
  EventResult result() const { return result_; }

  LatencyInfo* latency() { return &latency_; }
  const LatencyInfo* latency() const { return &latency_; }
  void set_latency(const LatencyInfo& latency) { latency_ = latency; }

  int source_device_id() const { return source_device_id_; }
  void set_source_device_id(int id) { source_device_id_ = id; }

  // Sets the properties associated with this Event.
  void SetProperties(const Properties& properties);

  // Returns the properties associated with this event, which may be null.
  // The properties are meant to provide a way to associate arbitrary key/value
  // pairs with Events and not used by Event.
  const Properties* properties() const { return properties_.get(); }

  // By default, events are "cancelable", this means any default processing that
  // the containing abstraction layer may perform can be prevented by calling
  // SetHandled(). SetHandled() or StopPropagation() must not be called for
  // events that are not cancelable.
  bool cancelable() const { return cancelable_; }

  // The following methods return true if the respective keys were pressed at
  // the time the event was created.
  bool IsShiftDown() const { return (flags_ & EF_SHIFT_DOWN) != 0; }
  bool IsControlDown() const { return (flags_ & EF_CONTROL_DOWN) != 0; }
  bool IsAltDown() const { return (flags_ & EF_ALT_DOWN) != 0; }
  bool IsCommandDown() const { return (flags_ & EF_COMMAND_DOWN) != 0; }
  bool IsAltGrDown() const { return (flags_ & EF_ALTGR_DOWN) != 0; }
  bool IsCapsLockOn() const { return (flags_ & EF_CAPS_LOCK_ON) != 0; }

  bool IsSynthesized() const { return (flags_ & EF_IS_SYNTHESIZED) != 0; }

  bool IsCancelModeEvent() const { return type_ == EventType::kCancelMode; }

  bool IsKeyEvent() const {
    return type_ == EventType::kKeyPressed || type_ == EventType::kKeyReleased;
  }

  bool IsMouseEvent() const {
    return type_ == EventType::kMousePressed ||
           type_ == EventType::kMouseDragged ||
           type_ == EventType::kMouseReleased ||
           type_ == EventType::kMouseMoved ||
           type_ == EventType::kMouseEntered ||
           type_ == EventType::kMouseExited ||
           type_ == EventType::kMousewheel ||
           type_ == EventType::kMouseCaptureChanged;
  }

  bool IsTouchEvent() const {
    return type_ == EventType::kTouchReleased ||
           type_ == EventType::kTouchPressed ||
           type_ == EventType::kTouchMoved ||
           type_ == EventType::kTouchCancelled;
  }

  bool IsGestureEvent() const {
    switch (type_) {
      case EventType::kGestureScrollBegin:
      case EventType::kGestureScrollEnd:
      case EventType::kGestureScrollUpdate:
      case EventType::kGestureTap:
      case EventType::kGestureDoubleTap:
      case EventType::kGestureTapCancel:
      case EventType::kGestureTapDown:
      case EventType::kGestureTapUnconfirmed:
      case EventType::kGestureBegin:
      case EventType::kGestureEnd:
      case EventType::kGestureTwoFingerTap:
      case EventType::kGesturePinchBegin:
      case EventType::kGesturePinchEnd:
      case EventType::kGesturePinchUpdate:
      case EventType::kGestureLongPress:
      case EventType::kGestureLongTap:
      case EventType::kGestureSwipe:
      case EventType::kGestureShowPress:
        // When adding a gesture event which is paired with an event which
        // occurs earlier, add the event to |IsEndingEvent|.
        return true;

      case EventType::kScrollFlingCancel:
      case EventType::kScrollFlingStart:
        // These can be ScrollEvents too. EF_FROM_TOUCH determines if they're
        // Gesture or Scroll events.
        return (flags_ & EF_FROM_TOUCH) == EF_FROM_TOUCH;

      default:
        break;
    }
    return false;
  }

  // An ending event is paired with the event which started it. Setting capture
  // should not prevent ending events from getting to their initial target.
  bool IsEndingEvent() const {
    switch (type_) {
      case EventType::kTouchCancelled:
      case EventType::kGestureTapCancel:
      case EventType::kGestureEnd:
      case EventType::kGestureScrollEnd:
      case EventType::kGesturePinchEnd:
        return true;
      default:
        return false;
    }
  }

  bool IsScrollEvent() const {
    // Flings can be GestureEvents too. EF_FROM_TOUCH determines if they're
    // Gesture or Scroll events.
    return type_ == EventType::kScroll ||
           ((type_ == EventType::kScrollFlingStart ||
             type_ == EventType::kScrollFlingCancel) &&
            !(flags() & EF_FROM_TOUCH));
  }

  bool IsPinchEvent() const {
    return type_ == EventType::kGesturePinchBegin ||
           type_ == EventType::kGesturePinchUpdate ||
           type_ == EventType::kGesturePinchEnd;
  }

  bool IsScrollGestureEvent() const {
    return type_ == EventType::kGestureScrollBegin ||
           type_ == EventType::kGestureScrollUpdate ||
           type_ == EventType::kGestureScrollEnd;
  }

  bool IsFlingScrollEvent() const {
    return type_ == EventType::kScrollFlingCancel ||
           type_ == EventType::kScrollFlingStart;
  }

  bool IsMouseWheelEvent() const { return type_ == EventType::kMousewheel; }

  bool IsLocatedEvent() const {
    return IsMouseEvent() || IsScrollEvent() || IsTouchEvent() ||
           IsGestureEvent() || type_ == EventType::kDropTargetEvent;
  }

  // Convenience methods to cast |this| to a CancelModeEvent.
  // IsCancelModeEvent() must be true as a precondition to calling these
  // methods.
  CancelModeEvent* AsCancelModeEvent();
  const CancelModeEvent* AsCancelModeEvent() const;

  // Convenience methods to cast |this| to a GestureEvent. IsGestureEvent()
  // must be true as a precondition to calling these methods.
  GestureEvent* AsGestureEvent();
  const GestureEvent* AsGestureEvent() const;

  // Convenience methods to cast |this| to a KeyEvent. IsKeyEvent()
  // must be true as a precondition to calling these methods.
  KeyEvent* AsKeyEvent();
  const KeyEvent* AsKeyEvent() const;

  // Convenience methods to cast |this| to a LocatedEvent. IsLocatedEvent()
  // must be true as a precondition to calling these methods.
  LocatedEvent* AsLocatedEvent();
  const LocatedEvent* AsLocatedEvent() const;

  // Convenience methods to cast |this| to a MouseEvent. IsMouseEvent()
  // must be true as a precondition to calling these methods.
  MouseEvent* AsMouseEvent();
  const MouseEvent* AsMouseEvent() const;

  // Convenience methods to cast |this| to a MouseWheelEvent.
  // IsMouseWheelEvent() must be true as a precondition to calling these
  // methods.
  MouseWheelEvent* AsMouseWheelEvent();
  const MouseWheelEvent* AsMouseWheelEvent() const;

  // Convenience methods to cast |this| to a ScrollEvent. IsScrollEvent()
  // must be true as a precondition to calling these methods.
  ScrollEvent* AsScrollEvent();
  const ScrollEvent* AsScrollEvent() const;

  // Convenience methods to cast |this| to a TouchEvent. IsTouchEvent()
  // must be true as a precondition to calling these methods.
  TouchEvent* AsTouchEvent();
  const TouchEvent* AsTouchEvent() const;

  // Returns true if the event has a valid |native_event_|.
  bool HasNativeEvent() const;

  // Immediately stops the propagation of the event. This must be called only
  // from an EventHandler during an event-dispatch. Any event handler that may
  // be in the list will not receive the event after this is called.
  // Note that StopPropagation() can be called only for cancelable events.
  void StopPropagation();
  bool stopped_propagation() const { return !!(result_ & ER_CONSUMED); }

  // Marks the event as having been handled. A handled event does not reach the
  // next event phase. For example, if an event is handled during the pre-target
  // phase, then the event is dispatched to all pre-target handlers, but not to
  // the target or post-target handlers.
  // Note that SetHandled() can be called only for cancelable events.
  void SetHandled();
  bool handled() const { return result_ != ER_UNHANDLED; }

  // Marks the event as skipped. This immediately stops the propagation of the
  // event like `StopPropagation()` but sets an extra bit so that the dispatcher
  // of the event can use this extra information to decide to handle the event
  // themselves. For example in the case of ash-chrome - lacros-chrome
  // interaction in ChromeOS, lacros-chrome can mark the event as 'skipped' to
  // stop the propagation, but also notifies ash-chroem that the event was not
  // handled in lacros. Note that `handled()` will still return true to stop the
  // event from being passed to the next phase. Note that SetSkipped() can be
  // called only for cancelable events.
  void SetSkipped();

  // For debugging. Not a stable serialization format.
  virtual std::string ToString() const;

  // Copies an arbitrary event. If you have a typed event (e.g. a MouseEvent)
  // just use its copy constructor.
  //
  // Every concrete class transitively inheriting Event must implement this
  // method, even if any ancestors have provided an implementation.
  virtual std::unique_ptr<Event> Clone() const = 0;

 protected:
  Event(EventType type, base::TimeTicks time_stamp, int flags);
  Event(const PlatformEvent& native_event, EventType type, int flags);
  Event(const Event& copy);
  Event& operator=(const Event& rhs);

  void SetType(EventType type);
  void set_cancelable(bool cancelable) { cancelable_ = cancelable; }

  void set_time_stamp(base::TimeTicks time_stamp) { time_stamp_ = time_stamp; }

  // Override per concrete class if some data needs to get invalidated or
  // updated when the event flags are updated.
  virtual void OnFlagsUpdated() {}

 private:
  friend class EventTestApi;
  friend class EventRewriter;

  EventType type_;
  base::TimeTicks time_stamp_;
  LatencyInfo latency_;
  int flags_;
  PlatformEvent native_event_;
  bool cancelable_ = true;
  // Neither Event copy constructor nor the assignment operator copies
  // `target_`, as `target_` should be explicitly set so the setter will be
  // responsible for tracking it.
  raw_ptr<EventTarget, AcrossTasksDanglingUntriaged> target_ = nullptr;
  EventPhase phase_ = EP_PREDISPATCH;
  EventResult result_ = ER_UNHANDLED;

  // The device id the event came from, or ED_UNKNOWN_DEVICE if the information
  // is not available.
  int source_device_id_ = ED_UNKNOWN_DEVICE;

  std::unique_ptr<Properties> properties_;
};

class EVENTS_EXPORT CancelModeEvent : public Event {
 public:
  CancelModeEvent();
  ~CancelModeEvent() override;

  // Event:
  std::unique_ptr<Event> Clone() const override;
};

class EVENTS_EXPORT LocatedEvent : public Event {
 public:
  // Convenience function that casts |event| to a LocatedEvent if it is one,
  // otherwise returns null.
  static const LocatedEvent* FromIfValid(const Event* event) {
    return event && event->IsLocatedEvent() ? event->AsLocatedEvent() : nullptr;
  }

  ~LocatedEvent() override;

  float x() const { return location_.x(); }
  float y() const { return location_.y(); }
  void set_location(const gfx::Point& location) {
    location_ = gfx::PointF(location);
  }
  void set_location_f(const gfx::PointF& location) { location_ = location; }
  gfx::Point location() const { return gfx::ToFlooredPoint(location_); }
  const gfx::PointF& location_f() const { return location_; }
  void set_root_location(const gfx::Point& root_location) {
    root_location_ = gfx::PointF(root_location);
  }
  void set_root_location_f(const gfx::PointF& root_location) {
    root_location_ = root_location;
  }
  gfx::Point root_location() const {
    return gfx::ToFlooredPoint(root_location_);
  }
  const gfx::PointF& root_location_f() const { return root_location_; }

  // Transform the locations using |inverted_root_transform| and
  // |inverted_local_transform|. |inverted_local_transform| is only used if
  // the event has a target.
  virtual void UpdateForRootTransform(
      const gfx::Transform& inverted_root_transform,
      const gfx::Transform& inverted_local_transform);

  template <class T>
  void ConvertLocationToTarget(const T* source, const T* target) {
    if (!target || target == source)
      return;
    gfx::Point offset = gfx::ToFlooredPoint(location_);
    T::ConvertPointToTarget(source, target, &offset);
    gfx::Vector2d diff = gfx::ToFlooredPoint(location_) - offset;
    location_ = location_ - diff;
  }

  // Event:
  std::string ToString() const override;

 protected:
  friend class LocatedEventTestApi;

  LocatedEvent(const LocatedEvent& copy);

  explicit LocatedEvent(const PlatformEvent& native_event);

  // Create a new LocatedEvent which is identical to the provided model.
  // If source / target windows are provided, the model location will be
  // converted from |source| coordinate system to |target| coordinate system.
  template <class T>
  LocatedEvent(const LocatedEvent& model, T* source, T* target)
      : Event(model),
        location_(model.location_),
        root_location_(model.root_location_) {
    ConvertLocationToTarget(source, target);
  }

  // Used for synthetic events in testing.
  LocatedEvent(EventType type,
               const gfx::PointF& location,
               const gfx::PointF& root_location,
               base::TimeTicks time_stamp,
               int flags);

  // Location of the event relative to the target window and in the target
  // window's coordinate space. If there is no target this is the same as
  // |root_location_|. Native events may generate float values with sub-pixel
  // precision.
  gfx::PointF location_;

  // Location of the event. What coordinate system this is in depends upon the
  // phase of event dispatch. For client code (meaning EventHandlers) it is
  // generally in screen coordinates, but early on it may be in pixels and
  // relative to a display. Native events may generate float values with
  // sub-pixel precision.
  gfx::PointF root_location_;
};

class EVENTS_EXPORT MouseEvent : public LocatedEvent {
 public:
  // NOTE: On some platforms this will allow an event to be constructed from a
  // void*, see PlatformEvent.
  explicit MouseEvent(const PlatformEvent& native_event);

  // Create a new MouseEvent based on the provided model.
  // Uses the provided |type| and |flags| for the new event.
  // If source / target windows are provided, the model location will be
  // converted from |source| coordinate system to |target| coordinate system.
  template <class T>
  MouseEvent(const MouseEvent& model, T* source, T* target)
      : LocatedEvent(model, source, target),
        changed_button_flags_(model.changed_button_flags_),
        movement_(model.movement_),
        pointer_details_(model.pointer_details_) {}

  template <class T>
  MouseEvent(const MouseEvent& model,
             T* source,
             T* target,
             EventType type,
             int flags)
      : LocatedEvent(model, source, target),
        changed_button_flags_(model.changed_button_flags_),
        movement_(model.movement_),
        pointer_details_(model.pointer_details_) {
    SetType(type);
    SetFlags(flags);
  }

  // Note: Use the ctor for MouseWheelEvent if type is EventType::kMousewheel.
  MouseEvent(EventType type,
             const gfx::PointF& location,
             const gfx::PointF& root_location,
             base::TimeTicks time_stamp,
             int flags,
             int changed_button_flags,
             const PointerDetails& pointer_details =
                 PointerDetails(EventPointerType::kMouse, kPointerIdMouse));

  // DEPRECATED: Prefer constructor that takes gfx::PointF.
  MouseEvent(EventType type,
             const gfx::Point& location,
             const gfx::Point& root_location,
             base::TimeTicks time_stamp,
             int flags,
             int changed_button_flags,
             const PointerDetails& pointer_details =
                 PointerDetails(EventPointerType::kMouse, kPointerIdMouse));

  MouseEvent(const MouseEvent& copy);
  ~MouseEvent() override;

  void InitializeNative();

  class DispatcherApi {
   public:
    explicit DispatcherApi(MouseEvent* event) : event_(event) {}

    DispatcherApi(const DispatcherApi&) = delete;
    DispatcherApi& operator=(const DispatcherApi&) = delete;

    // TODO(eirage): convert this to builder pattern.
    void set_movement(const gfx::Vector2dF& movement) {
      event_->movement_ = movement;
      event_->SetFlags(event_->flags() | EF_UNADJUSTED_MOUSE);
    }

   private:
    raw_ptr<MouseEvent> event_;
  };

  // Conveniences to quickly test what button is down
  bool IsOnlyLeftMouseButton() const {
    return button_flags() == EF_LEFT_MOUSE_BUTTON;
  }

  bool IsLeftMouseButton() const {
    return (flags() & EF_LEFT_MOUSE_BUTTON) != 0;
  }

  bool IsOnlyMiddleMouseButton() const {
    return button_flags() == EF_MIDDLE_MOUSE_BUTTON;
  }

  bool IsMiddleMouseButton() const {
    return (flags() & EF_MIDDLE_MOUSE_BUTTON) != 0;
  }

  bool IsOnlyRightMouseButton() const {
    return button_flags() == EF_RIGHT_MOUSE_BUTTON;
  }

  bool IsRightMouseButton() const {
    return (flags() & EF_RIGHT_MOUSE_BUTTON) != 0;
  }

  bool IsAnyButton() const { return button_flags() != 0; }

  // Returns the flags for the mouse buttons.
  int button_flags() const {
    return flags() & (EF_LEFT_MOUSE_BUTTON | EF_MIDDLE_MOUSE_BUTTON |
                      EF_RIGHT_MOUSE_BUTTON | EF_BACK_MOUSE_BUTTON |
                      EF_FORWARD_MOUSE_BUTTON);
  }

  // Compares two mouse down events and returns true if the second one should
  // be considered a repeat of the first.
  static bool IsRepeatedClickEvent(const MouseEvent& event1,
                                   const MouseEvent& event2);

  // Get the click count. Can be 1, 2 or 3 for mousedown messages, 0 otherwise.
  int GetClickCount() const;

  // Set the click count for a mousedown message. Can be 1, 2 or 3.
  void SetClickCount(int click_count);

  // Identifies the button that changed. During a press this corresponds to the
  // button that was pressed and during a release this corresponds to the button
  // that was released.
  // NOTE: during a press and release flags() contains the complete set of
  // flags. Use this to determine the button that was pressed or released.
  int changed_button_flags() const { return changed_button_flags_; }

  // Updates the button that changed.
  void set_changed_button_flags(int flags) { changed_button_flags_ = flags; }

  const gfx::Vector2dF& movement() const { return movement_; }

  const PointerDetails& pointer_details() const { return pointer_details_; }

  // Event:
  std::string ToString() const override;
  std::unique_ptr<Event> Clone() const override;

  // Resets the last_click_event_ for unit tests.
  static void ResetLastClickForTest();

 private:
  FRIEND_TEST_ALL_PREFIXES(EventTest, DoubleClickRequiresUniqueTimestamp);
  FRIEND_TEST_ALL_PREFIXES(EventTest, SingleClickRightLeft);

  // Returns the repeat count based on the previous mouse click, if it is
  // recent enough and within a small enough distance.
  static int GetRepeatCount(const MouseEvent& click_event);

  // See description above getter for details.
  int changed_button_flags_;

  // Raw mouse movement value reported from mouse hardware. The value of this is
  // platform dependent and may change depending upon the hardware connected to
  // the device. This field is only set if the flag EF_UNADJUSTED_MOUSE is
  // present (which is the case on windows platforms, and CrOS if the
  // kEnableOrdinalMotion flag is set).
  //
  // TODO(b/171249701): always enable on CrOS.
  gfx::Vector2dF movement_;

  // The most recent user-generated MouseEvent, used to detect double clicks.
  static MouseEvent* last_click_event_;

  // Structure for holding pointer details for implementing PointerEvents API.
  PointerDetails pointer_details_;
};

class ScrollEvent;

class EVENTS_EXPORT MouseWheelEvent : public MouseEvent {
 public:
  // See |offset| for details.
  static const int kWheelDelta;

  explicit MouseWheelEvent(const PlatformEvent& native_event);
  explicit MouseWheelEvent(const ScrollEvent& scroll_event);
  MouseWheelEvent(const MouseEvent& mouse_event, int x_offset, int y_offset);
  MouseWheelEvent(const MouseWheelEvent& copy);
  ~MouseWheelEvent() override;

  template <class T>
  MouseWheelEvent(const MouseWheelEvent& model, T* source, T* target)
      : MouseEvent(model, source, target, model.type(), model.flags()),
        offset_(model.x_offset(), model.y_offset()),
        tick_120ths_(model.tick_120ths()) {}

  // Used for synthetic events in testing and by the gesture recognizer.
  MouseWheelEvent(
      const gfx::Vector2d& offset,
      const gfx::PointF& location,
      const gfx::PointF& root_location,
      base::TimeTicks time_stamp,
      int flags,
      int changed_button_flags,
      const std::optional<gfx::Vector2d> tick_120ths = std::nullopt);

  // DEPRECATED: Prefer the constructor that takes gfx::PointF.
  MouseWheelEvent(const gfx::Vector2d& offset,
                  const gfx::Point& location,
                  const gfx::Point& root_location,
                  base::TimeTicks time_stamp,
                  int flags,
                  int changed_button_flags);

  // The amount to scroll. This is not necessarily linearly related to the
  // amount that the wheel moved, due to scroll acceleration.
  // Note: x_offset() > 0/y_offset() > 0 means scroll left/up.
  int x_offset() const { return offset_.x(); }
  int y_offset() const { return offset_.y(); }
  const gfx::Vector2d& offset() const { return offset_; }

  // The amount the wheel(s) moved, in 120ths of a tick.
  const gfx::Vector2d& tick_120ths() const { return tick_120ths_; }

  // Event:
  std::unique_ptr<Event> Clone() const override;

 private:
  gfx::Vector2d offset_;
  gfx::Vector2d tick_120ths_;
};

// NOTE: Pen (stylus) events use TouchEvent with kPen. They were
// originally implemented as MouseEvent but switched to TouchEvent when UX
// decided not to show hover effects for pen.
class EVENTS_EXPORT TouchEvent : public LocatedEvent {
 public:
  explicit TouchEvent(const PlatformEvent& native_event);

  // Create a new TouchEvent which is identical to the provided model.
  // If source / target windows are provided, the model location will be
  // converted from |source| coordinate system to |target| coordinate system.
  template <class T>
  TouchEvent(const TouchEvent& model, T* source, T* target)
      : LocatedEvent(model, source, target),
        unique_event_id_(model.unique_event_id_),
        may_cause_scrolling_(model.may_cause_scrolling_),
        pointer_details_(model.pointer_details_) {}

  TouchEvent(EventType type,
             const gfx::PointF& location,
             const gfx::PointF& root_location,
             base::TimeTicks time_stamp,
             const PointerDetails& pointer_details,
             int flags = 0);

  // DEPRECATED: Prefer the constructor that takes gfx::PointF.
  TouchEvent(EventType type,
             const gfx::Point& location,
             base::TimeTicks time_stamp,
             const PointerDetails& pointer_details,
             int flags = 0);

  TouchEvent(const TouchEvent& copy);

  ~TouchEvent() override;

  // A unique identifier for this event.
  uint32_t unique_event_id() const { return unique_event_id_; }

  void set_may_cause_scrolling(bool causes) { may_cause_scrolling_ = causes; }
  bool may_cause_scrolling() const { return may_cause_scrolling_; }

  void set_hovering(bool hovering) { hovering_ = hovering; }
  bool hovering() const { return hovering_; }

  // Overridden from LocatedEvent.
  void UpdateForRootTransform(
      const gfx::Transform& inverted_root_transform,
      const gfx::Transform& inverted_local_transform) override;

  // Marks the event as not participating in synchronous gesture recognition.
  void DisableSynchronousHandling();
  bool synchronous_handling_disabled() const {
    return !!(result() & ER_DISABLE_SYNC_HANDLING);
  }

  // Forces to process the gesture recognition even if the event is marked
  // as `handled` or `stopped_propagation`.
  void ForceProcessGesture();
  bool force_process_gesture() const {
    return !!(result() & ER_FORCE_PROCESS_GESTURE);
  }

  const PointerDetails& pointer_details() const { return pointer_details_; }
  void SetPointerDetailsForTest(const PointerDetails& pointer_details);

  float ComputeRotationAngle() const;

  // Event:
  std::unique_ptr<Event> Clone() const override;

 private:
  // A unique identifier for the touch event.
  // NOTE: this is *not* serialized over mojom, as the number is unique to
  // a particular process, and as mojom may go cross process, to serialize could
  // lead to conflicts.
  uint32_t unique_event_id_;

  // Whether the (unhandled) touch event will produce a scroll event (e.g., a
  // touchmove that exceeds the platform slop region, or a touchend that
  // causes a fling). Defaults to false.
  bool may_cause_scrolling_ = false;

  // True for devices like some pens when they support hovering over
  // digitizer and they send events while hovering.
  bool hovering_ = false;

  // Structure for holding pointer details for implementing PointerEvents API.
  PointerDetails pointer_details_;
};

// A KeyEvent is really two distinct classes, melded together due to the
// DOM legacy of Windows key events: a keystroke event (is_char_ == false),
// or a character event (is_char_ == true).
//
// For a keystroke event,
// -- |bool is_char_| is false.
// -- |EventType Event::type()| can be EventType::kKeyPressed or
// EventType::kKeyReleased.
// -- |DomCode code_| and |int Event::flags()| represent the physical key event.
//    - code_ is a platform-independent representation of the physical key,
//      based on DOM UI Events KeyboardEvent |code| values. It does not
//      vary depending on key layout.
//      http://www.w3.org/TR/DOM-Level-3-Events-code/
//    - Event::flags() provides the active modifiers for the physical key
//      press. Its value reflects the state after the event; that is, for
//      a modifier key, a press includes the corresponding flag and a release
//      does not.
// -- |DomKey key_| provides the meaning (character or action) of the key
//    event, in the context of the active layout and modifiers. It corresponds
//    to DOM UI Events KeyboardEvent |key| values.
//    http://www.w3.org/TR/DOM-Level-3-Events-key/
// -- |KeyboardCode key_code_| supports the legacy web event |keyCode| field,
//    and its VKEY_ values are chosen to match Windows/IE for compatibility.
//    For printable characters, this may or may not be a layout-mapped value,
//    imitating MS Windows: if the mapped key generates a character that has
//    an associated VKEY_ code, then key_code_ is that code; if not, then
//    key_code_ is the unmapped VKEY_ code. For example, US, Greek, Cyrillic,
//    Japanese, etc. all use VKEY_Q for the key beside Tab, while French uses
//    VKEY_A. The stored key_code_ is non-located (e.g. VKEY_SHIFT rather than
//    VKEY_LSHIFT, VKEY_1 rather than VKEY_NUMPAD1).
// -- |uint32_t scan_code_| [IS_OZONE only] supports remapping of the top
//    function row based on a sysfs attribute provided by the kernel. This
//    allows devices to have a custom top row layout and still be able to
//    perform translation back and forth between F-Key and Action-Key. The
//    value of this scan code is device specific.
//
// For a character event,
// -- |bool is_char_| is true.
// -- |EventType Event::type()| is EventType::kKeyPressed.
// -- |DomCode code_| is DomCode::NONE.
// -- |DomKey key_| is a UTF-16 code point.
// -- |KeyboardCode key_code_| is conflated with the character-valued key_
//    by some code, because both arrive in the wParam field of a Windows event.
//
class EVENTS_EXPORT KeyEvent : public Event {
 public:
  // Create a KeyEvent from a NativeEvent. For Windows this native event can
  // be either a keystroke message (WM_KEYUP/WM_KEYDOWN) or a character message
  // (WM_CHAR). Other systems have only keystroke events.
  explicit KeyEvent(const PlatformEvent& native_event);

  // Create a KeyEvent from a NativeEvent but with mocked flags.
  // This method is necessary for Windows since MSG does not contain all flags.
  KeyEvent(const PlatformEvent& native_event, int event_flags);

  // Create a keystroke event from a legacy KeyboardCode.
  // This should not be used in new code.
  KeyEvent(EventType type,
           KeyboardCode key_code,
           int flags,
           base::TimeTicks time_stamp = base::TimeTicks());

  // Create a fully defined keystroke event.
  KeyEvent(EventType type,
           KeyboardCode key_code,
           DomCode code,
           int flags,
           DomKey key,
           base::TimeTicks time_stamp,
           bool is_char = false);

  // Create an event with event type.
  KeyEvent(EventType type,
           KeyboardCode key_code,
           DomCode code,
           int flags,
           base::TimeTicks time_stamp);

  // Used for synthetic events with code of DOM KeyboardEvent (e.g. 'KeyA')
  // See also: ui/events/keycodes/dom/dom_values.txt
  KeyEvent(EventType type, KeyboardCode key_code, DomCode code, int flags);

  KeyEvent(const KeyEvent& rhs);

  KeyEvent& operator=(const KeyEvent& rhs);

  ~KeyEvent() override;

  // Returns true if synthesizing key repeat in InitializeNative is enabled.
  static bool IsSynthesizeKeyRepeatEnabled();

  // Sets whether to enable synthesizing key repeat in InitializeNative().
  static void SetSynthesizeKeyRepeatEnabled(bool enabled);

  static ui::KeyEvent FromCharacter(
      char16_t character,
      KeyboardCode key_code,
      DomCode code,
      int flags,
      base::TimeTicks time_stamp = base::TimeTicks());

  void InitializeNative();

  // This bypasses the normal mapping from keystroke events to characters,
  // which allows an I18N virtual keyboard to fabricate a keyboard event that
  // does not have a corresponding KeyboardCode (example: U+00E1 Latin small
  // letter A with acute, U+0410 Cyrillic capital letter A).
  void set_character(char16_t character) {
    key_ = DomKey::FromCharacter(character);
  }

  // Gets the character generated by this key event. It only supports Unicode
  // BMP characters.
  char16_t GetCharacter() const;

  // If this is a keystroke event with key_code_ VKEY_RETURN, returns '\r';
  // otherwise returns the same as GetCharacter().
  char16_t GetUnmodifiedText() const;

  // If the Control key is down in the event, returns a layout-independent
  // character (corresponding to US layout); otherwise returns the same
  // as GetUnmodifiedText().
  char16_t GetText() const;

  // True if this is a character event, false if this is a keystroke event.
  bool is_char() const { return is_char_; }

  bool is_repeat() const { return (flags() & EF_IS_REPEAT) != 0; }

  // Gets the associated (Windows-based) KeyboardCode for this key event.
  // Historically, this has also been used to obtain the character associated
  // with a character event, because both use the Window message 'wParam' field.
  // This should be avoided; if necessary for backwards compatibility, use
  // GetConflatedWindowsKeyCode().
  KeyboardCode key_code() const { return key_code_; }

  // This is only intended to be used externally by classes that are modifying
  // events in an EventRewriter.
  void set_key_code(KeyboardCode key_code) { key_code_ = key_code; }

#if BUILDFLAG(IS_OZONE)
  // The scan code of the physical key. This is used to perform the mapping
  // of top row keys from Actions back to F-Keys on new Chrome OS keyboards
  // that supply the mapping via the kernel.
  uint32_t scan_code() const { return scan_code_; }
  void set_scan_code(uint32_t scan_code) { scan_code_ = scan_code; }
#endif  // BUILDFLAG(IS_OZONE)

  // Returns the same value as key_code(), except that located codes are
  // returned in place of non-located ones (e.g. VKEY_LSHIFT or VKEY_RSHIFT
  // instead of VKEY_SHIFT). This is a hybrid of semantic and physical
  // for legacy DOM reasons.
  KeyboardCode GetLocatedWindowsKeyboardCode() const;

  // For a keystroke event, returns the same value as key_code().
  // For a character event, returns the same value as GetCharacter().
  // This exists for backwards compatibility with Windows key events.
  uint16_t GetConflatedWindowsKeyCode() const;

  // Returns true for [Alt]+<num-pad digit> Unicode alt key codes used by Win.
  // TODO(msw): Additional work may be needed for analogues on other platforms.
  bool IsUnicodeKeyCode() const;

  // Returns the DOM .code (physical key identifier) for a keystroke event.
  DomCode code() const { return code_; }
  std::string GetCodeString() const;

  // Returns the DOM .key (layout meaning) for a keystroke event.
  DomKey GetDomKey() const;

  // Normalizes flags_ so that it describes the state after the event.
  // (Native X11 event flags describe the state before the event.)
  void NormalizeFlags();

  // Invalidates the DomKey associated with this event as the flags updating can
  // change the semantic meaning of the key.
  void OnFlagsUpdated() override;

  // Event:
  std::string ToString() const override;
  std::unique_ptr<Event> Clone() const override;

 protected:
  friend class KeyEventTestApi;

  // This allows a subclass TranslatedKeyEvent to be a non character event.
  void set_is_char(bool is_char) { is_char_ = is_char; }

 private:
  // Determine key_ on a keystroke event from code_ and flags().
  void ApplyLayout() const;

  // Tells if this is a repeated KeyEvent based on |last_key_event|, which is
  // then updated with the new last KeyEvent address.
  bool IsRepeated(KeyEvent** last_key_event);

  // Returns the pointer for the last processed KeyEvent.
  KeyEvent** GetLastKeyEvent();

  KeyboardCode key_code_;

#if BUILDFLAG(IS_OZONE)
  // The scan code of the physical key on Chrome OS.
  uint32_t scan_code_ = 0;
#endif  // BUILDFLAG(IS_OZONE)

  // DOM KeyboardEvent |code| (e.g. DomCode::US_A, DomCode::SPACE).
  // http://www.w3.org/TR/DOM-Level-3-Events-code/
  //
  // This value represents the physical position in the keyboard and can be
  // converted from / to keyboard scan code like XKB.
  DomCode code_;

  // True if this is a character event, false if this is a keystroke event.
  bool is_char_ = false;

  // TODO(kpschoedel): refactor so that key_ is not mutable.
  // This requires defining the KeyEvent completely at construction rather
  // than lazily under GetCharacter(), which likely also means removing
  // the two 'incomplete' constructors. crbug.com/444045
  //
  // DOM KeyboardEvent |key|
  // http://www.w3.org/TR/DOM-Level-3-Events-key/
  //
  // This value represents the meaning of a key, which is either a Unicode
  // character, or a named DomKey:: value.
  // This is not necessarily initialized when the event is constructed;
  // it may be set only if and when GetCharacter() or GetDomKey() is called.
  mutable DomKey key_ = DomKey::NONE;

  static KeyEvent* last_key_event_;
#if BUILDFLAG(IS_OZONE)
  static KeyEvent* last_ibus_key_event_;
#endif

  static bool synthesize_key_repeat_enabled_;
};

class EVENTS_EXPORT ScrollEvent : public MouseEvent {
 public:
  explicit ScrollEvent(const PlatformEvent& native_event);

  template <class T>
  ScrollEvent(const ScrollEvent& model, T* source, T* target)
      : MouseEvent(model, source, target),
        x_offset_(model.x_offset_),
        y_offset_(model.y_offset_),
        x_offset_ordinal_(model.x_offset_ordinal_),
        y_offset_ordinal_(model.y_offset_ordinal_),
        finger_count_(model.finger_count_),
        momentum_phase_(model.momentum_phase_),
        scroll_event_phase_(model.scroll_event_phase_) {}

  ScrollEvent(EventType type,
              const gfx::PointF& location,
              const gfx::PointF& root_location,
              base::TimeTicks time_stamp,
              int flags,
              float x_offset,
              float y_offset,
              float x_offset_ordinal,
              float y_offset_ordinal,
              int finger_count,
              EventMomentumPhase momentum_phase = EventMomentumPhase::NONE,
              ScrollEventPhase phase = ScrollEventPhase::kNone);

  // DEPRECATED: Prefer the constructor that takes gfx::PointF.
  ScrollEvent(EventType type,
              const gfx::Point& location,
              base::TimeTicks time_stamp,
              int flags,
              float x_offset,
              float y_offset,
              float x_offset_ordinal,
              float y_offset_ordinal,
              int finger_count,
              EventMomentumPhase momentum_phase = EventMomentumPhase::NONE,
              ScrollEventPhase phase = ScrollEventPhase::kNone);

  ScrollEvent(const ScrollEvent& copy);
  ~ScrollEvent() override;

  // Scale the scroll event's offset value.
  // This is useful in the multi-monitor setup where it needs to be scaled
  // to provide a consistent user experience.
  void Scale(const float factor);

  float x_offset() const { return x_offset_; }
  float y_offset() const { return y_offset_; }
  float x_offset_ordinal() const { return x_offset_ordinal_; }
  float y_offset_ordinal() const { return y_offset_ordinal_; }
  int finger_count() const { return finger_count_; }
  EventMomentumPhase momentum_phase() const { return momentum_phase_; }
  ScrollEventPhase scroll_event_phase() const { return scroll_event_phase_; }

  // Event:
  std::string ToString() const override;
  std::unique_ptr<Event> Clone() const override;

 private:
  // Potential accelerated offsets.
  float x_offset_;
  float y_offset_;
  // Unaccelerated offsets.
  float x_offset_ordinal_;
  float y_offset_ordinal_;
  // Number of fingers on the pad.
  int finger_count_;

  // For non-fling events, provides momentum information (e.g. for the case
  // where the device provides continuous event updates during a fling).
  EventMomentumPhase momentum_phase_ = EventMomentumPhase::NONE;

  // Provides phase information if device can provide.
  ScrollEventPhase scroll_event_phase_ = ScrollEventPhase::kNone;
};

class EVENTS_EXPORT GestureEvent : public LocatedEvent {
 public:
  // The constructor takes a default unique_touch_id of zero to support many
  // (80+) existing tests that doesn't care about this id.
  GestureEvent(float x,
               float y,
               int flags,
               base::TimeTicks time_stamp,
               const GestureEventDetails& details,
               uint32_t unique_touch_event_id = 0);

  // Create a new GestureEvent which is identical to the provided model.
  // If source / target windows are provided, the model location will be
  // converted from |source| coordinate system to |target| coordinate system.
  template <typename T>
  GestureEvent(const GestureEvent& model, T* source, T* target)
      : LocatedEvent(model, source, target), details_(model.details_) {}
  GestureEvent(const GestureEvent& copy);
  ~GestureEvent() override;

  const GestureEventDetails& details() const { return details_; }

  uint32_t unique_touch_event_id() const { return unique_touch_event_id_; }

  // Event:
  std::string ToString() const override;
  std::unique_ptr<Event> Clone() const override;

 private:
  GestureEventDetails details_;

  // The unique id of the touch event that caused the gesture event to be
  // dispatched. This field gets a non-zero value only for gestures that are
  // released through TouchDispositionGestureFilter::SendGesture. The gesture
  // events that aren't fired directly in response to processing a touch-event
  // (e.g. timer fired ones), this id is zero. See crbug.com/618738.
  uint32_t unique_touch_event_id_;
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_H_
