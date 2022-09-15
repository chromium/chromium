// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_event_source.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/cxx20_erase.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_event_watcher.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

namespace ui {

namespace {

bool HasAnyPointerButtonFlag(int flags) {
  return (flags & (EF_LEFT_MOUSE_BUTTON | EF_MIDDLE_MOUSE_BUTTON |
                   EF_RIGHT_MOUSE_BUTTON | EF_BACK_MOUSE_BUTTON |
                   EF_FORWARD_MOUSE_BUTTON)) != 0;
}

std::vector<uint8_t> ToLittleEndianByteVector(uint32_t value) {
  return {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
          static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)};
}

EventTarget* GetRootTarget(EventTarget* target) {
  EventTarget* parent = target->GetParentTarget();
  return parent ? GetRootTarget(parent) : target;
}

gfx::Point GetOriginInScreen(WaylandWindow* target) {
  // The origin for located events and positions of popup windows is the window
  // geometry.
  // See https://crbug.com/1292486
  gfx::Point origin = target->GetBoundsInDIP().origin() -
                      target->GetWindowGeometryOffsetInDIP();
  auto* parent = static_cast<WaylandWindow*>(target->GetParentTarget());
  while (parent) {
    origin += parent->GetBoundsInDIP().origin().OffsetFromOrigin();
    parent = static_cast<WaylandWindow*>(parent->GetParentTarget());
  }
  return origin;
}

gfx::Point GetLocationInScreen(LocatedEvent* event) {
  auto* root_window =
      static_cast<WaylandWindow*>(GetRootTarget(event->target()));
  return event->root_location() +
         root_window->GetBoundsInDIP().origin().OffsetFromOrigin();
}

void SetRootLocation(LocatedEvent* event) {
  gfx::PointF location = event->location_f();
  auto* target = static_cast<WaylandWindow*>(event->target());

  while (target->GetParentTarget()) {
    location += target->GetBoundsInDIP().origin().OffsetFromOrigin();
    target = static_cast<WaylandWindow*>(target->GetParentTarget());
  }
  event->set_root_location_f(location);
}

// Number of fingers for scroll gestures.
constexpr int kGestureScrollFingerCount = 2;

// Maximum size of the latest pointer scroll data set to be stored.
constexpr int kPointerScrollDataSetMaxSize = 20;

}  // namespace

struct WaylandEventSource::TouchPoint {
  TouchPoint(gfx::PointF location, WaylandWindow* current_window);
  ~TouchPoint() = default;

  raw_ptr<WaylandWindow> window;
  gfx::PointF last_known_location;
};

WaylandEventSource::TouchPoint::TouchPoint(gfx::PointF location,
                                           WaylandWindow* current_window)
    : window(current_window), last_known_location(location) {
  DCHECK(window);
}

// WaylandEventSource::PointerScrollData implementation
WaylandEventSource::PointerScrollData::PointerScrollData() = default;
WaylandEventSource::PointerScrollData::PointerScrollData(
    const PointerScrollData&) = default;
WaylandEventSource::PointerScrollData::PointerScrollData(PointerScrollData&&) =
    default;
WaylandEventSource::PointerScrollData::~PointerScrollData() = default;

WaylandEventSource::PointerScrollData&
WaylandEventSource::PointerScrollData::operator=(const PointerScrollData&) =
    default;
WaylandEventSource::PointerScrollData&
WaylandEventSource::PointerScrollData::operator=(PointerScrollData&&) = default;

// WaylandEventSource::FrameData implementation
WaylandEventSource::FrameData::FrameData(const Event& e,
                                         base::OnceCallback<void()> cb)
    : event(e.Clone()), completion_cb(std::move(cb)) {}

WaylandEventSource::FrameData::~FrameData() = default;

// WaylandEventSource implementation

