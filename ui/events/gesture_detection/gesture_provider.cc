// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_provider.h"

#include <stddef.h>

#include <cmath>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/gesture_listeners.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/events/gesture_detection/motion_event_generic.h"
#include "ui/events/gesture_detection/scale_gesture_listeners.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {
namespace {

// Double-tap drag zoom sensitivity (speed).
const float kDoubleTapDragZoomSpeed = 0.005f;

const char* GetMotionEventActionName(MotionEvent::Action action) {
  switch (action) {
    case MotionEvent::Action::NONE:
      return "Action::NONE";
    case MotionEvent::Action::POINTER_DOWN:
      return "Action::POINTER_DOWN";
    case MotionEvent::Action::POINTER_UP:
      return "Action::POINTER_UP";
    case MotionEvent::Action::DOWN:
      return "Action::DOWN";
    case MotionEvent::Action::UP:
      return "Action::UP";
    case MotionEvent::Action::CANCEL:
      return "Action::CANCEL";
    case MotionEvent::Action::MOVE:
      return "Action::MOVE";
    case MotionEvent::Action::HOVER_ENTER:
      return "Action::HOVER_ENTER";
    case MotionEvent::Action::HOVER_EXIT:
      return "Action::HOVER_EXIT";
    case MotionEvent::Action::HOVER_MOVE:
      return "Action::HOVER_MOVE";
    case MotionEvent::Action::BUTTON_PRESS:
      return "Action::BUTTON_PRESS";
    case MotionEvent::Action::BUTTON_RELEASE:
      return "Action::BUTTON_RELEASE";
  }
  return "";
}

gfx::RectF ClampBoundingBox(const gfx::RectF& bounds,
                            float min_length,
                            float max_length) {
  float width = bounds.width();
  float height = bounds.height();
  if (min_length) {
    width = std::max(min_length, width);
    height = std::max(min_length, height);
  }
  if (max_length) {
    width = std::min(max_length, width);
    height = std::min(max_length, height);
  }
  const gfx::PointF center = bounds.CenterPoint();
  return gfx::RectF(
      center.x() - width / 2.f, center.y() - height / 2.f, width, height);
}

}  // namespace

// GestureProviderClient:

bool GestureProviderClient::RequiresDoubleTapGestureEvents() const {
  return false;
}

// GestureProvider:::Config

GestureProvider::Config::Config()
    : display(display::kInvalidDisplayId, gfx::Rect(1, 1)),
      double_tap_support_for_platform_enabled(true),
      gesture_begin_end_types_enabled(false),
      min_gesture_bounds_length(0),
      max_gesture_bounds_length(0) {}

GestureProvider::Config::Config(const Config& other) = default;

GestureProvider::Config::~Config() {
}

// GestureProvider::GestureListener

