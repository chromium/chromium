// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/event_reader_libevdev_cros.h"

#include <errno.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

namespace ui {

namespace {

std::string FormatLog(const char* fmt, va_list args) {
  std::string msg = base::StringPrintV(fmt, args);
  if (!msg.empty() && msg.back() == '\n')
    msg.pop_back();
  return msg;
}

}  // namespace

EventReaderLibevdevCros::EventReaderLibevdevCros(
    base::ScopedFD fd,
    const base::FilePath& path,
    int id,
    const EventDeviceInfo& devinfo,
    std::unique_ptr<Delegate> delegate)
    : EventConverterEvdev(fd.get(),
                          path,
                          id,
                          devinfo.device_type(),
                          devinfo.name(),
                          devinfo.phys(),
                          devinfo.vendor_id(),
                          devinfo.product_id(),
                          devinfo.version()),
      has_keyboard_(devinfo.HasKeyboard()),
      has_mouse_(devinfo.HasMouse()),
      has_pointing_stick_(devinfo.HasPointingStick()),
      has_touchpad_(devinfo.HasTouchpad()),
      has_stylus_switch_(devinfo.HasSwEvent(SW_PEN_INSERTED)),
      has_caps_lock_led_(devinfo.HasLedEvent(LED_CAPSL)),
      touch_count_(0),
      delegate_(std::move(delegate)) {
  // This class assumes it does not deal with internal keyboards.
  CHECK(!has_keyboard_ || type() != INPUT_DEVICE_INTERNAL);

  memset(&evdev_, 0, sizeof(evdev_));
  evdev_.log = OnLogMessage;
  evdev_.log_udata = this;
  evdev_.syn_report = OnSynReport;
  evdev_.syn_report_udata = this;
  evdev_.fd = fd.release();

  memset(&evstate_, 0, sizeof(evstate_));
  evdev_.evstate = &evstate_;
  Event_Init(&evdev_);

  Event_Open(&evdev_);

  haptic_touchpad_handler_ = HapticTouchpadHandler::Create(fd_, devinfo);
  delegate_->OnLibEvdevCrosOpen(&evdev_, &evstate_);
  if (haptic_touchpad_handler_) {
    delegate_->SetupHapticButtonGeneration(
        base::BindRepeating(&HapticTouchpadHandler::OnGestureClick,
                            base::Unretained(haptic_touchpad_handler_.get())));
  }
}

EventReaderLibevdevCros::~EventReaderLibevdevCros() {
  Stop();
  EvdevClose(&evdev_);
}

EventReaderLibevdevCros::Delegate::~Delegate() {}

void EventReaderLibevdevCros::OnFileCanReadWithoutBlocking(int fd) {
  TRACE_EVENT1("evdev", "EventReaderLibevdevCros::OnFileCanReadWithoutBlocking",
               "fd", fd);

  if (EvdevRead(&evdev_)) {
    if (errno == EINTR || errno == EAGAIN)
      return;
    if (errno != ENODEV)
      PLOG(ERROR) << "error reading device " << path_.value();
    Stop();
    return;
  }
}

bool EventReaderLibevdevCros::HasKeyboard() const {
  return has_keyboard_;
}

bool EventReaderLibevdevCros::HasMouse() const {
  return has_mouse_;
}

bool EventReaderLibevdevCros::HasPointingStick() const {
  return has_pointing_stick_;
}

bool EventReaderLibevdevCros::HasTouchpad() const {
  return has_touchpad_;
}

bool EventReaderLibevdevCros::HasHapticTouchpad() const {
  return haptic_touchpad_handler_ != nullptr;
}

bool EventReaderLibevdevCros::CanHandleHapticFeedback() const {
  return haptic_touchpad_handler_ && haptic_feedback_enabled_ &&
         touch_count_ > 0;
}

void EventReaderLibevdevCros::PlayHapticTouchpadEffect(
    HapticTouchpadEffect effect,
    HapticTouchpadEffectStrength strength) {
  if (CanHandleHapticFeedback())
    haptic_touchpad_handler_->PlayEffect(effect, strength);
}

void EventReaderLibevdevCros::SetHapticTouchpadEffectForNextButtonRelease(
    HapticTouchpadEffect effect,
    HapticTouchpadEffectStrength strength) {
  if (CanHandleHapticFeedback())
    haptic_touchpad_handler_->SetEffectForNextButtonRelease(effect, strength);
}

void EventReaderLibevdevCros::ApplyDeviceSettings(
    const InputDeviceSettingsEvdev& settings) {
  const auto& touchpad_settings = settings.GetTouchpadSettings(id());
  if (haptic_touchpad_handler_) {
    haptic_touchpad_handler_->SetClickStrength(
        static_cast<HapticTouchpadEffectStrength>(
            touchpad_settings.haptic_click_sensitivity));
  }
  haptic_feedback_enabled_ = touchpad_settings.haptic_feedback_enabled;
}

void EventReaderLibevdevCros::ReceivedKeyboardInput(uint64_t key) {
  if (!IsSuspectedKeyboardImposter() || !IsValidKeyboardKeyPress(key)) {
    return;
  }

  SetSuspectedKeyboardImposter(false);
  received_valid_input_callback_.Run(this);
}

void EventReaderLibevdevCros::ReceivedMouseInput(int rel_value) {
  if (!IsSuspectedMouseImposter() || rel_value == 0) {
    return;
  }

  SetSuspectedMouseImposter(false);
  received_valid_input_callback_.Run(this);
}

void EventReaderLibevdevCros::SetReceivedValidInputCallback(
    ReceivedValidInputCallback callback) {
  delegate_->SetReceivedValidKeyboardInputCallback(base::BindRepeating(
      &EventReaderLibevdevCros::ReceivedKeyboardInput, base::Unretained(this)));
  delegate_->SetReceivedValidMouseInputCallback(base::BindRepeating(
      &EventReaderLibevdevCros::ReceivedMouseInput, base::Unretained(this)));
  received_valid_input_callback_ = std::move(callback);
}

void EventReaderLibevdevCros::SetBlockModifiers(bool block_modifiers) {
  delegate_->SetBlockModifiers(block_modifiers);
}

bool EventReaderLibevdevCros::HasCapsLockLed() const {
  return has_caps_lock_led_;
}

bool EventReaderLibevdevCros::HasStylusSwitch() const {
  return has_stylus_switch_;
}

void EventReaderLibevdevCros::OnDisabled() {
  delegate_->OnLibEvdevCrosStopped(&evdev_, &evstate_);
}

std::ostream& EventReaderLibevdevCros::DescribeForLog(std::ostream& os) const {
  os << "class=EventReaderLibevdevCros id=" << input_device_.id << std::endl
     << " has_keyboard=" << has_keyboard_ << std::endl
     << " has_mouse=" << has_mouse_ << std::endl
     << " has_pointing_stick=" << has_pointing_stick_ << std::endl
     << " HasHapticTouchpad=" << HasHapticTouchpad() << std::endl
     << " CanHandleHapticFeedback=" << CanHandleHapticFeedback() << std::endl
     << " has_caps_lock_led=" << has_caps_lock_led_ << std::endl
     << " has_stylus_switch=" << has_stylus_switch_ << std::endl
     << "base ";
  return EventConverterEvdev::DescribeForLog(os);
}

// static
void EventReaderLibevdevCros::OnSynReport(void* data,
                                          EventStateRec* evstate,
                                          struct timeval* tv) {
  EventReaderLibevdevCros* reader = static_cast<EventReaderLibevdevCros*>(data);
  if (!reader->IsEnabled())
    return;

  reader->touch_count_ = Event_Get_Touch_Count(&reader->evdev_);
  reader->delegate_->OnLibEvdevCrosEvent(&reader->evdev_, evstate, *tv);
}

// static
void EventReaderLibevdevCros::OnLogMessage(void* data,
                                           int level,
                                           const char* fmt,
                                           ...) {
  va_list args;
  va_start(args, fmt);
  if (level >= LOGLEVEL_ERROR)
    LOG(ERROR) << "libevdev: " << FormatLog(fmt, args);
  else if (level >= LOGLEVEL_WARNING)
    LOG(WARNING) << "libevdev: " << FormatLog(fmt, args);
  else
    DVLOG(3) << "libevdev: " << FormatLog(fmt, args);
  va_end(args);
}

}  // namespace ui
