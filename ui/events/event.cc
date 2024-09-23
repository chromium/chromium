// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/base/ui_base_features.h"                               // nogncheck
#include "ui/events/ozone/events_ozone.h"                           // nogncheck
#include "ui/events/ozone/layout/keyboard_layout_engine.h"          // nogncheck
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/events/keycodes/platform_key_map_win.h"
#endif

namespace ui {
namespace {

constexpr int kChangedButtonFlagMask =
    EF_LEFT_MOUSE_BUTTON | EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON |
    EF_BACK_MOUSE_BUTTON | EF_FORWARD_MOUSE_BUTTON;

std::string MomentumPhaseToString(EventMomentumPhase phase) {
  switch (phase) {
    case EventMomentumPhase::NONE:
      return "NONE";
    case EventMomentumPhase::BEGAN:
      return "BEGAN";
    case EventMomentumPhase::MAY_BEGIN:
      return "MAY_BEGIN";
    case EventMomentumPhase::INERTIAL_UPDATE:
      return "INERTIAL_UPDATE";
    case EventMomentumPhase::END:
      return "END";
    case EventMomentumPhase::BLOCKED:
      return "BLOCKED";
  }
}

std::string ScrollEventPhaseToString(ScrollEventPhase phase) {
  switch (phase) {
    case ScrollEventPhase::kNone:
      return "kEnd";
    case ScrollEventPhase::kBegan:
      return "kBegan";
    case ScrollEventPhase::kUpdate:
      return "kUpdate";
    case ScrollEventPhase::kEnd:
      return "kEnd";
  }
}

#if BUILDFLAG(IS_OZONE)
uint32_t ScanCodeFromNative(const PlatformEvent& native_event) {
  const KeyEvent* event = static_cast<const KeyEvent*>(native_event);
  DCHECK(event->IsKeyEvent());
  return event->scan_code();
}
#endif  // BUILDFLAG(IS_OZONE)

bool IsNearZero(const float num) {
  // Epsilon of 1e-10 at 0.
  return (std::fabs(num) < 1e-10);
}

bool IsRepeatedClickTimes(const base::TimeTicks& time_stamp1,
                          const base::TimeTicks& time_stamp2) {
  // The new event has been created from the same native event.
  if (time_stamp1 == time_stamp2) {
    return false;
  }

  base::TimeDelta time_difference = time_stamp2 - time_stamp1;
  return time_difference <= base::Milliseconds(ui::kDoubleClickTimeMs);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Event

Event::~Event() = default;

void Event::SetNativeEvent(const PlatformEvent& event) {
  if (!ShouldCopyPlatformEvents()) {
    return;
  }
  native_event_ = event;
}

const char* Event::GetName() const {
  return EventTypeName(type_).data();
}

void Event::SetProperties(const Properties& properties) {
  properties_ = std::make_unique<Properties>(properties);
}

CancelModeEvent* Event::AsCancelModeEvent() {
  CHECK(IsCancelModeEvent());
  return static_cast<CancelModeEvent*>(this);
}

const CancelModeEvent* Event::AsCancelModeEvent() const {
  CHECK(IsCancelModeEvent());
  return static_cast<const CancelModeEvent*>(this);
}

GestureEvent* Event::AsGestureEvent() {
  CHECK(IsGestureEvent());
  return static_cast<GestureEvent*>(this);
}

const GestureEvent* Event::AsGestureEvent() const {
  CHECK(IsGestureEvent());
  return static_cast<const GestureEvent*>(this);
}

KeyEvent* Event::AsKeyEvent() {
  CHECK(IsKeyEvent());
  return static_cast<KeyEvent*>(this);
}

const KeyEvent* Event::AsKeyEvent() const {
  CHECK(IsKeyEvent());
  return static_cast<const KeyEvent*>(this);
}

LocatedEvent* Event::AsLocatedEvent() {
  CHECK(IsLocatedEvent());
  return static_cast<LocatedEvent*>(this);
}

const LocatedEvent* Event::AsLocatedEvent() const {
  CHECK(IsLocatedEvent());
  return static_cast<const LocatedEvent*>(this);
}

MouseEvent* Event::AsMouseEvent() {
  CHECK(IsMouseEvent());
  return static_cast<MouseEvent*>(this);
}

const MouseEvent* Event::AsMouseEvent() const {
  CHECK(IsMouseEvent());
  return static_cast<const MouseEvent*>(this);
}

MouseWheelEvent* Event::AsMouseWheelEvent() {
  CHECK(IsMouseWheelEvent());
  return static_cast<MouseWheelEvent*>(this);
}

const MouseWheelEvent* Event::AsMouseWheelEvent() const {
  CHECK(IsMouseWheelEvent());
  return static_cast<const MouseWheelEvent*>(this);
}

ScrollEvent* Event::AsScrollEvent() {
  CHECK(IsScrollEvent());
  return static_cast<ScrollEvent*>(this);
}

const ScrollEvent* Event::AsScrollEvent() const {
  CHECK(IsScrollEvent());
  return static_cast<const ScrollEvent*>(this);
}

TouchEvent* Event::AsTouchEvent() {
  CHECK(IsTouchEvent());
  return static_cast<TouchEvent*>(this);
}

const TouchEvent* Event::AsTouchEvent() const {
  CHECK(IsTouchEvent());
  return static_cast<const TouchEvent*>(this);
}

bool Event::HasNativeEvent() const {
  return IsPlatformEventValid(native_event_);
}

void Event::StopPropagation() {
  // TODO(sad): Re-enable these checks once View uses dispatcher to dispatch
  // events.
  // CHECK(phase_ != EP_PREDISPATCH && phase_ != EP_POSTDISPATCH);
  CHECK(cancelable_);
  result_ = static_cast<EventResult>(result_ | ER_CONSUMED);
}

void Event::SetHandled() {
  // TODO(sad): Re-enable these checks once View uses dispatcher to dispatch
  // events.
  // CHECK(phase_ != EP_PREDISPATCH && phase_ != EP_POSTDISPATCH);
  CHECK(cancelable_);
  result_ = static_cast<EventResult>(result_ | ER_HANDLED);
}

void Event::SetSkipped() {
  CHECK(cancelable_);
  result_ = static_cast<EventResult>(result_ | ER_CONSUMED | ER_SKIPPED);
}

void Event::SetFlags(int flags) {
  flags_ = flags;
  OnFlagsUpdated();
}

std::string Event::ToString() const {
  return base::StrCat(
      {GetName(), " time_stamp=",
       base::NumberToString(time_stamp_.since_origin().InSecondsF()),
       " source_device_id=", base::NumberToString(source_device_id_)});
}

Event::Event(EventType type, base::TimeTicks time_stamp, int flags)
    : type_(type),
      time_stamp_(time_stamp.is_null() ? EventTimeForNow() : time_stamp),
      flags_(flags),
      native_event_(CreateInvalidPlatformEvent()) {
}

Event::Event(const PlatformEvent& native_event, EventType type, int flags)
    : type_(type),
      time_stamp_(EventTimeFromNative(native_event)),
      flags_(flags),
      // Note that the construction of an Event directly from a PlatformEvent
      // is the only time that ShouldCopyPlatformEvents() is not consulted.
      native_event_(native_event) {
  ComputeEventLatencyOS(native_event);

#if BUILDFLAG(IS_OZONE)
  source_device_id_ = native_event->source_device_id();
  if (auto* properties = native_event->properties())
    properties_ = std::make_unique<Properties>(*properties);
#endif
}

Event::Event(const Event& copy)
    : type_(copy.type_),
      time_stamp_(copy.time_stamp_),
      latency_(copy.latency_),
      flags_(copy.flags_),
      native_event_(ShouldCopyPlatformEvents() ? copy.native_event_
                                               : CreateInvalidPlatformEvent()),
      source_device_id_(copy.source_device_id_),
      properties_(copy.properties_
                      ? std::make_unique<Properties>(*copy.properties_)
                      : nullptr) {}

Event& Event::operator=(const Event& rhs) {
  if (this != &rhs) {
    type_ = rhs.type_;
    time_stamp_ = rhs.time_stamp_;
    latency_ = rhs.latency_;
    flags_ = rhs.flags_;
    native_event_ = ShouldCopyPlatformEvents() ? rhs.native_event_
                                               : CreateInvalidPlatformEvent();
    cancelable_ = rhs.cancelable_;
    phase_ = rhs.phase_;
    result_ = rhs.result_;
    source_device_id_ = rhs.source_device_id_;
    if (rhs.properties_)
      properties_ = std::make_unique<Properties>(*rhs.properties_);
    else
      properties_.reset();
  }
  return *this;
}

void Event::SetType(EventType type) {
  type_ = type;
}

////////////////////////////////////////////////////////////////////////////////
// CancelModeEvent

CancelModeEvent::CancelModeEvent()
    : Event(EventType::kCancelMode, base::TimeTicks(), 0) {
  set_cancelable(false);
}

CancelModeEvent::~CancelModeEvent() = default;

std::unique_ptr<Event> CancelModeEvent::Clone() const {
  return std::make_unique<CancelModeEvent>(*this);
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent

LocatedEvent::~LocatedEvent() = default;

LocatedEvent::LocatedEvent(const PlatformEvent& native_event)
    : Event(native_event,
            EventTypeFromNative(native_event),
            EventFlagsFromNative(native_event)),
      location_(EventLocationFromNative(native_event)),
      root_location_(location_) {}

LocatedEvent::LocatedEvent(EventType type,
                           const gfx::PointF& location,
                           const gfx::PointF& root_location,
                           base::TimeTicks time_stamp,
                           int flags)
    : Event(type, time_stamp, flags),
      location_(location),
      root_location_(root_location) {}

LocatedEvent::LocatedEvent(const LocatedEvent& copy) = default;

void LocatedEvent::UpdateForRootTransform(
    const gfx::Transform& reversed_root_transform,
    const gfx::Transform& reversed_local_transform) {
  if (target()) {
    location_ = reversed_local_transform.MapPoint(location_);
    root_location_ = reversed_root_transform.MapPoint(root_location_);
  } else {
    // This mirrors what the code previously did.
    location_ = reversed_root_transform.MapPoint(location_);
    root_location_ = location_;
  }
}

std::string LocatedEvent::ToString() const {
  return base::StrCat({Event::ToString(), " location=", location_.ToString(),
                       " root_location=", root_location_.ToString()});
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent

MouseEvent::MouseEvent(const PlatformEvent& native_event)
    : LocatedEvent(native_event),
      changed_button_flags_(GetChangedMouseButtonFlagsFromNative(native_event)),
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
      movement_(GetMouseMovementFromNative(native_event)),
#endif
      pointer_details_(GetMousePointerDetailsFromNative(native_event)) {
  latency()->AddLatencyNumberWithTimestamp(
      INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, time_stamp());
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT);
  InitializeNative();
}

MouseEvent::MouseEvent(EventType type,
                       const gfx::PointF& location,
                       const gfx::PointF& root_location,
                       base::TimeTicks time_stamp,
                       int flags,
                       int changed_button_flags,
                       const PointerDetails& pointer_details)
    : LocatedEvent(type, location, root_location, time_stamp, flags),
      changed_button_flags_(changed_button_flags),
      pointer_details_(pointer_details) {
  DCHECK_NE(EventType::kMousewheel, type);
  DCHECK_EQ(changed_button_flags_,
            changed_button_flags_ & kChangedButtonFlagMask);
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT);
  if (this->type() == EventType::kMouseMoved && IsAnyButton()) {
    SetType(EventType::kMouseDragged);
  }
}

MouseEvent::MouseEvent(EventType type,
                       const gfx::Point& location,
                       const gfx::Point& root_location,
                       base::TimeTicks time_stamp,
                       int flags,
                       int changed_button_flags,
                       const PointerDetails& pointer_details)
    : MouseEvent(type,
                 gfx::PointF(location),
                 gfx::PointF(root_location),
                 time_stamp,
                 flags,
                 changed_button_flags,
                 pointer_details) {}

MouseEvent::MouseEvent(const MouseEvent& other) = default;

MouseEvent::~MouseEvent() = default;

void MouseEvent::InitializeNative() {
  if (type() == EventType::kMousePressed ||
      type() == EventType::kMouseReleased) {
    SetClickCount(GetRepeatCount(*this));
  }
}

// static
bool MouseEvent::IsRepeatedClickEvent(const MouseEvent& event1,
                                      const MouseEvent& event2) {
  // These values match the Windows defaults.
  static const int kDoubleClickWidth = 4;
  static const int kDoubleClickHeight = 4;

  if (event1.type() != EventType::kMousePressed ||
      event2.type() != EventType::kMousePressed) {
    return false;
  }

  // Compare flags, but ignore EF_IS_DOUBLE_CLICK to allow triple clicks.
  if ((event1.flags() & ~EF_IS_DOUBLE_CLICK) !=
      (event2.flags() & ~EF_IS_DOUBLE_CLICK))
    return false;

  if (!IsRepeatedClickTimes(event1.time_stamp(), event2.time_stamp())) {
    return false;
  }

  if (std::abs(event2.x() - event1.x()) > kDoubleClickWidth / 2)
    return false;

  if (std::abs(event2.y() - event1.y()) > kDoubleClickHeight / 2)
    return false;

  return true;
}

// static
int MouseEvent::GetRepeatCount(const MouseEvent& event) {
  int click_count = 1;
  if (last_click_event_) {
    if (event.type() == EventType::kMouseReleased) {
      if (event.changed_button_flags() ==
          last_click_event_->changed_button_flags()) {
        return last_click_event_->GetClickCount();
      } else {
        // If last_click_event_ has changed since this button was pressed
        // return a click count of 1.
        return click_count;
      }
    }
    // Return the prior click count and do not update |last_click_event_| when
    // re-processing a native event, or when proccesing a reposted event.
    if (event.time_stamp() == last_click_event_->time_stamp())
      return last_click_event_->GetClickCount();
    if (IsRepeatedClickEvent(*last_click_event_, event))
      click_count = last_click_event_->GetClickCount() + 1;
    delete last_click_event_;
  }
  last_click_event_ = new MouseEvent(event);
  if (click_count > 3)
    click_count = 3;
  last_click_event_->SetClickCount(click_count);
  return click_count;
}

void MouseEvent::ResetLastClickForTest() {
  if (last_click_event_) {
    delete last_click_event_;
    last_click_event_ = nullptr;
  }
}

// static
MouseEvent* MouseEvent::last_click_event_ = nullptr;

int MouseEvent::GetClickCount() const {
  if (type() != EventType::kMousePressed &&
      type() != EventType::kMouseReleased) {
    return 0;
  }

  if (flags() & EF_IS_TRIPLE_CLICK)
    return 3;
  else if (flags() & EF_IS_DOUBLE_CLICK)
    return 2;
  else
    return 1;
}

void MouseEvent::SetClickCount(int click_count) {
  if (type() != EventType::kMousePressed &&
      type() != EventType::kMouseReleased) {
    return;
  }

  DCHECK_LT(0, click_count);
  DCHECK_GE(3, click_count);

  int f = flags();
  switch (click_count) {
    case 1:
      f &= ~EF_IS_DOUBLE_CLICK;
      f &= ~EF_IS_TRIPLE_CLICK;
      break;
    case 2:
      f |= EF_IS_DOUBLE_CLICK;
      f &= ~EF_IS_TRIPLE_CLICK;
      break;
    case 3:
      f &= ~EF_IS_DOUBLE_CLICK;
      f |= EF_IS_TRIPLE_CLICK;
      break;
  }
  SetFlags(f);
}

std::string MouseEvent::ToString() const {
  return base::StrCat({
      LocatedEvent::ToString(),
      " flags=",
      base::JoinString(base::make_span(MouseEventFlagsNames(flags())), "|"),
      base::StringPrintf("(0x%04x)", flags()),
  });
}

std::unique_ptr<Event> MouseEvent::Clone() const {
  return std::make_unique<MouseEvent>(*this);
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent

MouseWheelEvent::MouseWheelEvent(const PlatformEvent& native_event)
    : MouseEvent(native_event),
      offset_(GetMouseWheelOffset(native_event)),
      tick_120ths_(GetMouseWheelTick120ths(native_event)) {}

MouseWheelEvent::MouseWheelEvent(const ScrollEvent& scroll_event)
    : MouseEvent(scroll_event),
      offset_(base::ClampRound(scroll_event.x_offset()),
              base::ClampRound(scroll_event.y_offset())) {
  SetType(EventType::kMousewheel);
}

MouseWheelEvent::MouseWheelEvent(const MouseEvent& mouse_event,
                                 int x_offset,
                                 int y_offset)
    : MouseEvent(mouse_event), offset_(x_offset, y_offset) {
  SetType(EventType::kMousewheel);
}

MouseWheelEvent::MouseWheelEvent(const MouseWheelEvent& mouse_wheel_event)
    : MouseEvent(mouse_wheel_event),
      offset_(mouse_wheel_event.offset()),
      tick_120ths_(mouse_wheel_event.tick_120ths()) {
  DCHECK_EQ(EventType::kMousewheel, type());
}

MouseWheelEvent::MouseWheelEvent(const gfx::Vector2d& offset,
                                 const gfx::PointF& location,
                                 const gfx::PointF& root_location,
                                 base::TimeTicks time_stamp,
                                 int flags,
                                 int changed_button_flags,
                                 const std::optional<gfx::Vector2d> tick_120ths)
    : MouseEvent(EventType::kUnknown,
                 location,
                 root_location,
                 time_stamp,
                 flags,
                 changed_button_flags),
      offset_(offset) {
  // Set event type to EventType::kUnknown initially in MouseEvent() to pass the
  // DCHECK for type to enforce that we use MouseWheelEvent() to create
  // a MouseWheelEvent.
  SetType(EventType::kMousewheel);

  if (!tick_120ths) {
    // Since no wheel ticks have been specified, assume that scrolling is linear
    // (not accelerated, like it is on Chrome OS).
    tick_120ths_ =
        gfx::Vector2d(offset_.x() / MouseWheelEvent::kWheelDelta * 120,
                      offset_.y() / MouseWheelEvent::kWheelDelta * 120);
  } else {
    tick_120ths_ = tick_120ths.value();
  }
}

MouseWheelEvent::MouseWheelEvent(const gfx::Vector2d& offset,
                                 const gfx::Point& location,
                                 const gfx::Point& root_location,
                                 base::TimeTicks time_stamp,
                                 int flags,
                                 int changed_button_flags)
    : MouseWheelEvent(offset,
                      gfx::PointF(location),
                      gfx::PointF(root_location),
                      time_stamp,
                      flags,
                      changed_button_flags) {}

MouseWheelEvent::~MouseWheelEvent() = default;

std::unique_ptr<Event> MouseWheelEvent::Clone() const {
  return std::make_unique<MouseWheelEvent>(*this);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)
// This value matches Windows, Fuchsia WHEEL_DELTA, and (roughly) Firefox on
// Linux.
// static
const int MouseWheelEvent::kWheelDelta = 120;
#else
// This is a legacy value that matches GTK+ wheel scroll amount.  Although being
// inherited from Linux, it is no longer used on Linux itself, but is still used
// on some other platforms.
// See https://crbug.com/1270089 for the detailed reasoning.
// static
const int MouseWheelEvent::kWheelDelta = 53;
#endif

////////////////////////////////////////////////////////////////////////////////
// TouchEvent

TouchEvent::TouchEvent(const PlatformEvent& native_event)
    : LocatedEvent(native_event),
      unique_event_id_(ui::GetNextTouchEventId()),
      pointer_details_(GetTouchPointerDetailsFromNative(native_event)) {
  latency()->AddLatencyNumberWithTimestamp(
      INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, time_stamp());
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT);
}

TouchEvent::TouchEvent(EventType type,
                       const gfx::PointF& location,
                       const gfx::PointF& root_location,
                       base::TimeTicks time_stamp,
                       const PointerDetails& pointer_details,
                       int flags)
    : LocatedEvent(type, location, root_location, time_stamp, flags),
      unique_event_id_(ui::GetNextTouchEventId()),
      pointer_details_(pointer_details) {
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT);
}

TouchEvent::TouchEvent(EventType type,
                       const gfx::Point& location,
                       base::TimeTicks time_stamp,
                       const PointerDetails& pointer_details,
                       int flags)
    : TouchEvent(type,
                 gfx::PointF(location),
                 gfx::PointF(location),
                 time_stamp,
                 pointer_details,
                 flags) {}

TouchEvent::TouchEvent(const TouchEvent& copy)
    : LocatedEvent(copy),
      unique_event_id_(copy.unique_event_id_),
      may_cause_scrolling_(copy.may_cause_scrolling_),
      hovering_(copy.hovering_),
      pointer_details_(copy.pointer_details_) {
  // Copied events should not remove touch id mapping, as this either causes the
  // mapping to be lost before the initial event has finished dispatching, or
  // the copy to attempt to remove the mapping from a null |native_event_|.
}

TouchEvent::~TouchEvent() = default;

void TouchEvent::UpdateForRootTransform(
    const gfx::Transform& inverted_root_transform,
    const gfx::Transform& inverted_local_transform) {
  LocatedEvent::UpdateForRootTransform(inverted_root_transform,
                                       inverted_local_transform);

  // We could create a vector and then rely on Transform::MapVector, but
  // that ends up creating a 4 dimensional vector and applying a 4 dim
  // transform. Really what we're looking at is only in the (x,y) plane, and
  // given that we can run this relatively frequently we will inline execute the
  // matrix here.
  const double new_x =
      fabs(pointer_details_.radius_x * inverted_root_transform.rc(0, 0) +
           pointer_details_.radius_y * inverted_root_transform.rc(0, 1));
  const double new_y =
      fabs(pointer_details_.radius_x * inverted_root_transform.rc(1, 0) +
           pointer_details_.radius_y * inverted_root_transform.rc(1, 1));
  pointer_details_.radius_x = new_x;
  pointer_details_.radius_y = new_y;

  // for stylus touches, tilt needs to be rotated appropriately. We don't handle
  // screen rotations other than 0/90/180/270, but those should be handled and
  // translated appropriately. Other rotations leave tilts untouched for now. We
  // add a small check that tilt is set at all before looking through this
  // section.
  if (!IsNearZero(pointer_details_.tilt_x) ||
      !IsNearZero(pointer_details_.tilt_y)) {
    if (IsNearZero(inverted_root_transform.rc(0, 1)) &&
        IsNearZero(inverted_root_transform.rc(1, 0))) {
      pointer_details_.tilt_x *=
          std::copysign(1, inverted_root_transform.rc(0, 0));
      pointer_details_.tilt_y *=
          std::copysign(1, inverted_root_transform.rc(1, 1));
    } else if (IsNearZero(inverted_root_transform.rc(0, 0)) &&
               IsNearZero(inverted_root_transform.rc(1, 1))) {
      double new_tilt_x = pointer_details_.tilt_y *
                          std::copysign(1, inverted_root_transform.rc(0, 1));
      double new_tilt_y = pointer_details_.tilt_x *
                          std::copysign(1, inverted_root_transform.rc(1, 0));
      pointer_details_.tilt_x = new_tilt_x;
      pointer_details_.tilt_y = new_tilt_y;
    }
  }
}

void TouchEvent::DisableSynchronousHandling() {
  DispatcherApi dispatcher_api(this);
  dispatcher_api.set_result(
      static_cast<EventResult>(result() | ER_DISABLE_SYNC_HANDLING));
}

void TouchEvent::ForceProcessGesture() {
  DispatcherApi dispatcher_api(this);
  dispatcher_api.set_result(
      static_cast<EventResult>(result() | ER_FORCE_PROCESS_GESTURE));
}

void TouchEvent::SetPointerDetailsForTest(
    const PointerDetails& pointer_details) {
  DCHECK_EQ(pointer_details_.id, pointer_details.id);
  pointer_details_ = pointer_details;
}

float TouchEvent::ComputeRotationAngle() const {
  float rotation_angle = pointer_details_.twist;
  while (rotation_angle < 0)
    rotation_angle += 180.f;
  while (rotation_angle >= 180)
    rotation_angle -= 180.f;
  return rotation_angle;
}

std::unique_ptr<Event> TouchEvent::Clone() const {
  return std::make_unique<TouchEvent>(*this);
}

////////////////////////////////////////////////////////////////////////////////
// KeyEvent

// static
KeyEvent* KeyEvent::last_key_event_ = nullptr;
#if BUILDFLAG(IS_OZONE)
KeyEvent* KeyEvent::last_ibus_key_event_ = nullptr;
#endif

KeyEvent::KeyEvent(const PlatformEvent& native_event)
    : KeyEvent(native_event, EventFlagsFromNative(native_event)) {}

KeyEvent::KeyEvent(const PlatformEvent& native_event, int event_flags)
    : Event(native_event, EventTypeFromNative(native_event), event_flags),
      key_code_(KeyboardCodeFromNative(native_event)),
#if BUILDFLAG(IS_OZONE)
      scan_code_(ScanCodeFromNative(native_event)),
#endif  // BUILDFLAG(IS_OZONE)
      code_(CodeFromNative(native_event)),
      is_char_(IsCharFromNative(native_event)) {
#if BUILDFLAG(IS_OZONE)
  DCHECK(native_event->IsKeyEvent());
  key_ = native_event->AsKeyEvent()->key_;
#endif
  InitializeNative();
}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   int flags,
                   base::TimeTicks time_stamp)
    : Event(type, time_stamp, flags),
      key_code_(key_code),
      code_(UsLayoutKeyboardCodeToDomCode(key_code)) {}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   DomCode code,
                   int flags)
    : Event(type, EventTimeForNow(), flags), key_code_(key_code), code_(code) {}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   DomCode code,
                   int flags,
                   DomKey key,
                   base::TimeTicks time_stamp,
                   bool is_char)
    : Event(type, time_stamp, flags),
      key_code_(key_code),
      code_(code),
      is_char_(is_char),
      key_(key) {}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   DomCode code,
                   int flags,
                   base::TimeTicks time_stamp)
    : Event(type, time_stamp, flags), key_code_(key_code), code_(code) {}