// static
void WaylandEventSource::ConvertEventToTarget(const EventTarget* new_target,
                                              LocatedEvent* event) {
  auto* current_target = static_cast<WaylandWindow*>(event->target());
  gfx::Vector2d diff = GetOriginInScreen(current_target) -
                       GetOriginInScreen(static_cast<WaylandWindow*>(
                           const_cast<EventTarget*>(new_target)));
  event->set_location_f(event->location_f() + diff);
}

WaylandEventSource::WaylandEventSource(wl_display* display,
                                       wl_event_queue* event_queue,
                                       WaylandWindowManager* window_manager,
                                       WaylandConnection* connection)
    : window_manager_(window_manager),
      connection_(connection),
      event_watcher_(
          WaylandEventWatcher::CreateWaylandEventWatcher(display,
                                                         event_queue)) {
  DCHECK(window_manager_);

  // Observes remove changes to know when touch points can be removed.
  window_manager_->AddObserver(this);
}

WaylandEventSource::~WaylandEventSource() = default;

void WaylandEventSource::SetShutdownCb(base::OnceCallback<void()> shutdown_cb) {
  event_watcher_->SetShutdownCb(std::move(shutdown_cb));
}

void WaylandEventSource::StartProcessingEvents() {
  event_watcher_->StartProcessingEvents();
}

void WaylandEventSource::OnKeyboardFocusChanged(WaylandWindow* window,
                                                bool focused) {
  DCHECK(window);
#if DCHECK_IS_ON()
  if (!focused)
    DCHECK_EQ(window, window_manager_->GetCurrentKeyboardFocusedWindow());
#endif
  window_manager_->SetKeyboardFocusedWindow(focused ? window : nullptr);
}

void WaylandEventSource::OnKeyboardModifiersChanged(int modifiers) {
  keyboard_modifiers_ = modifiers;
}

uint32_t WaylandEventSource::OnKeyboardKeyEvent(
    EventType type,
    DomCode dom_code,
    bool repeat,
    absl::optional<uint32_t> serial,
    base::TimeTicks timestamp,
    int device_id,
    WaylandKeyboard::KeyEventKind kind) {
  DCHECK(type == ET_KEY_PRESSED || type == ET_KEY_RELEASED);

  DomKey dom_key;
  KeyboardCode key_code;
  auto* layout_engine = KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  if (!layout_engine || !layout_engine->Lookup(dom_code, keyboard_modifiers_,
                                               &dom_key, &key_code)) {
    LOG(ERROR) << "Failed to decode key event.";
    return POST_DISPATCH_NONE;
  }

#if BUILDFLAG(USE_GTK)
  // GTK expects the state of a key event to be the mask of modifier keys
  // _prior_ to this event. Some IMEs rely on this behavior. See
  // https://crbug.com/1086946#c11.
  int state_before_event = keyboard_modifiers_;
#endif

  KeyEvent event(type, key_code, dom_code,
                 keyboard_modifiers_ | (repeat ? EF_IS_REPEAT : 0), dom_key,
                 timestamp);
  event.set_source_device_id(device_id);

  auto* focus = window_manager_->GetCurrentKeyboardFocusedWindow();
  if (!focus)
    return POST_DISPATCH_STOP_PROPAGATION;

  Event::DispatcherApi(&event).set_target(focus);

  Event::Properties properties;
#if BUILDFLAG(USE_GTK)
  // GTK uses XKB keycodes.
  uint32_t converted_key_code =
      ui::KeycodeConverter::DomCodeToXkbKeycode(dom_code);
  properties.emplace(
      kPropertyKeyboardHwKeyCode,
      std::vector<uint8_t>{static_cast<unsigned char>(converted_key_code)});
  // Save state before event. The flags have different values than what GTK
  // expects, but GtkUiPlatformWayland::GetGdkKeyEventState() takes care of the
  // conversion.
  properties.emplace(kPropertyKeyboardState,
                     ToLittleEndianByteVector(state_before_event));
#endif

  if (serial.has_value()) {
    properties.emplace(WaylandKeyboard::kPropertyWaylandSerial,
                       ToLittleEndianByteVector(serial.value()));
  }

  if (kind == WaylandKeyboard::KeyEventKind::kKey) {
    // Mark that this is the key event which IME did not consume.
    properties.emplace(kPropertyKeyboardImeFlag,
                       std::vector<uint8_t>{kPropertyKeyboardImeIgnoredFlag});
  }
  event.SetProperties(properties);
  return DispatchEvent(&event);
}