class GestureProvider::GestureListenerImpl : public ScaleGestureListener,
                                             public GestureListener,
                                             public DoubleTapListener {
 public:
  GestureListenerImpl(const GestureProvider::Config& config,
                      GestureProviderClient* client)
      : config_(config),
        client_(client),
        gesture_detector_(config.gesture_detector_config, this, this),
        scale_gesture_detector_(config.scale_gesture_detector_config, this),
        snap_scroll_controller_(config.gesture_detector_config.touch_slop,
                                gfx::SizeF(config.display.size())),
        ignore_multitouch_zoom_events_(false),
        ignore_single_tap_(false),
        pinch_event_sent_(false),
        scroll_event_sent_(false),
        max_diameter_before_show_press_(0),
        show_press_event_sent_(false) {}

  void OnTouchEvent(const MotionEvent& event) {
    const bool in_scale_gesture = IsScaleGestureDetectionInProgress();
    snap_scroll_controller_.SetSnapScrollMode(event, in_scale_gesture);
    if (in_scale_gesture)
      SetIgnoreSingleTap(true);

    const MotionEvent::Action action = event.GetAction();
    if (action == MotionEvent::Action::DOWN) {
      current_down_action_event_time_ = event.GetEventTime();
      current_longpress_time_ = base::TimeTicks();
      ignore_single_tap_ = false;
      scroll_event_sent_ = false;
      pinch_event_sent_ = false;
      show_press_event_sent_ = false;
      gesture_detector_.set_longpress_enabled(true);
      tap_down_point_ = gfx::PointF(event.GetX(), event.GetY());
      max_diameter_before_show_press_ = event.GetTouchMajor();
    }
    gesture_detector_.OnTouchEvent(event,
                                   client_->RequiresDoubleTapGestureEvents());
    scale_gesture_detector_.OnTouchEvent(event);

    if (action == MotionEvent::Action::UP ||
        action == MotionEvent::Action::CANCEL) {
      // Note: This call will have no effect if a fling was just generated, as
      // |Fling()| will have already signalled an end to touch-scrolling.
      if (scroll_event_sent_)
        Send(CreateGesture(ET_GESTURE_SCROLL_END, event));

      // If this was the last pointer that was canceled or lifted reset the
      // |current_down_action_event_time_| to indicate no sequence is going on.
      if (action != MotionEvent::Action::CANCEL ||
          !GestureConfiguration::GetInstance()
               ->single_pointer_cancel_enabled() ||
          event.GetPointerCount() == 1)
        current_down_action_event_time_ = base::TimeTicks();
    } else if (action == MotionEvent::Action::MOVE) {
      if (!show_press_event_sent_ && !scroll_event_sent_) {
        max_diameter_before_show_press_ =
            std::max(max_diameter_before_show_press_, event.GetTouchMajor());
      }
    }
  }

  void Send(GestureEventData gesture) {
    DCHECK(!gesture.time.is_null());
    // The only valid events that should be sent without an active touch
    // sequence are SHOW_PRESS, TAP and TAP_CANCEL, potentially triggered by
    // the double-tap delay timing out or being cancelled.
    DCHECK(!current_down_action_event_time_.is_null() ||
           gesture.type() == ET_GESTURE_TAP ||
           gesture.type() == ET_GESTURE_SHOW_PRESS ||
           gesture.type() == ET_GESTURE_TAP_CANCEL ||
           gesture.type() == ET_GESTURE_BEGIN ||
           gesture.type() == ET_GESTURE_END);

    if (gesture.primary_tool_type == MotionEvent::ToolType::UNKNOWN ||
        gesture.primary_tool_type == MotionEvent::ToolType::FINGER) {
      gesture.details.set_bounding_box(
          ClampBoundingBox(gesture.details.bounding_box_f(),
                           config_.min_gesture_bounds_length,
                           config_.max_gesture_bounds_length));
    }

    switch (gesture.type()) {
      case ET_GESTURE_LONG_PRESS:
        DCHECK(!IsScaleGestureDetectionInProgress());
        current_longpress_time_ = gesture.time;
        break;
      case ET_GESTURE_LONG_TAP:
        current_longpress_time_ = base::TimeTicks();
        break;
      case ET_GESTURE_SCROLL_BEGIN:
        DCHECK(!scroll_event_sent_);
        scroll_event_sent_ = true;
        break;
      case ET_GESTURE_SCROLL_END:
        DCHECK(scroll_event_sent_);
        if (pinch_event_sent_)
          Send(GestureEventData(ET_GESTURE_PINCH_END, gesture));
        scroll_event_sent_ = false;
        break;
      case ET_SCROLL_FLING_START:
        DCHECK(scroll_event_sent_);
        scroll_event_sent_ = false;
        break;
      case ET_GESTURE_PINCH_BEGIN:
        DCHECK(!pinch_event_sent_);
        if (!scroll_event_sent_ &&
            !scale_gesture_detector_.InAnchoredScaleMode()) {
          Send(GestureEventData(ET_GESTURE_SCROLL_BEGIN, gesture));
        }
        pinch_event_sent_ = true;
        break;
      case ET_GESTURE_PINCH_END:
        DCHECK(pinch_event_sent_);
        pinch_event_sent_ = false;
        break;
      case ET_GESTURE_SHOW_PRESS:
        // It's possible that a double-tap drag zoom (from ScaleGestureDetector)
        // will start before the press gesture fires (from GestureDetector), in
        // which case the press should simply be dropped.
        if (pinch_event_sent_ || scroll_event_sent_)
          return;
        break;
      default:
        break;
    };

    client_->OnGestureEvent(gesture);
    GestureTouchUMAHistogram::RecordGestureEvent(gesture);
  }

  // ScaleGestureListener implementation.
  bool OnScaleBegin(const ScaleGestureDetector& detector,
                    const MotionEvent& e) override {
    if (ignore_multitouch_zoom_events_ && !detector.InAnchoredScaleMode())
      return false;
    return true;
  }

  void OnScaleEnd(const ScaleGestureDetector& detector,
                  const MotionEvent& e) override {
    if (!pinch_event_sent_)
      return;
    Send(CreateGesture(ET_GESTURE_PINCH_END, e));
  }

  bool OnScale(const ScaleGestureDetector& detector,
               const MotionEvent& e) override {
    if (ignore_multitouch_zoom_events_ && !detector.InAnchoredScaleMode())
      return false;
    bool first_scale = false;
    if (!pinch_event_sent_) {
      first_scale = true;
      Send(CreateGesture(ET_GESTURE_PINCH_BEGIN,
                         e.GetPointerId(),
                         e.GetToolType(),
                         detector.GetEventTime(),
                         detector.GetFocusX(),
                         detector.GetFocusY(),
                         detector.GetFocusX() + e.GetRawOffsetX(),
                         detector.GetFocusY() + e.GetRawOffsetY(),
                         e.GetPointerCount(),
                         GetBoundingBox(e, ET_GESTURE_PINCH_BEGIN),
                         e.GetFlags()));
    }

    if (std::abs(detector.GetCurrentSpan() - detector.GetPreviousSpan()) <
        config_.scale_gesture_detector_config.min_pinch_update_span_delta) {
      return false;
    }

    float scale = detector.GetScaleFactor();
    if (scale == 1)
      return true;

    if (detector.InAnchoredScaleMode()) {
      // Relative changes in the double-tap scale factor computed by |detector|
      // diminish as the touch moves away from the original double-tap focus.
      // For historical reasons, Chrome has instead adopted a scale factor
      // computation that is invariant to the focal distance, where
      // the scale delta remains constant if the touch velocity is constant.
      // Note: Because we calculate the scale here manually based on the
      // y-span, but the scale factor accounts for slop in the first previous
      // span, we manaully reproduce the behavior here for previous span y.
      float prev_y = first_scale
                         ? config_.gesture_detector_config.touch_slop * 2
                         : detector.GetPreviousSpanY();
      float dy = (detector.GetCurrentSpanY() - prev_y) * 0.5f;
      scale = std::pow(scale > 1 ? 1.0f + kDoubleTapDragZoomSpeed
                                 : 1.0f - kDoubleTapDragZoomSpeed,
                       std::abs(dy));
    }
    GestureEventDetails pinch_details(ET_GESTURE_PINCH_UPDATE);
    pinch_details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
    pinch_details.set_scale(scale);
    Send(CreateGesture(pinch_details,
                       e.GetPointerId(),
                       e.GetToolType(),
                       detector.GetEventTime(),
                       detector.GetFocusX(),
                       detector.GetFocusY(),
                       detector.GetFocusX() + e.GetRawOffsetX(),
                       detector.GetFocusY() + e.GetRawOffsetY(),
                       e.GetPointerCount(),
                       GetBoundingBox(e, pinch_details.type()),
                       e.GetFlags()));
    return true;
  }

  // GestureListener implementation.
  bool OnDown(const MotionEvent& e) override {
    GestureEventDetails tap_details(ET_GESTURE_TAP_DOWN);
    tap_details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
    Send(CreateGesture(tap_details, e));

    // Return true to indicate that we want to handle touch.
    return true;
  }

  bool OnScroll(const MotionEvent& e1,
                const MotionEvent& e2,
                const MotionEvent& secondary_pointer_down,
                float raw_distance_x,
                float raw_distance_y) override {
    float distance_x = raw_distance_x;
    float distance_y = raw_distance_y;
    if (!scroll_event_sent_ && e2.GetPointerCount() < 3) {
      // Remove the touch slop region from the first scroll event to avoid a
      // jump. Touch slop isn't used for scroll gestures with greater than 2
      // pointers down, in those cases we don't subtract the slop.
      gfx::Vector2dF delta =
          ComputeFirstScrollDelta(e1, e2, secondary_pointer_down);
      distance_x = delta.x();
      distance_y = delta.y();
    }

    snap_scroll_controller_.UpdateSnapScrollMode(distance_x, distance_y);
    if (snap_scroll_controller_.IsSnappingScrolls()) {
      if (snap_scroll_controller_.IsSnapHorizontal())
        distance_y = 0;
      else
        distance_x = 0;
    }

    if (!distance_x && !distance_y)
      return true;

    if (!scroll_event_sent_) {
      // Note that scroll start hints are in distance traveled, where
      // scroll deltas are in the opposite direction.
      GestureEventDetails scroll_details(ET_GESTURE_SCROLL_BEGIN, -distance_x,
                                         -distance_y);
      scroll_details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);

      // Scroll focus point always starts with the first touch down point.
      scroll_focus_point_.SetPoint(e1.GetX(), e1.GetY());

      // Use the co-ordinates from the touch down, as these co-ordinates are
      // used to determine which layer the scroll should affect.
      Send(CreateGesture(scroll_details, e2.GetPointerId(), e2.GetToolType(),
                         e2.GetEventTime(), e1.GetX(), e1.GetY(), e1.GetRawX(),
                         e1.GetRawY(), e2.GetPointerCount(),
                         GetBoundingBox(e2, scroll_details.type()),
                         e2.GetFlags()));
      DCHECK(scroll_event_sent_);
    }
    scroll_focus_point_.SetPoint(scroll_focus_point_.x() - raw_distance_x,
                                 scroll_focus_point_.y() - raw_distance_y);

    GestureEventDetails scroll_details(ET_GESTURE_SCROLL_UPDATE, -distance_x,
                                       -distance_y);
    scroll_details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
    const gfx::RectF bounding_box = GetBoundingBox(e2, scroll_details.type());
    const gfx::PointF raw_center =
        scroll_focus_point_ +
        gfx::Vector2dF(e2.GetRawOffsetX(), e2.GetRawOffsetY());
    Send(CreateGesture(scroll_details, e2.GetPointerId(), e2.GetToolType(),
                       e2.GetEventTime(), scroll_focus_point_.x(),
                       scroll_focus_point_.y(), raw_center.x(), raw_center.y(),
                       e2.GetPointerCount(), bounding_box, e2.GetFlags()));

    return true;
  }

  bool OnFling(const MotionEvent& e1,
               const MotionEvent& e2,
               float velocity_x,
               float velocity_y) override {
    if (snap_scroll_controller_.IsSnappingScrolls()) {
      if (snap_scroll_controller_.IsSnapHorizontal()) {
        velocity_y = 0;
      } else {
        velocity_x = 0;
      }
    }

    if (!velocity_x && !velocity_y)
      return true;

    DCHECK(scroll_event_sent_);
    if (!scroll_event_sent_) {
      // The native side needs a ET_GESTURE_SCROLL_BEGIN before
      // ET_SCROLL_FLING_START to send the fling to the correct target.
      // The distance traveled in one second is a reasonable scroll start hint.
      GestureEventDetails scroll_details(
          ET_GESTURE_SCROLL_BEGIN, velocity_x, velocity_y);
      scroll_details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
      Send(CreateGesture(scroll_details, e2));
    }

    GestureEventDetails fling_details(
        ET_SCROLL_FLING_START, velocity_x, velocity_y);
    fling_details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
    Send(CreateGesture(fling_details, e2));
    return true;
  }

  bool OnSwipe(const MotionEvent& e1,
               const MotionEvent& e2,
               float velocity_x,
               float velocity_y) override {
    GestureEventDetails swipe_details(ET_GESTURE_SWIPE, velocity_x, velocity_y);
    swipe_details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
    Send(CreateGesture(swipe_details, e2));
    return true;
  }

  bool OnTwoFingerTap(const MotionEvent& e1, const MotionEvent& e2) override {
    // The location of the two finger tap event should be the location of the
    // primary pointer.
    GestureEventDetails two_finger_tap_details(
        ET_GESTURE_TWO_FINGER_TAP, e1.GetTouchMajor(), e1.GetTouchMajor());
    two_finger_tap_details.set_device_type(
        GestureDeviceType::DEVICE_TOUCHSCREEN);
    Send(CreateGesture(two_finger_tap_details,
                       e2.GetPointerId(),
                       e2.GetToolType(),
                       e2.GetEventTime(),
                       e1.GetX(),
                       e1.GetY(),
                       e1.GetRawX(),
                       e1.GetRawY(),
                       e2.GetPointerCount(),
                       GetBoundingBox(e2, two_finger_tap_details.type()),
                       e2.GetFlags()));
    return true;
  }

  void OnTapCancel(const MotionEvent& e) override {
    Send(CreateGesture(ET_GESTURE_TAP_CANCEL, e));
  }

  void OnShowPress(const MotionEvent& e) override {
    GestureEventDetails show_press_details(ET_GESTURE_SHOW_PRESS);
    show_press_details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
    show_press_event_sent_ = true;
    Send(CreateGesture(show_press_details, e));
  }

  bool OnSingleTapUp(const MotionEvent& e, int tap_count) override {
    // This is a hack to address the issue where user hovers
    // over a link for longer than double_tap_timeout_, then
    // OnSingleTapConfirmed() is not triggered. But we still
    // want to trigger the tap event at UP. So we override
    // OnSingleTapUp() in this case. This assumes singleTapUp
    // gets always called before singleTapConfirmed.
    if (!ignore_single_tap_) {
      if (e.GetEventTime() - current_down_action_event_time_ >
          config_.gesture_detector_config.double_tap_timeout) {
        return OnSingleTapImpl(e, tap_count);
      } else if (!IsDoubleTapEnabled()) {
        // If double-tap has been disabled, there is no need to wait
        // for the double-tap timeout.
        return OnSingleTapImpl(e, tap_count);
      } else {
        // Notify Blink about this tapUp event anyway, when none of the above
        // conditions applied.
        Send(CreateTapGesture(ET_GESTURE_TAP_UNCONFIRMED, e, 1));
      }
    }

    if (e.GetAction() == MotionEvent::Action::UP &&
        !current_longpress_time_.is_null() &&
        !IsScaleGestureDetectionInProgress()) {
      GestureEventDetails long_tap_details(ET_GESTURE_LONG_TAP);
      long_tap_details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
      Send(CreateGesture(long_tap_details, e));
      return true;
    }

    return false;
  }

  // DoubleTapListener implementation.
  bool OnSingleTapConfirmed(const MotionEvent& e) override {
    return OnSingleTapImpl(e, 1);
  }

  bool OnDoubleTap(const MotionEvent& e) override {
    return scale_gesture_detector_.OnDoubleTap(e);
  }

  bool OnDoubleTapEvent(const MotionEvent& e) override {
    switch (e.GetAction()) {
      case MotionEvent::Action::DOWN:
        gesture_detector_.set_longpress_enabled(false);
        break;

      case MotionEvent::Action::UP:
        if (!IsPinchInProgress() && !IsScrollInProgress()) {
          Send(CreateTapGesture(ET_GESTURE_DOUBLE_TAP, e, 1));
          return true;
        }
        break;

      default:
        break;
    }
    return false;
  }

  void OnLongPress(const MotionEvent& e) override {
    DCHECK(!IsDoubleTapInProgress());
    SetIgnoreSingleTap(true);
    GestureEventDetails long_press_details(ET_GESTURE_LONG_PRESS);
    long_press_details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
    Send(CreateGesture(long_press_details, e));
  }

  GestureEventData CreateGesture(const GestureEventDetails& details,
                                 int motion_event_id,
                                 MotionEvent::ToolType primary_tool_type,
                                 base::TimeTicks time,
                                 float x,
                                 float y,
                                 float raw_x,
                                 float raw_y,
                                 size_t touch_point_count,
                                 const gfx::RectF& bounding_box,
                                 int flags) const {
    return GestureEventData(details,
                            motion_event_id,
                            primary_tool_type,
                            time,
                            x,
                            y,
                            raw_x,
                            raw_y,
                            touch_point_count,
                            bounding_box,
                            flags,
                            0U);
  }

  GestureEventData CreateGesture(EventType type,
                                 int motion_event_id,
                                 MotionEvent::ToolType primary_tool_type,
                                 base::TimeTicks time,
                                 float x,
                                 float y,
                                 float raw_x,
                                 float raw_y,
                                 size_t touch_point_count,
                                 const gfx::RectF& bounding_box,
                                 int flags) const {
    GestureEventDetails details(type);
    details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
    return GestureEventData(details,
                            motion_event_id,
                            primary_tool_type,
                            time,
                            x,
                            y,
                            raw_x,
                            raw_y,
                            touch_point_count,
                            bounding_box,
                            flags,
                            0U);
  }

  GestureEventData CreateGesture(const GestureEventDetails& details,
                                 const MotionEvent& event) const {
    return GestureEventData(details, event.GetPointerId(), event.GetToolType(),
                            event.GetEventTime(), event.GetX(), event.GetY(),
                            event.GetRawX(), event.GetRawY(),
                            event.GetPointerCount(),
                            GetBoundingBox(event, details.type()),
                            event.GetFlags(), event.GetUniqueEventId());
  }

  GestureEventData CreateGesture(EventType type,
                                 const MotionEvent& event) const {
    GestureEventDetails details(type);
    details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
    return CreateGesture(details, event);
  }

  GestureEventData CreateTapGesture(EventType type,
                                    const MotionEvent& event,
                                    int tap_count) const {
    DCHECK_GE(tap_count, 0);
    GestureEventDetails details(type);
    details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
    details.set_tap_count(tap_count);
    return CreateGesture(details, event);
  }

  gfx::RectF GetBoundingBox(const MotionEvent& event, EventType type) const {
    // Can't use gfx::RectF::Union, as it ignores touches with a radius of 0.
    float left = std::numeric_limits<float>::max();
    float top = std::numeric_limits<float>::max();
    float right = -std::numeric_limits<float>::max();
    float bottom = -std::numeric_limits<float>::max();
    for (size_t i = 0; i < event.GetPointerCount(); ++i) {
      float x, y, diameter;
      // Only for the show press and tap events, the bounding box is calculated
      // based on the touch start point and the maximum diameter before the
      // show press event is sent.
      if (type == ET_GESTURE_SHOW_PRESS || type == ET_GESTURE_TAP ||
          type == ET_GESTURE_TAP_UNCONFIRMED) {
        DCHECK_EQ(0U, i);
        diameter = max_diameter_before_show_press_;
        x = tap_down_point_.x();
        y = tap_down_point_.y();
      } else {
        diameter = event.GetTouchMajor(i);
        x = event.GetX(i);
        y = event.GetY(i);
      }
      x = x - diameter / 2;
      y = y - diameter / 2;
      left = std::min(left, x);
      right = std::max(right, x + diameter);
      top = std::min(top, y);
      bottom = std::max(bottom, y + diameter);
    }
    return gfx::RectF(left, top, right - left, bottom - top);
  }

  void SetDoubleTapEnabled(bool enabled) {
    DCHECK(!IsDoubleTapInProgress());
    gesture_detector_.SetDoubleTapListener(enabled ? this : NULL);
  }

  void SetMultiTouchZoomEnabled(bool enabled) {
    // Note that returning false from |OnScaleBegin()| or |OnScale()| prevents
    // the detector from emitting further scale updates for the current touch
    // sequence. Thus, if multitouch events are enabled in the middle of a
    // gesture, it will only take effect with the next gesture.
    ignore_multitouch_zoom_events_ = !enabled;
  }

  bool IsDoubleTapInProgress() const {
    return gesture_detector_.is_double_tapping() ||
           (IsScaleGestureDetectionInProgress() && InAnchoredScaleMode());
  }

  bool IsScrollInProgress() const { return scroll_event_sent_; }

  bool IsPinchInProgress() const { return pinch_event_sent_; }

 private:
  bool OnSingleTapImpl(const MotionEvent& e, int tap_count) {
    // Long taps in the edges of the screen have their events delayed by
    // ContentViewHolder for tab swipe operations. As a consequence of the delay
    // this method might be called after receiving the up event.
    // These corner cases should be ignored.
    if (ignore_single_tap_)
      return true;

    ignore_single_tap_ = true;

    Send(CreateTapGesture(ET_GESTURE_TAP, e, tap_count));
    return true;
  }

  bool IsScaleGestureDetectionInProgress() const {
    return scale_gesture_detector_.IsInProgress();
  }

  bool InAnchoredScaleMode() const {
    return scale_gesture_detector_.InAnchoredScaleMode();
  }

  bool IsDoubleTapEnabled() const {
    return gesture_detector_.has_doubletap_listener() &&
           client_->RequiresDoubleTapGestureEvents();
  }

  void SetIgnoreSingleTap(bool value) { ignore_single_tap_ = value; }

  gfx::Vector2dF SubtractSlopRegion(const float dx, const float dy) {
    float distance = std::sqrt(dx * dx + dy * dy);
    float epsilon = 1e-3f;
    if (distance > epsilon) {
      float ratio =
          std::max(0.f, distance - config_.gesture_detector_config.touch_slop) /
          distance;
      gfx::Vector2dF delta(dx * ratio, dy * ratio);
      return delta;
    }
    gfx::Vector2dF delta(dx, dy);
    return delta;
  }

  // When any of the currently down pointers exceeds its slop region
  // for the first time, scroll delta is adjusted.
  // The new deltas are calculated for each pointer individually,
  // and the final scroll delta is the average over all delta values.
  gfx::Vector2dF ComputeFirstScrollDelta(
      const MotionEvent& ev1,
      const MotionEvent& ev2,
      const MotionEvent& secondary_pointer_down) {
    // If there are more than two down pointers, tapping is not possible,
    // so Slop region is not deducted.
    DCHECK(ev2.GetPointerCount() < 3);

    gfx::Vector2dF delta(0, 0);
    for (size_t i = 0; i < ev2.GetPointerCount(); i++) {
      const int pointer_id = ev2.GetPointerId(i);
      const MotionEvent* source_pointer_down_event =
          gesture_detector_.GetSourcePointerDownEvent(
              ev1, &secondary_pointer_down, pointer_id);

      if (!source_pointer_down_event)
        continue;
      int source_index =
          source_pointer_down_event->FindPointerIndexOfId(pointer_id);
      DCHECK_GE(source_index, 0);
      if (source_index < 0)
        continue;
      float dx = source_pointer_down_event->GetX(source_index) - ev2.GetX(i);
      float dy = source_pointer_down_event->GetY(source_index) - ev2.GetY(i);
      delta += SubtractSlopRegion(dx, dy);
    }
    delta.Scale(1.0 / ev2.GetPointerCount());
    return delta;
  }

  const GestureProvider::Config config_;
  GestureProviderClient* const client_;

  GestureDetector gesture_detector_;
  ScaleGestureDetector scale_gesture_detector_;
  SnapScrollController snap_scroll_controller_;

  // Keeps track of the event time of the first down action in current touch
  // sequence.
  base::TimeTicks current_down_action_event_time_;

  // Keeps track of the current GESTURE_LONG_PRESS event. If a context menu is
  // opened after a GESTURE_LONG_PRESS, this is used to insert a
  // GESTURE_TAP_CANCEL for removing any ::active styling.
  base::TimeTicks current_longpress_time_;

  // Completely silence multi-touch (pinch) scaling events. Used in WebView when
  // zoom support is turned off.
  bool ignore_multitouch_zoom_events_;

  // TODO(klobag): This is to avoid a bug in GestureDetector. With multi-touch,
  // always_in_tap_region_ is not reset. So when the last finger is up,
  // |OnSingleTapUp()| will be mistakenly fired.
  bool ignore_single_tap_;

  // Tracks whether {PINCH|SCROLL}_BEGIN events have been forwarded for the
  // current touch sequence.
  bool pinch_event_sent_;
  bool scroll_event_sent_;

  // Only track the maximum diameter before the show press event has been
  // sent and a tap must still be possible for this touch sequence.
  float max_diameter_before_show_press_;

  gfx::PointF tap_down_point_;

  // Tracks whether an ET_GESTURE_SHOW_PRESS event has been sent for this touch
  // sequence.
  bool show_press_event_sent_;

  // The scroll focus point is set to the first touch down point when scroll
  // begins and is later updated based on the delta of touch points.
  gfx::PointF scroll_focus_point_;
  DISALLOW_COPY_AND_ASSIGN(GestureListenerImpl);
};

