// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_

#include <bitset>
#include <ostream>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_epoll.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"
#include "ui/ozone/public/input_controller.h"

struct input_event;

namespace ui {

class DeviceEventDispatcherEvdev;

class COMPONENT_EXPORT(EVDEV) EventConverterEvdevImpl
    : public EventConverterEvdev {
 public:
  EventConverterEvdevImpl(base::ScopedFD fd,
                          base::FilePath path,
                          int id,
                          const EventDeviceInfo& info,
                          CursorDelegateEvdev* cursor,
                          DeviceEventDispatcherEvdev* dispatcher);

  EventConverterEvdevImpl(const EventConverterEvdevImpl&) = delete;
  EventConverterEvdevImpl& operator=(const EventConverterEvdevImpl&) = delete;

  ~EventConverterEvdevImpl() override;

  // EventConverterEvdev:
  void OnFileCanReadWithoutBlocking(int fd) override;
  KeyboardType GetKeyboardType() const override;
  bool HasKeyboard() const override;
  bool HasTouchpad() const override;
  bool HasCapsLockLed() const override;
  bool HasStylusSwitch() const override;
  ui::StylusState GetStylusSwitchState() override;
  bool HasAssistantKey() const override;
  bool HasFunctionKey() const override;
  void SetKeyFilter(bool enable_filter,
                    std::vector<DomCode> allowed_keys) override;
  void OnDisabled() override;
  std::vector<uint64_t> GetKeyboardKeyBits() const override;
  void SetBlockModifiers(bool block_modifiers) override;

  void ProcessEvents(const struct input_event* inputs, int count);

  std::ostream& DescribeForLog(std::ostream& os) const override;

 private:
  void ConvertKeyEvent(const input_event& input);
  void ConvertMouseMoveEvent(const input_event& input);
  void OnKeyChange(unsigned int key,
                   bool down,
                   const base::TimeTicks& timestamp);
  void ReleaseKeys();
  void ReleaseMouseButtons();
  void OnLostSync();
  void DispatchMouseButton(const input_event& input);
  void OnButtonChange(int code, bool down, base::TimeTicks timestamp);

  // Opportunity to generate metrics for each key change
  void GenerateKeyMetrics(unsigned key, bool down);

  // Flush events delimited by EV_SYN. This is useful for handling
  // non-axis-aligned movement properly.
  void FlushEvents(const input_event& input);

  // Sets Callback to enable refreshing keyboard list after
  // a valid input is initially received
  void SetReceivedValidInputCallback(
      ReceivedValidInputCallback callback) override;

  // Input device file descriptor.
  const base::ScopedFD input_device_fd_;

  // KeyboardType
  KeyboardType keyboard_type_;

  // Input modalities for this device.
  bool has_touchpad_;
  bool has_numberpad_;
  bool has_stylus_switch_;
  // `has_assistant_key_` can only be true if the device is a keyboard.
  bool has_assistant_key_;
  // `has_function_key_` can only be true if the device is a keyboard.
  bool has_function_key_;

  // LEDs for this device.
  bool has_caps_lock_led_;

  // Save x-axis events of relative devices to be flushed at EV_SYN time.
  int x_offset_ = 0;

  // Save y-axis events of relative devices to be flushed at EV_SYN time.
  int y_offset_ = 0;

  // Saves the last scan code seen in order to attach it to the next key event.
  // The scan code is sent as a separate event message EV_MSC with subtype
  // MSC_SCAN prior to the EV_KEY message that contains the key code.
  unsigned int last_scan_code_ = 0;

  // Controller for watching the input fd.
  base::MessagePumpEpoll::FdWatchController controller_;

  // The evdev codes of the keys which should be blocked.
  std::bitset<KEY_CNT> blocked_keys_;

  // Pressed keys bitset.
  std::bitset<KEY_CNT> key_state_;

  // Whether modifier keys should be blocked from the input device.
  bool block_modifiers_ = false;

  // Last mouse button state.
  static const int kMouseButtonCount = BTN_JOYSTICK - BTN_MOUSE;
  std::bitset<kMouseButtonCount> mouse_button_state_;

  // Shared cursor state.
  const raw_ptr<CursorDelegateEvdev> cursor_;

  // Callbacks for dispatching events.
  const raw_ptr<DeviceEventDispatcherEvdev> dispatcher_;

  // Callback to update keyboard devices when valid input is received.
  ReceivedValidInputCallback received_valid_input_callback_;

  // Supported keyboard key bits.
  std::vector<uint64_t> key_bits_;

  // Whether telephony device phone mute scan code should be blocked.
  bool block_telephony_device_phone_mute_ = false;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_IMPL_H_