void WaylandEventSource::OnPointerFocusChanged(
    WaylandWindow* window,
    const gfx::PointF& location,
    wl::EventDispatchPolicy dispatch_policy) {
  bool focused = !!window;
  if (focused) {
    // Save new pointer location.
    pointer_location_ = location;
    window_manager_->SetPointerFocusedWindow(window);
  }

  auto closure = focused ? base::NullCallback()
                         : base::BindOnce(
                               [](WaylandWindowManager* wwm) {
                                 wwm->SetPointerFocusedWindow(nullptr);
                               },
                               window_manager_);

  auto* target = window_manager_->GetCurrentPointerFocusedWindow();
  if (target) {
    EventType type = focused ? ET_MOUSE_ENTERED : ET_MOUSE_EXITED;
    MouseEvent event(type, pointer_location_, pointer_location_,
                     EventTimeForNow(), pointer_flags_, 0);
    if (dispatch_policy == wl::EventDispatchPolicy::kImmediate) {
      SetTargetAndDispatchEvent(&event, target);
    } else {
      pointer_frames_.push_back(
          std::make_unique<FrameData>(event, std::move(closure)));
      return;
    }
  }

  if (!closure.is_null())
    std::move(closure).Run();
}

void WaylandEventSource::OnPointerButtonEvent(
    EventType type,
    int changed_button,
    WaylandWindow* window,
    wl::EventDispatchPolicy dispatch_policy) {
  DCHECK(type == ET_MOUSE_PRESSED || type == ET_MOUSE_RELEASED);
  DCHECK(HasAnyPointerButtonFlag(changed_button));

  WaylandWindow* prev_focused_window =
      window_manager_->GetCurrentPointerFocusedWindow();
  if (window)
    window_manager_->SetPointerFocusedWindow(window);

  auto closure = base::BindOnce(
      &WaylandEventSource::OnPointerButtonEventInternal, base::Unretained(this),
      (window ? prev_focused_window : nullptr), type);

  pointer_flags_ = type == ET_MOUSE_PRESSED
                       ? (pointer_flags_ | changed_button)
                       : (pointer_flags_ & ~changed_button);
  last_pointer_button_pressed_ = changed_button;

  auto* target = window_manager_->GetCurrentPointerFocusedWindow();
  // A window may be deleted when the event arrived from the server.
  if (target) {
    // MouseEvent's flags should contain the button that was released too.
    int flags = pointer_flags_ | keyboard_modifiers_ | changed_button;
    MouseEvent event(type, pointer_location_, pointer_location_,
                     EventTimeForNow(), flags, changed_button);
    if (dispatch_policy == wl::EventDispatchPolicy::kImmediate) {
      SetTargetAndDispatchEvent(&event, target);
    } else {
      pointer_frames_.push_back(
          std::make_unique<FrameData>(event, std::move(closure)));
      return;
    }
  }

  if (!closure.is_null())
    std::move(closure).Run();
}

void WaylandEventSource::OnPointerButtonEventInternal(WaylandWindow* window,
                                                      EventType type) {
  if (window)
    window_manager_->SetPointerFocusedWindow(window);

  if (type == ET_MOUSE_RELEASED)
    last_pointer_stylus_tool_.reset();
}

void WaylandEventSource::OnPointerMotionEvent(
    const gfx::PointF& location,
    wl::EventDispatchPolicy dispatch_policy) {
  pointer_location_ = location;

  int flags = pointer_flags_ | keyboard_modifiers_;
  MouseEvent event(ET_MOUSE_MOVED, pointer_location_, pointer_location_,
                   EventTimeForNow(), flags, 0);
  auto* target = window_manager_->GetCurrentPointerFocusedWindow();

  // A window may be deleted when the event arrived from the server.
  if (!target)
    return;

  if (dispatch_policy == wl::EventDispatchPolicy::kImmediate) {
    SetTargetAndDispatchEvent(&event, target);
  } else {
    pointer_frames_.push_back(
        std::make_unique<FrameData>(event, base::NullCallback()));
  }
}