KeyEvent::KeyEvent(const KeyEvent& rhs)
    : Event(rhs),
      key_code_(rhs.key_code_),
#if BUILDFLAG(IS_OZONE)
      scan_code_(rhs.scan_code_),
#endif  // BUILDFLAG(IS_OZONE)
      code_(rhs.code_),
      is_char_(rhs.is_char_),
      key_(rhs.key_) {
}

KeyEvent& KeyEvent::operator=(const KeyEvent& rhs) {
  if (this != &rhs) {
    Event::operator=(rhs);
    key_code_ = rhs.key_code_;
#if BUILDFLAG(IS_OZONE)
    scan_code_ = rhs.scan_code_;
#endif  // BUILDFLAG(IS_OZONE)
    code_ = rhs.code_;
    key_ = rhs.key_;
    is_char_ = rhs.is_char_;
  }
  return *this;
}

KeyEvent::~KeyEvent() = default;

// static
// For compatibility, this is enabled by default.
bool KeyEvent::synthesize_key_repeat_enabled_ = true;

// static
bool KeyEvent::IsSynthesizeKeyRepeatEnabled() {
  return synthesize_key_repeat_enabled_;
}

// static
void KeyEvent::SetSynthesizeKeyRepeatEnabled(bool enabled) {
  synthesize_key_repeat_enabled_ = enabled;
}