// GestureProvider

GestureProvider::GestureProvider(const Config& config,
                                 GestureProviderClient* client)
    : double_tap_support_for_page_(true),
      double_tap_support_for_platform_(
          config.double_tap_support_for_platform_enabled),
      gesture_begin_end_types_enabled_(config.gesture_begin_end_types_enabled) {
  DCHECK(client);
  DCHECK(!config.min_gesture_bounds_length ||
         !config.max_gesture_bounds_length ||
         config.min_gesture_bounds_length <= config.max_gesture_bounds_length);
  TRACE_EVENT0("input", "GestureProvider::InitGestureDetectors");
  gesture_listener_ = std::make_unique<GestureListenerImpl>(config, client);
  UpdateDoubleTapDetectionSupport();
}

GestureProvider::~GestureProvider() {
}

bool GestureProvider::OnTouchEvent(const MotionEvent& event) {
  TRACE_EVENT1("input",
               "GestureProvider::OnTouchEvent",
               "action",
               GetMotionEventActionName(event.GetAction()));
  DCHECK_NE(0u, event.GetPointerCount());

  if (!CanHandle(event))
    return false;

  OnTouchEventHandlingBegin(event);
  gesture_listener_->OnTouchEvent(event);
  OnTouchEventHandlingEnd(event);
  uma_histogram_.RecordTouchEvent(event);
  return true;
}