void WaylandEventSource::OnPointerAxisEvent(const gfx::Vector2dF& offset) {
  EnsurePointerScrollData().dx += offset.x();
  EnsurePointerScrollData().dy += offset.y();
}

void WaylandEventSource::OnResetPointerFlags() {
  ResetPointerFlags();
}

void WaylandEventSource::RoundTripQueue() {
  event_watcher_->RoundTripQueue();
}

const gfx::PointF& WaylandEventSource::GetPointerLocation() const {
  return pointer_location_;
}

void WaylandEventSource::OnPointerFrameEvent() {
  base::TimeTicks now = EventTimeForNow();
  if (pointer_scroll_data_) {
    pointer_scroll_data_->dt = now - last_pointer_frame_time_;
    ProcessPointerScrollData();
  }

  last_pointer_frame_time_ = now;

  auto* target = window_manager_->GetCurrentPointerFocusedWindow();
  if (!target)
    return;

  while (!pointer_frames_.empty()) {
    // It is safe to pop the first queued event for processing.
    auto pointer_frame = std::move(pointer_frames_.front());
    pointer_frames_.pop_front();

    // In case there are pointer stylus information, override the current
    // 'event' instance, given that PointerDetails is 'const'.
    auto pointer_details_with_stylus_data = AmendStylusData();
    if (pointer_details_with_stylus_data &&
        pointer_frame->event->IsMouseEvent() &&
        pointer_frame->event->AsMouseEvent()->IsOnlyLeftMouseButton()) {
      auto old_event = std::move(pointer_frame->event);
      pointer_frame->event = std::make_unique<MouseEvent>(
          old_event->type(), old_event->AsMouseEvent()->location(),
          old_event->AsMouseEvent()->root_location(), old_event->time_stamp(),
          old_event->flags(), old_event->AsMouseEvent()->changed_button_flags(),
          pointer_details_with_stylus_data.value());
    }

    SetTargetAndDispatchEvent(pointer_frame->event.get(), target);
    if (!pointer_frame->completion_cb.is_null())
      std::move(pointer_frame->completion_cb).Run();
  }
}

void WaylandEventSource::OnPointerAxisSourceEvent(uint32_t axis_source) {
  EnsurePointerScrollData().axis_source = axis_source;
}

void WaylandEventSource::OnPointerAxisStopEvent(uint32_t axis) {
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    EnsurePointerScrollData().dy = 0;
  } else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
    EnsurePointerScrollData().dx = 0;
  }
  EnsurePointerScrollData().is_axis_stop = true;
}

void WaylandEventSource::OnTouchPressEvent(
    WaylandWindow* window,
    const gfx::PointF& location,
    base::TimeTicks timestamp,
    PointerId id,
    wl::EventDispatchPolicy dispatch_policy) {
  DCHECK(window);
  HandleTouchFocusChange(window, true);

  // Make sure this touch point wasn't present before.
  auto success = touch_points_.try_emplace(
      id, std::make_unique<TouchPoint>(location, window));
  if (!success.second) {
    LOG(WARNING) << "Touch down fired with wrong id";
    return;
  }

  PointerDetails details(EventPointerType::kTouch, id);
  TouchEvent event(ET_TOUCH_PRESSED, location, location, timestamp, details,
                   keyboard_modifiers_);
  DCHECK_EQ(dispatch_policy, wl::EventDispatchPolicy::kOnFrame);
  touch_frames_.push_back(
      std::make_unique<FrameData>(event, base::NullCallback()));
}