KeyEvent KeyEvent::FromCharacter(char16_t character,
                                 KeyboardCode key_code,
                                 DomCode code,
                                 int flags,
                                 base::TimeTicks time_stamp) {
  return KeyEvent(EventType::kKeyPressed, key_code, code, flags,
                  DomKey::FromCharacter(character), time_stamp, true);
}

void KeyEvent::InitializeNative() {
  latency()->AddLatencyNumberWithTimestamp(
      INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, time_stamp());
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT);

  // Check if this is a key repeat. This must be called before initial flags
  // processing, e.g: NormalizeFlags(), to avoid issues like crbug.com/1069690.
  if (synthesize_key_repeat_enabled_ && IsRepeated(GetLastKeyEvent()))
    SetFlags(flags() | EF_IS_REPEAT);

#if BUILDFLAG(IS_LINUX)
  NormalizeFlags();
#elif BUILDFLAG(IS_WIN)
  // Only Windows has native character events.
  if (is_char_) {
    key_ = DomKey::FromCharacter(static_cast<int32_t>(native_event().wParam));
    SetFlags(PlatformKeyMap::ReplaceControlAndAltWithAltGraph(flags()));
  } else {
    int adjusted_flags = flags();
    key_ = PlatformKeyMap::DomKeyFromKeyboardCode(key_code(), &adjusted_flags);
    SetFlags(adjusted_flags);
  }