void GestureProvider::ResetDetection() {
  MotionEventGeneric generic_cancel_event(
      MotionEvent::Action::CANCEL, base::TimeTicks::Now(), PointerProperties());
  OnTouchEvent(generic_cancel_event);
}

void GestureProvider::SetMultiTouchZoomSupportEnabled(bool enabled) {
  gesture_listener_->SetMultiTouchZoomEnabled(enabled);
}

void GestureProvider::SetDoubleTapSupportForPlatformEnabled(bool enabled) {
  if (double_tap_support_for_platform_ == enabled)
    return;
  double_tap_support_for_platform_ = enabled;
  UpdateDoubleTapDetectionSupport();
}

void GestureProvider::SetDoubleTapSupportForPageEnabled(bool enabled) {
  if (double_tap_support_for_page_ == enabled)
    return;
  double_tap_support_for_page_ = enabled;
  UpdateDoubleTapDetectionSupport();
}

bool GestureProvider::IsScrollInProgress() const {
  return gesture_listener_->IsScrollInProgress();
}

bool GestureProvider::IsPinchInProgress() const {
  return gesture_listener_->IsPinchInProgress();
}

bool GestureProvider::IsDoubleTapInProgress() const {
  return gesture_listener_->IsDoubleTapInProgress();
}

bool GestureProvider::CanHandle(const MotionEvent& event) const {
  // Aura requires one cancel event per touch point, whereas Android requires
  // one cancel event per touch sequence. Thus we need to allow extra cancel
  // events.
  return current_down_event_ ||
         event.GetAction() == MotionEvent::Action::DOWN ||
         event.GetAction() == MotionEvent::Action::CANCEL;
}