void WaylandEventSource::OnTouchReleaseEvent(
    base::TimeTicks timestamp,
    PointerId id,
    wl::EventDispatchPolicy dispatch_policy) {
  // Make sure this touch point was present before.
  const auto it = touch_points_.find(id);
  if (it == touch_points_.end()) {
    LOG(WARNING) << "Touch up fired with no matching touch down";
    return;
  }

  TouchPoint* touch_point = it->second.get();
  gfx::PointF location = touch_point->last_known_location;
  PointerDetails details(EventPointerType::kTouch, id);

  TouchEvent event(ET_TOUCH_RELEASED, location, location, timestamp, details,
                   keyboard_modifiers_);
  if (dispatch_policy == wl::EventDispatchPolicy::kImmediate) {
    SetTouchTargetAndDispatchTouchEvent(&event);
    OnTouchReleaseInternal(id);
  } else {
    touch_frames_.push_back(std::make_unique<FrameData>(
        event, base::BindOnce(&WaylandEventSource::OnTouchReleaseInternal,
                              base::Unretained(this), id)));
  }
}

void WaylandEventSource::OnTouchReleaseInternal(PointerId id) {
  // It is possible that an user interaction triggers nested loops
  // in higher levels of the application stack in order to process a
  // given touch down/up action.
  // For instance, a modal dialog might block this execution point,
  // and trigger thread to continue to process events.
  // The auxiliary flow might clear entries in touch_points_.
  //
  // Hence, we check whether the TouchId is still being held.
  const auto it = touch_points_.find(id);
  if (it == touch_points_.end()) {
    LOG(WARNING) << "Touch has been released during processing.";
    return;
  }

  TouchPoint* touch_point = it->second.get();
  HandleTouchFocusChange(touch_point->window, false, id);
  touch_points_.erase(it);

  // Clean up stylus touch tracking, if any.
  const auto stylus_data_it = last_touch_stylus_data_.find(id);
  if (stylus_data_it != last_touch_stylus_data_.end())
    last_touch_stylus_data_.erase(stylus_data_it);
}

void WaylandEventSource::SetTargetAndDispatchEvent(Event* event,
                                                   EventTarget* target) {
  Event::DispatcherApi(event).set_target(target);
  if (event->IsLocatedEvent()) {
    SetRootLocation(event->AsLocatedEvent());
    auto* cursor_position = connection_->wayland_cursor_position();
    if (cursor_position) {
      cursor_position->OnCursorPositionChanged(
          GetLocationInScreen(event->AsLocatedEvent()));
    }
  }
  DispatchEvent(event);
}

void WaylandEventSource::SetTouchTargetAndDispatchTouchEvent(
    TouchEvent* event) {
  auto iter = touch_points_.find(event->pointer_details().id);
  auto target = iter != touch_points_.end() ? iter->second->window : nullptr;
  // Skip if the touch target has alrady been removed.
  if (!target.get())
    return;
  SetTargetAndDispatchEvent(event, target.get());
}

void WaylandEventSource::OnTouchMotionEvent(
    const gfx::PointF& location,
    base::TimeTicks timestamp,
    PointerId id,
    wl::EventDispatchPolicy dispatch_policy) {
  const auto it = touch_points_.find(id);
  // Make sure this touch point was present before.
  if (it == touch_points_.end()) {
    LOG(WARNING) << "Touch event fired with wrong id";
    return;
  }
  it->second->last_known_location = location;
  PointerDetails details(EventPointerType::kTouch, id);
  TouchEvent event(ET_TOUCH_MOVED, location, location, timestamp, details,
                   keyboard_modifiers_);
  if (dispatch_policy == wl::EventDispatchPolicy::kImmediate) {
    SetTouchTargetAndDispatchTouchEvent(&event);
  } else {
    touch_frames_.push_back(
        std::make_unique<FrameData>(event, base::NullCallback()));
  }
}