#endif
}

void KeyEvent::ApplyLayout() const {
  DomCode code = code_;
  if (code == DomCode::NONE) {
    // Catch old code that tries to do layout without a physical key, and try
    // to recover using the KeyboardCode. Once key events are fully defined
    // on construction (see TODO in event.h) this will go away.
    VLOG(2) << "DomCode::NONE keycode=" << key_code_;
    code = UsLayoutKeyboardCodeToDomCode(key_code_);
    if (code == DomCode::NONE) {
      key_ = DomKey::UNIDENTIFIED;
      return;
    }
  }

  if (key_ != DomKey::NONE)
    return;

  KeyboardCode dummy_key_code;
#if BUILDFLAG(IS_OZONE)
  if (KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()->Lookup(
          code, flags(), &key_, &dummy_key_code)) {
    return;
  }
#endif

#if !BUILDFLAG(IS_WIN)
  // Native Windows character events always have is_char_ == true,
  // so this is a synthetic or native keystroke event.
  // Therefore, perform only the fallback action.
  if (IsPlatformEventValid(native_event())) {
    DCHECK(EventTypeFromNative(native_event()) == EventType::kKeyPressed ||
           EventTypeFromNative(native_event()) == EventType::kKeyReleased);
  }
#endif

  if (!DomCodeToUsLayoutDomKey(code, flags(), &key_, &dummy_key_code))
    key_ = DomKey::UNIDENTIFIED;
}

