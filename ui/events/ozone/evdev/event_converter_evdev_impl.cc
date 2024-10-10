// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ozone/evdev/event_converter_evdev_impl.h"

#include <errno.h>
#include <linux/input.h>
#include <stddef.h>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_device_util.h"
#include "ui/events/ozone/evdev/numberpad_metrics.h"
#include "ui/events/ozone/features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/events/ozone/evdev/numberpad_metrics.h"
#endif

namespace ui {

namespace {

// Values for EV_KEY.
const int kKeyReleaseValue = 0;
const int kKeyRepeatValue = 2;

// Values for the EV_SW code.
const int kSwitchStylusInserted = SW_PEN_INSERTED;

// Telephony Device Page (0x0B) Phone Mute (0x2F) is defined in
// https://usb.org/sites/default/files/hut1_5.pdf.
const int kTelephonyDevicePhoneMute = 0x0b002f;

constexpr unsigned int kModifierEvdevCodes[] = {
    KEY_LEFTALT,  KEY_RIGHTALT,  KEY_LEFTMETA,  KEY_RIGHTMETA,
    KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTSHIFT, KEY_RIGHTSHIFT};

}  // namespace

EventConverterEvdevImpl::EventConverterEvdevImpl(
    base::ScopedFD fd,
    base::FilePath path,
    int id,
    const EventDeviceInfo& devinfo,
    CursorDelegateEvdev* cursor,
    DeviceEventDispatcherEvdev* dispatcher)
    : EventConverterEvdev(fd.get(),
                          path,
                          id,
                          devinfo.device_type(),
                          devinfo.name(),
                          devinfo.phys(),
                          devinfo.vendor_id(),
                          devinfo.product_id(),
                          devinfo.version()),
      input_device_fd_(std::move(fd)),
      keyboard_type_(devinfo.GetKeyboardType()),
      has_touchpad_(devinfo.HasTouchpad()),
      has_numberpad_(devinfo.HasNumberpad()),
      has_stylus_switch_(devinfo.HasStylusSwitch()),
      has_assistant_key_(devinfo.HasKeyEvent(KEY_ASSISTANT)),
      has_function_key_(devinfo.HasKeyEvent(KEY_FN)),
      has_caps_lock_led_(devinfo.HasLedEvent(LED_CAPSL)),
      controller_(FROM_HERE),
      cursor_(cursor),
      dispatcher_(dispatcher) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (has_numberpad_)
    NumberpadMetricsRecorder::GetInstance()->AddDevice(input_device_);
#endif
  // Converts unsigned long to uint64_t.
  const auto key_bits = devinfo.GetKeyBits();
  key_bits_.resize(EVDEV_BITS_TO_INT64(KEY_CNT));
  for (int i = 0; i < KEY_CNT; i++) {
    if (EvdevBitIsSet(key_bits.data(), i)) {
      EvdevSetUint64Bit(key_bits_.data(), i);
    }
  }

  if (base::FeatureList::IsEnabled(kBlockTelephonyDevicePhoneMute)) {
    block_telephony_device_phone_mute_ = true;
  }
}

EventConverterEvdevImpl::~EventConverterEvdevImpl() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (has_numberpad_)
    NumberpadMetricsRecorder::GetInstance()->RemoveDevice(input_device_);
#endif
}

void EventConverterEvdevImpl::OnFileCanReadWithoutBlocking(int fd) {
  TRACE_EVENT1("evdev", "EventConverterEvdevImpl::OnFileCanReadWithoutBlocking",
               "fd", fd);

  input_event inputs[4];
  ssize_t read_size = read(fd, inputs, sizeof(inputs));
  if (read_size < 0) {
    if (errno == EINTR || errno == EAGAIN)
      return;
    if (errno != ENODEV)
      PLOG(ERROR) << "error reading device " << path_.value();
    Stop();
    return;
  }

  if (!IsEnabled())
    return;

  DCHECK_EQ(read_size % sizeof(*inputs), 0u);
  ProcessEvents(inputs, read_size / sizeof(*inputs));
}

KeyboardType EventConverterEvdevImpl::GetKeyboardType() const {
  return keyboard_type_;
}

bool EventConverterEvdevImpl::HasKeyboard() const {
  return keyboard_type_ == KeyboardType::VALID_KEYBOARD;
}

bool EventConverterEvdevImpl::HasTouchpad() const {
  return has_touchpad_;
}

bool EventConverterEvdevImpl::HasCapsLockLed() const {
  return has_caps_lock_led_;
}

bool EventConverterEvdevImpl::HasStylusSwitch() const {
  return has_stylus_switch_;
}

bool EventConverterEvdevImpl::HasAssistantKey() const {
  return has_assistant_key_;
}

