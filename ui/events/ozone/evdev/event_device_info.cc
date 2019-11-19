// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_device_info.h"

#include <linux/input.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "ui/events/devices/device_util_linux.h"

#if !defined(EVIOCGMTSLOTS)
#define EVIOCGMTSLOTS(len) _IOC(_IOC_READ, 'E', 0x0a, len)
#endif

namespace ui {

namespace {

// USB vendor and product strings are pragmatically limited to 126
// characters each, so device names more than twice that should be
// unusual.
const size_t kMaximumDeviceNameLength = 256;

bool GetEventBits(int fd,
                  const base::FilePath& path,
                  unsigned int type,
                  void* buf,
                  unsigned int size) {
  if (ioctl(fd, EVIOCGBIT(type, size), buf) < 0) {
    PLOG(ERROR) << "Failed EVIOCGBIT (path=" << path.value() << " type=" << type
                << " size=" << size << ")";
    return false;
  }

  return true;
}

bool GetPropBits(int fd,
                 const base::FilePath& path,
                 void* buf,
                 unsigned int size) {
  if (ioctl(fd, EVIOCGPROP(size), buf) < 0) {
    PLOG(ERROR) << "Failed EVIOCGPROP (path=" << path.value() << ")";
    return false;
  }

  return true;
}

bool GetAbsInfo(int fd,
                const base::FilePath& path,
                int code,
                struct input_absinfo* absinfo) {
  if (ioctl(fd, EVIOCGABS(code), absinfo)) {
    PLOG(ERROR) << "Failed EVIOCGABS (path=" << path.value() << " code=" << code
                << ")";
    return false;
  }
  return true;
}

bool GetDeviceName(int fd, const base::FilePath& path, std::string* name) {
  char device_name[kMaximumDeviceNameLength];
  if (ioctl(fd, EVIOCGNAME(kMaximumDeviceNameLength - 1), &device_name) < 0) {
    PLOG(INFO) << "Failed EVIOCGNAME (path=" << path.value() << ")";
    return false;
  }
  *name = device_name;
  return true;
}

bool GetDeviceIdentifiers(int fd, const base::FilePath& path, input_id* id) {
  *id = {};
  if (ioctl(fd, EVIOCGID, id) < 0) {
    PLOG(INFO) << "Failed EVIOCGID (path=" << path.value() << ")";
    return false;
  }
  return true;
}

void GetDevicePhysInfo(int fd, const base::FilePath& path, std::string* phys) {
  char device_phys[kMaximumDeviceNameLength];
  if (ioctl(fd, EVIOCGPHYS(kMaximumDeviceNameLength - 1), &device_phys) < 0) {
    PLOG(INFO) << "Failed EVIOCGPHYS (path=" << path.value() << ")";
    return;
  }
  *phys = device_phys;
}

// |request| needs to be the equivalent to:
// struct input_mt_request_layout {
//   uint32_t code;
//   int32_t values[num_slots];
// };
//
// |size| is num_slots + 1 (for code).
void GetSlotValues(int fd,
                   const base::FilePath& path,
                   int32_t* request,
                   unsigned int size) {
  size_t data_size = size * sizeof(*request);
  if (ioctl(fd, EVIOCGMTSLOTS(data_size), request) < 0) {
    PLOG(ERROR) << "Failed EVIOCGMTSLOTS (code=" << request[0]
                << " path=" << path.value() << ")";
  }
}

void AssignBitset(const unsigned long* src,
                  size_t src_len,
                  unsigned long* dst,
                  size_t dst_len) {
  memcpy(dst, src, std::min(src_len, dst_len) * sizeof(unsigned long));
  if (src_len < dst_len)
    memset(&dst[src_len], 0, (dst_len - src_len) * sizeof(unsigned long));
}

bool IsBlacklistedAbsoluteMouseDevice(const input_id& id) {
  static constexpr struct {
    uint16_t vid;
    uint16_t pid;
  } kUSBLegacyBlackListedDevices[] = {
      {0x222a, 0x0001},  // ILITEK ILITEK-TP
  };

  for (size_t i = 0; i < base::size(kUSBLegacyBlackListedDevices); ++i) {
    if (id.vendor == kUSBLegacyBlackListedDevices[i].vid &&
        id.product == kUSBLegacyBlackListedDevices[i].pid) {
      return true;
    }
  }

  return false;
}

}  // namespace

EventDeviceInfo::EventDeviceInfo() {
  memset(ev_bits_, 0, sizeof(ev_bits_));
  memset(key_bits_, 0, sizeof(key_bits_));
  memset(rel_bits_, 0, sizeof(rel_bits_));
  memset(abs_bits_, 0, sizeof(abs_bits_));
  memset(msc_bits_, 0, sizeof(msc_bits_));
  memset(sw_bits_, 0, sizeof(sw_bits_));
  memset(led_bits_, 0, sizeof(led_bits_));
  memset(prop_bits_, 0, sizeof(prop_bits_));
  memset(abs_info_, 0, sizeof(abs_info_));
}

EventDeviceInfo::~EventDeviceInfo() {}

bool EventDeviceInfo::Initialize(int fd, const base::FilePath& path) {
  if (!GetEventBits(fd, path, 0, ev_bits_, sizeof(ev_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_KEY, key_bits_, sizeof(key_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_REL, rel_bits_, sizeof(rel_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_ABS, abs_bits_, sizeof(abs_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_MSC, msc_bits_, sizeof(msc_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_SW, sw_bits_, sizeof(sw_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_LED, led_bits_, sizeof(led_bits_)))
    return false;

  if (!GetPropBits(fd, path, prop_bits_, sizeof(prop_bits_)))
    return false;

  for (unsigned int i = 0; i < ABS_CNT; ++i)
    if (HasAbsEvent(i))
      if (!GetAbsInfo(fd, path, i, &abs_info_[i]))
        return false;

  int max_num_slots = GetAbsMtSlotCount();

  // |request| is MT code + slots.
  int32_t request[max_num_slots + 1];
  int32_t* request_code = &request[0];
  int32_t* request_slots = &request[1];
  for (unsigned int i = EVDEV_ABS_MT_FIRST; i <= EVDEV_ABS_MT_LAST; ++i) {
    if (!HasAbsEvent(i))
      continue;

    memset(request, 0, sizeof(request));
    *request_code = i;
    GetSlotValues(fd, path, request, max_num_slots + 1);

    std::vector<int32_t>* slots = &slot_values_[i - EVDEV_ABS_MT_FIRST];
    slots->assign(request_slots, request_slots + max_num_slots);
  }

  if (!GetDeviceName(fd, path, &name_))
    return false;

  if (!GetDeviceIdentifiers(fd, path, &input_id_))
    return false;

  GetDevicePhysInfo(fd, path, &phys_);

  device_type_ = GetInputDeviceTypeFromId(input_id_);
  if (device_type_ == InputDeviceType::INPUT_DEVICE_UNKNOWN)
    device_type_ = GetInputDeviceTypeFromPath(path);

  return true;
}

void EventDeviceInfo::SetEventTypes(const unsigned long* ev_bits, size_t len) {
  AssignBitset(ev_bits, len, ev_bits_, base::size(ev_bits_));
}

void EventDeviceInfo::SetKeyEvents(const unsigned long* key_bits, size_t len) {
  AssignBitset(key_bits, len, key_bits_, base::size(key_bits_));
}

void EventDeviceInfo::SetRelEvents(const unsigned long* rel_bits, size_t len) {
  AssignBitset(rel_bits, len, rel_bits_, base::size(rel_bits_));
}

void EventDeviceInfo::SetAbsEvents(const unsigned long* abs_bits, size_t len) {
  AssignBitset(abs_bits, len, abs_bits_, base::size(abs_bits_));
}

void EventDeviceInfo::SetMscEvents(const unsigned long* msc_bits, size_t len) {
  AssignBitset(msc_bits, len, msc_bits_, base::size(msc_bits_));
}

void EventDeviceInfo::SetSwEvents(const unsigned long* sw_bits, size_t len) {
  AssignBitset(sw_bits, len, sw_bits_, base::size(sw_bits_));
}

void EventDeviceInfo::SetLedEvents(const unsigned long* led_bits, size_t len) {
  AssignBitset(led_bits, len, led_bits_, base::size(led_bits_));
}

void EventDeviceInfo::SetProps(const unsigned long* prop_bits, size_t len) {
  AssignBitset(prop_bits, len, prop_bits_, base::size(prop_bits_));
}

void EventDeviceInfo::SetAbsInfo(unsigned int code,
                                 const input_absinfo& abs_info) {
  if (code > ABS_MAX)
    return;

  memcpy(&abs_info_[code], &abs_info, sizeof(abs_info));
}

void EventDeviceInfo::SetAbsMtSlots(unsigned int code,
                                    const std::vector<int32_t>& values) {
  DCHECK_EQ(GetAbsMtSlotCount(), values.size());
  int index = code - EVDEV_ABS_MT_FIRST;
  if (index < 0 || index >= EVDEV_ABS_MT_COUNT)
    return;
  slot_values_[index] = values;
}

void EventDeviceInfo::SetAbsMtSlot(unsigned int code,
                                   unsigned int slot,
                                   uint32_t value) {
  int index = code - EVDEV_ABS_MT_FIRST;
  if (index < 0 || index >= EVDEV_ABS_MT_COUNT)
    return;
  slot_values_[index][slot] = value;
}

void EventDeviceInfo::SetDeviceType(InputDeviceType type) {
  device_type_ = type;
}

void EventDeviceInfo::SetId(input_id id) {
  input_id_ = id;
}
void EventDeviceInfo::SetName(const std::string& name) {
  name_ = name;
}

bool EventDeviceInfo::HasEventType(unsigned int type) const {
  if (type > EV_MAX)
    return false;
  return EvdevBitIsSet(ev_bits_, type);
}

bool EventDeviceInfo::HasKeyEvent(unsigned int code) const {
  if (code > KEY_MAX)
    return false;
  return EvdevBitIsSet(key_bits_, code);
}

bool EventDeviceInfo::HasRelEvent(unsigned int code) const {
  if (code > REL_MAX)
    return false;
  return EvdevBitIsSet(rel_bits_, code);
}

bool EventDeviceInfo::HasAbsEvent(unsigned int code) const {
  if (code > ABS_MAX)
    return false;
  return EvdevBitIsSet(abs_bits_, code);
}

bool EventDeviceInfo::HasMscEvent(unsigned int code) const {
  if (code > MSC_MAX)
    return false;
  return EvdevBitIsSet(msc_bits_, code);
}

bool EventDeviceInfo::HasSwEvent(unsigned int code) const {
  if (code > SW_MAX)
    return false;
  return EvdevBitIsSet(sw_bits_, code);
}

bool EventDeviceInfo::HasLedEvent(unsigned int code) const {
  if (code > LED_MAX)
    return false;
  return EvdevBitIsSet(led_bits_, code);
}

bool EventDeviceInfo::HasProp(unsigned int code) const {
  if (code > INPUT_PROP_MAX)
    return false;
  return EvdevBitIsSet(prop_bits_, code);
}

int32_t EventDeviceInfo::GetAbsMinimum(unsigned int code) const {
  return abs_info_[code].minimum;
}

int32_t EventDeviceInfo::GetAbsMaximum(unsigned int code) const {
  return abs_info_[code].maximum;
}

int32_t EventDeviceInfo::GetAbsResolution(unsigned int code) const {
  return abs_info_[code].resolution;
}

int32_t EventDeviceInfo::GetAbsValue(unsigned int code) const {
  return abs_info_[code].value;
}

input_absinfo EventDeviceInfo::GetAbsInfoByCode(unsigned int code) const {
  return abs_info_[code];
}

uint32_t EventDeviceInfo::GetAbsMtSlotCount() const {
  if (!HasAbsEvent(ABS_MT_SLOT))
    return 0;
  return GetAbsMaximum(ABS_MT_SLOT) + 1;
}

int32_t EventDeviceInfo::GetAbsMtSlotValue(unsigned int code,
                                           unsigned int slot) const {
  unsigned int index = code - EVDEV_ABS_MT_FIRST;
  DCHECK(index < EVDEV_ABS_MT_COUNT);
  return slot_values_[index][slot];
}

int32_t EventDeviceInfo::GetAbsMtSlotValueWithDefault(
    unsigned int code,
    unsigned int slot,
    int32_t default_value) const {
  if (!HasAbsEvent(code))
    return default_value;
  return GetAbsMtSlotValue(code, slot);
}

bool EventDeviceInfo::HasAbsXY() const {
  return HasAbsEvent(ABS_X) && HasAbsEvent(ABS_Y);
}

bool EventDeviceInfo::HasMTAbsXY() const {
  return HasAbsEvent(ABS_MT_POSITION_X) && HasAbsEvent(ABS_MT_POSITION_Y);
}

bool EventDeviceInfo::HasRelXY() const {
  return HasRelEvent(REL_X) && HasRelEvent(REL_Y);
}

bool EventDeviceInfo::HasMultitouch() const {
  return HasAbsEvent(ABS_MT_SLOT);
}

bool EventDeviceInfo::HasDirect() const {
  bool has_direct = HasProp(INPUT_PROP_DIRECT);
  bool has_pointer = HasProp(INPUT_PROP_POINTER);
  if (has_direct || has_pointer)
    return has_direct;

  switch (ProbeLegacyAbsoluteDevice()) {
    case LegacyAbsoluteDeviceType::TOUCHSCREEN:
      return true;

    case LegacyAbsoluteDeviceType::TABLET:
    case LegacyAbsoluteDeviceType::TOUCHPAD:
    case LegacyAbsoluteDeviceType::NONE:
      return false;
  }

  NOTREACHED();
  return false;
}

bool EventDeviceInfo::HasPointer() const {
  bool has_direct = HasProp(INPUT_PROP_DIRECT);
  bool has_pointer = HasProp(INPUT_PROP_POINTER);
  if (has_direct || has_pointer)
    return has_pointer;

  switch (ProbeLegacyAbsoluteDevice()) {
    case LegacyAbsoluteDeviceType::TOUCHPAD:
    case LegacyAbsoluteDeviceType::TABLET:
      return true;

    case LegacyAbsoluteDeviceType::TOUCHSCREEN:
    case LegacyAbsoluteDeviceType::NONE:
      return false;
  }

  NOTREACHED();
  return false;
}

bool EventDeviceInfo::HasStylus() const {
  return HasKeyEvent(BTN_TOOL_PEN) || HasKeyEvent(BTN_STYLUS) ||
         HasKeyEvent(BTN_STYLUS2);
}

bool EventDeviceInfo::HasKeyboard() const {
  if (!HasEventType(EV_KEY))
    return false;

  // Check first 31 keys: If we have all of them, consider it a full
  // keyboard. This is exactly what udev does for ID_INPUT_KEYBOARD.
  for (int key = KEY_ESC; key <= KEY_D; ++key)
    if (!HasKeyEvent(key))
      return false;

  return true;
}

bool EventDeviceInfo::HasMouse() const {
  return HasRelXY();
}

bool EventDeviceInfo::HasTouchpad() const {
  return HasAbsXY() && HasPointer() && !HasStylus();
}

bool EventDeviceInfo::HasTablet() const {
  return HasAbsXY() && HasPointer() && HasStylus();
}

bool EventDeviceInfo::HasTouchscreen() const {
  return HasAbsXY() && HasDirect();
}

bool EventDeviceInfo::HasGamepad() const {
  if (!HasEventType(EV_KEY))
    return false;

  // If the device has gamepad button, and it's not keyboard or tablet, it will
  // be considered to be a gamepad. Note: this WILL have false positives and
  // false negatives. A concrete solution will use ID_INPUT_JOYSTICK with some
  // patch removing false positives.
  bool support_gamepad_btn = false;
  for (int key = BTN_JOYSTICK; key <= BTN_THUMBR; ++key) {
    if (HasKeyEvent(key))
      support_gamepad_btn = true;
  }

  return support_gamepad_btn && !HasTablet() && !HasKeyboard();
}

// static
ui::InputDeviceType EventDeviceInfo::GetInputDeviceTypeFromId(input_id id) {
  static constexpr struct {
    uint16_t vid;
    uint16_t pid;
  } kUSBInternalDevices[] = {
      {0x18d1, 0x502b},  // Google, Hammer PID (soraka)
      {0x18d1, 0x5030},  // Google, Whiskers PID (nocturne)
      {0x18d1, 0x503c},  // Google, Masterball PID (krane)
      {0x18d1, 0x503d},  // Google, Magnemite PID (kodama)
      {0x1fd2, 0x8103},  // LG, Internal TouchScreen PID
  };

  if (id.bustype == BUS_USB) {
    for (size_t i = 0; i < base::size(kUSBInternalDevices); ++i) {
      if (id.vendor == kUSBInternalDevices[i].vid &&
          id.product == kUSBInternalDevices[i].pid)
        return InputDeviceType::INPUT_DEVICE_INTERNAL;
    }
  }

  switch (id.bustype) {
    case BUS_I2C:
    case BUS_I8042:
      return ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
    case BUS_USB:
      return ui::InputDeviceType::INPUT_DEVICE_USB;
    case BUS_BLUETOOTH:
      return ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH;
    default:
      return ui::InputDeviceType::INPUT_DEVICE_UNKNOWN;
  }
}

EventDeviceInfo::LegacyAbsoluteDeviceType
EventDeviceInfo::ProbeLegacyAbsoluteDevice() const {
  if (!HasAbsXY())
    return LegacyAbsoluteDeviceType::NONE;

  // Treat internal stylus devices as touchscreens.
  if (device_type_ == INPUT_DEVICE_INTERNAL && HasStylus())
    return LegacyAbsoluteDeviceType::TOUCHSCREEN;

  if (HasStylus())
    return LegacyAbsoluteDeviceType::TABLET;

  if (HasKeyEvent(BTN_TOOL_FINGER) && HasKeyEvent(BTN_TOUCH))
    return LegacyAbsoluteDeviceType::TOUCHPAD;

  if (HasKeyEvent(BTN_TOUCH))
    return LegacyAbsoluteDeviceType::TOUCHSCREEN;

  // ABS_Z mitigation for extra device on some Elo devices.
  if (HasKeyEvent(BTN_LEFT) && !HasAbsEvent(ABS_Z) &&
      !IsBlacklistedAbsoluteMouseDevice(input_id_))
    return LegacyAbsoluteDeviceType::TOUCHSCREEN;

  return LegacyAbsoluteDeviceType::NONE;
}

}  // namespace ui