bool KeyEvent::IsRepeated(KeyEvent** last_key_event) {
  DCHECK(last_key_event);

  // A safe guard in case if there were continuous key pressed events that are
  // not auto repeat.
  const int kMaxAutoRepeatTimeMs = 2000;

  if (is_char())
    return false;
  if (type() == EventType::kKeyReleased) {
    delete *last_key_event;
    *last_key_event = nullptr;
    return false;
  }

  CHECK_EQ(EventType::kKeyPressed, type());
  KeyEvent* last = *last_key_event;

  if (!last) {
    *last_key_event = new KeyEvent(*this);
    return false;
  } else if (time_stamp() == last->time_stamp()) {
    // The KeyEvent is created from the same native event.
    return (last->flags() & EF_IS_REPEAT) != 0;
  }

  DCHECK(last);
  bool is_repeat = false;

#if BUILDFLAG(IS_WIN)
  if (HasNativeEvent()) {
    // Bit 30 of lParam represents the "previous key state". If set, the key
    // was already down, therefore this is an auto-repeat.
    is_repeat = (native_event().lParam & 0x40000000) != 0;
  }
#endif
  if (!is_repeat) {
    if (key_code() == last->key_code() &&
        (flags() & ~EF_MOUSE_BUTTON) ==
            (last->flags() & ~EF_IS_REPEAT & ~EF_MOUSE_BUTTON) &&
        (time_stamp() - last->time_stamp()).InMilliseconds() <
            kMaxAutoRepeatTimeMs) {
      is_repeat = true;
    }
  }

  if (is_repeat) {
    last->set_time_stamp(time_stamp());
    last->SetFlags(last->flags() | EF_IS_REPEAT);
    return true;
  }

  delete *last_key_event;
  *last_key_event = new KeyEvent(*this);

  return false;
}