void WaylandEventSource::OnTouchCancelEvent() {
  // Some compositors emit a TouchCancel event when a drag'n drop
  // session is started on the server, eg Exo.
  // On Chrome, this event would actually abort the whole drag'n drop
  // session on the client side.
  if (connection_->IsDragInProgress())
    return;

  gfx::PointF location;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  for (auto& touch_point : touch_points_) {
    PointerId id = touch_point.first;
    TouchEvent event(ET_TOUCH_CANCELLED, location, location, timestamp,
                     PointerDetails(EventPointerType::kTouch, id));
    SetTouchTargetAndDispatchTouchEvent(&event);
    HandleTouchFocusChange(touch_point.second->window, false);
  }
  touch_points_.clear();
  last_touch_stylus_data_.clear();
}

void WaylandEventSource::OnTouchFrame() {
  while (!touch_frames_.empty()) {
    // It is safe to pop the first queued event for processing.
    auto touch_frame = std::move(touch_frames_.front());
    touch_frames_.pop_front();

    // In case there are touch stylus information, override the current 'event'
    // instance, given that PointerDetails is 'const'.
    auto pointer_details_with_stylus_data = AmendStylusData(
        touch_frame->event->AsTouchEvent()->pointer_details().id);
    if (pointer_details_with_stylus_data) {
      auto old_event = std::move(touch_frame->event);
      touch_frame->event = std::make_unique<TouchEvent>(
          old_event->type(), old_event->AsTouchEvent()->location_f(),
          old_event->AsTouchEvent()->root_location_f(), old_event->time_stamp(),
          pointer_details_with_stylus_data.value(), old_event->flags());
    }
    SetTouchTargetAndDispatchTouchEvent(touch_frame->event->AsTouchEvent());
    if (!touch_frame->completion_cb.is_null())
      std::move(touch_frame->completion_cb).Run();
  }
}

void WaylandEventSource::OnTouchFocusChanged(WaylandWindow* window) {
  window_manager_->SetTouchFocusedWindow(window);
}

std::vector<PointerId> WaylandEventSource::GetActiveTouchPointIds() {
  std::vector<PointerId> pointer_ids;
  for (auto& touch_point : touch_points_)
    pointer_ids.push_back(touch_point.first);
  return pointer_ids;
}

void WaylandEventSource::OnTouchStylusToolChanged(
    PointerId pointer_id,
    EventPointerType pointer_type) {
  StylusData stylus_data = {.type = pointer_type,
                            .tilt = gfx::Vector2dF(),
                            .force = std::numeric_limits<float>::quiet_NaN()};
  bool inserted =
      last_touch_stylus_data_.try_emplace(pointer_id, stylus_data).second;
  DCHECK(inserted);
}

void WaylandEventSource::OnTouchStylusForceChanged(PointerId pointer_id,
                                                   float force) {
  DCHECK(last_touch_stylus_data_[pointer_id].has_value());
  last_touch_stylus_data_[pointer_id]->force = force;
}

void WaylandEventSource::OnTouchStylusTiltChanged(PointerId pointer_id,
                                                  const gfx::Vector2dF& tilt) {
  DCHECK(last_touch_stylus_data_[pointer_id].has_value());
  last_touch_stylus_data_[pointer_id]->tilt = tilt;
}

const WaylandWindow* WaylandEventSource::GetTouchTarget(PointerId id) const {
  const auto it = touch_points_.find(id);
  return it == touch_points_.end() ? nullptr : it->second->window.get();
}

void WaylandEventSource::OnPinchEvent(EventType event_type,
                                      const gfx::Vector2dF& delta,
                                      base::TimeTicks timestamp,
                                      int device_id,
                                      absl::optional<float> scale_delta) {
  GestureEventDetails details(event_type);
  details.set_device_type(GestureDeviceType::DEVICE_TOUCHPAD);
  if (scale_delta)
    details.set_scale(*scale_delta);

  auto location = pointer_location_ + delta;
  GestureEvent event(location.x(), location.y(), 0 /* flags */, timestamp,
                     details);
  event.set_source_device_id(device_id);

  auto* target = window_manager_->GetCurrentPointerFocusedWindow();
  // A window may be deleted when the event arrived from the server.
  if (!target)
    return;

  SetTargetAndDispatchEvent(&event, target);
}