bool EventConverterEvdevImpl::HasFunctionKey() const {
  return has_function_key_;
}

void EventConverterEvdevImpl::SetKeyFilter(bool enable_filter,
                                           std::vector<DomCode> allowed_keys) {
  if (!enable_filter) {
    blocked_keys_.reset();
    return;
  }

  blocked_keys_.set();
  for (const DomCode& code : allowed_keys) {
    blocked_keys_.reset(KeycodeConverter::DomCodeToEvdevCode(code));
  }

  // Release any pressed blocked keys.
  base::TimeTicks timestamp = ui::EventTimeForNow();
  for (int key = 0; key < KEY_CNT; ++key) {
    if (blocked_keys_.test(key))
      OnKeyChange(key, false /* down */, timestamp);
  }
}

void EventConverterEvdevImpl::SetBlockModifiers(bool block_modifiers) {
  // Release held modifiers if we are changing from not blocking modifiers ->
  // blocking modifiers.
  const bool should_release_held_modifiers =
      block_modifiers && !block_modifiers_;
  if (should_release_held_modifiers) {
    base::TimeTicks timestamp = ui::EventTimeForNow();
    for (const int key : kModifierEvdevCodes) {
      if (key_state_.test(key)) {
        OnKeyChange(key, false /* down */, timestamp);
      }
    }
  }

  // Update flag for blocking modifiers only after releasing the already pressed
  // keys.
  block_modifiers_ = block_modifiers;
}

void EventConverterEvdevImpl::OnDisabled() {
  ReleaseKeys();
  ReleaseMouseButtons();
}

std::vector<uint64_t> EventConverterEvdevImpl::GetKeyboardKeyBits() const {
  return key_bits_;
}

ui::StylusState EventConverterEvdevImpl::GetStylusSwitchState() {
  if (!HasStylusSwitch()) {
    return ui::StylusState::REMOVED;
  }

  // Prepare storage for SW_MAX bits
  unsigned long array[EVDEV_BITS_TO_LONGS(SW_MAX)] = {0};
  int result = ioctl(input_device_fd_.get(), EVIOCGSW(SW_MAX), array);
  if (result == -1) {
    return ui::StylusState::REMOVED;
  }

  return EvdevBitIsSet(array, kSwitchStylusInserted) ? ui::StylusState::INSERTED
                                                     : ui::StylusState::REMOVED;
}

void EventConverterEvdevImpl::ProcessEvents(const input_event* inputs,
                                            int count) {
  for (int i = 0; i < count; ++i) {
    const input_event& input = inputs[i];
    switch (input.type) {
      case EV_MSC:
        if (input.code == MSC_SCAN)
          last_scan_code_ = input.value;
        break;
      case EV_KEY:
        ConvertKeyEvent(input);
        last_scan_code_ = 0;
        break;
      case EV_REL:
        ConvertMouseMoveEvent(input);
        break;
      case EV_SYN:
        if (input.code == SYN_DROPPED)
          OnLostSync();
        else if (input.code == SYN_REPORT)
          FlushEvents(input);
        last_scan_code_ = 0;
        break;
      case EV_SW:
        if (input.code == kSwitchStylusInserted) {
          dispatcher_->DispatchStylusStateChanged(
              input.value ? ui::StylusState::INSERTED
                          : ui::StylusState::REMOVED);
        }
        break;
    }
  }
}

void EventConverterEvdevImpl::ConvertKeyEvent(const input_event& input) {
  // Ignore repeat events.
  if (input.value == kKeyRepeatValue)
    return;

  // Mouse processing.
  if (input.code >= BTN_MOUSE && input.code < BTN_JOYSTICK) {
    DispatchMouseButton(input);
    return;
  }

  // Keyboard processing.
  OnKeyChange(input.code, input.value != kKeyReleaseValue,
              TimeTicksFromInputEvent(input));
}

void EventConverterEvdevImpl::ConvertMouseMoveEvent(const input_event& input) {
  if (!cursor_)
    return;
  switch (input.code) {
    case REL_X:
      x_offset_ = input.value;
      break;
    case REL_Y:
      y_offset_ = input.value;
      break;
  }
}