void GestureProvider::OnTouchEventHandlingBegin(const MotionEvent& event) {
  switch (event.GetAction()) {
    case MotionEvent::Action::DOWN:
      current_down_event_ = event.Clone();
      if (gesture_begin_end_types_enabled_)
        gesture_listener_->Send(
            gesture_listener_->CreateGesture(ET_GESTURE_BEGIN, event));
      break;
    case MotionEvent::Action::POINTER_DOWN:
      if (gesture_begin_end_types_enabled_) {
        const int action_index = event.GetActionIndex();
        gesture_listener_->Send(gesture_listener_->CreateGesture(
            ET_GESTURE_BEGIN,
            event.GetPointerId(),
            event.GetToolType(),
            event.GetEventTime(),
            event.GetX(action_index),
            event.GetY(action_index),
            event.GetRawX(action_index),
            event.GetRawY(action_index),
            event.GetPointerCount(),
            gesture_listener_->GetBoundingBox(event, ET_GESTURE_BEGIN),
            event.GetFlags()));
      }
      break;
    case MotionEvent::Action::POINTER_UP:
    case MotionEvent::Action::UP:
    case MotionEvent::Action::CANCEL:
    case MotionEvent::Action::MOVE:
      break;
    case MotionEvent::Action::NONE:
    case MotionEvent::Action::HOVER_ENTER:
    case MotionEvent::Action::HOVER_EXIT:
    case MotionEvent::Action::HOVER_MOVE:
    case MotionEvent::Action::BUTTON_PRESS:
    case MotionEvent::Action::BUTTON_RELEASE:
      NOTREACHED();
      break;
  }
}