KeyEvent** KeyEvent::GetLastKeyEvent() {
#if BUILDFLAG(IS_OZONE)
  // Use a different static variable for key events that have non standard
  // state masks as it may be reposted by an IME. IBUS-GTK and fcitx-GTK uses
  // this field to detect the re-posted event for example. crbug.com/385873.
  return properties() && properties()->contains(kPropertyKeyboardImeFlag)
             ? &last_ibus_key_event_
             : &last_key_event_;
#else
  return &last_key_event_;
#endif
}

DomKey KeyEvent::GetDomKey() const {
  // Determination of key_ may be done lazily.
  if (key_ == DomKey::NONE)
    ApplyLayout();
  return key_;
}

void KeyEvent::OnFlagsUpdated() {
  // TODO(https://crbug.com/324462727): this is problematic on windows.
#if BUILDFLAG(IS_CHROMEOS)
  key_ = DomKey::NONE;
#endif
}

char16_t KeyEvent::GetCharacter() const {
  // Determination of key_ may be done lazily.
  if (key_ == DomKey::NONE)
    ApplyLayout();
  if (key_.IsCharacter()) {
    // Historically ui::KeyEvent has held only BMP characters.
    // Until this explicitly changes, require |key_| to hold a BMP character.
    DomKey::Base utf32_character = key_.ToCharacter();
    char16_t ucs2_character = static_cast<char16_t>(utf32_character);
    DCHECK_EQ(static_cast<DomKey::Base>(ucs2_character), utf32_character);
    // Check if the control character is down. Note that ALTGR is represented
    // on Windows as CTRL|ALT, so we need to make sure that is not set.
    if ((flags() & (EF_ALTGR_DOWN | EF_CONTROL_DOWN)) == EF_CONTROL_DOWN) {
      // For a control character, key_ contains the corresponding printable
      // character. To preserve existing behaviour for now, return the control
      // character here; this will likely change -- see e.g. crbug.com/471488.
      if (ucs2_character >= 0x20 && ucs2_character <= 0x7E)
        return ucs2_character & 0x1F;
      if (ucs2_character == '\r')
        return '\n';
    }
    return ucs2_character;
  }
  return 0;
}