void EventConverterEvdevImpl::OnKeyChange(unsigned int key,
                                          bool down,
                                          const base::TimeTicks& timestamp) {
  if (key > KEY_MAX)
    return;

  // TODO: crbug.com/356306613 - Sync mute state between telephony devices and
  // CrOS
  if (block_telephony_device_phone_mute_) {
    // Ignore Telephony Phone Mute scan code so that it does not toggle system
    // mic mute to resolve user confusions. We don't want to block `KEY_MICMUTE`
    // as there are other scan codes that map to the same key code. Not suitable
    // to use `blocked_keys_`.
    if (key == KEY_MICMUTE && last_scan_code_ == kTelephonyDevicePhoneMute) {
      return;
    }
  }

  if (down == key_state_.test(key))
    return;

  // Apply key filter (releases for previously pressed keys are excepted).
  if (down && blocked_keys_.test(key))
    return;

  // Block all modifiers from continuing down stream from this device if the
  // flag is set.
  if (block_modifiers_ && base::Contains(kModifierEvdevCodes, key)) {
    return;
  }

  // State transition: !(down) -> (down)
  key_state_.set(key, down);

  GenerateKeyMetrics(key, down);

  // Checks for a key press that could only have occurred from a non-imposter
  // keyboard. Disables Imposter flag and triggers a callback which will update
  // the dispatched list of keyboards with this new information.
  if (key_state_.count() == 1 && IsValidKeyboardKeyPress(key)) {
    bool was_suspected = IsSuspectedKeyboardImposter();
    SetSuspectedKeyboardImposter(false);
    if (was_suspected && received_valid_input_callback_) {
      received_valid_input_callback_.Run(this);
    }
  }

  dispatcher_->DispatchKeyEvent(
      KeyEventParams(input_device_.id, ui::EF_NONE, key, last_scan_code_, down,
                     false /* suppress_auto_repeat */, timestamp));
}

void EventConverterEvdevImpl::GenerateKeyMetrics(unsigned int key, bool down) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!has_numberpad_)
    return;
  NumberpadMetricsRecorder::GetInstance()->ProcessKey(key, down, input_device_);
#endif
}

void EventConverterEvdevImpl::ReleaseKeys() {
  base::TimeTicks timestamp = ui::EventTimeForNow();
  for (int key = 0; key < KEY_CNT; ++key)
    OnKeyChange(key, false /* down */, timestamp);
}

void EventConverterEvdevImpl::ReleaseMouseButtons() {
  base::TimeTicks timestamp = ui::EventTimeForNow();
  for (int code = BTN_MOUSE; code < BTN_JOYSTICK; ++code)
    OnButtonChange(code, false /* down */, timestamp);
}

void EventConverterEvdevImpl::OnLostSync() {
  LOG(WARNING) << "kernel dropped input events";

  // We may have missed key releases. Release everything.
  // TODO(spang): Use EVIOCGKEY to avoid releasing keys that are still held.
  ReleaseKeys();
  ReleaseMouseButtons();
}

void EventConverterEvdevImpl::DispatchMouseButton(const input_event& input) {
  if (!cursor_)
    return;

  OnButtonChange(input.code, input.value, TimeTicksFromInputEvent(input));
}

void EventConverterEvdevImpl::OnButtonChange(int code,
                                             bool down,
                                             base::TimeTicks timestamp) {
  int button_offset = code - BTN_MOUSE;
  if (mouse_button_state_.test(button_offset) == down)
    return;

  mouse_button_state_.set(button_offset, down);

  dispatcher_->DispatchMouseButtonEvent(MouseButtonEventParams(
      input_device_.id, EF_NONE, cursor_->GetLocation(), code, down,
      MouseButtonMapType::kMouse, PointerDetails(EventPointerType::kMouse),
      timestamp));
}

void EventConverterEvdevImpl::SetReceivedValidInputCallback(
    ReceivedValidInputCallback callback) {
  received_valid_input_callback_ = callback;
}

void EventConverterEvdevImpl::FlushEvents(const input_event& input) {
  if (!cursor_ || (x_offset_ == 0 && y_offset_ == 0))
    return;

  cursor_->MoveCursor(gfx::Vector2dF(x_offset_, y_offset_));

  dispatcher_->DispatchMouseMoveEvent(MouseMoveEventParams(
      input_device_.id, EF_NONE, cursor_->GetLocation(),
      nullptr /* ordinal_delta*/, PointerDetails(EventPointerType::kMouse),
      TimeTicksFromInputEvent(input)));

  x_offset_ = 0;
  y_offset_ = 0;
}

std::ostream& EventConverterEvdevImpl::DescribeForLog(std::ostream& os) const {
  os << "class=ui::EventConverterEvdevImpl id=" << input_device_.id << std::endl
     << " keyboard_type=" << keyboard_type_ << std::endl
     << " has_keyboard=" << HasKeyboard() << std::endl
     << " has_touchpad=" << has_touchpad_ << std::endl
     << " has_caps_lock_led=" << has_caps_lock_led_ << std::endl
     << " has_stylus_switch=" << has_stylus_switch_ << std::endl
     << "base ";
  return EventConverterEvdev::DescribeForLog(os);
}

}  // namespace ui
