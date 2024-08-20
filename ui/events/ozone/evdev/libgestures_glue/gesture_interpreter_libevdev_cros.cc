// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h"

#include <gestures/gestures.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/fixed_array.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_util.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_timer_provider.h"
#include "ui/events/ozone/features.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

#ifndef REL_WHEEL_HI_RES
#define REL_WHEEL_HI_RES 0x0b
#endif

#ifndef INPUT_PROP_HAPTICPAD
#define INPUT_PROP_HAPTICPAD 0x07
#endif

namespace ui {

namespace {

constexpr int kNumTimeBuckets = 13;
float ClickDurationMetricBuckets[kNumTimeBuckets] = {
    0.15, 0.16, 0.17, 0.18, 0.19, 0.2, 0.25, 0.3, 0.35, 0.45, 0.55, 0.65, 0.75,
};
const char* ClickDurationMetricNames[kNumTimeBuckets] = {
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.150ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.160ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.170ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.180ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.190ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.200ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.250ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.300ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.350ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.450ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.550ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.650ms",
    "Ozone.GestureInterpreterLibevdevCros.TouchpadClick.750ms",
};

// Convert libevdev device class to libgestures device class.
GestureInterpreterDeviceClass GestureDeviceClass(Evdev* evdev) {
  switch (evdev->info.evdev_class) {
    case EvdevClassMouse:
      return GESTURES_DEVCLASS_MOUSE;
    case EvdevClassPointingStick:
      return GESTURES_DEVCLASS_POINTING_STICK;
    case EvdevClassMultitouchMouse:
      return GESTURES_DEVCLASS_MULTITOUCH_MOUSE;
    case EvdevClassTouchpad:
      return GESTURES_DEVCLASS_TOUCHPAD;
    case EvdevClassTouchscreen:
      return GESTURES_DEVCLASS_TOUCHSCREEN;
    default:
      return GESTURES_DEVCLASS_UNKNOWN;
  }
}

// Convert libevdev state to libgestures hardware properties.
HardwareProperties GestureHardwareProperties(
    Evdev* evdev,
    const GestureDeviceProperties* props) {
  HardwareProperties hwprops;
  hwprops.left = props->area_left;
  hwprops.top = props->area_top;
  hwprops.right = props->area_right;
  hwprops.bottom = props->area_bottom;
  hwprops.res_x = props->res_x;
  hwprops.res_y = props->res_y;
  hwprops.orientation_minimum = props->orientation_minimum;
  hwprops.orientation_maximum = props->orientation_maximum;
  hwprops.max_finger_cnt = Event_Get_Slot_Count(evdev);
  hwprops.max_touch_cnt = Event_Get_Touch_Count_Max(evdev);
  hwprops.supports_t5r2 = Event_Get_T5R2(evdev);
  hwprops.support_semi_mt = Event_Get_Semi_MT(evdev);
  /* buttonpad means a physical button under the touch surface */
  hwprops.is_button_pad = Event_Get_Button_Pad(evdev);
  hwprops.is_haptic_pad =
      EvdevBitIsSet(evdev->info.prop_bitmask, INPUT_PROP_HAPTICPAD);
  hwprops.has_wheel = EvdevBitIsSet(evdev->info.rel_bitmask, REL_WHEEL) ||
                      EvdevBitIsSet(evdev->info.rel_bitmask, REL_HWHEEL);
  hwprops.wheel_is_hi_res =
	  EvdevBitIsSet(evdev->info.rel_bitmask, REL_WHEEL_HI_RES);
  hwprops.reports_pressure =
      EvdevBitIsSet(evdev->info.abs_bitmask, ABS_MT_PRESSURE) ||
      EvdevBitIsSet(evdev->info.abs_bitmask, ABS_PRESSURE);

  return hwprops;
}

// Callback from libgestures when a gesture is ready.
void OnGestureReadyHelper(void* client_data, const Gesture* gesture) {
  GestureInterpreterLibevdevCros* interpreter =
      static_cast<GestureInterpreterLibevdevCros*>(client_data);
  interpreter->OnGestureReady(gesture);
}

// Convert gestures timestamp (stime_t) to ui::Event timestamp.
base::TimeTicks StimeToTimeTicks(stime_t timestamp) {
  return ui::EventTimeStampFromSeconds(timestamp);
}

// Number of fingers for scroll gestures.
const int kGestureScrollFingerCount = 2;

// Number of fingers for swipe gestures.
const int kGestureSwipeFingerCount = 3;

static constexpr unsigned int kModifierEvdevCodes[] = {
    KEY_LEFTALT,  KEY_RIGHTALT,  KEY_LEFTMETA,  KEY_RIGHTMETA,
    KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTSHIFT, KEY_RIGHTSHIFT};

}  // namespace

void GestureInterpreterLibevdevCros::RecordClickMetric(stime_t duration,
                                                       float movement) {
  int time_bucket;
  // Tap-to-click will have 0 duration, which we want to exclude.
  if (duration <= 0.0) {
    return;
  }
  for (time_bucket = 0; time_bucket < kNumTimeBuckets; time_bucket++) {
    if (duration <= ClickDurationMetricBuckets[time_bucket]) {
      break;
    }
  }
  // Don't record clicks longer than the maximum duration we care about.
  if (time_bucket == kNumTimeBuckets) {
    return;
  }

  // Create buckets for movement distances under 10.0 mm in increments of
  // 1.0 mm, with a separate bucket for exactly 0 movement.
  int num_move_buckets = 11;
  int move_bucket = (int)std::ceil(movement);
  // Clicks with movement above 10.0 mm are assumed to be intentional drag
  // gestures.
  if (move_bucket >= num_move_buckets) {
    return;
  }
  base::UmaHistogramExactLinear(ClickDurationMetricNames[time_bucket],
                                move_bucket, num_move_buckets);
}

GestureInterpreterLibevdevCros::GestureInterpreterLibevdevCros(
    int id,
    CursorDelegateEvdev* cursor,
    GesturePropertyProvider* property_provider,
    DeviceEventDispatcherEvdev* dispatcher)
    : id_(id),
      cursor_(cursor),
      property_provider_(property_provider),
      dispatcher_(dispatcher),
      device_properties_(new GestureDeviceProperties) {
  memset(&prev_key_state_, 0, sizeof(prev_key_state_));
}

GestureInterpreterLibevdevCros::~GestureInterpreterLibevdevCros() {
  // Note that this destructor got called after the evdev device node has been
  // closed. Therefore, all clean-up codes here shouldn't depend on the device
  // information (except for the pointer address itself).

  // Clean-up if the gesture interpreter has been successfully created.
  if (interpreter_) {
    // Unset callbacks.
    GestureInterpreterSetCallback(interpreter_, NULL, NULL);
    GestureInterpreterSetPropProvider(interpreter_, NULL, NULL);
    GestureInterpreterSetTimerProvider(interpreter_, NULL, NULL);
    DeleteGestureInterpreter(interpreter_);
    interpreter_ = NULL;
  }

  // Unregister device from the gesture property provider.
  GesturesPropFunctionsWrapper::UnregisterDevice(this);
}

void GestureInterpreterLibevdevCros::OnLibEvdevCrosOpen(
    Evdev* evdev,
    EventStateRec* evstate) {
  DCHECK(evdev->info.is_monotonic) << "libevdev must use monotonic timestamps";

  // Set device pointer and initialize properties.
  evdev_ = evdev;
  GesturesPropFunctionsWrapper::InitializeDeviceProperties(
      this, device_properties_.get());
  HardwareProperties hwprops =
      GestureHardwareProperties(evdev, device_properties_.get());
  GestureInterpreterDeviceClass devclass = GestureDeviceClass(evdev);
  is_mouse_ = property_provider_->IsDeviceIdOfType(id_, DT_MOUSE);
  is_pointing_stick_ =
      property_provider_->IsDeviceIdOfType(id_, DT_POINTING_STICK);

  // Create & initialize GestureInterpreter.
  DCHECK(!interpreter_);
  interpreter_ = NewGestureInterpreter();
  GestureInterpreterSetPropProvider(
      interpreter_,
      const_cast<GesturesPropProvider*>(&kGesturePropProvider),
      this);
  GestureInterpreterInitialize(interpreter_, devclass);
  GestureInterpreterSetHardwareProperties(interpreter_, &hwprops);
  GestureInterpreterSetTimerProvider(
      interpreter_,
      const_cast<GesturesTimerProvider*>(&kGestureTimerProvider),
      this);
  GestureInterpreterSetCallback(interpreter_, OnGestureReadyHelper, this);

  if (base::FeatureList::IsEnabled(kEnableFastTouchpadClick)) {
    GesturesProp* property =
        property_provider_->GetProperty(id_, "Wiggle Button Down Timeout");
    if (property) {
      property->SetDoubleValue(std::vector<double>(1, 0.15));
    }
  }
}

void GestureInterpreterLibevdevCros::OnLibEvdevCrosEvent(Evdev* evdev,
                                                         EventStateRec* evstate,
                                                         const timeval& time) {
  stime_t timestamp = StimeFromTimeval(&time);

  // If the device has keys on it, dispatch any presses/release.
  DispatchChangedKeys(evdev->key_state_bitmask, timestamp);

  HardwareState hwstate;
  memset(&hwstate, 0, sizeof(hwstate));
  hwstate.timestamp = timestamp;

  // Mouse.
  hwstate.rel_x = evstate->rel_x;
  hwstate.rel_y = evstate->rel_y;
  hwstate.rel_wheel = evstate->rel_wheel;
  hwstate.rel_wheel_hi_res = evstate->rel_wheel_hi_res;
  hwstate.rel_hwheel = evstate->rel_hwheel;

  if (received_mouse_input_) {
    received_mouse_input_.Run(evstate->rel_x);
    received_mouse_input_.Run(evstate->rel_y);
  }

  // Touch.
  base::FixedArray<FingerState> fingers(Event_Get_Slot_Count(evdev));
  memset(fingers.data(), 0, fingers.memsize());
  int current_finger = 0;
  for (int i = 0; i < evstate->slot_count; i++) {
    MtSlotPtr slot = &evstate->slots[i];
    if (slot->tracking_id == -1)
      continue;
    fingers[current_finger].touch_major = slot->touch_major;
    fingers[current_finger].touch_minor = slot->touch_minor;
    fingers[current_finger].width_major = slot->width_major;
    fingers[current_finger].width_minor = slot->width_minor;
    fingers[current_finger].pressure = slot->pressure;
    fingers[current_finger].orientation = slot->orientation;
    fingers[current_finger].position_x = slot->position_x;
    fingers[current_finger].position_y = slot->position_y;
    fingers[current_finger].tracking_id = slot->tracking_id;
    current_finger++;
  }
  hwstate.touch_cnt = Event_Get_Touch_Count(evdev);
  hwstate.finger_cnt = current_finger;
  hwstate.fingers = fingers.data();

  // Buttons.
  if (Event_Get_Button_Left(evdev))
    hwstate.buttons_down |= GESTURES_BUTTON_LEFT;
  if (Event_Get_Button_Middle(evdev))
    hwstate.buttons_down |= GESTURES_BUTTON_MIDDLE;
  if (Event_Get_Button_Right(evdev))
    hwstate.buttons_down |= GESTURES_BUTTON_RIGHT;
  if (Event_Get_Button(evdev, BTN_BACK))
    hwstate.buttons_down |= GESTURES_BUTTON_BACK;
  if (Event_Get_Button(evdev, BTN_SIDE))
    hwstate.buttons_down |= GESTURES_BUTTON_SIDE;
  if (Event_Get_Button(evdev, BTN_FORWARD))
    hwstate.buttons_down |= GESTURES_BUTTON_FORWARD;
  if (Event_Get_Button(evdev, BTN_EXTRA))
    hwstate.buttons_down |= GESTURES_BUTTON_EXTRA;

  // Check if this event has an MSC_TIMESTAMP field
  if (EvdevBitIsSet(evdev->info.msc_bitmask, MSC_TIMESTAMP)) {
    hwstate.msc_timestamp = static_cast<stime_t>(Event_Get_Timestamp(evdev)) /
                            base::Time::kMicrosecondsPerSecond;
  } else {
    hwstate.msc_timestamp = 0.0;
  }

  GestureInterpreterPushHardwareState(interpreter_, &hwstate);
}

void GestureInterpreterLibevdevCros::OnLibEvdevCrosStopped(
    Evdev* evdev,
    EventStateRec* state) {
  stime_t timestamp = StimeNow();

  ReleaseKeys(timestamp);
  ReleaseMouseButtons(timestamp);
}

void GestureInterpreterLibevdevCros::SetupHapticButtonGeneration(
    const base::RepeatingCallback<void(bool)>& callback) {
  click_callback_ = callback;

  GesturesProp* property =
      property_provider_->GetProperty(id_, "Enable Haptic Button Generation");
  property->SetBoolValue(std::vector<bool>(1, true));
}

void GestureInterpreterLibevdevCros::OnGestureReady(const Gesture* gesture) {
  switch (gesture->type) {
    case kGestureTypeMove:
      OnGestureMove(gesture, &gesture->details.move);
      break;
    case kGestureTypeScroll:
      OnGestureScroll(gesture, &gesture->details.scroll);
      break;
    case kGestureTypeMouseWheel:
      OnGestureMouseWheel(gesture, &gesture->details.wheel);
      break;
    case kGestureTypeButtonsChange:
      OnGestureButtonsChange(gesture, &gesture->details.buttons);
      break;
    case kGestureTypeContactInitiated:
      OnGestureContactInitiated(gesture);
      break;
    case kGestureTypeFling:
      OnGestureFling(gesture, &gesture->details.fling);
      break;
    case kGestureTypeSwipe:
      OnGestureSwipe(gesture, &gesture->details.swipe);
      break;
    case kGestureTypeSwipeLift:
      OnGestureSwipeLift(gesture, &gesture->details.swipe_lift);
      break;
    case kGestureTypeFourFingerSwipe:
      OnGestureFourFingerSwipe(gesture, &gesture->details.four_finger_swipe);
      break;
    case kGestureTypeFourFingerSwipeLift:
      OnGestureFourFingerSwipeLift(gesture,
                                   &gesture->details.four_finger_swipe_lift);
      break;
    case kGestureTypePinch:
      OnGesturePinch(gesture, &gesture->details.pinch);
      break;
    case kGestureTypeMetrics:
      OnGestureMetrics(gesture, &gesture->details.metrics);
      break;
    default:
      LOG(WARNING) << base::StringPrintf("Unrecognized gesture type (%u)",
                                         gesture->type);
      break;
  }
}

void GestureInterpreterLibevdevCros::OnGestureMove(const Gesture* gesture,
                                                   const GestureMove* move) {
  DVLOG(3) << base::StringPrintf("Gesture Move: (%f, %f) [%f, %f]",
                                 move->dx,
                                 move->dy,
                                 move->ordinal_dx,
                                 move->ordinal_dy);
  if (!cursor_)
    return;  // No cursor!

  cursor_->MoveCursor(gfx::Vector2dF(move->dx, move->dy));
  gfx::Vector2dF ordinal_delta(move->ordinal_dx, move->ordinal_dy);
  click_movement_ += ordinal_delta;
  dispatcher_->DispatchMouseMoveEvent(
      MouseMoveEventParams(id_, EF_NONE, cursor_->GetLocation(), &ordinal_delta,
                           PointerDetails(EventPointerType::kMouse),
                           StimeToTimeTicks(gesture->end_time)));
}

void GestureInterpreterLibevdevCros::OnGestureScroll(
    const Gesture* gesture,
    const GestureScroll* scroll) {
  DVLOG(3) << base::StringPrintf("Gesture Scroll: (%f, %f) [%f, %f]",
                                 scroll->dx,
                                 scroll->dy,
                                 scroll->ordinal_dx,
                                 scroll->ordinal_dy);
  if (!cursor_)
    return;  // No cursor!

  if (is_mouse_) {
    // Traditional mice don't emit scroll events, but multitouch mice still do.
    dispatcher_->DispatchMouseWheelEvent(MouseWheelEventParams(
        id_, cursor_->GetLocation(), gfx::Vector2d(scroll->dx, scroll->dy),
        gfx::Vector2d(scroll->dx / kMultitouchMousePixelsPerTick * 120,
                      scroll->dy / kMultitouchMousePixelsPerTick * 120),
        StimeToTimeTicks(gesture->end_time)));
  } else {
    dispatcher_->DispatchScrollEvent(ScrollEventParams(
        id_, EventType::kScroll, cursor_->GetLocation(),
        gfx::Vector2dF(scroll->dx, scroll->dy),
        gfx::Vector2dF(scroll->ordinal_dx, scroll->ordinal_dy),
        kGestureScrollFingerCount, StimeToTimeTicks(gesture->end_time)));
  }
}

void GestureInterpreterLibevdevCros::OnGestureMouseWheel(
    const Gesture* gesture,
    const GestureMouseWheel* wheel) {
  DVLOG(3) << base::StringPrintf("Gesture Mouse Wheel: (%f, %f) [%d, %d]",
                                 wheel->dx, wheel->dy, wheel->tick_120ths_dx,
                                 wheel->tick_120ths_dy);
  if (!cursor_)
    return;  // No cursor!

  dispatcher_->DispatchMouseWheelEvent(MouseWheelEventParams(
      id_, cursor_->GetLocation(), gfx::Vector2d(wheel->dx, wheel->dy),
      gfx::Vector2d(wheel->tick_120ths_dx, wheel->tick_120ths_dy),
      StimeToTimeTicks(gesture->end_time)));
}

void GestureInterpreterLibevdevCros::OnGestureButtonsChange(
    const Gesture* gesture,
    const GestureButtonsChange* buttons) {
  DVLOG(3) << base::StringPrintf("Gesture Button Change: down=0x%02x up=0x%02x",
                                 buttons->down,
                                 buttons->up);

  if (!cursor_)
    return;  // No cursor!

  if (!buttons->is_tap && click_callback_) {
    click_callback_.Run(buttons->down);
  }

  DispatchChangedMouseButtons(buttons->down, true, gesture->end_time);
  DispatchChangedMouseButtons(buttons->up, false, gesture->end_time);
}

void GestureInterpreterLibevdevCros::OnGestureContactInitiated(
    const Gesture* gesture) {
  // TODO(spang): handle contact initiated.
}

void GestureInterpreterLibevdevCros::OnGestureFling(const Gesture* gesture,
                                                    const GestureFling* fling) {
  DVLOG(3) << base::StringPrintf(
                  "Gesture Fling: (%f, %f) [%f, %f] fling_state=%d",
                  fling->vx,
                  fling->vy,
                  fling->ordinal_vx,
                  fling->ordinal_vy,
                  fling->fling_state);

  if (!cursor_)
    return;  // No cursor!

  EventType type = (fling->fling_state == GESTURES_FLING_START
                        ? EventType::kScrollFlingStart
                        : EventType::kScrollFlingCancel);

  // Fling is like 2-finger scrolling but with velocity instead of displacement.
  dispatcher_->DispatchScrollEvent(ScrollEventParams(
      id_, type, cursor_->GetLocation(), gfx::Vector2dF(fling->vx, fling->vy),
      gfx::Vector2dF(fling->ordinal_vx, fling->ordinal_vy),
      kGestureScrollFingerCount, StimeToTimeTicks(gesture->end_time)));
}

void GestureInterpreterLibevdevCros::OnGestureSwipe(const Gesture* gesture,
                                                    const GestureSwipe* swipe) {
  DVLOG(3) << base::StringPrintf("Gesture Swipe: (%f, %f) [%f, %f]",
                                 swipe->dx,
                                 swipe->dy,
                                 swipe->ordinal_dx,
                                 swipe->ordinal_dy);

  if (!cursor_)
    return;  // No cursor!

  // Swipe is 3-finger scrolling.
  dispatcher_->DispatchScrollEvent(ScrollEventParams(
      id_, EventType::kScroll, cursor_->GetLocation(),
      gfx::Vector2dF(swipe->dx, swipe->dy),
      gfx::Vector2dF(swipe->ordinal_dx, swipe->ordinal_dy),
      kGestureSwipeFingerCount, StimeToTimeTicks(gesture->end_time)));
}

void GestureInterpreterLibevdevCros::OnGestureSwipeLift(
    const Gesture* gesture,
    const GestureSwipeLift* swipelift) {
  DVLOG(3) << base::StringPrintf("Gesture Swipe Lift");

  if (!cursor_)
    return;  // No cursor!

  // Turn a swipe lift into a fling start.
  // TODO(spang): Figure out why and put it in this comment.

  dispatcher_->DispatchScrollEvent(ScrollEventParams(
      id_, EventType::kScrollFlingStart, cursor_->GetLocation(),
      gfx::Vector2dF() /* delta */, gfx::Vector2dF() /* ordinal_delta */,
      kGestureScrollFingerCount, StimeToTimeTicks(gesture->end_time)));
}

void GestureInterpreterLibevdevCros::OnGestureFourFingerSwipe(
    const Gesture* gesture,
    const GestureFourFingerSwipe* swipe) {
  DVLOG(3) << base::StringPrintf("Gesture Four Finger Swipe: (%f, %f) [%f, %f]",
                                 swipe->dx, swipe->dy, swipe->ordinal_dx,
                                 swipe->ordinal_dy);

  if (!cursor_)
    return;  // No cursor!

  dispatcher_->DispatchScrollEvent(ScrollEventParams(
      id_, EventType::kScroll, cursor_->GetLocation(),
      gfx::Vector2dF(swipe->dx, swipe->dy),
      gfx::Vector2dF(swipe->ordinal_dx, swipe->ordinal_dy),
      /*finger_count=*/4, StimeToTimeTicks(gesture->end_time)));
}

void GestureInterpreterLibevdevCros::OnGestureFourFingerSwipeLift(
    const Gesture* gesture,
    const GestureFourFingerSwipeLift* swipe) {
  DVLOG(3) << base::StringPrintf("Gesture Four Finger Swipe Lift");

  if (!cursor_)
    return;  // No cursor!

  // Turn a swipe lift into a fling start.
  // TODO(spang): Figure out why and put it in this comment.

  dispatcher_->DispatchScrollEvent(ScrollEventParams(
      id_, EventType::kScrollFlingStart, cursor_->GetLocation(),
      /*delta=*/gfx::Vector2dF(), /*ordinal_delta=*/gfx::Vector2dF(),
      /*finger_count=*/4, StimeToTimeTicks(gesture->end_time)));
}

void GestureInterpreterLibevdevCros::OnGesturePinch(const Gesture* gesture,
                                                    const GesturePinch* pinch) {
  DVLOG(3) << base::StringPrintf("Gesture Pinch: dz=%f [%f] zoom_state=%u",
                                 pinch->dz, pinch->ordinal_dz,
                                 pinch->zoom_state);

  if (!cursor_)
    return;  // No cursor!

  EventType type;
  switch (pinch->zoom_state) {
    case GESTURES_ZOOM_START:
      type = EventType::kGesturePinchBegin;
      break;
    case GESTURES_ZOOM_UPDATE:
      type = EventType::kGesturePinchUpdate;
      break;
    case GESTURES_ZOOM_END:
      type = EventType::kGesturePinchEnd;
      break;
    default:
      LOG(WARNING) << base::StringPrintf("Unrecognized pinch zoom state (%u)",
                                         pinch->zoom_state);
      return;
  }
  dispatcher_->DispatchPinchEvent(
      PinchEventParams(id_, type, cursor_->GetLocation(), pinch->dz,
                       StimeToTimeTicks(gesture->end_time)));
}

void GestureInterpreterLibevdevCros::OnGestureMetrics(
    const Gesture* gesture,
    const GestureMetrics* metrics) {
  DVLOG(3) << base::StringPrintf("Gesture Metrics: [%f, %f] type=%d",
                                 metrics->data[0],
                                 metrics->data[1],
                                 metrics->type);

  // TODO(spang): Hook up metrics.
}

void GestureInterpreterLibevdevCros::DispatchChangedMouseButtons(
    unsigned int changed_buttons, bool down, stime_t time) {
  if (changed_buttons & GESTURES_BUTTON_LEFT)
    DispatchMouseButton(BTN_LEFT, down, time);
  if (changed_buttons & GESTURES_BUTTON_MIDDLE)
    DispatchMouseButton(BTN_MIDDLE, down, time);
  if (changed_buttons & GESTURES_BUTTON_RIGHT)
    DispatchMouseButton(BTN_RIGHT, down, time);
  if (changed_buttons & GESTURES_BUTTON_BACK)
    DispatchMouseButton(BTN_BACK, down, time);
  if (changed_buttons & GESTURES_BUTTON_FORWARD)
    DispatchMouseButton(BTN_FORWARD, down, time);
  if (changed_buttons & GESTURES_BUTTON_EXTRA)
    DispatchMouseButton(BTN_EXTRA, down, time);
  if (changed_buttons & GESTURES_BUTTON_SIDE)
    DispatchMouseButton(BTN_SIDE, down, time);
}

void GestureInterpreterLibevdevCros::DispatchMouseButton(unsigned int button,
                                                         bool down,
                                                         stime_t time) {
  if (!SetMouseButtonState(button, down))
    return;  // No change.

  if (!is_mouse_ && !is_pointing_stick_ && button == BTN_LEFT) {
    if (down) {
      click_down_time_ = time;
      click_movement_.set_x(0);
      click_movement_.set_y(0);
    } else {
      RecordClickMetric(time - click_down_time_, click_movement_.Length());
    }
  }

  MouseButtonMapType map_type = MouseButtonMapType::kNone;
  if (is_mouse_)
    map_type = MouseButtonMapType::kMouse;
  else if (is_pointing_stick_)
    map_type = MouseButtonMapType::kPointingStick;

  dispatcher_->DispatchMouseButtonEvent(MouseButtonEventParams(
      id_, EF_NONE, cursor_->GetLocation(), button, down, map_type,
      PointerDetails(EventPointerType::kMouse), StimeToTimeTicks(time)));
}

void GestureInterpreterLibevdevCros::SetReceivedValidKeyboardInputCallback(
    base::RepeatingCallback<void(uint64_t)> callback) {
  received_keyboard_input_ = std::move(callback);
}

void GestureInterpreterLibevdevCros::SetReceivedValidMouseInputCallback(
    base::RepeatingCallback<void(int)> callback) {
  received_mouse_input_ = std::move(callback);
}

void GestureInterpreterLibevdevCros::DispatchChangedKeys(
    unsigned long* new_key_state,
    stime_t timestamp) {
  unsigned long key_state_diff[EVDEV_BITS_TO_LONGS(KEY_CNT)];

  // Clear any set modifiers so they do not generate downstream events.
  if (block_modifiers_) {
    for (const auto key : kModifierEvdevCodes) {
      if (EvdevBitIsSet(new_key_state, key)) {
        EvdevClearBit(new_key_state, key);
      }
    }
  }

  // Find changed keys.
  for (unsigned long i = 0; i < std::size(key_state_diff); ++i)
    key_state_diff[i] = new_key_state[i] ^ prev_key_state_[i];

  // Dispatch events for changed keys.
  for (unsigned long key = 0; key < KEY_CNT; ++key) {
    if (EvdevBitIsSet(key_state_diff, key)) {
      bool value = EvdevBitIsSet(new_key_state, key);

      // Mouse buttons are handled by DispatchMouseButton.
      if (key >= BTN_MOUSE && key < BTN_JOYSTICK)
        continue;

      // Ignore digi buttons (e.g. BTN_TOOL_FINGER).
      if (key >= BTN_DIGI && key < BTN_WHEEL)
        continue;

      // Checks for a key press that could only have occurred from a
      // non-imposter keyboard. Disables Imposter flag and triggers a callback
      // which will update the dispatched list of keyboards with this new
      // information.
      if (received_keyboard_input_) {
        received_keyboard_input_.Run(key);
      }

      // Dispatch key press or release to keyboard.
      dispatcher_->DispatchKeyEvent(KeyEventParams(
          id_, ui::EF_NONE, key, 0 /* scan_code */, value,
          false /* suppress_auto_repeat */, StimeToTimeTicks(timestamp)));
    }
  }

  // Update internal key state.
  for (unsigned long i = 0; i < EVDEV_BITS_TO_LONGS(KEY_CNT); ++i)
    prev_key_state_[i] = new_key_state[i];
}

void GestureInterpreterLibevdevCros::ReleaseKeys(stime_t timestamp) {
  unsigned long new_key_state[EVDEV_BITS_TO_LONGS(KEY_CNT)];
  memset(&new_key_state, 0, sizeof(new_key_state));

  DispatchChangedKeys(new_key_state, timestamp);
}

bool GestureInterpreterLibevdevCros::SetMouseButtonState(unsigned int button,
                                                         bool down) {
  DCHECK(BTN_MOUSE <= button && button < BTN_JOYSTICK);
  int button_offset = button - BTN_MOUSE;

  if (mouse_button_state_.test(button_offset) == down)
    return false;

  // State transition: !(down) -> (down)
  mouse_button_state_.set(button_offset, down);

  return true;
}

void GestureInterpreterLibevdevCros::ReleaseMouseButtons(stime_t timestamp) {
  DispatchMouseButton(BTN_LEFT, false /* down */, timestamp);
  DispatchMouseButton(BTN_MIDDLE, false /* down */, timestamp);
  DispatchMouseButton(BTN_RIGHT, false /* down */, timestamp);
  DispatchMouseButton(BTN_BACK, false /* down */, timestamp);
  DispatchMouseButton(BTN_FORWARD, false /* down */, timestamp);
}

void GestureInterpreterLibevdevCros::SetBlockModifiers(bool block_modifiers) {
  // Release held modifiers if we are changing from not blocking modifiers ->
  // blocking modifiers.
  const bool should_release_held_modifiers =
      block_modifiers && !block_modifiers_;
  block_modifiers_ = block_modifiers;

  // If we should release held modifiers, create just a copy of
  // `prev_key_state_` to represent the new state. `DispatchChangedKeys` will
  // update it in the normal code path to remove pressed modifier keys which
  // will in turn generate the release events.
  if (should_release_held_modifiers) {
    unsigned long copy_key_state[EVDEV_BITS_TO_LONGS(KEY_CNT)];
    static_assert(sizeof(copy_key_state) == sizeof(prev_key_state_));
    memcpy(copy_key_state, prev_key_state_, sizeof(prev_key_state_));
    DispatchChangedKeys(copy_key_state,
                        ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  }
}

}  // namespace ui