char16_t KeyEvent::GetText() const {
  if ((flags() & EF_CONTROL_DOWN) != 0) {
    DomKey key;
    KeyboardCode key_code;
    if (DomCodeToControlCharacter(code_, flags(), &key, &key_code))
      return key.ToCharacter();
  }
  return GetUnmodifiedText();
}

char16_t KeyEvent::GetUnmodifiedText() const {
  if (!is_char_ && (key_code_ == VKEY_RETURN))
    return '\r';
  return GetCharacter();
}

bool KeyEvent::IsUnicodeKeyCode() const {
#if BUILDFLAG(IS_WIN)
  if (!IsAltDown())
    return false;
  const int key = key_code();
  if (key >= VKEY_NUMPAD0 && key <= VKEY_NUMPAD9)
    return true;
  // Check whether the user is using the numeric keypad with num-lock off.
  // In that case, EF_EXTENDED will not be set; if it is set, the key event
  // originated from the relevant non-numpad dedicated key, e.g. [Insert].
  return (!(flags() & EF_IS_EXTENDED_KEY) &&
          (key == VKEY_INSERT || key == VKEY_END || key == VKEY_DOWN ||
           key == VKEY_NEXT || key == VKEY_LEFT || key == VKEY_CLEAR ||
           key == VKEY_RIGHT || key == VKEY_HOME || key == VKEY_UP ||
           key == VKEY_PRIOR));
#else
  return false;
#endif
}

void KeyEvent::NormalizeFlags() {
  int mask = 0;
  switch (key_code()) {
    case VKEY_CONTROL:
      mask = EF_CONTROL_DOWN;
      break;
    case VKEY_SHIFT:
      mask = EF_SHIFT_DOWN;
      break;
    case VKEY_MENU:
      mask = EF_ALT_DOWN;
      break;
    default:
      return;
  }
  if (type() == EventType::kKeyPressed) {
    SetFlags(flags() | mask);
  } else {
    SetFlags(flags() & ~mask);
  }
}

std::string KeyEvent::ToString() const {
  auto dom_key = GetDomKey();
  return base::StrCat({
      Event::ToString(),
      " code=",
      KeycodeConverter::DomCodeToCodeString(code()),
      base::StringPrintf("(0x%04x)", static_cast<uint32_t>(code_)),
      " key=",
      KeycodeConverter::DomKeyToKeyString(dom_key),
      base::StringPrintf("(0x%04x)", static_cast<uint32_t>(dom_key)),
      " keycode=",
      base::StringPrintf("(0x%04x)", key_code_),
#if BUILDFLAG(IS_OZONE)
      " scan_code=",
      base::StringPrintf("(0x%04x)", scan_code_),
#endif  // BUILDFLAG(IS_OZONE)
      " flags=",
      base::JoinString(base::make_span(KeyEventFlagsNames(flags())), "|"),
      base::StringPrintf("(0x%04x)", flags()),
  });
}