void WaylandEventSource::SetRelativePointerMotionEnabled(bool enabled) {
  if (enabled)
    relative_pointer_location_ = pointer_location_;
  else
    relative_pointer_location_.reset();
}

void WaylandEventSource::OnRelativePointerMotion(const gfx::Vector2dF& delta) {
  DCHECK(relative_pointer_location_.has_value());
  // TODO(oshima): Investigate if we need to scale the delta
  // when surface_submission_in_pixel_coordinates is on.
  relative_pointer_location_ = *relative_pointer_location_ + delta;
  OnPointerMotionEvent(*relative_pointer_location_,
                       wl::EventDispatchPolicy::kImmediate);
}

bool WaylandEventSource::IsPointerButtonPressed(EventFlags button) const {
  DCHECK(HasAnyPointerButtonFlag(button));
  return pointer_flags_ & button;
}

void WaylandEventSource::OnPointerStylusToolChanged(
    EventPointerType pointer_type) {
  last_pointer_stylus_tool_ = {
      .type = pointer_type,
      .tilt = gfx::Vector2dF(),
      .force = std::numeric_limits<float>::quiet_NaN()};
}

const WaylandWindow* WaylandEventSource::GetPointerTarget() const {
  return window_manager_->GetCurrentPointerFocusedWindow();
}

void WaylandEventSource::ResetPointerFlags() {
  pointer_flags_ = 0;
}

void WaylandEventSource::OnDispatcherListChanged() {
  StartProcessingEvents();
}

void WaylandEventSource::OnWindowRemoved(WaylandWindow* window) {
  if (connection_->IsDragInProgress()) {
    auto* target_window = window_manager_->GetCurrentTouchFocusedWindow();
    for (auto& touch_point : touch_points_)
      touch_point.second->window = target_window;
    return;
  }

  // Clear touch-related data.
  base::EraseIf(touch_points_, [window](const auto& point) {
    return point.second->window == window;
  });
}

void WaylandEventSource::HandleTouchFocusChange(WaylandWindow* window,
                                                bool focused,
                                                absl::optional<PointerId> id) {
  DCHECK(window);
  bool actual_focus = id ? !ShouldUnsetTouchFocus(window, id.value()) : focused;
  window->set_touch_focus(actual_focus);
}

// Focus must not be unset if there is another touch point within |window|.
bool WaylandEventSource::ShouldUnsetTouchFocus(WaylandWindow* win,
                                               PointerId id) {
  auto result = std::find_if(
      touch_points_.begin(), touch_points_.end(),
      [win, id](auto& p) { return p.second->window == win && p.first != id; });
  return result == touch_points_.end();
}

gfx::Vector2dF WaylandEventSource::ComputeFlingVelocity() {
  // Return average velocity in the last 200ms.
  // TODO(fukino): Make the formula similar to libgestures's
  // RegressScrollVelocity(). crbug.com/1129263.
  base::TimeDelta dt;
  float dx = 0.0f;
  float dy = 0.0f;
  for (auto& frame : pointer_scroll_data_set_) {
    if (frame.axis_source &&
        *frame.axis_source != WL_POINTER_AXIS_SOURCE_FINGER) {
      break;
    }
    if (frame.dx == 0 && frame.dy == 0)
      break;
    if (dt + frame.dt > base::Milliseconds(200))
      break;

    dx += frame.dx;
    dy += frame.dy;
    dt += frame.dt;
  }
  pointer_scroll_data_set_.clear();

  float dt_inv = 1.0f / dt.InSecondsF();
  return dt.is_zero() ? gfx::Vector2dF()
                      : gfx::Vector2dF(dx * dt_inv, dy * dt_inv);
}

absl::optional<PointerDetails> WaylandEventSource::AmendStylusData() const {
  if (!last_pointer_stylus_tool_)
    return absl::nullopt;

  DCHECK_NE(last_pointer_stylus_tool_->type, EventPointerType::kUnknown);
  return PointerDetails(last_pointer_stylus_tool_->type, /*pointer_id=*/0,
                        /*radius_x=*/1.0f,
                        /*radius_y=*/1.0f, last_pointer_stylus_tool_->force,
                        /*twist=*/0.0f, last_pointer_stylus_tool_->tilt.x(),
                        last_pointer_stylus_tool_->tilt.y());
}