void GestureProvider::OnTouchEventHandlingEnd(const MotionEvent& event) {
  switch (event.GetAction()) {
    case MotionEvent::Action::UP:
    case MotionEvent::Action::CANCEL: {
      if (gesture_begin_end_types_enabled_)
        gesture_listener_->Send(
            gesture_listener_->CreateGesture(ET_GESTURE_END, event));

      if (event.GetAction() != MotionEvent::Action::CANCEL ||
          !GestureConfiguration::GetInstance()
               ->single_pointer_cancel_enabled() ||
          event.GetPointerCount() == 1)
        current_down_event_.reset();

      UpdateDoubleTapDetectionSupport();
      break;
    }
    case MotionEvent::Action::POINTER_UP:
      if (gesture_begin_end_types_enabled_)
        gesture_listener_->Send(
            gesture_listener_->CreateGesture(ET_GESTURE_END, event));
      break;
    case MotionEvent::Action::DOWN:
    case MotionEvent::Action::POINTER_DOWN:
    case MotionEvent::Action::MOVE:
      break;
    case MotionEvent::Action::NONE:
    case MotionEvent::Action::HOVER_ENTER:
    case MotionEvent::Action::HOVER_EXIT:
    case MotionEvent::Action::HOVER_MOVE:
    case MotionEvent::Action::BUTTON_PRESS:
    case MotionEvent::Action::BUTTON_RELEASE:
      NOTREACHED();
      break;
  }
}

void GestureProvider::UpdateDoubleTapDetectionSupport() {
  // The GestureDetector requires that any provided DoubleTapListener remain
  // attached to it for the duration of a touch sequence. Defer any potential
  // null'ing of the listener until the sequence has ended.
  if (current_down_event_)
    return;

  const bool double_tap_enabled =
      double_tap_support_for_page_ && double_tap_support_for_platform_;
  gesture_listener_->SetDoubleTapEnabled(double_tap_enabled);
}

}  //  namespace ui