std::unique_ptr<Event> KeyEvent::Clone() const {
  return std::make_unique<KeyEvent>(*this);
}

KeyboardCode KeyEvent::GetLocatedWindowsKeyboardCode() const {
  return NonLocatedToLocatedKeyboardCode(key_code_, code_);
}

uint16_t KeyEvent::GetConflatedWindowsKeyCode() const {
  if (is_char_)
    return key_.ToCharacter();
  return key_code_;
}

std::string KeyEvent::GetCodeString() const {
  return KeycodeConverter::DomCodeToCodeString(code_);
}

////////////////////////////////////////////////////////////////////////////////
// ScrollEvent

ScrollEvent::ScrollEvent(const PlatformEvent& native_event)
    : MouseEvent(native_event),
      x_offset_(0.0f),
      y_offset_(0.0f),
      x_offset_ordinal_(0.0f),
      y_offset_ordinal_(0.0f),
      finger_count_(0),
      momentum_phase_(EventMomentumPhase::NONE),
      scroll_event_phase_(ScrollEventPhase::kNone) {
  // TODO(bokan): This should be populating the |scroll_event_phase_| member but
  // currently isn't.
  if (type() == EventType::kScroll) {
    GetScrollOffsets(native_event, &x_offset_, &y_offset_, &x_offset_ordinal_,
                     &y_offset_ordinal_, &finger_count_, &momentum_phase_);
  } else if (type() == EventType::kScrollFlingStart ||
             type() == EventType::kScrollFlingCancel) {
    GetFlingData(native_event, &x_offset_, &y_offset_, &x_offset_ordinal_,
                 &y_offset_ordinal_, nullptr);
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Unexpected event type " << base::to_underlying(type())
        << " when constructing a ScrollEvent.";
  }
}

ScrollEvent::ScrollEvent(EventType type,
                         const gfx::PointF& location,
                         const gfx::PointF& root_location,
                         base::TimeTicks time_stamp,
                         int flags,
                         float x_offset,
                         float y_offset,
                         float x_offset_ordinal,
                         float y_offset_ordinal,
                         int finger_count,
                         EventMomentumPhase momentum_phase,
                         ScrollEventPhase scroll_event_phase)
    : MouseEvent(type, location, root_location, time_stamp, flags, 0),
      x_offset_(x_offset),
      y_offset_(y_offset),
      x_offset_ordinal_(x_offset_ordinal),
      y_offset_ordinal_(y_offset_ordinal),
      finger_count_(finger_count),
      momentum_phase_(momentum_phase),
      scroll_event_phase_(scroll_event_phase) {
  CHECK(IsScrollEvent());
}

ScrollEvent::ScrollEvent(EventType type,
                         const gfx::Point& location,
                         base::TimeTicks time_stamp,
                         int flags,
                         float x_offset,
                         float y_offset,
                         float x_offset_ordinal,
                         float y_offset_ordinal,
                         int finger_count,
                         EventMomentumPhase momentum_phase,
                         ScrollEventPhase scroll_event_phase)
    : ScrollEvent(type,
                  gfx::PointF(location),
                  gfx::PointF(location),
                  time_stamp,
                  flags,
                  x_offset,
                  y_offset,
                  x_offset_ordinal,
                  y_offset_ordinal,
                  finger_count,
                  momentum_phase,
                  scroll_event_phase) {}

ScrollEvent::ScrollEvent(const ScrollEvent& other) = default;

ScrollEvent::~ScrollEvent() = default;

void ScrollEvent::Scale(const float factor) {
  x_offset_ *= factor;
  y_offset_ *= factor;
  x_offset_ordinal_ *= factor;
  y_offset_ordinal_ *= factor;
}

std::string ScrollEvent::ToString() const {
  return base::StrCat({
      MouseEvent::ToString(),
      base::StringPrintf(" offset=%g,%g", x_offset_, y_offset_),
      base::StringPrintf(" offset_ordinal=%g,%g", x_offset_ordinal_,
                         y_offset_ordinal_),
      " momentum_phase=",
      MomentumPhaseToString(momentum_phase_),
      " event_phase=",
      ScrollEventPhaseToString(scroll_event_phase_),
  });
}

std::unique_ptr<Event> ScrollEvent::Clone() const {
  return std::make_unique<ScrollEvent>(*this);
}

////////////////////////////////////////////////////////////////////////////////
// GestureEvent

GestureEvent::GestureEvent(float x,
                           float y,
                           int flags,
                           base::TimeTicks time_stamp,
                           const GestureEventDetails& details,
                           uint32_t unique_touch_event_id)
    : LocatedEvent(details.type(),
                   gfx::PointF(x, y),
                   gfx::PointF(x, y),
                   time_stamp,
                   flags | EF_FROM_TOUCH),
      details_(details),
      unique_touch_event_id_(unique_touch_event_id) {
}

GestureEvent::GestureEvent(const GestureEvent& other) = default;

GestureEvent::~GestureEvent() = default;

std::string GestureEvent::ToString() const {
  return base::StrCat({
      LocatedEvent::ToString(),
      " touch_event_id=",
      base::NumberToString(unique_touch_event_id_),
  });
}

std::unique_ptr<Event> GestureEvent::Clone() const {
  return std::make_unique<GestureEvent>(*this);
}

}  // namespace ui
