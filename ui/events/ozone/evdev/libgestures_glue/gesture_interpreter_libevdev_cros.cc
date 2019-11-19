// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h"

#include <gestures/gestures.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/timer/timer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_util.h"
#include "ui/events/ozone/evdev/keyboard_util_evdev.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_timer_provider.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

namespace {

// Convert libevdev device class to libgestures device class.
GestureInterpreterDeviceClass GestureDeviceClass(Evdev* evdev) {
  switch (evdev->info.evdev_class) {
    case EvdevClassMouse:
      return GESTURES_DEVCLASS_MOUSE;
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
  hwprops.screen_x_dpi = 133;
  hwprops.screen_y_dpi = 133;
  hwprops.orientation_minimum = props->orientation_minimum;
  hwprops.orientation_maximum = props->orientation_maximum;
  hwprops.max_finger_cnt = Event_Get_Slot_Count(evdev);
  hwprops.max_touch_cnt = Event_Get_Touch_Count_Max(evdev);
  hwprops.supports_t5r2 = Event_Get_T5R2(evdev);
  hwprops.support_semi_mt = Event_Get_Semi_MT(evdev);
  /* buttonpad means a physical button under the touch surface */
  hwprops.is_button_pad = Event_Get_Button_Pad(evdev);
  hwprops.has_wheel = EvdevBitIsSet(evdev->info.rel_bitmask, REL_WHEEL) ||
                      EvdevBitIsSet(evdev->info.rel_bitmask, REL_HWHEEL);

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

}  // namespace

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
  VLOG(9) << "HACK DO NOT REMOVE OR LINK WILL FAIL" << (void*)gestures_log;

  // Set device pointer and initialize properties.
  evdev_ = evdev;
  GesturesPropFunctionsWrapper::InitializeDeviceProperties(
      this, device_properties_.get());
  HardwareProperties hwprops =
      GestureHardwareProperties(evdev, device_properties_.get());
  GestureInterpreterDeviceClass devclass = GestureDeviceClass(evdev);
  is_mouse_ = property_provider_->IsDeviceIdOfType(id_, DT_MOUSE);

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
  hwstate.rel_hwheel = evstate->rel_hwheel;

  // Touch.
  FingerState fingers[Event_Get_Slot_Count(evdev)];
  memset(&fingers, 0, sizeof(fingers));
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
  hwstate.fingers = fingers;

  // Buttons.
  if (Event_Get_Button_Left(evdev))
    hwstate.buttons_down |= GESTURES_BUTTON_LEFT;
  if (Event_Get_Button_Middle(evdev))
    hwstate.buttons_down |= GESTURES_BUTTON_MIDDLE;
  if (Event_Get_Button_Right(evdev))
    hwstate.buttons_down |= GESTURES_BUTTON_RIGHT;
  if (Event_Get_Button(evdev, BTN_SIDE) ||
      Event_Get_Button(evdev, BTN_BACK)) {
    hwstate.buttons_down |= GESTURES_BUTTON_BACK;
  }
  if (Event_Get_Button(evdev, BTN_EXTRA) ||
      Event_Get_Button(evdev, BTN_FORWARD)) {
    hwstate.buttons_down |= GESTURES_BUTTON_FORWARD;
  }

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

void GestureInterpreterLibevdevCros::OnGestureReady(const Gesture* gesture) {
  switch (gesture->type) {
    case kGestureTypeMove:
      OnGestureMove(gesture, &gesture->details.move);
      break;
    case kGestureTypeScroll:
      OnGestureScroll(gesture, &gesture->details.scroll);
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
  // TODO(spang): Use move->ordinal_dx, move->ordinal_dy
  dispatcher_->DispatchMouseMoveEvent(
      MouseMoveEventParams(id_, EF_NONE, cursor_->GetLocation(),
                           PointerDetails(EventPointerType::POINTER_TYPE_MOUSE),
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
    dispatcher_->DispatchMouseWheelEvent(MouseWheelEventParams(
        id_, cursor_->GetLocation(), gfx::Vector2d(scroll->dx, scroll->dy),
        StimeToTimeTicks(gesture->end_time)));
  } else {
    dispatcher_->DispatchScrollEvent(ScrollEventParams(
        id_, ET_SCROLL, cursor_->GetLocation(),
        gfx::Vector2dF(scroll->dx, scroll->dy),
        gfx::Vector2dF(scroll->ordinal_dx, scroll->ordinal_dy),
        kGestureScrollFingerCount, StimeToTimeTicks(gesture->end_time)));
  }
}

void GestureInterpreterLibevdevCros::OnGestureButtonsChange(
    const Gesture* gesture,
    const GestureButtonsChange* buttons) {
  DVLOG(3) << base::StringPrintf("Gesture Button Change: down=0x%02x up=0x%02x",
                                 buttons->down,
                                 buttons->up);

  if (!cursor_)
    return;  // No cursor!

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

  EventType type =
      (fling->fling_state == GESTURES_FLING_START ? ET_SCROLL_FLING_START
                                                  : ET_SCROLL_FLING_CANCEL);

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
      id_, ET_SCROLL, cursor_->GetLocation(),
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
      id_, ET_SCROLL_FLING_START, cursor_->GetLocation(),
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
      id_, ET_SCROLL, cursor_->GetLocation(),
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
      id_, ET_SCROLL_FLING_START, cursor_->GetLocation(),
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
      type = ET_GESTURE_PINCH_BEGIN;
      break;
    case GESTURES_ZOOM_UPDATE:
      type = ET_GESTURE_PINCH_UPDATE;
      break;
    case GESTURES_ZOOM_END:
      type = ET_GESTURE_PINCH_END;
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
}

void GestureInterpreterLibevdevCros::DispatchMouseButton(unsigned int button,
                                                         bool down,
                                                         stime_t time) {
  if (!SetMouseButtonState(button, down))
    return;  // No change.

  bool allow_remap = is_mouse_;
  dispatcher_->DispatchMouseButtonEvent(MouseButtonEventParams(
      id_, EF_NONE, cursor_->GetLocation(), button, down, allow_remap,
      PointerDetails(EventPointerType::POINTER_TYPE_MOUSE),
      StimeToTimeTicks(time)));
}

void GestureInterpreterLibevdevCros::DispatchChangedKeys(
    unsigned long* new_key_state,
    stime_t timestamp) {
  unsigned long key_state_diff[EVDEV_BITS_TO_LONGS(KEY_CNT)];

  // Find changed keys.
  for (unsigned long i = 0; i < base::size(key_state_diff); ++i)
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

      // Dispatch key press or release to keyboard.
      dispatcher_->DispatchKeyEvent(
          KeyEventParams(id_, key, value, false /* suppress_auto_repeat */,
                         StimeToTimeTicks(timestamp)));
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

}  // namespace ui