absl::optional<PointerDetails> WaylandEventSource::AmendStylusData(
    PointerId pointer_id) const {
  const auto it = last_touch_stylus_data_.find(pointer_id);
  if (it == last_touch_stylus_data_.end() || !it->second ||
      it->second->type == EventPointerType::kTouch) {
    return absl::nullopt;
  }

  // The values below come from the default values in pointer_details.cc|h.
  return PointerDetails(it->second->type, pointer_id,
                        /*radius_x=*/1.0f,
                        /*radius_y=*/1.0f, it->second->force,
                        /*twist=*/0.0f, it->second->tilt.x(),
                        it->second->tilt.y());
}

WaylandEventSource::PointerScrollData&
WaylandEventSource::EnsurePointerScrollData() {
  if (!pointer_scroll_data_)
    pointer_scroll_data_ = PointerScrollData();

  return *pointer_scroll_data_;
}

void WaylandEventSource::ProcessPointerScrollData() {
  DCHECK(pointer_scroll_data_);

  int flags = pointer_flags_ | keyboard_modifiers_;

  static constexpr bool supports_trackpad_kinetic_scrolling =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      true;
#else
      false;
#endif

  // Dispatch Fling event if pointer.axis_stop is notified and the recent
  // pointer.axis events meets the criteria to start fling scroll.
  if (pointer_scroll_data_->dx == 0 && pointer_scroll_data_->dy == 0 &&
      pointer_scroll_data_->is_axis_stop &&
      supports_trackpad_kinetic_scrolling) {
    gfx::Vector2dF initial_velocity = ComputeFlingVelocity();
    float vx = initial_velocity.x();
    float vy = initial_velocity.y();
    ScrollEvent event(
        vx == 0 && vy == 0 ? ET_SCROLL_FLING_CANCEL : ET_SCROLL_FLING_START,
        pointer_location_, pointer_location_, EventTimeForNow(), flags, vx, vy,
        vx, vy, kGestureScrollFingerCount);
    pointer_frames_.push_back(
        std::make_unique<FrameData>(event, base::NullCallback()));
  } else if (pointer_scroll_data_->axis_source) {
    if (*pointer_scroll_data_->axis_source == WL_POINTER_AXIS_SOURCE_WHEEL ||
        *pointer_scroll_data_->axis_source ==
            WL_POINTER_AXIS_SOURCE_WHEEL_TILT) {
      MouseWheelEvent event(
          gfx::Vector2d(pointer_scroll_data_->dx, pointer_scroll_data_->dy),
          pointer_location_, pointer_location_, EventTimeForNow(), flags, 0);
      pointer_frames_.push_back(
          std::make_unique<FrameData>(event, base::NullCallback()));
    } else if (*pointer_scroll_data_->axis_source ==
                   WL_POINTER_AXIS_SOURCE_FINGER ||
               *pointer_scroll_data_->axis_source ==
                   WL_POINTER_AXIS_SOURCE_CONTINUOUS) {
      ScrollEvent event(ET_SCROLL, pointer_location_, pointer_location_,
                        EventTimeForNow(), flags, pointer_scroll_data_->dx,
                        pointer_scroll_data_->dy, pointer_scroll_data_->dx,
                        pointer_scroll_data_->dy, kGestureScrollFingerCount);
      pointer_frames_.push_back(
          std::make_unique<FrameData>(event, base::NullCallback()));
    }

    if (pointer_scroll_data_set_.size() + 1 > kPointerScrollDataSetMaxSize)
      pointer_scroll_data_set_.pop_back();
    pointer_scroll_data_set_.push_front(*pointer_scroll_data_);
  }

  pointer_scroll_data_.reset();
}

}  // namespace ui
