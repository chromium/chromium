// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    dri3 \
//    glx \
//    randr \
//    render \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xfixes \
//    xinput \
//    xkb \
//    xproto \
//    xtest

#include "xinput.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Input::Input(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::DeviceValuatorEvent>(Input::DeviceValuatorEvent* event_,
                                           ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& device_id = (*event_).device_id;
  auto& sequence = (*event_).sequence;
  auto& device_state = (*event_).device_state;
  auto& num_valuators = (*event_).num_valuators;
  auto& first_valuator = (*event_).first_valuator;
  auto& valuators = (*event_).valuators;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // device_id
  Read(&device_id, &buf);

  // sequence
  Read(&sequence, &buf);

  // device_state
  Read(&device_state, &buf);

  // num_valuators
  Read(&num_valuators, &buf);

  // first_valuator
  Read(&first_valuator, &buf);

  // valuators
  for (auto& valuators_elem : valuators) {
    // valuators_elem
    Read(&valuators_elem, &buf);
  }

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::LegacyDeviceEvent>(Input::LegacyDeviceEvent* event_,
                                         ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& detail = (*event_).detail;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& root = (*event_).root;
  auto& event = (*event_).event;
  auto& child = (*event_).child;
  auto& root_x = (*event_).root_x;
  auto& root_y = (*event_).root_y;
  auto& event_x = (*event_).event_x;
  auto& event_y = (*event_).event_y;
  auto& state = (*event_).state;
  auto& same_screen = (*event_).same_screen;
  auto& device_id = (*event_).device_id;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // detail
  Read(&detail, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // root
  Read(&root, &buf);

  // event
  Read(&event, &buf);

  // child
  Read(&child, &buf);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // event_x
  Read(&event_x, &buf);

  // event_y
  Read(&event_y, &buf);

  // state
  uint16_t tmp0;
  Read(&tmp0, &buf);
  state = static_cast<KeyButMask>(tmp0);

  // same_screen
  Read(&same_screen, &buf);

  // device_id
  Read(&device_id, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::DeviceFocusEvent>(Input::DeviceFocusEvent* event_,
                                        ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& detail = (*event_).detail;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& window = (*event_).window;
  auto& mode = (*event_).mode;
  auto& device_id = (*event_).device_id;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // detail
  uint8_t tmp1;
  Read(&tmp1, &buf);
  detail = static_cast<NotifyDetail>(tmp1);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // window
  Read(&window, &buf);

  // mode
  uint8_t tmp2;
  Read(&tmp2, &buf);
  mode = static_cast<NotifyMode>(tmp2);

  // device_id
  Read(&device_id, &buf);

  // pad0
  Pad(&buf, 18);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::DeviceStateNotifyEvent>(
    Input::DeviceStateNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& device_id = (*event_).device_id;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& num_keys = (*event_).num_keys;
  auto& num_buttons = (*event_).num_buttons;
  auto& num_valuators = (*event_).num_valuators;
  auto& classes_reported = (*event_).classes_reported;
  auto& buttons = (*event_).buttons;
  auto& keys = (*event_).keys;
  auto& valuators = (*event_).valuators;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // device_id
  Read(&device_id, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // num_keys
  Read(&num_keys, &buf);

  // num_buttons
  Read(&num_buttons, &buf);

  // num_valuators
  Read(&num_valuators, &buf);

  // classes_reported
  uint8_t tmp3;
  Read(&tmp3, &buf);
  classes_reported = static_cast<Input::ClassesReportedMask>(tmp3);

  // buttons
  for (auto& buttons_elem : buttons) {
    // buttons_elem
    Read(&buttons_elem, &buf);
  }

  // keys
  for (auto& keys_elem : keys) {
    // keys_elem
    Read(&keys_elem, &buf);
  }

  // valuators
  for (auto& valuators_elem : valuators) {
    // valuators_elem
    Read(&valuators_elem, &buf);
  }

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::DeviceMappingNotifyEvent>(
    Input::DeviceMappingNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& device_id = (*event_).device_id;
  auto& sequence = (*event_).sequence;
  auto& request = (*event_).request;
  auto& first_keycode = (*event_).first_keycode;
  auto& count = (*event_).count;
  auto& time = (*event_).time;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // device_id
  Read(&device_id, &buf);

  // sequence
  Read(&sequence, &buf);

  // request
  uint8_t tmp4;
  Read(&tmp4, &buf);
  request = static_cast<Mapping>(tmp4);

  // first_keycode
  Read(&first_keycode, &buf);

  // count
  Read(&count, &buf);

  // pad0
  Pad(&buf, 1);

  // time
  Read(&time, &buf);

  // pad1
  Pad(&buf, 20);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::ChangeDeviceNotifyEvent>(
    Input::ChangeDeviceNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& device_id = (*event_).device_id;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& request = (*event_).request;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // device_id
  Read(&device_id, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // request
  uint8_t tmp5;
  Read(&tmp5, &buf);
  request = static_cast<Input::ChangeDevice>(tmp5);

  // pad0
  Pad(&buf, 23);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::DeviceKeyStateNotifyEvent>(
    Input::DeviceKeyStateNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& device_id = (*event_).device_id;
  auto& sequence = (*event_).sequence;
  auto& keys = (*event_).keys;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // device_id
  Read(&device_id, &buf);

  // sequence
  Read(&sequence, &buf);

  // keys
  for (auto& keys_elem : keys) {
    // keys_elem
    Read(&keys_elem, &buf);
  }

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::DeviceButtonStateNotifyEvent>(
    Input::DeviceButtonStateNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& device_id = (*event_).device_id;
  auto& sequence = (*event_).sequence;
  auto& buttons = (*event_).buttons;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // device_id
  Read(&device_id, &buf);

  // sequence
  Read(&sequence, &buf);

  // buttons
  for (auto& buttons_elem : buttons) {
    // buttons_elem
    Read(&buttons_elem, &buf);
  }

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::DevicePresenceNotifyEvent>(
    Input::DevicePresenceNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& devchange = (*event_).devchange;
  auto& device_id = (*event_).device_id;
  auto& control = (*event_).control;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // devchange
  uint8_t tmp6;
  Read(&tmp6, &buf);
  devchange = static_cast<Input::DeviceChange>(tmp6);

  // device_id
  Read(&device_id, &buf);

  // control
  Read(&control, &buf);

  // pad1
  Pad(&buf, 20);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::DevicePropertyNotifyEvent>(
    Input::DevicePropertyNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& state = (*event_).state;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& property = (*event_).property;
  auto& device_id = (*event_).device_id;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // state
  uint8_t tmp7;
  Read(&tmp7, &buf);
  state = static_cast<Property>(tmp7);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // property
  Read(&property, &buf);

  // pad0
  Pad(&buf, 19);

  // device_id
  Read(&device_id, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::DeviceChangedEvent>(Input::DeviceChangedEvent* event_,
                                          ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& deviceid = (*event_).deviceid;
  auto& time = (*event_).time;
  uint16_t num_classes{};
  auto& sourceid = (*event_).sourceid;
  auto& reason = (*event_).reason;
  auto& classes = (*event_).classes;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // deviceid
  Read(&deviceid, &buf);

  // time
  Read(&time, &buf);

  // num_classes
  Read(&num_classes, &buf);

  // sourceid
  Read(&sourceid, &buf);

  // reason
  uint8_t tmp8;
  Read(&tmp8, &buf);
  reason = static_cast<Input::ChangeReason>(tmp8);

  // pad0
  Pad(&buf, 11);

  // classes
  classes.resize(num_classes);
  for (auto& classes_elem : classes) {
    // classes_elem
    {
      Input::DeviceClassType type{};
      auto& len = classes_elem.len;
      auto& sourceid = classes_elem.sourceid;
      auto& data = classes_elem;

      // type
      uint16_t tmp9;
      Read(&tmp9, &buf);
      type = static_cast<Input::DeviceClassType>(tmp9);

      // len
      Read(&len, &buf);

      // sourceid
      Read(&sourceid, &buf);

      // data
      auto data_expr = type;
      if (CaseEq(data_expr, Input::DeviceClassType::Key)) {
        data.key.emplace(decltype(data.key)::value_type());
        uint16_t num_keys{};
        auto& keys = (*data.key).keys;

        // num_keys
        Read(&num_keys, &buf);

        // keys
        keys.resize(num_keys);
        for (auto& keys_elem : keys) {
          // keys_elem
          Read(&keys_elem, &buf);
        }
      }
      if (CaseEq(data_expr, Input::DeviceClassType::Button)) {
        data.button.emplace(decltype(data.button)::value_type());
        uint16_t num_buttons{};
        auto& state = (*data.button).state;
        auto& labels = (*data.button).labels;

        // num_buttons
        Read(&num_buttons, &buf);

        // state
        state.resize(((num_buttons) + (31)) / (32));
        for (auto& state_elem : state) {
          // state_elem
          Read(&state_elem, &buf);
        }

        // labels
        labels.resize(num_buttons);
        for (auto& labels_elem : labels) {
          // labels_elem
          Read(&labels_elem, &buf);
        }
      }
      if (CaseEq(data_expr, Input::DeviceClassType::Valuator)) {
        data.valuator.emplace(decltype(data.valuator)::value_type());
        auto& number = (*data.valuator).number;
        auto& label = (*data.valuator).label;
        auto& min = (*data.valuator).min;
        auto& max = (*data.valuator).max;
        auto& value = (*data.valuator).value;
        auto& resolution = (*data.valuator).resolution;
        auto& mode = (*data.valuator).mode;

        // number
        Read(&number, &buf);

        // label
        Read(&label, &buf);

        // min
        {
          auto& integral = min.integral;
          auto& frac = min.frac;

          // integral
          Read(&integral, &buf);

          // frac
          Read(&frac, &buf);
        }

        // max
        {
          auto& integral = max.integral;
          auto& frac = max.frac;

          // integral
          Read(&integral, &buf);

          // frac
          Read(&frac, &buf);
        }

        // value
        {
          auto& integral = value.integral;
          auto& frac = value.frac;

          // integral
          Read(&integral, &buf);

          // frac
          Read(&frac, &buf);
        }

        // resolution
        Read(&resolution, &buf);

        // mode
        uint8_t tmp10;
        Read(&tmp10, &buf);
        mode = static_cast<Input::ValuatorMode>(tmp10);

        // pad0
        Pad(&buf, 3);
      }
      if (CaseEq(data_expr, Input::DeviceClassType::Scroll)) {
        data.scroll.emplace(decltype(data.scroll)::value_type());
        auto& number = (*data.scroll).number;
        auto& scroll_type = (*data.scroll).scroll_type;
        auto& flags = (*data.scroll).flags;
        auto& increment = (*data.scroll).increment;

        // number
        Read(&number, &buf);

        // scroll_type
        uint16_t tmp11;
        Read(&tmp11, &buf);
        scroll_type = static_cast<Input::ScrollType>(tmp11);

        // pad1
        Pad(&buf, 2);

        // flags
        uint32_t tmp12;
        Read(&tmp12, &buf);
        flags = static_cast<Input::ScrollFlags>(tmp12);

        // increment
        {
          auto& integral = increment.integral;
          auto& frac = increment.frac;

          // integral
          Read(&integral, &buf);

          // frac
          Read(&frac, &buf);
        }
      }
      if (CaseEq(data_expr, Input::DeviceClassType::Touch)) {
        data.touch.emplace(decltype(data.touch)::value_type());
        auto& mode = (*data.touch).mode;
        auto& num_touches = (*data.touch).num_touches;

        // mode
        uint8_t tmp13;
        Read(&tmp13, &buf);
        mode = static_cast<Input::TouchMode>(tmp13);

        // num_touches
        Read(&num_touches, &buf);
      }
      if (CaseEq(data_expr, Input::DeviceClassType::Gesture)) {
        data.gesture.emplace(decltype(data.gesture)::value_type());
        auto& num_touches = (*data.gesture).num_touches;

        // num_touches
        Read(&num_touches, &buf);

        // pad2
        Pad(&buf, 1);
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::DeviceEvent>(Input::DeviceEvent* event_,
                                   ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& deviceid = (*event_).deviceid;
  auto& time = (*event_).time;
  auto& detail = (*event_).detail;
  auto& root = (*event_).root;
  auto& event = (*event_).event;
  auto& child = (*event_).child;
  auto& root_x = (*event_).root_x;
  auto& root_y = (*event_).root_y;
  auto& event_x = (*event_).event_x;
  auto& event_y = (*event_).event_y;
  uint16_t buttons_len{};
  uint16_t valuators_len{};
  auto& sourceid = (*event_).sourceid;
  auto& flags = (*event_).flags;
  auto& mods = (*event_).mods;
  auto& group = (*event_).group;
  auto& button_mask = (*event_).button_mask;
  auto& valuator_mask = (*event_).valuator_mask;
  auto& axisvalues = (*event_).axisvalues;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // deviceid
  Read(&deviceid, &buf);

  // time
  Read(&time, &buf);

  // detail
  Read(&detail, &buf);

  // root
  Read(&root, &buf);

  // event
  Read(&event, &buf);

  // child
  Read(&child, &buf);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // event_x
  Read(&event_x, &buf);

  // event_y
  Read(&event_y, &buf);

  // buttons_len
  Read(&buttons_len, &buf);

  // valuators_len
  Read(&valuators_len, &buf);

  // sourceid
  Read(&sourceid, &buf);

  // pad0
  Pad(&buf, 2);

  // flags
  uint32_t tmp14;
  Read(&tmp14, &buf);
  flags = static_cast<Input::KeyEventFlags>(tmp14);

  // mods
  {
    auto& base = mods.base;
    auto& latched = mods.latched;
    auto& locked = mods.locked;
    auto& effective = mods.effective;

    // base
    Read(&base, &buf);

    // latched
    Read(&latched, &buf);

    // locked
    Read(&locked, &buf);

    // effective
    Read(&effective, &buf);
  }

  // group
  {
    auto& base = group.base;
    auto& latched = group.latched;
    auto& locked = group.locked;
    auto& effective = group.effective;

    // base
    Read(&base, &buf);

    // latched
    Read(&latched, &buf);

    // locked
    Read(&locked, &buf);

    // effective
    Read(&effective, &buf);
  }

  // button_mask
  button_mask.resize(buttons_len);
  for (auto& button_mask_elem : button_mask) {
    // button_mask_elem
    Read(&button_mask_elem, &buf);
  }

  // valuator_mask
  valuator_mask.resize(valuators_len);
  for (auto& valuator_mask_elem : valuator_mask) {
    // valuator_mask_elem
    Read(&valuator_mask_elem, &buf);
  }

  // axisvalues
  auto sum15_ = SumOf([](auto& listelem_ref) { return PopCount(listelem_ref); },
                      valuator_mask);
  axisvalues.resize(sum15_);
  for (auto& axisvalues_elem : axisvalues) {
    // axisvalues_elem
    {
      auto& integral = axisvalues_elem.integral;
      auto& frac = axisvalues_elem.frac;

      // integral
      Read(&integral, &buf);

      // frac
      Read(&frac, &buf);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::CrossingEvent>(Input::CrossingEvent* event_,
                                     ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& deviceid = (*event_).deviceid;
  auto& time = (*event_).time;
  auto& sourceid = (*event_).sourceid;
  auto& mode = (*event_).mode;
  auto& detail = (*event_).detail;
  auto& root = (*event_).root;
  auto& event = (*event_).event;
  auto& child = (*event_).child;
  auto& root_x = (*event_).root_x;
  auto& root_y = (*event_).root_y;
  auto& event_x = (*event_).event_x;
  auto& event_y = (*event_).event_y;
  auto& same_screen = (*event_).same_screen;
  auto& focus = (*event_).focus;
  uint16_t buttons_len{};
  auto& mods = (*event_).mods;
  auto& group = (*event_).group;
  auto& buttons = (*event_).buttons;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // deviceid
  Read(&deviceid, &buf);

  // time
  Read(&time, &buf);

  // sourceid
  Read(&sourceid, &buf);

  // mode
  uint8_t tmp16;
  Read(&tmp16, &buf);
  mode = static_cast<Input::NotifyMode>(tmp16);

  // detail
  uint8_t tmp17;
  Read(&tmp17, &buf);
  detail = static_cast<Input::NotifyDetail>(tmp17);

  // root
  Read(&root, &buf);

  // event
  Read(&event, &buf);

  // child
  Read(&child, &buf);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // event_x
  Read(&event_x, &buf);

  // event_y
  Read(&event_y, &buf);

  // same_screen
  Read(&same_screen, &buf);

  // focus
  Read(&focus, &buf);

  // buttons_len
  Read(&buttons_len, &buf);

  // mods
  {
    auto& base = mods.base;
    auto& latched = mods.latched;
    auto& locked = mods.locked;
    auto& effective = mods.effective;

    // base
    Read(&base, &buf);

    // latched
    Read(&latched, &buf);

    // locked
    Read(&locked, &buf);

    // effective
    Read(&effective, &buf);
  }

  // group
  {
    auto& base = group.base;
    auto& latched = group.latched;
    auto& locked = group.locked;
    auto& effective = group.effective;

    // base
    Read(&base, &buf);

    // latched
    Read(&latched, &buf);

    // locked
    Read(&locked, &buf);

    // effective
    Read(&effective, &buf);
  }

  // buttons
  buttons.resize(buttons_len);
  for (auto& buttons_elem : buttons) {
    // buttons_elem
    Read(&buttons_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::HierarchyEvent>(Input::HierarchyEvent* event_,
                                      ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& deviceid = (*event_).deviceid;
  auto& time = (*event_).time;
  auto& flags = (*event_).flags;
  uint16_t num_infos{};
  auto& infos = (*event_).infos;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // deviceid
  Read(&deviceid, &buf);

  // time
  Read(&time, &buf);

  // flags
  uint32_t tmp18;
  Read(&tmp18, &buf);
  flags = static_cast<Input::HierarchyMask>(tmp18);

  // num_infos
  Read(&num_infos, &buf);

  // pad0
  Pad(&buf, 10);

  // infos
  infos.resize(num_infos);
  for (auto& infos_elem : infos) {
    // infos_elem
    {
      auto& deviceid = infos_elem.deviceid;
      auto& attachment = infos_elem.attachment;
      auto& type = infos_elem.type;
      auto& enabled = infos_elem.enabled;
      auto& flags = infos_elem.flags;

      // deviceid
      Read(&deviceid, &buf);

      // attachment
      Read(&attachment, &buf);

      // type
      uint8_t tmp19;
      Read(&tmp19, &buf);
      type = static_cast<Input::DeviceType>(tmp19);

      // enabled
      Read(&enabled, &buf);

      // pad0
      Pad(&buf, 2);

      // flags
      uint32_t tmp20;
      Read(&tmp20, &buf);
      flags = static_cast<Input::HierarchyMask>(tmp20);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::PropertyEvent>(Input::PropertyEvent* event_,
                                     ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& deviceid = (*event_).deviceid;
  auto& time = (*event_).time;
  auto& property = (*event_).property;
  auto& what = (*event_).what;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // deviceid
  Read(&deviceid, &buf);

  // time
  Read(&time, &buf);

  // property
  Read(&property, &buf);

  // what
  uint8_t tmp21;
  Read(&tmp21, &buf);
  what = static_cast<Input::PropertyFlag>(tmp21);

  // pad0
  Pad(&buf, 11);

  Align(&buf, 4);
  CHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::RawDeviceEvent>(Input::RawDeviceEvent* event_,
                                      ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& deviceid = (*event_).deviceid;
  auto& time = (*event_).time;
  auto& detail = (*event_).detail;
  auto& sourceid = (*event_).sourceid;
  uint16_t valuators_len{};
  auto& flags = (*event_).flags;
  auto& valuator_mask = (*event_).valuator_mask;
  auto& axisvalues = (*event_).axisvalues;
  auto& axisvalues_raw = (*event_).axisvalues_raw;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // deviceid
  Read(&deviceid, &buf);

  // time
  Read(&time, &buf);

  // detail
  Read(&detail, &buf);

  // sourceid
  Read(&sourceid, &buf);

  // valuators_len
  Read(&valuators_len, &buf);

  // flags
  uint32_t tmp22;
  Read(&tmp22, &buf);
  flags = static_cast<Input::KeyEventFlags>(tmp22);

  // pad0
  Pad(&buf, 4);

  // valuator_mask
  valuator_mask.resize(valuators_len);
  for (auto& valuator_mask_elem : valuator_mask) {
    // valuator_mask_elem
    Read(&valuator_mask_elem, &buf);
  }

  // axisvalues
  auto sum23_ = SumOf([](auto& listelem_ref) { return PopCount(listelem_ref); },
                      valuator_mask);
  axisvalues.resize(sum23_);
  for (auto& axisvalues_elem : axisvalues) {
    // axisvalues_elem
    {
      auto& integral = axisvalues_elem.integral;
      auto& frac = axisvalues_elem.frac;

      // integral
      Read(&integral, &buf);

      // frac
      Read(&frac, &buf);
    }
  }

  // axisvalues_raw
  auto sum24_ = SumOf([](auto& listelem_ref) { return PopCount(listelem_ref); },
                      valuator_mask);
  axisvalues_raw.resize(sum24_);
  for (auto& axisvalues_raw_elem : axisvalues_raw) {
    // axisvalues_raw_elem
    {
      auto& integral = axisvalues_raw_elem.integral;
      auto& frac = axisvalues_raw_elem.frac;

      // integral
      Read(&integral, &buf);

      // frac
      Read(&frac, &buf);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::TouchOwnershipEvent>(Input::TouchOwnershipEvent* event_,
                                           ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& deviceid = (*event_).deviceid;
  auto& time = (*event_).time;
  auto& touchid = (*event_).touchid;
  auto& root = (*event_).root;
  auto& event = (*event_).event;
  auto& child = (*event_).child;
  auto& sourceid = (*event_).sourceid;
  auto& flags = (*event_).flags;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // deviceid
  Read(&deviceid, &buf);

  // time
  Read(&time, &buf);

  // touchid
  Read(&touchid, &buf);

  // root
  Read(&root, &buf);

  // event
  Read(&event, &buf);

  // child
  Read(&child, &buf);

  // sourceid
  Read(&sourceid, &buf);

  // pad0
  Pad(&buf, 2);

  // flags
  uint32_t tmp25;
  Read(&tmp25, &buf);
  flags = static_cast<Input::TouchOwnershipFlags>(tmp25);

  // pad1
  Pad(&buf, 8);

  Align(&buf, 4);
  CHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::BarrierEvent>(Input::BarrierEvent* event_,
                                    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& deviceid = (*event_).deviceid;
  auto& time = (*event_).time;
  auto& eventid = (*event_).eventid;
  auto& root = (*event_).root;
  auto& event = (*event_).event;
  auto& barrier = (*event_).barrier;
  auto& dtime = (*event_).dtime;
  auto& flags = (*event_).flags;
  auto& sourceid = (*event_).sourceid;
  auto& root_x = (*event_).root_x;
  auto& root_y = (*event_).root_y;
  auto& dx = (*event_).dx;
  auto& dy = (*event_).dy;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // deviceid
  Read(&deviceid, &buf);

  // time
  Read(&time, &buf);

  // eventid
  Read(&eventid, &buf);

  // root
  Read(&root, &buf);

  // event
  Read(&event, &buf);

  // barrier
  Read(&barrier, &buf);

  // dtime
  Read(&dtime, &buf);

  // flags
  uint32_t tmp26;
  Read(&tmp26, &buf);
  flags = static_cast<Input::BarrierFlags>(tmp26);

  // sourceid
  Read(&sourceid, &buf);

  // pad0
  Pad(&buf, 2);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // dx
  {
    auto& integral = dx.integral;
    auto& frac = dx.frac;

    // integral
    Read(&integral, &buf);

    // frac
    Read(&frac, &buf);
  }

  // dy
  {
    auto& integral = dy.integral;
    auto& frac = dy.frac;

    // integral
    Read(&integral, &buf);

    // frac
    Read(&frac, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::GesturePinchEvent>(Input::GesturePinchEvent* event_,
                                         ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& deviceid = (*event_).deviceid;
  auto& time = (*event_).time;
  auto& detail = (*event_).detail;
  auto& root = (*event_).root;
  auto& event = (*event_).event;
  auto& child = (*event_).child;
  auto& root_x = (*event_).root_x;
  auto& root_y = (*event_).root_y;
  auto& event_x = (*event_).event_x;
  auto& event_y = (*event_).event_y;
  auto& delta_x = (*event_).delta_x;
  auto& delta_y = (*event_).delta_y;
  auto& delta_unaccel_x = (*event_).delta_unaccel_x;
  auto& delta_unaccel_y = (*event_).delta_unaccel_y;
  auto& scale = (*event_).scale;
  auto& delta_angle = (*event_).delta_angle;
  auto& sourceid = (*event_).sourceid;
  auto& mods = (*event_).mods;
  auto& group = (*event_).group;
  auto& flags = (*event_).flags;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // deviceid
  Read(&deviceid, &buf);

  // time
  Read(&time, &buf);

  // detail
  Read(&detail, &buf);

  // root
  Read(&root, &buf);

  // event
  Read(&event, &buf);

  // child
  Read(&child, &buf);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // event_x
  Read(&event_x, &buf);

  // event_y
  Read(&event_y, &buf);

  // delta_x
  Read(&delta_x, &buf);

  // delta_y
  Read(&delta_y, &buf);

  // delta_unaccel_x
  Read(&delta_unaccel_x, &buf);

  // delta_unaccel_y
  Read(&delta_unaccel_y, &buf);

  // scale
  Read(&scale, &buf);

  // delta_angle
  Read(&delta_angle, &buf);

  // sourceid
  Read(&sourceid, &buf);

  // pad0
  Pad(&buf, 2);

  // mods
  {
    auto& base = mods.base;
    auto& latched = mods.latched;
    auto& locked = mods.locked;
    auto& effective = mods.effective;

    // base
    Read(&base, &buf);

    // latched
    Read(&latched, &buf);

    // locked
    Read(&locked, &buf);

    // effective
    Read(&effective, &buf);
  }

  // group
  {
    auto& base = group.base;
    auto& latched = group.latched;
    auto& locked = group.locked;
    auto& effective = group.effective;

    // base
    Read(&base, &buf);

    // latched
    Read(&latched, &buf);

    // locked
    Read(&locked, &buf);

    // effective
    Read(&effective, &buf);
  }

  // flags
  uint32_t tmp27;
  Read(&tmp27, &buf);
  flags = static_cast<Input::GesturePinchEventFlags>(tmp27);

  Align(&buf, 4);
  CHECK_EQ(buf.offset, 32 + 4 * length);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Input::GestureSwipeEvent>(Input::GestureSwipeEvent* event_,
                                         ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*event_).sequence;
  auto& deviceid = (*event_).deviceid;
  auto& time = (*event_).time;
  auto& detail = (*event_).detail;
  auto& root = (*event_).root;
  auto& event = (*event_).event;
  auto& child = (*event_).child;
  auto& root_x = (*event_).root_x;
  auto& root_y = (*event_).root_y;
  auto& event_x = (*event_).event_x;
  auto& event_y = (*event_).event_y;
  auto& delta_x = (*event_).delta_x;
  auto& delta_y = (*event_).delta_y;
  auto& delta_unaccel_x = (*event_).delta_unaccel_x;
  auto& delta_unaccel_y = (*event_).delta_unaccel_y;
  auto& sourceid = (*event_).sourceid;
  auto& mods = (*event_).mods;
  auto& group = (*event_).group;
  auto& flags = (*event_).flags;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // extension
  uint8_t extension;
  Read(&extension, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // event_type
  uint16_t event_type;
  Read(&event_type, &buf);

  // deviceid
  Read(&deviceid, &buf);

  // time
  Read(&time, &buf);

  // detail
  Read(&detail, &buf);

  // root
  Read(&root, &buf);

  // event
  Read(&event, &buf);

  // child
  Read(&child, &buf);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // event_x
  Read(&event_x, &buf);

  // event_y
  Read(&event_y, &buf);

  // delta_x
  Read(&delta_x, &buf);

  // delta_y
  Read(&delta_y, &buf);

  // delta_unaccel_x
  Read(&delta_unaccel_x, &buf);

  // delta_unaccel_y
  Read(&delta_unaccel_y, &buf);

  // sourceid
  Read(&sourceid, &buf);

  // pad0
  Pad(&buf, 2);

  // mods
  {
    auto& base = mods.base;
    auto& latched = mods.latched;
    auto& locked = mods.locked;
    auto& effective = mods.effective;

    // base
    Read(&base, &buf);

    // latched
    Read(&latched, &buf);

    // locked
    Read(&locked, &buf);

    // effective
    Read(&effective, &buf);
  }

  // group
  {
    auto& base = group.base;
    auto& latched = group.latched;
    auto& locked = group.locked;
    auto& effective = group.effective;

    // base
    Read(&base, &buf);

    // latched
    Read(&latched, &buf);

    // locked
    Read(&locked, &buf);

    // effective
    Read(&effective, &buf);
  }

  // flags
  uint32_t tmp28;
  Read(&tmp28, &buf);
  flags = static_cast<Input::GestureSwipeEventFlags>(tmp28);

  Align(&buf, 4);
  CHECK_EQ(buf.offset, 32 + 4 * length);
}

std::string Input::DeviceError::ToString() const {
  std::stringstream ss_;
  ss_ << "Input::DeviceError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Input::DeviceError>(Input::DeviceError* error_,
                                   ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  CHECK_LE(buf.offset, 32ul);
}

std::string Input::EventError::ToString() const {
  std::stringstream ss_;
  ss_ << "Input::EventError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Input::EventError>(Input::EventError* error_,
                                  ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  CHECK_LE(buf.offset, 32ul);
}

std::string Input::ModeError::ToString() const {
  std::stringstream ss_;
  ss_ << "Input::ModeError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Input::ModeError>(Input::ModeError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  CHECK_LE(buf.offset, 32ul);
}

std::string Input::DeviceBusyError::ToString() const {
  std::stringstream ss_;
  ss_ << "Input::DeviceBusyError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Input::DeviceBusyError>(Input::DeviceBusyError* error_,
                                       ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  CHECK_LE(buf.offset, 32ul);
}

std::string Input::ClassError::ToString() const {
  std::stringstream ss_;
  ss_ << "Input::ClassError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_value = " << static_cast<uint64_t>(bad_value) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Input::ClassError>(Input::ClassError* error_,
                                  ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_value = (*error_).bad_value;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_value
  Read(&bad_value, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  CHECK_LE(buf.offset, 32ul);
}

Future<Input::GetExtensionVersionReply> Input::GetExtensionVersion(
    const Input::GetExtensionVersionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint16_t name_len{};
  auto& name = request.name;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // name_len
  name_len = name.size();
  buf.Write(&name_len);

  // pad0
  Pad(&buf, 2);

  // name
  CHECK_EQ(static_cast<size_t>(name_len), name.size());
  for (auto& name_elem : name) {
    // name_elem
    buf.Write(&name_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<Input::GetExtensionVersionReply>(
      &buf, "Input::GetExtensionVersion", false);
}

Future<Input::GetExtensionVersionReply> Input::GetExtensionVersion(
    const std::string& name) {
  return Input::GetExtensionVersion(Input::GetExtensionVersionRequest{name});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GetExtensionVersionReply> detail::ReadReply<
    Input::GetExtensionVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GetExtensionVersionReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& server_major = (*reply).server_major;
  auto& server_minor = (*reply).server_minor;
  auto& present = (*reply).present;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // server_major
  Read(&server_major, &buf);

  // server_minor
  Read(&server_minor, &buf);

  // present
  Read(&present, &buf);

  // pad0
  Pad(&buf, 19);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::ListInputDevicesReply> Input::ListInputDevices(
    const Input::ListInputDevicesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<Input::ListInputDevicesReply>(
      &buf, "Input::ListInputDevices", false);
}

Future<Input::ListInputDevicesReply> Input::ListInputDevices() {
  return Input::ListInputDevices(Input::ListInputDevicesRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::ListInputDevicesReply> detail::ReadReply<
    Input::ListInputDevicesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::ListInputDevicesReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  uint8_t devices_len{};
  auto& devices = (*reply).devices;
  auto& infos = (*reply).infos;
  auto& names = (*reply).names;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // devices_len
  Read(&devices_len, &buf);

  // pad0
  Pad(&buf, 23);

  // devices
  devices.resize(devices_len);
  for (auto& devices_elem : devices) {
    // devices_elem
    {
      auto& device_type = devices_elem.device_type;
      auto& device_id = devices_elem.device_id;
      auto& num_class_info = devices_elem.num_class_info;
      auto& device_use = devices_elem.device_use;

      // device_type
      Read(&device_type, &buf);

      // device_id
      Read(&device_id, &buf);

      // num_class_info
      Read(&num_class_info, &buf);

      // device_use
      uint8_t tmp29;
      Read(&tmp29, &buf);
      device_use = static_cast<Input::DeviceUse>(tmp29);

      // pad0
      Pad(&buf, 1);
    }
  }

  // infos
  auto sum30_ = SumOf(
      [](auto& listelem_ref) {
        auto& device_type = listelem_ref.device_type;
        auto& device_id = listelem_ref.device_id;
        auto& num_class_info = listelem_ref.num_class_info;
        auto& device_use = listelem_ref.device_use;

        return num_class_info;
      },
      devices);
  infos.resize(sum30_);
  for (auto& infos_elem : infos) {
    // infos_elem
    {
      Input::InputClass class_id{};
      auto& len = infos_elem.len;
      auto& info = infos_elem;

      // class_id
      uint8_t tmp31;
      Read(&tmp31, &buf);
      class_id = static_cast<Input::InputClass>(tmp31);

      // len
      Read(&len, &buf);

      // info
      auto info_expr = class_id;
      if (CaseEq(info_expr, Input::InputClass::Key)) {
        info.key.emplace(decltype(info.key)::value_type());
        auto& min_keycode = (*info.key).min_keycode;
        auto& max_keycode = (*info.key).max_keycode;
        auto& num_keys = (*info.key).num_keys;

        // min_keycode
        Read(&min_keycode, &buf);

        // max_keycode
        Read(&max_keycode, &buf);

        // num_keys
        Read(&num_keys, &buf);

        // pad0
        Pad(&buf, 2);
      }
      if (CaseEq(info_expr, Input::InputClass::Button)) {
        info.button.emplace(decltype(info.button)::value_type());
        auto& num_buttons = (*info.button).num_buttons;

        // num_buttons
        Read(&num_buttons, &buf);
      }
      if (CaseEq(info_expr, Input::InputClass::Valuator)) {
        info.valuator.emplace(decltype(info.valuator)::value_type());
        uint8_t axes_len{};
        auto& mode = (*info.valuator).mode;
        auto& motion_size = (*info.valuator).motion_size;
        auto& axes = (*info.valuator).axes;

        // axes_len
        Read(&axes_len, &buf);

        // mode
        uint8_t tmp32;
        Read(&tmp32, &buf);
        mode = static_cast<Input::ValuatorMode>(tmp32);

        // motion_size
        Read(&motion_size, &buf);

        // axes
        axes.resize(axes_len);
        for (auto& axes_elem : axes) {
          // axes_elem
          {
            auto& resolution = axes_elem.resolution;
            auto& minimum = axes_elem.minimum;
            auto& maximum = axes_elem.maximum;

            // resolution
            Read(&resolution, &buf);

            // minimum
            Read(&minimum, &buf);

            // maximum
            Read(&maximum, &buf);
          }
        }
      }
    }
  }

  // names
  names.resize(devices_len);
  for (auto& names_elem : names) {
    // names_elem
    {
      uint8_t name_len{};
      auto& name = names_elem.name;

      // name_len
      Read(&name_len, &buf);

      // name
      name.resize(name_len);
      for (auto& name_elem : name) {
        // name_elem
        Read(&name_elem, &buf);
      }
    }
  }

  // pad1
  Pad(&buf, 1);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::OpenDeviceReply> Input::OpenDevice(
    const Input::OpenDeviceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Input::OpenDeviceReply>(
      &buf, "Input::OpenDevice", false);
}

Future<Input::OpenDeviceReply> Input::OpenDevice(const uint8_t& device_id) {
  return Input::OpenDevice(Input::OpenDeviceRequest{device_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::OpenDeviceReply> detail::ReadReply<
    Input::OpenDeviceReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::OpenDeviceReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  uint8_t num_classes{};
  auto& class_info = (*reply).class_info;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_classes
  Read(&num_classes, &buf);

  // pad0
  Pad(&buf, 23);

  // class_info
  class_info.resize(num_classes);
  for (auto& class_info_elem : class_info) {
    // class_info_elem
    {
      auto& class_id = class_info_elem.class_id;
      auto& event_type_base = class_info_elem.event_type_base;

      // class_id
      uint8_t tmp33;
      Read(&tmp33, &buf);
      class_id = static_cast<Input::InputClass>(tmp33);

      // event_type_base
      Read(&event_type_base, &buf);
    }
  }

  // pad1
  Align(&buf, 4);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::CloseDevice(const Input::CloseDeviceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::CloseDevice", false);
}

Future<void> Input::CloseDevice(const uint8_t& device_id) {
  return Input::CloseDevice(Input::CloseDeviceRequest{device_id});
}

Future<Input::SetDeviceModeReply> Input::SetDeviceMode(
    const Input::SetDeviceModeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;
  auto& mode = request.mode;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // mode
  uint8_t tmp34;
  tmp34 = static_cast<uint8_t>(mode);
  buf.Write(&tmp34);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<Input::SetDeviceModeReply>(
      &buf, "Input::SetDeviceMode", false);
}

Future<Input::SetDeviceModeReply> Input::SetDeviceMode(
    const uint8_t& device_id,
    const ValuatorMode& mode) {
  return Input::SetDeviceMode(Input::SetDeviceModeRequest{device_id, mode});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::SetDeviceModeReply> detail::ReadReply<
    Input::SetDeviceModeReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::SetDeviceModeReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& status = (*reply).status;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // status
  uint8_t tmp35;
  Read(&tmp35, &buf);
  status = static_cast<GrabStatus>(tmp35);

  // pad0
  Pad(&buf, 23);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::SelectExtensionEvent(
    const Input::SelectExtensionEventRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  uint16_t num_classes{};
  auto& classes = request.classes;
  size_t classes_len = classes.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // num_classes
  num_classes = classes.size();
  buf.Write(&num_classes);

  // pad0
  Pad(&buf, 2);

  // classes
  CHECK_EQ(static_cast<size_t>(num_classes), classes.size());
  for (auto& classes_elem : classes) {
    // classes_elem
    buf.Write(&classes_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::SelectExtensionEvent",
                                        false);
}

Future<void> Input::SelectExtensionEvent(
    const Window& window,
    const std::vector<EventClass>& classes) {
  return Input::SelectExtensionEvent(
      Input::SelectExtensionEventRequest{window, classes});
}

Future<Input::GetSelectedExtensionEventsReply>
Input::GetSelectedExtensionEvents(
    const Input::GetSelectedExtensionEventsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<Input::GetSelectedExtensionEventsReply>(
      &buf, "Input::GetSelectedExtensionEvents", false);
}

Future<Input::GetSelectedExtensionEventsReply>
Input::GetSelectedExtensionEvents(const Window& window) {
  return Input::GetSelectedExtensionEvents(
      Input::GetSelectedExtensionEventsRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GetSelectedExtensionEventsReply> detail::ReadReply<
    Input::GetSelectedExtensionEventsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GetSelectedExtensionEventsReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  uint16_t num_this_classes{};
  uint16_t num_all_classes{};
  auto& this_classes = (*reply).this_classes;
  auto& all_classes = (*reply).all_classes;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_this_classes
  Read(&num_this_classes, &buf);

  // num_all_classes
  Read(&num_all_classes, &buf);

  // pad0
  Pad(&buf, 20);

  // this_classes
  this_classes.resize(num_this_classes);
  for (auto& this_classes_elem : this_classes) {
    // this_classes_elem
    Read(&this_classes_elem, &buf);
  }

  // all_classes
  all_classes.resize(num_all_classes);
  for (auto& all_classes_elem : all_classes) {
    // all_classes_elem
    Read(&all_classes_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::ChangeDeviceDontPropagateList(
    const Input::ChangeDeviceDontPropagateListRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  uint16_t num_classes{};
  auto& mode = request.mode;
  auto& classes = request.classes;
  size_t classes_len = classes.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // num_classes
  num_classes = classes.size();
  buf.Write(&num_classes);

  // mode
  uint8_t tmp36;
  tmp36 = static_cast<uint8_t>(mode);
  buf.Write(&tmp36);

  // pad0
  Pad(&buf, 1);

  // classes
  CHECK_EQ(static_cast<size_t>(num_classes), classes.size());
  for (auto& classes_elem : classes) {
    // classes_elem
    buf.Write(&classes_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(
      &buf, "Input::ChangeDeviceDontPropagateList", false);
}

Future<void> Input::ChangeDeviceDontPropagateList(
    const Window& window,
    const PropagateMode& mode,
    const std::vector<EventClass>& classes) {
  return Input::ChangeDeviceDontPropagateList(
      Input::ChangeDeviceDontPropagateListRequest{window, mode, classes});
}

Future<Input::GetDeviceDontPropagateListReply>
Input::GetDeviceDontPropagateList(
    const Input::GetDeviceDontPropagateListRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 9;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<Input::GetDeviceDontPropagateListReply>(
      &buf, "Input::GetDeviceDontPropagateList", false);
}

Future<Input::GetDeviceDontPropagateListReply>
Input::GetDeviceDontPropagateList(const Window& window) {
  return Input::GetDeviceDontPropagateList(
      Input::GetDeviceDontPropagateListRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GetDeviceDontPropagateListReply> detail::ReadReply<
    Input::GetDeviceDontPropagateListReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GetDeviceDontPropagateListReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  uint16_t num_classes{};
  auto& classes = (*reply).classes;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_classes
  Read(&num_classes, &buf);

  // pad0
  Pad(&buf, 22);

  // classes
  classes.resize(num_classes);
  for (auto& classes_elem : classes) {
    // classes_elem
    Read(&classes_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::GetDeviceMotionEventsReply> Input::GetDeviceMotionEvents(
    const Input::GetDeviceMotionEventsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& start = request.start;
  auto& stop = request.stop;
  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // start
  buf.Write(&start);

  // stop
  buf.Write(&stop);

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Input::GetDeviceMotionEventsReply>(
      &buf, "Input::GetDeviceMotionEvents", false);
}

Future<Input::GetDeviceMotionEventsReply> Input::GetDeviceMotionEvents(
    const Time& start,
    const Time& stop,
    const uint8_t& device_id) {
  return Input::GetDeviceMotionEvents(
      Input::GetDeviceMotionEventsRequest{start, stop, device_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GetDeviceMotionEventsReply> detail::ReadReply<
    Input::GetDeviceMotionEventsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GetDeviceMotionEventsReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  uint32_t num_events{};
  auto& num_axes = (*reply).num_axes;
  auto& device_mode = (*reply).device_mode;
  auto& events = (*reply).events;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_events
  Read(&num_events, &buf);

  // num_axes
  Read(&num_axes, &buf);

  // device_mode
  uint8_t tmp37;
  Read(&tmp37, &buf);
  device_mode = static_cast<Input::ValuatorMode>(tmp37);

  // pad0
  Pad(&buf, 18);

  // events
  events.resize(num_events);
  for (auto& events_elem : events) {
    // events_elem
    {
      auto& time = events_elem.time;
      auto& axisvalues = events_elem.axisvalues;

      // time
      Read(&time, &buf);

      // axisvalues
      axisvalues.resize(num_axes);
      for (auto& axisvalues_elem : axisvalues) {
        // axisvalues_elem
        Read(&axisvalues_elem, &buf);
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::ChangeKeyboardDeviceReply> Input::ChangeKeyboardDevice(
    const Input::ChangeKeyboardDeviceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Input::ChangeKeyboardDeviceReply>(
      &buf, "Input::ChangeKeyboardDevice", false);
}

Future<Input::ChangeKeyboardDeviceReply> Input::ChangeKeyboardDevice(
    const uint8_t& device_id) {
  return Input::ChangeKeyboardDevice(
      Input::ChangeKeyboardDeviceRequest{device_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::ChangeKeyboardDeviceReply> detail::ReadReply<
    Input::ChangeKeyboardDeviceReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::ChangeKeyboardDeviceReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& status = (*reply).status;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // status
  uint8_t tmp38;
  Read(&tmp38, &buf);
  status = static_cast<GrabStatus>(tmp38);

  // pad0
  Pad(&buf, 23);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::ChangePointerDeviceReply> Input::ChangePointerDevice(
    const Input::ChangePointerDeviceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& x_axis = request.x_axis;
  auto& y_axis = request.y_axis;
  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // x_axis
  buf.Write(&x_axis);

  // y_axis
  buf.Write(&y_axis);

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 1);

  Align(&buf, 4);

  return connection_->SendRequest<Input::ChangePointerDeviceReply>(
      &buf, "Input::ChangePointerDevice", false);
}

Future<Input::ChangePointerDeviceReply> Input::ChangePointerDevice(
    const uint8_t& x_axis,
    const uint8_t& y_axis,
    const uint8_t& device_id) {
  return Input::ChangePointerDevice(
      Input::ChangePointerDeviceRequest{x_axis, y_axis, device_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::ChangePointerDeviceReply> detail::ReadReply<
    Input::ChangePointerDeviceReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::ChangePointerDeviceReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& status = (*reply).status;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // status
  uint8_t tmp39;
  Read(&tmp39, &buf);
  status = static_cast<GrabStatus>(tmp39);

  // pad0
  Pad(&buf, 23);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::GrabDeviceReply> Input::GrabDevice(
    const Input::GrabDeviceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& grab_window = request.grab_window;
  auto& time = request.time;
  uint16_t num_classes{};
  auto& this_device_mode = request.this_device_mode;
  auto& other_device_mode = request.other_device_mode;
  auto& owner_events = request.owner_events;
  auto& device_id = request.device_id;
  auto& classes = request.classes;
  size_t classes_len = classes.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 13;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grab_window
  buf.Write(&grab_window);

  // time
  buf.Write(&time);

  // num_classes
  num_classes = classes.size();
  buf.Write(&num_classes);

  // this_device_mode
  uint8_t tmp40;
  tmp40 = static_cast<uint8_t>(this_device_mode);
  buf.Write(&tmp40);

  // other_device_mode
  uint8_t tmp41;
  tmp41 = static_cast<uint8_t>(other_device_mode);
  buf.Write(&tmp41);

  // owner_events
  buf.Write(&owner_events);

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 2);

  // classes
  CHECK_EQ(static_cast<size_t>(num_classes), classes.size());
  for (auto& classes_elem : classes) {
    // classes_elem
    buf.Write(&classes_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<Input::GrabDeviceReply>(
      &buf, "Input::GrabDevice", false);
}

Future<Input::GrabDeviceReply> Input::GrabDevice(
    const Window& grab_window,
    const Time& time,
    const GrabMode& this_device_mode,
    const GrabMode& other_device_mode,
    const uint8_t& owner_events,
    const uint8_t& device_id,
    const std::vector<EventClass>& classes) {
  return Input::GrabDevice(Input::GrabDeviceRequest{
      grab_window, time, this_device_mode, other_device_mode, owner_events,
      device_id, classes});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GrabDeviceReply> detail::ReadReply<
    Input::GrabDeviceReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GrabDeviceReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& status = (*reply).status;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // status
  uint8_t tmp42;
  Read(&tmp42, &buf);
  status = static_cast<GrabStatus>(tmp42);

  // pad0
  Pad(&buf, 23);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::UngrabDevice(const Input::UngrabDeviceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& time = request.time;
  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 14;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // time
  buf.Write(&time);

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::UngrabDevice", false);
}

Future<void> Input::UngrabDevice(const Time& time, const uint8_t& device_id) {
  return Input::UngrabDevice(Input::UngrabDeviceRequest{time, device_id});
}

Future<void> Input::GrabDeviceKey(const Input::GrabDeviceKeyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& grab_window = request.grab_window;
  uint16_t num_classes{};
  auto& modifiers = request.modifiers;
  auto& modifier_device = request.modifier_device;
  auto& grabbed_device = request.grabbed_device;
  auto& key = request.key;
  auto& this_device_mode = request.this_device_mode;
  auto& other_device_mode = request.other_device_mode;
  auto& owner_events = request.owner_events;
  auto& classes = request.classes;
  size_t classes_len = classes.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 15;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grab_window
  buf.Write(&grab_window);

  // num_classes
  num_classes = classes.size();
  buf.Write(&num_classes);

  // modifiers
  uint16_t tmp43;
  tmp43 = static_cast<uint16_t>(modifiers);
  buf.Write(&tmp43);

  // modifier_device
  buf.Write(&modifier_device);

  // grabbed_device
  buf.Write(&grabbed_device);

  // key
  buf.Write(&key);

  // this_device_mode
  uint8_t tmp44;
  tmp44 = static_cast<uint8_t>(this_device_mode);
  buf.Write(&tmp44);

  // other_device_mode
  uint8_t tmp45;
  tmp45 = static_cast<uint8_t>(other_device_mode);
  buf.Write(&tmp45);

  // owner_events
  buf.Write(&owner_events);

  // pad0
  Pad(&buf, 2);

  // classes
  CHECK_EQ(static_cast<size_t>(num_classes), classes.size());
  for (auto& classes_elem : classes) {
    // classes_elem
    buf.Write(&classes_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::GrabDeviceKey", false);
}

Future<void> Input::GrabDeviceKey(const Window& grab_window,
                                  const ModMask& modifiers,
                                  const uint8_t& modifier_device,
                                  const uint8_t& grabbed_device,
                                  const uint8_t& key,
                                  const GrabMode& this_device_mode,
                                  const GrabMode& other_device_mode,
                                  const uint8_t& owner_events,
                                  const std::vector<EventClass>& classes) {
  return Input::GrabDeviceKey(Input::GrabDeviceKeyRequest{
      grab_window, modifiers, modifier_device, grabbed_device, key,
      this_device_mode, other_device_mode, owner_events, classes});
}

Future<void> Input::UngrabDeviceKey(
    const Input::UngrabDeviceKeyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& grabWindow = request.grabWindow;
  auto& modifiers = request.modifiers;
  auto& modifier_device = request.modifier_device;
  auto& key = request.key;
  auto& grabbed_device = request.grabbed_device;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 16;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grabWindow
  buf.Write(&grabWindow);

  // modifiers
  uint16_t tmp46;
  tmp46 = static_cast<uint16_t>(modifiers);
  buf.Write(&tmp46);

  // modifier_device
  buf.Write(&modifier_device);

  // key
  buf.Write(&key);

  // grabbed_device
  buf.Write(&grabbed_device);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::UngrabDeviceKey", false);
}

Future<void> Input::UngrabDeviceKey(const Window& grabWindow,
                                    const ModMask& modifiers,
                                    const uint8_t& modifier_device,
                                    const uint8_t& key,
                                    const uint8_t& grabbed_device) {
  return Input::UngrabDeviceKey(Input::UngrabDeviceKeyRequest{
      grabWindow, modifiers, modifier_device, key, grabbed_device});
}

Future<void> Input::GrabDeviceButton(
    const Input::GrabDeviceButtonRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& grab_window = request.grab_window;
  auto& grabbed_device = request.grabbed_device;
  auto& modifier_device = request.modifier_device;
  uint16_t num_classes{};
  auto& modifiers = request.modifiers;
  auto& this_device_mode = request.this_device_mode;
  auto& other_device_mode = request.other_device_mode;
  auto& button = request.button;
  auto& owner_events = request.owner_events;
  auto& classes = request.classes;
  size_t classes_len = classes.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 17;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grab_window
  buf.Write(&grab_window);

  // grabbed_device
  buf.Write(&grabbed_device);

  // modifier_device
  buf.Write(&modifier_device);

  // num_classes
  num_classes = classes.size();
  buf.Write(&num_classes);

  // modifiers
  uint16_t tmp47;
  tmp47 = static_cast<uint16_t>(modifiers);
  buf.Write(&tmp47);

  // this_device_mode
  uint8_t tmp48;
  tmp48 = static_cast<uint8_t>(this_device_mode);
  buf.Write(&tmp48);

  // other_device_mode
  uint8_t tmp49;
  tmp49 = static_cast<uint8_t>(other_device_mode);
  buf.Write(&tmp49);

  // button
  buf.Write(&button);

  // owner_events
  buf.Write(&owner_events);

  // pad0
  Pad(&buf, 2);

  // classes
  CHECK_EQ(static_cast<size_t>(num_classes), classes.size());
  for (auto& classes_elem : classes) {
    // classes_elem
    buf.Write(&classes_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::GrabDeviceButton", false);
}

Future<void> Input::GrabDeviceButton(const Window& grab_window,
                                     const uint8_t& grabbed_device,
                                     const uint8_t& modifier_device,
                                     const ModMask& modifiers,
                                     const GrabMode& this_device_mode,
                                     const GrabMode& other_device_mode,
                                     const uint8_t& button,
                                     const uint8_t& owner_events,
                                     const std::vector<EventClass>& classes) {
  return Input::GrabDeviceButton(Input::GrabDeviceButtonRequest{
      grab_window, grabbed_device, modifier_device, modifiers, this_device_mode,
      other_device_mode, button, owner_events, classes});
}

Future<void> Input::UngrabDeviceButton(
    const Input::UngrabDeviceButtonRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& grab_window = request.grab_window;
  auto& modifiers = request.modifiers;
  auto& modifier_device = request.modifier_device;
  auto& button = request.button;
  auto& grabbed_device = request.grabbed_device;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 18;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grab_window
  buf.Write(&grab_window);

  // modifiers
  uint16_t tmp50;
  tmp50 = static_cast<uint16_t>(modifiers);
  buf.Write(&tmp50);

  // modifier_device
  buf.Write(&modifier_device);

  // button
  buf.Write(&button);

  // grabbed_device
  buf.Write(&grabbed_device);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::UngrabDeviceButton",
                                        false);
}

Future<void> Input::UngrabDeviceButton(const Window& grab_window,
                                       const ModMask& modifiers,
                                       const uint8_t& modifier_device,
                                       const uint8_t& button,
                                       const uint8_t& grabbed_device) {
  return Input::UngrabDeviceButton(Input::UngrabDeviceButtonRequest{
      grab_window, modifiers, modifier_device, button, grabbed_device});
}

Future<void> Input::AllowDeviceEvents(
    const Input::AllowDeviceEventsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& time = request.time;
  auto& mode = request.mode;
  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 19;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // time
  buf.Write(&time);

  // mode
  uint8_t tmp51;
  tmp51 = static_cast<uint8_t>(mode);
  buf.Write(&tmp51);

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::AllowDeviceEvents",
                                        false);
}

Future<void> Input::AllowDeviceEvents(const Time& time,
                                      const DeviceInputMode& mode,
                                      const uint8_t& device_id) {
  return Input::AllowDeviceEvents(
      Input::AllowDeviceEventsRequest{time, mode, device_id});
}

Future<Input::GetDeviceFocusReply> Input::GetDeviceFocus(
    const Input::GetDeviceFocusRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 20;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Input::GetDeviceFocusReply>(
      &buf, "Input::GetDeviceFocus", false);
}

Future<Input::GetDeviceFocusReply> Input::GetDeviceFocus(
    const uint8_t& device_id) {
  return Input::GetDeviceFocus(Input::GetDeviceFocusRequest{device_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GetDeviceFocusReply> detail::ReadReply<
    Input::GetDeviceFocusReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GetDeviceFocusReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& focus = (*reply).focus;
  auto& time = (*reply).time;
  auto& revert_to = (*reply).revert_to;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // focus
  Read(&focus, &buf);

  // time
  Read(&time, &buf);

  // revert_to
  uint8_t tmp52;
  Read(&tmp52, &buf);
  revert_to = static_cast<InputFocus>(tmp52);

  // pad0
  Pad(&buf, 15);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::SetDeviceFocus(
    const Input::SetDeviceFocusRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& focus = request.focus;
  auto& time = request.time;
  auto& revert_to = request.revert_to;
  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 21;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // focus
  buf.Write(&focus);

  // time
  buf.Write(&time);

  // revert_to
  uint8_t tmp53;
  tmp53 = static_cast<uint8_t>(revert_to);
  buf.Write(&tmp53);

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::SetDeviceFocus", false);
}

Future<void> Input::SetDeviceFocus(const Window& focus,
                                   const Time& time,
                                   const InputFocus& revert_to,
                                   const uint8_t& device_id) {
  return Input::SetDeviceFocus(
      Input::SetDeviceFocusRequest{focus, time, revert_to, device_id});
}

Future<Input::GetFeedbackControlReply> Input::GetFeedbackControl(
    const Input::GetFeedbackControlRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 22;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Input::GetFeedbackControlReply>(
      &buf, "Input::GetFeedbackControl", false);
}

Future<Input::GetFeedbackControlReply> Input::GetFeedbackControl(
    const uint8_t& device_id) {
  return Input::GetFeedbackControl(Input::GetFeedbackControlRequest{device_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GetFeedbackControlReply> detail::ReadReply<
    Input::GetFeedbackControlReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GetFeedbackControlReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  uint16_t num_feedbacks{};
  auto& feedbacks = (*reply).feedbacks;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_feedbacks
  Read(&num_feedbacks, &buf);

  // pad0
  Pad(&buf, 22);

  // feedbacks
  feedbacks.resize(num_feedbacks);
  for (auto& feedbacks_elem : feedbacks) {
    // feedbacks_elem
    {
      Input::FeedbackClass class_id{};
      auto& feedback_id = feedbacks_elem.feedback_id;
      auto& len = feedbacks_elem.len;
      auto& data = feedbacks_elem;

      // class_id
      uint8_t tmp54;
      Read(&tmp54, &buf);
      class_id = static_cast<Input::FeedbackClass>(tmp54);

      // feedback_id
      Read(&feedback_id, &buf);

      // len
      Read(&len, &buf);

      // data
      auto data_expr = class_id;
      if (CaseEq(data_expr, Input::FeedbackClass::Keyboard)) {
        data.keyboard.emplace(decltype(data.keyboard)::value_type());
        auto& pitch = (*data.keyboard).pitch;
        auto& duration = (*data.keyboard).duration;
        auto& led_mask = (*data.keyboard).led_mask;
        auto& led_values = (*data.keyboard).led_values;
        auto& global_auto_repeat = (*data.keyboard).global_auto_repeat;
        auto& click = (*data.keyboard).click;
        auto& percent = (*data.keyboard).percent;
        auto& auto_repeats = (*data.keyboard).auto_repeats;

        // pitch
        Read(&pitch, &buf);

        // duration
        Read(&duration, &buf);

        // led_mask
        Read(&led_mask, &buf);

        // led_values
        Read(&led_values, &buf);

        // global_auto_repeat
        Read(&global_auto_repeat, &buf);

        // click
        Read(&click, &buf);

        // percent
        Read(&percent, &buf);

        // pad0
        Pad(&buf, 1);

        // auto_repeats
        for (auto& auto_repeats_elem : auto_repeats) {
          // auto_repeats_elem
          Read(&auto_repeats_elem, &buf);
        }
      }
      if (CaseEq(data_expr, Input::FeedbackClass::Pointer)) {
        data.pointer.emplace(decltype(data.pointer)::value_type());
        auto& accel_num = (*data.pointer).accel_num;
        auto& accel_denom = (*data.pointer).accel_denom;
        auto& threshold = (*data.pointer).threshold;

        // pad1
        Pad(&buf, 2);

        // accel_num
        Read(&accel_num, &buf);

        // accel_denom
        Read(&accel_denom, &buf);

        // threshold
        Read(&threshold, &buf);
      }
      if (CaseEq(data_expr, Input::FeedbackClass::String)) {
        data.string.emplace(decltype(data.string)::value_type());
        auto& max_symbols = (*data.string).max_symbols;
        uint16_t num_keysyms{};
        auto& keysyms = (*data.string).keysyms;

        // max_symbols
        Read(&max_symbols, &buf);

        // num_keysyms
        Read(&num_keysyms, &buf);

        // keysyms
        keysyms.resize(num_keysyms);
        for (auto& keysyms_elem : keysyms) {
          // keysyms_elem
          Read(&keysyms_elem, &buf);
        }
      }
      if (CaseEq(data_expr, Input::FeedbackClass::Integer)) {
        data.integer.emplace(decltype(data.integer)::value_type());
        auto& resolution = (*data.integer).resolution;
        auto& min_value = (*data.integer).min_value;
        auto& max_value = (*data.integer).max_value;

        // resolution
        Read(&resolution, &buf);

        // min_value
        Read(&min_value, &buf);

        // max_value
        Read(&max_value, &buf);
      }
      if (CaseEq(data_expr, Input::FeedbackClass::Led)) {
        data.led.emplace(decltype(data.led)::value_type());
        auto& led_mask = (*data.led).led_mask;
        auto& led_values = (*data.led).led_values;

        // led_mask
        Read(&led_mask, &buf);

        // led_values
        Read(&led_values, &buf);
      }
      if (CaseEq(data_expr, Input::FeedbackClass::Bell)) {
        data.bell.emplace(decltype(data.bell)::value_type());
        auto& percent = (*data.bell).percent;
        auto& pitch = (*data.bell).pitch;
        auto& duration = (*data.bell).duration;

        // percent
        Read(&percent, &buf);

        // pad2
        Pad(&buf, 3);

        // pitch
        Read(&pitch, &buf);

        // duration
        Read(&duration, &buf);
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::ChangeFeedbackControl(
    const Input::ChangeFeedbackControlRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& mask = request.mask;
  auto& device_id = request.device_id;
  auto& feedback_id = request.feedback_id;
  auto& feedback = request.feedback;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 23;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // mask
  uint32_t tmp55;
  tmp55 = static_cast<uint32_t>(mask);
  buf.Write(&tmp55);

  // device_id
  buf.Write(&device_id);

  // feedback_id
  buf.Write(&feedback_id);

  // pad0
  Pad(&buf, 2);

  // feedback
  {
    FeedbackClass class_id{};
    auto& feedback_id = feedback.feedback_id;
    auto& len = feedback.len;
    auto& data = feedback;

    // class_id
    SwitchVar(FeedbackClass::Keyboard, data.keyboard.has_value(), false,
              &class_id);
    SwitchVar(FeedbackClass::Pointer, data.pointer.has_value(), false,
              &class_id);
    SwitchVar(FeedbackClass::String, data.string.has_value(), false, &class_id);
    SwitchVar(FeedbackClass::Integer, data.integer.has_value(), false,
              &class_id);
    SwitchVar(FeedbackClass::Led, data.led.has_value(), false, &class_id);
    SwitchVar(FeedbackClass::Bell, data.bell.has_value(), false, &class_id);
    uint8_t tmp56;
    tmp56 = static_cast<uint8_t>(class_id);
    buf.Write(&tmp56);

    // feedback_id
    buf.Write(&feedback_id);

    // len
    buf.Write(&len);

    // data
    auto data_expr = class_id;
    if (CaseEq(data_expr, FeedbackClass::Keyboard)) {
      auto& key = (*data.keyboard).key;
      auto& auto_repeat_mode = (*data.keyboard).auto_repeat_mode;
      auto& key_click_percent = (*data.keyboard).key_click_percent;
      auto& bell_percent = (*data.keyboard).bell_percent;
      auto& bell_pitch = (*data.keyboard).bell_pitch;
      auto& bell_duration = (*data.keyboard).bell_duration;
      auto& led_mask = (*data.keyboard).led_mask;
      auto& led_values = (*data.keyboard).led_values;

      // key
      buf.Write(&key);

      // auto_repeat_mode
      buf.Write(&auto_repeat_mode);

      // key_click_percent
      buf.Write(&key_click_percent);

      // bell_percent
      buf.Write(&bell_percent);

      // bell_pitch
      buf.Write(&bell_pitch);

      // bell_duration
      buf.Write(&bell_duration);

      // led_mask
      buf.Write(&led_mask);

      // led_values
      buf.Write(&led_values);
    }
    if (CaseEq(data_expr, FeedbackClass::Pointer)) {
      auto& num = (*data.pointer).num;
      auto& denom = (*data.pointer).denom;
      auto& threshold = (*data.pointer).threshold;

      // pad0
      Pad(&buf, 2);

      // num
      buf.Write(&num);

      // denom
      buf.Write(&denom);

      // threshold
      buf.Write(&threshold);
    }
    if (CaseEq(data_expr, FeedbackClass::String)) {
      uint16_t num_keysyms{};
      auto& keysyms = (*data.string).keysyms;

      // pad1
      Pad(&buf, 2);

      // num_keysyms
      num_keysyms = keysyms.size();
      buf.Write(&num_keysyms);

      // keysyms
      CHECK_EQ(static_cast<size_t>(num_keysyms), keysyms.size());
      for (auto& keysyms_elem : keysyms) {
        // keysyms_elem
        buf.Write(&keysyms_elem);
      }
    }
    if (CaseEq(data_expr, FeedbackClass::Integer)) {
      auto& int_to_display = (*data.integer).int_to_display;

      // int_to_display
      buf.Write(&int_to_display);
    }
    if (CaseEq(data_expr, FeedbackClass::Led)) {
      auto& led_mask = (*data.led).led_mask;
      auto& led_values = (*data.led).led_values;

      // led_mask
      buf.Write(&led_mask);

      // led_values
      buf.Write(&led_values);
    }
    if (CaseEq(data_expr, FeedbackClass::Bell)) {
      auto& percent = (*data.bell).percent;
      auto& pitch = (*data.bell).pitch;
      auto& duration = (*data.bell).duration;

      // percent
      buf.Write(&percent);

      // pad2
      Pad(&buf, 3);

      // pitch
      buf.Write(&pitch);

      // duration
      buf.Write(&duration);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::ChangeFeedbackControl",
                                        false);
}

Future<void> Input::ChangeFeedbackControl(const ChangeFeedbackControlMask& mask,
                                          const uint8_t& device_id,
                                          const uint8_t& feedback_id,
                                          const FeedbackCtl& feedback) {
  return Input::ChangeFeedbackControl(Input::ChangeFeedbackControlRequest{
      mask, device_id, feedback_id, feedback});
}

Future<Input::GetDeviceKeyMappingReply> Input::GetDeviceKeyMapping(
    const Input::GetDeviceKeyMappingRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;
  auto& first_keycode = request.first_keycode;
  auto& count = request.count;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 24;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // first_keycode
  buf.Write(&first_keycode);

  // count
  buf.Write(&count);

  // pad0
  Pad(&buf, 1);

  Align(&buf, 4);

  return connection_->SendRequest<Input::GetDeviceKeyMappingReply>(
      &buf, "Input::GetDeviceKeyMapping", false);
}

Future<Input::GetDeviceKeyMappingReply> Input::GetDeviceKeyMapping(
    const uint8_t& device_id,
    const KeyCode& first_keycode,
    const uint8_t& count) {
  return Input::GetDeviceKeyMapping(
      Input::GetDeviceKeyMappingRequest{device_id, first_keycode, count});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GetDeviceKeyMappingReply> detail::ReadReply<
    Input::GetDeviceKeyMappingReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GetDeviceKeyMappingReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& keysyms_per_keycode = (*reply).keysyms_per_keycode;
  auto& keysyms = (*reply).keysyms;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // keysyms_per_keycode
  Read(&keysyms_per_keycode, &buf);

  // pad0
  Pad(&buf, 23);

  // keysyms
  keysyms.resize(length);
  for (auto& keysyms_elem : keysyms) {
    // keysyms_elem
    Read(&keysyms_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::ChangeDeviceKeyMapping(
    const Input::ChangeDeviceKeyMappingRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;
  auto& first_keycode = request.first_keycode;
  auto& keysyms_per_keycode = request.keysyms_per_keycode;
  auto& keycode_count = request.keycode_count;
  auto& keysyms = request.keysyms;
  size_t keysyms_len = keysyms.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 25;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // first_keycode
  buf.Write(&first_keycode);

  // keysyms_per_keycode
  buf.Write(&keysyms_per_keycode);

  // keycode_count
  buf.Write(&keycode_count);

  // keysyms
  CHECK_EQ(static_cast<size_t>((keycode_count) * (keysyms_per_keycode)),
           keysyms.size());
  for (auto& keysyms_elem : keysyms) {
    // keysyms_elem
    buf.Write(&keysyms_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::ChangeDeviceKeyMapping",
                                        false);
}

Future<void> Input::ChangeDeviceKeyMapping(const uint8_t& device_id,
                                           const KeyCode& first_keycode,
                                           const uint8_t& keysyms_per_keycode,
                                           const uint8_t& keycode_count,
                                           const std::vector<KeySym>& keysyms) {
  return Input::ChangeDeviceKeyMapping(Input::ChangeDeviceKeyMappingRequest{
      device_id, first_keycode, keysyms_per_keycode, keycode_count, keysyms});
}

Future<Input::GetDeviceModifierMappingReply> Input::GetDeviceModifierMapping(
    const Input::GetDeviceModifierMappingRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 26;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Input::GetDeviceModifierMappingReply>(
      &buf, "Input::GetDeviceModifierMapping", false);
}

Future<Input::GetDeviceModifierMappingReply> Input::GetDeviceModifierMapping(
    const uint8_t& device_id) {
  return Input::GetDeviceModifierMapping(
      Input::GetDeviceModifierMappingRequest{device_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GetDeviceModifierMappingReply> detail::ReadReply<
    Input::GetDeviceModifierMappingReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GetDeviceModifierMappingReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& keycodes_per_modifier = (*reply).keycodes_per_modifier;
  auto& keymaps = (*reply).keymaps;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // keycodes_per_modifier
  Read(&keycodes_per_modifier, &buf);

  // pad0
  Pad(&buf, 23);

  // keymaps
  keymaps.resize((keycodes_per_modifier) * (8));
  for (auto& keymaps_elem : keymaps) {
    // keymaps_elem
    Read(&keymaps_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::SetDeviceModifierMappingReply> Input::SetDeviceModifierMapping(
    const Input::SetDeviceModifierMappingRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;
  auto& keycodes_per_modifier = request.keycodes_per_modifier;
  auto& keymaps = request.keymaps;
  size_t keymaps_len = keymaps.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 27;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // keycodes_per_modifier
  buf.Write(&keycodes_per_modifier);

  // pad0
  Pad(&buf, 2);

  // keymaps
  CHECK_EQ(static_cast<size_t>((keycodes_per_modifier) * (8)), keymaps.size());
  for (auto& keymaps_elem : keymaps) {
    // keymaps_elem
    buf.Write(&keymaps_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<Input::SetDeviceModifierMappingReply>(
      &buf, "Input::SetDeviceModifierMapping", false);
}

Future<Input::SetDeviceModifierMappingReply> Input::SetDeviceModifierMapping(
    const uint8_t& device_id,
    const uint8_t& keycodes_per_modifier,
    const std::vector<uint8_t>& keymaps) {
  return Input::SetDeviceModifierMapping(Input::SetDeviceModifierMappingRequest{
      device_id, keycodes_per_modifier, keymaps});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::SetDeviceModifierMappingReply> detail::ReadReply<
    Input::SetDeviceModifierMappingReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::SetDeviceModifierMappingReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& status = (*reply).status;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // status
  uint8_t tmp57;
  Read(&tmp57, &buf);
  status = static_cast<MappingStatus>(tmp57);

  // pad0
  Pad(&buf, 23);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::GetDeviceButtonMappingReply> Input::GetDeviceButtonMapping(
    const Input::GetDeviceButtonMappingRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 28;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Input::GetDeviceButtonMappingReply>(
      &buf, "Input::GetDeviceButtonMapping", false);
}

Future<Input::GetDeviceButtonMappingReply> Input::GetDeviceButtonMapping(
    const uint8_t& device_id) {
  return Input::GetDeviceButtonMapping(
      Input::GetDeviceButtonMappingRequest{device_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GetDeviceButtonMappingReply> detail::ReadReply<
    Input::GetDeviceButtonMappingReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GetDeviceButtonMappingReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  uint8_t map_size{};
  auto& map = (*reply).map;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // map_size
  Read(&map_size, &buf);

  // pad0
  Pad(&buf, 23);

  // map
  map.resize(map_size);
  for (auto& map_elem : map) {
    // map_elem
    Read(&map_elem, &buf);
  }

  // pad1
  Align(&buf, 4);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::SetDeviceButtonMappingReply> Input::SetDeviceButtonMapping(
    const Input::SetDeviceButtonMappingRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;
  uint8_t map_size{};
  auto& map = request.map;
  size_t map_len = map.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 29;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // map_size
  map_size = map.size();
  buf.Write(&map_size);

  // pad0
  Pad(&buf, 2);

  // map
  CHECK_EQ(static_cast<size_t>(map_size), map.size());
  for (auto& map_elem : map) {
    // map_elem
    buf.Write(&map_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<Input::SetDeviceButtonMappingReply>(
      &buf, "Input::SetDeviceButtonMapping", false);
}

Future<Input::SetDeviceButtonMappingReply> Input::SetDeviceButtonMapping(
    const uint8_t& device_id,
    const std::vector<uint8_t>& map) {
  return Input::SetDeviceButtonMapping(
      Input::SetDeviceButtonMappingRequest{device_id, map});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::SetDeviceButtonMappingReply> detail::ReadReply<
    Input::SetDeviceButtonMappingReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::SetDeviceButtonMappingReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& status = (*reply).status;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // status
  uint8_t tmp58;
  Read(&tmp58, &buf);
  status = static_cast<MappingStatus>(tmp58);

  // pad0
  Pad(&buf, 23);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::QueryDeviceStateReply> Input::QueryDeviceState(
    const Input::QueryDeviceStateRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 30;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Input::QueryDeviceStateReply>(
      &buf, "Input::QueryDeviceState", false);
}

Future<Input::QueryDeviceStateReply> Input::QueryDeviceState(
    const uint8_t& device_id) {
  return Input::QueryDeviceState(Input::QueryDeviceStateRequest{device_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::QueryDeviceStateReply> detail::ReadReply<
    Input::QueryDeviceStateReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::QueryDeviceStateReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  uint8_t num_classes{};
  auto& classes = (*reply).classes;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_classes
  Read(&num_classes, &buf);

  // pad0
  Pad(&buf, 23);

  // classes
  classes.resize(num_classes);
  for (auto& classes_elem : classes) {
    // classes_elem
    {
      Input::InputClass class_id{};
      auto& len = classes_elem.len;
      auto& data = classes_elem;

      // class_id
      uint8_t tmp59;
      Read(&tmp59, &buf);
      class_id = static_cast<Input::InputClass>(tmp59);

      // len
      Read(&len, &buf);

      // data
      auto data_expr = class_id;
      if (CaseEq(data_expr, Input::InputClass::Key)) {
        data.key.emplace(decltype(data.key)::value_type());
        auto& num_keys = (*data.key).num_keys;
        auto& keys = (*data.key).keys;

        // num_keys
        Read(&num_keys, &buf);

        // pad0
        Pad(&buf, 1);

        // keys
        for (auto& keys_elem : keys) {
          // keys_elem
          Read(&keys_elem, &buf);
        }
      }
      if (CaseEq(data_expr, Input::InputClass::Button)) {
        data.button.emplace(decltype(data.button)::value_type());
        auto& num_buttons = (*data.button).num_buttons;
        auto& buttons = (*data.button).buttons;

        // num_buttons
        Read(&num_buttons, &buf);

        // pad1
        Pad(&buf, 1);

        // buttons
        for (auto& buttons_elem : buttons) {
          // buttons_elem
          Read(&buttons_elem, &buf);
        }
      }
      if (CaseEq(data_expr, Input::InputClass::Valuator)) {
        data.valuator.emplace(decltype(data.valuator)::value_type());
        uint8_t num_valuators{};
        auto& mode = (*data.valuator).mode;
        auto& valuators = (*data.valuator).valuators;

        // num_valuators
        Read(&num_valuators, &buf);

        // mode
        uint8_t tmp60;
        Read(&tmp60, &buf);
        mode = static_cast<Input::ValuatorStateModeMask>(tmp60);

        // valuators
        valuators.resize(num_valuators);
        for (auto& valuators_elem : valuators) {
          // valuators_elem
          Read(&valuators_elem, &buf);
        }
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::DeviceBell(const Input::DeviceBellRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;
  auto& feedback_id = request.feedback_id;
  auto& feedback_class = request.feedback_class;
  auto& percent = request.percent;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 32;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // feedback_id
  buf.Write(&feedback_id);

  // feedback_class
  buf.Write(&feedback_class);

  // percent
  buf.Write(&percent);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::DeviceBell", false);
}

Future<void> Input::DeviceBell(const uint8_t& device_id,
                               const uint8_t& feedback_id,
                               const uint8_t& feedback_class,
                               const int8_t& percent) {
  return Input::DeviceBell(Input::DeviceBellRequest{device_id, feedback_id,
                                                    feedback_class, percent});
}

Future<Input::SetDeviceValuatorsReply> Input::SetDeviceValuators(
    const Input::SetDeviceValuatorsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;
  auto& first_valuator = request.first_valuator;
  uint8_t num_valuators{};
  auto& valuators = request.valuators;
  size_t valuators_len = valuators.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 33;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // first_valuator
  buf.Write(&first_valuator);

  // num_valuators
  num_valuators = valuators.size();
  buf.Write(&num_valuators);

  // pad0
  Pad(&buf, 1);

  // valuators
  CHECK_EQ(static_cast<size_t>(num_valuators), valuators.size());
  for (auto& valuators_elem : valuators) {
    // valuators_elem
    buf.Write(&valuators_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<Input::SetDeviceValuatorsReply>(
      &buf, "Input::SetDeviceValuators", false);
}

Future<Input::SetDeviceValuatorsReply> Input::SetDeviceValuators(
    const uint8_t& device_id,
    const uint8_t& first_valuator,
    const std::vector<int32_t>& valuators) {
  return Input::SetDeviceValuators(
      Input::SetDeviceValuatorsRequest{device_id, first_valuator, valuators});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::SetDeviceValuatorsReply> detail::ReadReply<
    Input::SetDeviceValuatorsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::SetDeviceValuatorsReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& status = (*reply).status;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // status
  uint8_t tmp61;
  Read(&tmp61, &buf);
  status = static_cast<GrabStatus>(tmp61);

  // pad0
  Pad(&buf, 23);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::GetDeviceControlReply> Input::GetDeviceControl(
    const Input::GetDeviceControlRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& control_id = request.control_id;
  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 34;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // control_id
  uint16_t tmp62;
  tmp62 = static_cast<uint16_t>(control_id);
  buf.Write(&tmp62);

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 1);

  Align(&buf, 4);

  return connection_->SendRequest<Input::GetDeviceControlReply>(
      &buf, "Input::GetDeviceControl", false);
}

Future<Input::GetDeviceControlReply> Input::GetDeviceControl(
    const DeviceControl& control_id,
    const uint8_t& device_id) {
  return Input::GetDeviceControl(
      Input::GetDeviceControlRequest{control_id, device_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GetDeviceControlReply> detail::ReadReply<
    Input::GetDeviceControlReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GetDeviceControlReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& status = (*reply).status;
  auto& control = (*reply).control;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // status
  Read(&status, &buf);

  // pad0
  Pad(&buf, 23);

  // control
  {
    Input::DeviceControl control_id{};
    auto& len = control.len;
    auto& data = control;

    // control_id
    uint16_t tmp63;
    Read(&tmp63, &buf);
    control_id = static_cast<Input::DeviceControl>(tmp63);

    // len
    Read(&len, &buf);

    // data
    auto data_expr = control_id;
    if (CaseEq(data_expr, Input::DeviceControl::resolution)) {
      data.resolution.emplace(decltype(data.resolution)::value_type());
      uint32_t num_valuators{};
      auto& resolution_values = (*data.resolution).resolution_values;
      auto& resolution_min = (*data.resolution).resolution_min;
      auto& resolution_max = (*data.resolution).resolution_max;

      // num_valuators
      Read(&num_valuators, &buf);

      // resolution_values
      resolution_values.resize(num_valuators);
      for (auto& resolution_values_elem : resolution_values) {
        // resolution_values_elem
        Read(&resolution_values_elem, &buf);
      }

      // resolution_min
      resolution_min.resize(num_valuators);
      for (auto& resolution_min_elem : resolution_min) {
        // resolution_min_elem
        Read(&resolution_min_elem, &buf);
      }

      // resolution_max
      resolution_max.resize(num_valuators);
      for (auto& resolution_max_elem : resolution_max) {
        // resolution_max_elem
        Read(&resolution_max_elem, &buf);
      }
    }
    if (CaseEq(data_expr, Input::DeviceControl::abs_calib)) {
      data.abs_calib.emplace(decltype(data.abs_calib)::value_type());
      auto& min_x = (*data.abs_calib).min_x;
      auto& max_x = (*data.abs_calib).max_x;
      auto& min_y = (*data.abs_calib).min_y;
      auto& max_y = (*data.abs_calib).max_y;
      auto& flip_x = (*data.abs_calib).flip_x;
      auto& flip_y = (*data.abs_calib).flip_y;
      auto& rotation = (*data.abs_calib).rotation;
      auto& button_threshold = (*data.abs_calib).button_threshold;

      // min_x
      Read(&min_x, &buf);

      // max_x
      Read(&max_x, &buf);

      // min_y
      Read(&min_y, &buf);

      // max_y
      Read(&max_y, &buf);

      // flip_x
      Read(&flip_x, &buf);

      // flip_y
      Read(&flip_y, &buf);

      // rotation
      Read(&rotation, &buf);

      // button_threshold
      Read(&button_threshold, &buf);
    }
    if (CaseEq(data_expr, Input::DeviceControl::core)) {
      data.core.emplace(decltype(data.core)::value_type());
      auto& status = (*data.core).status;
      auto& iscore = (*data.core).iscore;

      // status
      Read(&status, &buf);

      // iscore
      Read(&iscore, &buf);

      // pad0
      Pad(&buf, 2);
    }
    if (CaseEq(data_expr, Input::DeviceControl::enable)) {
      data.enable.emplace(decltype(data.enable)::value_type());
      auto& enable = (*data.enable).enable;

      // enable
      Read(&enable, &buf);

      // pad1
      Pad(&buf, 3);
    }
    if (CaseEq(data_expr, Input::DeviceControl::abs_area)) {
      data.abs_area.emplace(decltype(data.abs_area)::value_type());
      auto& offset_x = (*data.abs_area).offset_x;
      auto& offset_y = (*data.abs_area).offset_y;
      auto& width = (*data.abs_area).width;
      auto& height = (*data.abs_area).height;
      auto& screen = (*data.abs_area).screen;
      auto& following = (*data.abs_area).following;

      // offset_x
      Read(&offset_x, &buf);

      // offset_y
      Read(&offset_y, &buf);

      // width
      Read(&width, &buf);

      // height
      Read(&height, &buf);

      // screen
      Read(&screen, &buf);

      // following
      Read(&following, &buf);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::ChangeDeviceControlReply> Input::ChangeDeviceControl(
    const Input::ChangeDeviceControlRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& control_id = request.control_id;
  auto& device_id = request.device_id;
  auto& control = request.control;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 35;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // control_id
  uint16_t tmp64;
  tmp64 = static_cast<uint16_t>(control_id);
  buf.Write(&tmp64);

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 1);

  // control
  {
    DeviceControl control_id{};
    auto& len = control.len;
    auto& data = control;

    // control_id
    SwitchVar(DeviceControl::resolution, data.resolution.has_value(), false,
              &control_id);
    SwitchVar(DeviceControl::abs_calib, data.abs_calib.has_value(), false,
              &control_id);
    SwitchVar(DeviceControl::core, data.core.has_value(), false, &control_id);
    SwitchVar(DeviceControl::enable, data.enable.has_value(), false,
              &control_id);
    SwitchVar(DeviceControl::abs_area, data.abs_area.has_value(), false,
              &control_id);
    uint16_t tmp65;
    tmp65 = static_cast<uint16_t>(control_id);
    buf.Write(&tmp65);

    // len
    buf.Write(&len);

    // data
    auto data_expr = control_id;
    if (CaseEq(data_expr, DeviceControl::resolution)) {
      auto& first_valuator = (*data.resolution).first_valuator;
      uint8_t num_valuators{};
      auto& resolution_values = (*data.resolution).resolution_values;

      // first_valuator
      buf.Write(&first_valuator);

      // num_valuators
      num_valuators = resolution_values.size();
      buf.Write(&num_valuators);

      // pad0
      Pad(&buf, 2);

      // resolution_values
      CHECK_EQ(static_cast<size_t>(num_valuators), resolution_values.size());
      for (auto& resolution_values_elem : resolution_values) {
        // resolution_values_elem
        buf.Write(&resolution_values_elem);
      }
    }
    if (CaseEq(data_expr, DeviceControl::abs_calib)) {
      auto& min_x = (*data.abs_calib).min_x;
      auto& max_x = (*data.abs_calib).max_x;
      auto& min_y = (*data.abs_calib).min_y;
      auto& max_y = (*data.abs_calib).max_y;
      auto& flip_x = (*data.abs_calib).flip_x;
      auto& flip_y = (*data.abs_calib).flip_y;
      auto& rotation = (*data.abs_calib).rotation;
      auto& button_threshold = (*data.abs_calib).button_threshold;

      // min_x
      buf.Write(&min_x);

      // max_x
      buf.Write(&max_x);

      // min_y
      buf.Write(&min_y);

      // max_y
      buf.Write(&max_y);

      // flip_x
      buf.Write(&flip_x);

      // flip_y
      buf.Write(&flip_y);

      // rotation
      buf.Write(&rotation);

      // button_threshold
      buf.Write(&button_threshold);
    }
    if (CaseEq(data_expr, DeviceControl::core)) {
      auto& status = (*data.core).status;

      // status
      buf.Write(&status);

      // pad1
      Pad(&buf, 3);
    }
    if (CaseEq(data_expr, DeviceControl::enable)) {
      auto& enable = (*data.enable).enable;

      // enable
      buf.Write(&enable);

      // pad2
      Pad(&buf, 3);
    }
    if (CaseEq(data_expr, DeviceControl::abs_area)) {
      auto& offset_x = (*data.abs_area).offset_x;
      auto& offset_y = (*data.abs_area).offset_y;
      auto& width = (*data.abs_area).width;
      auto& height = (*data.abs_area).height;
      auto& screen = (*data.abs_area).screen;
      auto& following = (*data.abs_area).following;

      // offset_x
      buf.Write(&offset_x);

      // offset_y
      buf.Write(&offset_y);

      // width
      buf.Write(&width);

      // height
      buf.Write(&height);

      // screen
      buf.Write(&screen);

      // following
      buf.Write(&following);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<Input::ChangeDeviceControlReply>(
      &buf, "Input::ChangeDeviceControl", false);
}

Future<Input::ChangeDeviceControlReply> Input::ChangeDeviceControl(
    const DeviceControl& control_id,
    const uint8_t& device_id,
    const DeviceCtl& control) {
  return Input::ChangeDeviceControl(
      Input::ChangeDeviceControlRequest{control_id, device_id, control});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::ChangeDeviceControlReply> detail::ReadReply<
    Input::ChangeDeviceControlReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::ChangeDeviceControlReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& status = (*reply).status;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // status
  Read(&status, &buf);

  // pad0
  Pad(&buf, 23);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::ListDevicePropertiesReply> Input::ListDeviceProperties(
    const Input::ListDevicePropertiesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 36;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<Input::ListDevicePropertiesReply>(
      &buf, "Input::ListDeviceProperties", false);
}

Future<Input::ListDevicePropertiesReply> Input::ListDeviceProperties(
    const uint8_t& device_id) {
  return Input::ListDeviceProperties(
      Input::ListDevicePropertiesRequest{device_id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::ListDevicePropertiesReply> detail::ReadReply<
    Input::ListDevicePropertiesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::ListDevicePropertiesReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  uint16_t num_atoms{};
  auto& atoms = (*reply).atoms;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_atoms
  Read(&num_atoms, &buf);

  // pad0
  Pad(&buf, 22);

  // atoms
  atoms.resize(num_atoms);
  for (auto& atoms_elem : atoms) {
    // atoms_elem
    Read(&atoms_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::ChangeDeviceProperty(
    const Input::ChangeDevicePropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& property = request.property;
  auto& type = request.type;
  auto& device_id = request.device_id;
  PropertyFormat format{};
  auto& mode = request.mode;
  auto& num_items = request.num_items;
  auto& items = request;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 37;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // property
  buf.Write(&property);

  // type
  buf.Write(&type);

  // device_id
  buf.Write(&device_id);

  // format
  SwitchVar(PropertyFormat::c_8Bits, items.data8.has_value(), false, &format);
  SwitchVar(PropertyFormat::c_16Bits, items.data16.has_value(), false, &format);
  SwitchVar(PropertyFormat::c_32Bits, items.data32.has_value(), false, &format);
  uint8_t tmp66;
  tmp66 = static_cast<uint8_t>(format);
  buf.Write(&tmp66);

  // mode
  uint8_t tmp67;
  tmp67 = static_cast<uint8_t>(mode);
  buf.Write(&tmp67);

  // pad0
  Pad(&buf, 1);

  // num_items
  buf.Write(&num_items);

  // items
  auto items_expr = format;
  if (CaseEq(items_expr, PropertyFormat::c_8Bits)) {
    auto& data8 = *items.data8;

    // data8
    CHECK_EQ(static_cast<size_t>(num_items), data8.size());
    for (auto& data8_elem : data8) {
      // data8_elem
      buf.Write(&data8_elem);
    }

    // pad1
    Align(&buf, 4);
  }
  if (CaseEq(items_expr, PropertyFormat::c_16Bits)) {
    auto& data16 = *items.data16;

    // data16
    CHECK_EQ(static_cast<size_t>(num_items), data16.size());
    for (auto& data16_elem : data16) {
      // data16_elem
      buf.Write(&data16_elem);
    }

    // pad2
    Align(&buf, 4);
  }
  if (CaseEq(items_expr, PropertyFormat::c_32Bits)) {
    auto& data32 = *items.data32;

    // data32
    CHECK_EQ(static_cast<size_t>(num_items), data32.size());
    for (auto& data32_elem : data32) {
      // data32_elem
      buf.Write(&data32_elem);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::ChangeDeviceProperty",
                                        false);
}

Future<void> Input::ChangeDeviceProperty(
    const Atom& property,
    const Atom& type,
    const uint8_t& device_id,
    const PropMode& mode,
    const uint32_t& num_items,
    const std::optional<std::vector<uint8_t>>& data8,
    const std::optional<std::vector<uint16_t>>& data16,
    const std::optional<std::vector<uint32_t>>& data32) {
  return Input::ChangeDeviceProperty(Input::ChangeDevicePropertyRequest{
      property, type, device_id, mode, num_items, data8, data16, data32});
}

Future<void> Input::DeleteDeviceProperty(
    const Input::DeleteDevicePropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& property = request.property;
  auto& device_id = request.device_id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 38;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // property
  buf.Write(&property);

  // device_id
  buf.Write(&device_id);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::DeleteDeviceProperty",
                                        false);
}

Future<void> Input::DeleteDeviceProperty(const Atom& property,
                                         const uint8_t& device_id) {
  return Input::DeleteDeviceProperty(
      Input::DeleteDevicePropertyRequest{property, device_id});
}

Future<Input::GetDevicePropertyReply> Input::GetDeviceProperty(
    const Input::GetDevicePropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& property = request.property;
  auto& type = request.type;
  auto& offset = request.offset;
  auto& len = request.len;
  auto& device_id = request.device_id;
  auto& c_delete = request.c_delete;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 39;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // property
  buf.Write(&property);

  // type
  buf.Write(&type);

  // offset
  buf.Write(&offset);

  // len
  buf.Write(&len);

  // device_id
  buf.Write(&device_id);

  // c_delete
  buf.Write(&c_delete);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<Input::GetDevicePropertyReply>(
      &buf, "Input::GetDeviceProperty", false);
}

Future<Input::GetDevicePropertyReply> Input::GetDeviceProperty(
    const Atom& property,
    const Atom& type,
    const uint32_t& offset,
    const uint32_t& len,
    const uint8_t& device_id,
    const uint8_t& c_delete) {
  return Input::GetDeviceProperty(Input::GetDevicePropertyRequest{
      property, type, offset, len, device_id, c_delete});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::GetDevicePropertyReply> detail::ReadReply<
    Input::GetDevicePropertyReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::GetDevicePropertyReply>();

  auto& xi_reply_type = (*reply).xi_reply_type;
  auto& sequence = (*reply).sequence;
  auto& type = (*reply).type;
  auto& bytes_after = (*reply).bytes_after;
  auto& num_items = (*reply).num_items;
  Input::PropertyFormat format{};
  auto& device_id = (*reply).device_id;
  auto& items = (*reply);

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xi_reply_type
  Read(&xi_reply_type, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // type
  Read(&type, &buf);

  // bytes_after
  Read(&bytes_after, &buf);

  // num_items
  Read(&num_items, &buf);

  // format
  uint8_t tmp68;
  Read(&tmp68, &buf);
  format = static_cast<Input::PropertyFormat>(tmp68);

  // device_id
  Read(&device_id, &buf);

  // pad0
  Pad(&buf, 10);

  // items
  auto items_expr = format;
  if (CaseEq(items_expr, Input::PropertyFormat::c_8Bits)) {
    items.data8.emplace(decltype(items.data8)::value_type());
    auto& data8 = *items.data8;

    // data8
    data8.resize(num_items);
    for (auto& data8_elem : data8) {
      // data8_elem
      Read(&data8_elem, &buf);
    }

    // pad1
    Align(&buf, 4);
  }
  if (CaseEq(items_expr, Input::PropertyFormat::c_16Bits)) {
    items.data16.emplace(decltype(items.data16)::value_type());
    auto& data16 = *items.data16;

    // data16
    data16.resize(num_items);
    for (auto& data16_elem : data16) {
      // data16_elem
      Read(&data16_elem, &buf);
    }

    // pad2
    Align(&buf, 4);
  }
  if (CaseEq(items_expr, Input::PropertyFormat::c_32Bits)) {
    items.data32.emplace(decltype(items.data32)::value_type());
    auto& data32 = *items.data32;

    // data32
    data32.resize(num_items);
    for (auto& data32_elem : data32) {
      // data32_elem
      Read(&data32_elem, &buf);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::XIQueryPointerReply> Input::XIQueryPointer(
    const Input::XIQueryPointerRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& deviceid = request.deviceid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 40;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // deviceid
  buf.Write(&deviceid);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<Input::XIQueryPointerReply>(
      &buf, "Input::XIQueryPointer", false);
}

Future<Input::XIQueryPointerReply> Input::XIQueryPointer(
    const Window& window,
    const DeviceId& deviceid) {
  return Input::XIQueryPointer(Input::XIQueryPointerRequest{window, deviceid});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::XIQueryPointerReply> detail::ReadReply<
    Input::XIQueryPointerReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::XIQueryPointerReply>();

  auto& sequence = (*reply).sequence;
  auto& root = (*reply).root;
  auto& child = (*reply).child;
  auto& root_x = (*reply).root_x;
  auto& root_y = (*reply).root_y;
  auto& win_x = (*reply).win_x;
  auto& win_y = (*reply).win_y;
  auto& same_screen = (*reply).same_screen;
  uint16_t buttons_len{};
  auto& mods = (*reply).mods;
  auto& group = (*reply).group;
  auto& buttons = (*reply).buttons;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // root
  Read(&root, &buf);

  // child
  Read(&child, &buf);

  // root_x
  Read(&root_x, &buf);

  // root_y
  Read(&root_y, &buf);

  // win_x
  Read(&win_x, &buf);

  // win_y
  Read(&win_y, &buf);

  // same_screen
  Read(&same_screen, &buf);

  // pad1
  Pad(&buf, 1);

  // buttons_len
  Read(&buttons_len, &buf);

  // mods
  {
    auto& base = mods.base;
    auto& latched = mods.latched;
    auto& locked = mods.locked;
    auto& effective = mods.effective;

    // base
    Read(&base, &buf);

    // latched
    Read(&latched, &buf);

    // locked
    Read(&locked, &buf);

    // effective
    Read(&effective, &buf);
  }

  // group
  {
    auto& base = group.base;
    auto& latched = group.latched;
    auto& locked = group.locked;
    auto& effective = group.effective;

    // base
    Read(&base, &buf);

    // latched
    Read(&latched, &buf);

    // locked
    Read(&locked, &buf);

    // effective
    Read(&effective, &buf);
  }

  // buttons
  buttons.resize(buttons_len);
  for (auto& buttons_elem : buttons) {
    // buttons_elem
    Read(&buttons_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::XIWarpPointer(const Input::XIWarpPointerRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& src_win = request.src_win;
  auto& dst_win = request.dst_win;
  auto& src_x = request.src_x;
  auto& src_y = request.src_y;
  auto& src_width = request.src_width;
  auto& src_height = request.src_height;
  auto& dst_x = request.dst_x;
  auto& dst_y = request.dst_y;
  auto& deviceid = request.deviceid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 41;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // src_win
  buf.Write(&src_win);

  // dst_win
  buf.Write(&dst_win);

  // src_x
  buf.Write(&src_x);

  // src_y
  buf.Write(&src_y);

  // src_width
  buf.Write(&src_width);

  // src_height
  buf.Write(&src_height);

  // dst_x
  buf.Write(&dst_x);

  // dst_y
  buf.Write(&dst_y);

  // deviceid
  buf.Write(&deviceid);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XIWarpPointer", false);
}

Future<void> Input::XIWarpPointer(const Window& src_win,
                                  const Window& dst_win,
                                  const Fp1616& src_x,
                                  const Fp1616& src_y,
                                  const uint16_t& src_width,
                                  const uint16_t& src_height,
                                  const Fp1616& dst_x,
                                  const Fp1616& dst_y,
                                  const DeviceId& deviceid) {
  return Input::XIWarpPointer(
      Input::XIWarpPointerRequest{src_win, dst_win, src_x, src_y, src_width,
                                  src_height, dst_x, dst_y, deviceid});
}

Future<void> Input::XIChangeCursor(
    const Input::XIChangeCursorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& cursor = request.cursor;
  auto& deviceid = request.deviceid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 42;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // cursor
  buf.Write(&cursor);

  // deviceid
  buf.Write(&deviceid);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XIChangeCursor", false);
}

Future<void> Input::XIChangeCursor(const Window& window,
                                   const Cursor& cursor,
                                   const DeviceId& deviceid) {
  return Input::XIChangeCursor(
      Input::XIChangeCursorRequest{window, cursor, deviceid});
}

Future<void> Input::XIChangeHierarchy(
    const Input::XIChangeHierarchyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint8_t num_changes{};
  auto& changes = request.changes;
  size_t changes_len = changes.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 43;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // num_changes
  num_changes = changes.size();
  buf.Write(&num_changes);

  // pad0
  Pad(&buf, 3);

  // changes
  CHECK_EQ(static_cast<size_t>(num_changes), changes.size());
  for (auto& changes_elem : changes) {
    // changes_elem
    {
      HierarchyChangeType type{};
      auto& len = changes_elem.len;
      auto& data = changes_elem;

      // type
      SwitchVar(HierarchyChangeType::AddMaster, data.add_master.has_value(),
                false, &type);
      SwitchVar(HierarchyChangeType::RemoveMaster,
                data.remove_master.has_value(), false, &type);
      SwitchVar(HierarchyChangeType::AttachSlave, data.attach_slave.has_value(),
                false, &type);
      SwitchVar(HierarchyChangeType::DetachSlave, data.detach_slave.has_value(),
                false, &type);
      uint16_t tmp69;
      tmp69 = static_cast<uint16_t>(type);
      buf.Write(&tmp69);

      // len
      buf.Write(&len);

      // data
      auto data_expr = type;
      if (CaseEq(data_expr, HierarchyChangeType::AddMaster)) {
        uint16_t name_len{};
        auto& send_core = (*data.add_master).send_core;
        auto& enable = (*data.add_master).enable;
        auto& name = (*data.add_master).name;

        // name_len
        name_len = name.size();
        buf.Write(&name_len);

        // send_core
        buf.Write(&send_core);

        // enable
        buf.Write(&enable);

        // name
        CHECK_EQ(static_cast<size_t>(name_len), name.size());
        for (auto& name_elem : name) {
          // name_elem
          buf.Write(&name_elem);
        }

        // pad0
        Align(&buf, 4);
      }
      if (CaseEq(data_expr, HierarchyChangeType::RemoveMaster)) {
        auto& deviceid = (*data.remove_master).deviceid;
        auto& return_mode = (*data.remove_master).return_mode;
        auto& return_pointer = (*data.remove_master).return_pointer;
        auto& return_keyboard = (*data.remove_master).return_keyboard;

        // deviceid
        buf.Write(&deviceid);

        // return_mode
        uint8_t tmp70;
        tmp70 = static_cast<uint8_t>(return_mode);
        buf.Write(&tmp70);

        // pad1
        Pad(&buf, 1);

        // return_pointer
        buf.Write(&return_pointer);

        // return_keyboard
        buf.Write(&return_keyboard);
      }
      if (CaseEq(data_expr, HierarchyChangeType::AttachSlave)) {
        auto& deviceid = (*data.attach_slave).deviceid;
        auto& master = (*data.attach_slave).master;

        // deviceid
        buf.Write(&deviceid);

        // master
        buf.Write(&master);
      }
      if (CaseEq(data_expr, HierarchyChangeType::DetachSlave)) {
        auto& deviceid = (*data.detach_slave).deviceid;

        // deviceid
        buf.Write(&deviceid);

        // pad2
        Pad(&buf, 2);
      }
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XIChangeHierarchy",
                                        false);
}

Future<void> Input::XIChangeHierarchy(
    const std::vector<HierarchyChange>& changes) {
  return Input::XIChangeHierarchy(Input::XIChangeHierarchyRequest{changes});
}

Future<void> Input::XISetClientPointer(
    const Input::XISetClientPointerRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& deviceid = request.deviceid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 44;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // deviceid
  buf.Write(&deviceid);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XISetClientPointer",
                                        false);
}

Future<void> Input::XISetClientPointer(const Window& window,
                                       const DeviceId& deviceid) {
  return Input::XISetClientPointer(
      Input::XISetClientPointerRequest{window, deviceid});
}

Future<Input::XIGetClientPointerReply> Input::XIGetClientPointer(
    const Input::XIGetClientPointerRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 45;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<Input::XIGetClientPointerReply>(
      &buf, "Input::XIGetClientPointer", false);
}

Future<Input::XIGetClientPointerReply> Input::XIGetClientPointer(
    const Window& window) {
  return Input::XIGetClientPointer(Input::XIGetClientPointerRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::XIGetClientPointerReply> detail::ReadReply<
    Input::XIGetClientPointerReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::XIGetClientPointerReply>();

  auto& sequence = (*reply).sequence;
  auto& set = (*reply).set;
  auto& deviceid = (*reply).deviceid;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // set
  Read(&set, &buf);

  // pad1
  Pad(&buf, 1);

  // deviceid
  Read(&deviceid, &buf);

  // pad2
  Pad(&buf, 20);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::XISelectEvents(
    const Input::XISelectEventsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  uint16_t num_mask{};
  auto& masks = request.masks;
  size_t masks_len = masks.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 46;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // num_mask
  num_mask = masks.size();
  buf.Write(&num_mask);

  // pad0
  Pad(&buf, 2);

  // masks
  CHECK_EQ(static_cast<size_t>(num_mask), masks.size());
  for (auto& masks_elem : masks) {
    // masks_elem
    {
      auto& deviceid = masks_elem.deviceid;
      uint16_t mask_len{};
      auto& mask = masks_elem.mask;

      // deviceid
      buf.Write(&deviceid);

      // mask_len
      mask_len = mask.size();
      buf.Write(&mask_len);

      // mask
      CHECK_EQ(static_cast<size_t>(mask_len), mask.size());
      for (auto& mask_elem : mask) {
        // mask_elem
        uint32_t tmp71;
        tmp71 = static_cast<uint32_t>(mask_elem);
        buf.Write(&tmp71);
      }
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XISelectEvents", false);
}

Future<void> Input::XISelectEvents(const Window& window,
                                   const std::vector<EventMask>& masks) {
  return Input::XISelectEvents(Input::XISelectEventsRequest{window, masks});
}

Future<Input::XIQueryVersionReply> Input::XIQueryVersion(
    const Input::XIQueryVersionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& major_version = request.major_version;
  auto& minor_version = request.minor_version;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 47;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // major_version
  buf.Write(&major_version);

  // minor_version
  buf.Write(&minor_version);

  Align(&buf, 4);

  return connection_->SendRequest<Input::XIQueryVersionReply>(
      &buf, "Input::XIQueryVersion", false);
}

Future<Input::XIQueryVersionReply> Input::XIQueryVersion(
    const uint16_t& major_version,
    const uint16_t& minor_version) {
  return Input::XIQueryVersion(
      Input::XIQueryVersionRequest{major_version, minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::XIQueryVersionReply> detail::ReadReply<
    Input::XIQueryVersionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::XIQueryVersionReply>();

  auto& sequence = (*reply).sequence;
  auto& major_version = (*reply).major_version;
  auto& minor_version = (*reply).minor_version;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // major_version
  Read(&major_version, &buf);

  // minor_version
  Read(&minor_version, &buf);

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::XIQueryDeviceReply> Input::XIQueryDevice(
    const Input::XIQueryDeviceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceid = request.deviceid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 48;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceid
  buf.Write(&deviceid);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<Input::XIQueryDeviceReply>(
      &buf, "Input::XIQueryDevice", false);
}

Future<Input::XIQueryDeviceReply> Input::XIQueryDevice(
    const DeviceId& deviceid) {
  return Input::XIQueryDevice(Input::XIQueryDeviceRequest{deviceid});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::XIQueryDeviceReply> detail::ReadReply<
    Input::XIQueryDeviceReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::XIQueryDeviceReply>();

  auto& sequence = (*reply).sequence;
  uint16_t num_infos{};
  auto& infos = (*reply).infos;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_infos
  Read(&num_infos, &buf);

  // pad1
  Pad(&buf, 22);

  // infos
  infos.resize(num_infos);
  for (auto& infos_elem : infos) {
    // infos_elem
    {
      auto& deviceid = infos_elem.deviceid;
      auto& type = infos_elem.type;
      auto& attachment = infos_elem.attachment;
      uint16_t num_classes{};
      uint16_t name_len{};
      auto& enabled = infos_elem.enabled;
      auto& name = infos_elem.name;
      auto& classes = infos_elem.classes;

      // deviceid
      Read(&deviceid, &buf);

      // type
      uint16_t tmp72;
      Read(&tmp72, &buf);
      type = static_cast<Input::DeviceType>(tmp72);

      // attachment
      Read(&attachment, &buf);

      // num_classes
      Read(&num_classes, &buf);

      // name_len
      Read(&name_len, &buf);

      // enabled
      Read(&enabled, &buf);

      // pad0
      Pad(&buf, 1);

      // name
      name.resize(name_len);
      for (auto& name_elem : name) {
        // name_elem
        Read(&name_elem, &buf);
      }

      // pad1
      Align(&buf, 4);

      // classes
      classes.resize(num_classes);
      for (auto& classes_elem : classes) {
        // classes_elem
        {
          Input::DeviceClassType type{};
          auto& len = classes_elem.len;
          auto& sourceid = classes_elem.sourceid;
          auto& data = classes_elem;

          // type
          uint16_t tmp73;
          Read(&tmp73, &buf);
          type = static_cast<Input::DeviceClassType>(tmp73);

          // len
          Read(&len, &buf);

          // sourceid
          Read(&sourceid, &buf);

          // data
          auto data_expr = type;
          if (CaseEq(data_expr, Input::DeviceClassType::Key)) {
            data.key.emplace(decltype(data.key)::value_type());
            uint16_t num_keys{};
            auto& keys = (*data.key).keys;

            // num_keys
            Read(&num_keys, &buf);

            // keys
            keys.resize(num_keys);
            for (auto& keys_elem : keys) {
              // keys_elem
              Read(&keys_elem, &buf);
            }
          }
          if (CaseEq(data_expr, Input::DeviceClassType::Button)) {
            data.button.emplace(decltype(data.button)::value_type());
            uint16_t num_buttons{};
            auto& state = (*data.button).state;
            auto& labels = (*data.button).labels;

            // num_buttons
            Read(&num_buttons, &buf);

            // state
            state.resize(((num_buttons) + (31)) / (32));
            for (auto& state_elem : state) {
              // state_elem
              Read(&state_elem, &buf);
            }

            // labels
            labels.resize(num_buttons);
            for (auto& labels_elem : labels) {
              // labels_elem
              Read(&labels_elem, &buf);
            }
          }
          if (CaseEq(data_expr, Input::DeviceClassType::Valuator)) {
            data.valuator.emplace(decltype(data.valuator)::value_type());
            auto& number = (*data.valuator).number;
            auto& label = (*data.valuator).label;
            auto& min = (*data.valuator).min;
            auto& max = (*data.valuator).max;
            auto& value = (*data.valuator).value;
            auto& resolution = (*data.valuator).resolution;
            auto& mode = (*data.valuator).mode;

            // number
            Read(&number, &buf);

            // label
            Read(&label, &buf);

            // min
            {
              auto& integral = min.integral;
              auto& frac = min.frac;

              // integral
              Read(&integral, &buf);

              // frac
              Read(&frac, &buf);
            }

            // max
            {
              auto& integral = max.integral;
              auto& frac = max.frac;

              // integral
              Read(&integral, &buf);

              // frac
              Read(&frac, &buf);
            }

            // value
            {
              auto& integral = value.integral;
              auto& frac = value.frac;

              // integral
              Read(&integral, &buf);

              // frac
              Read(&frac, &buf);
            }

            // resolution
            Read(&resolution, &buf);

            // mode
            uint8_t tmp74;
            Read(&tmp74, &buf);
            mode = static_cast<Input::ValuatorMode>(tmp74);

            // pad0
            Pad(&buf, 3);
          }
          if (CaseEq(data_expr, Input::DeviceClassType::Scroll)) {
            data.scroll.emplace(decltype(data.scroll)::value_type());
            auto& number = (*data.scroll).number;
            auto& scroll_type = (*data.scroll).scroll_type;
            auto& flags = (*data.scroll).flags;
            auto& increment = (*data.scroll).increment;

            // number
            Read(&number, &buf);

            // scroll_type
            uint16_t tmp75;
            Read(&tmp75, &buf);
            scroll_type = static_cast<Input::ScrollType>(tmp75);

            // pad1
            Pad(&buf, 2);

            // flags
            uint32_t tmp76;
            Read(&tmp76, &buf);
            flags = static_cast<Input::ScrollFlags>(tmp76);

            // increment
            {
              auto& integral = increment.integral;
              auto& frac = increment.frac;

              // integral
              Read(&integral, &buf);

              // frac
              Read(&frac, &buf);
            }
          }
          if (CaseEq(data_expr, Input::DeviceClassType::Touch)) {
            data.touch.emplace(decltype(data.touch)::value_type());
            auto& mode = (*data.touch).mode;
            auto& num_touches = (*data.touch).num_touches;

            // mode
            uint8_t tmp77;
            Read(&tmp77, &buf);
            mode = static_cast<Input::TouchMode>(tmp77);

            // num_touches
            Read(&num_touches, &buf);
          }
          if (CaseEq(data_expr, Input::DeviceClassType::Gesture)) {
            data.gesture.emplace(decltype(data.gesture)::value_type());
            auto& num_touches = (*data.gesture).num_touches;

            // num_touches
            Read(&num_touches, &buf);

            // pad2
            Pad(&buf, 1);
          }
        }
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::XISetFocus(const Input::XISetFocusRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& time = request.time;
  auto& deviceid = request.deviceid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 49;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // time
  buf.Write(&time);

  // deviceid
  buf.Write(&deviceid);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XISetFocus", false);
}

Future<void> Input::XISetFocus(const Window& window,
                               const Time& time,
                               const DeviceId& deviceid) {
  return Input::XISetFocus(Input::XISetFocusRequest{window, time, deviceid});
}

Future<Input::XIGetFocusReply> Input::XIGetFocus(
    const Input::XIGetFocusRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceid = request.deviceid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 50;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceid
  buf.Write(&deviceid);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<Input::XIGetFocusReply>(
      &buf, "Input::XIGetFocus", false);
}

Future<Input::XIGetFocusReply> Input::XIGetFocus(const DeviceId& deviceid) {
  return Input::XIGetFocus(Input::XIGetFocusRequest{deviceid});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::XIGetFocusReply> detail::ReadReply<
    Input::XIGetFocusReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::XIGetFocusReply>();

  auto& sequence = (*reply).sequence;
  auto& focus = (*reply).focus;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // focus
  Read(&focus, &buf);

  // pad1
  Pad(&buf, 20);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::XIGrabDeviceReply> Input::XIGrabDevice(
    const Input::XIGrabDeviceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;
  auto& time = request.time;
  auto& cursor = request.cursor;
  auto& deviceid = request.deviceid;
  auto& mode = request.mode;
  auto& paired_device_mode = request.paired_device_mode;
  auto& owner_events = request.owner_events;
  uint16_t mask_len{};
  auto& mask = request.mask;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 51;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  // time
  buf.Write(&time);

  // cursor
  buf.Write(&cursor);

  // deviceid
  buf.Write(&deviceid);

  // mode
  uint8_t tmp78;
  tmp78 = static_cast<uint8_t>(mode);
  buf.Write(&tmp78);

  // paired_device_mode
  uint8_t tmp79;
  tmp79 = static_cast<uint8_t>(paired_device_mode);
  buf.Write(&tmp79);

  // owner_events
  uint8_t tmp80;
  tmp80 = static_cast<uint8_t>(owner_events);
  buf.Write(&tmp80);

  // pad0
  Pad(&buf, 1);

  // mask_len
  mask_len = mask.size();
  buf.Write(&mask_len);

  // mask
  CHECK_EQ(static_cast<size_t>(mask_len), mask.size());
  for (auto& mask_elem : mask) {
    // mask_elem
    buf.Write(&mask_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<Input::XIGrabDeviceReply>(
      &buf, "Input::XIGrabDevice", false);
}

Future<Input::XIGrabDeviceReply> Input::XIGrabDevice(
    const Window& window,
    const Time& time,
    const Cursor& cursor,
    const DeviceId& deviceid,
    const GrabMode& mode,
    const GrabMode& paired_device_mode,
    const GrabOwner& owner_events,
    const std::vector<uint32_t>& mask) {
  return Input::XIGrabDevice(
      Input::XIGrabDeviceRequest{window, time, cursor, deviceid, mode,
                                 paired_device_mode, owner_events, mask});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::XIGrabDeviceReply> detail::ReadReply<
    Input::XIGrabDeviceReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::XIGrabDeviceReply>();

  auto& sequence = (*reply).sequence;
  auto& status = (*reply).status;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // status
  uint8_t tmp81;
  Read(&tmp81, &buf);
  status = static_cast<GrabStatus>(tmp81);

  // pad1
  Pad(&buf, 23);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::XIUngrabDevice(
    const Input::XIUngrabDeviceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& time = request.time;
  auto& deviceid = request.deviceid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 52;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // time
  buf.Write(&time);

  // deviceid
  buf.Write(&deviceid);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XIUngrabDevice", false);
}

Future<void> Input::XIUngrabDevice(const Time& time, const DeviceId& deviceid) {
  return Input::XIUngrabDevice(Input::XIUngrabDeviceRequest{time, deviceid});
}

Future<void> Input::XIAllowEvents(const Input::XIAllowEventsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& time = request.time;
  auto& deviceid = request.deviceid;
  auto& event_mode = request.event_mode;
  auto& touchid = request.touchid;
  auto& grab_window = request.grab_window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 53;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // time
  buf.Write(&time);

  // deviceid
  buf.Write(&deviceid);

  // event_mode
  uint8_t tmp82;
  tmp82 = static_cast<uint8_t>(event_mode);
  buf.Write(&tmp82);

  // pad0
  Pad(&buf, 1);

  // touchid
  buf.Write(&touchid);

  // grab_window
  buf.Write(&grab_window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XIAllowEvents", false);
}

Future<void> Input::XIAllowEvents(const Time& time,
                                  const DeviceId& deviceid,
                                  const EventMode& event_mode,
                                  const uint32_t& touchid,
                                  const Window& grab_window) {
  return Input::XIAllowEvents(Input::XIAllowEventsRequest{
      time, deviceid, event_mode, touchid, grab_window});
}

Future<Input::XIPassiveGrabDeviceReply> Input::XIPassiveGrabDevice(
    const Input::XIPassiveGrabDeviceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& time = request.time;
  auto& grab_window = request.grab_window;
  auto& cursor = request.cursor;
  auto& detail = request.detail;
  auto& deviceid = request.deviceid;
  uint16_t num_modifiers{};
  uint16_t mask_len{};
  auto& grab_type = request.grab_type;
  auto& grab_mode = request.grab_mode;
  auto& paired_device_mode = request.paired_device_mode;
  auto& owner_events = request.owner_events;
  auto& mask = request.mask;
  auto& modifiers = request.modifiers;
  size_t modifiers_len = modifiers.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 54;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // time
  buf.Write(&time);

  // grab_window
  buf.Write(&grab_window);

  // cursor
  buf.Write(&cursor);

  // detail
  buf.Write(&detail);

  // deviceid
  buf.Write(&deviceid);

  // num_modifiers
  num_modifiers = modifiers.size();
  buf.Write(&num_modifiers);

  // mask_len
  mask_len = mask.size();
  buf.Write(&mask_len);

  // grab_type
  uint8_t tmp83;
  tmp83 = static_cast<uint8_t>(grab_type);
  buf.Write(&tmp83);

  // grab_mode
  uint8_t tmp84;
  tmp84 = static_cast<uint8_t>(grab_mode);
  buf.Write(&tmp84);

  // paired_device_mode
  uint8_t tmp85;
  tmp85 = static_cast<uint8_t>(paired_device_mode);
  buf.Write(&tmp85);

  // owner_events
  uint8_t tmp86;
  tmp86 = static_cast<uint8_t>(owner_events);
  buf.Write(&tmp86);

  // pad0
  Pad(&buf, 2);

  // mask
  CHECK_EQ(static_cast<size_t>(mask_len), mask.size());
  for (auto& mask_elem : mask) {
    // mask_elem
    buf.Write(&mask_elem);
  }

  // modifiers
  CHECK_EQ(static_cast<size_t>(num_modifiers), modifiers.size());
  for (auto& modifiers_elem : modifiers) {
    // modifiers_elem
    buf.Write(&modifiers_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<Input::XIPassiveGrabDeviceReply>(
      &buf, "Input::XIPassiveGrabDevice", false);
}

Future<Input::XIPassiveGrabDeviceReply> Input::XIPassiveGrabDevice(
    const Time& time,
    const Window& grab_window,
    const Cursor& cursor,
    const uint32_t& detail,
    const DeviceId& deviceid,
    const GrabType& grab_type,
    const GrabMode22& grab_mode,
    const GrabMode& paired_device_mode,
    const GrabOwner& owner_events,
    const std::vector<uint32_t>& mask,
    const std::vector<uint32_t>& modifiers) {
  return Input::XIPassiveGrabDevice(Input::XIPassiveGrabDeviceRequest{
      time, grab_window, cursor, detail, deviceid, grab_type, grab_mode,
      paired_device_mode, owner_events, mask, modifiers});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::XIPassiveGrabDeviceReply> detail::ReadReply<
    Input::XIPassiveGrabDeviceReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::XIPassiveGrabDeviceReply>();

  auto& sequence = (*reply).sequence;
  uint16_t num_modifiers{};
  auto& modifiers = (*reply).modifiers;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_modifiers
  Read(&num_modifiers, &buf);

  // pad1
  Pad(&buf, 22);

  // modifiers
  modifiers.resize(num_modifiers);
  for (auto& modifiers_elem : modifiers) {
    // modifiers_elem
    {
      auto& modifiers = modifiers_elem.modifiers;
      auto& status = modifiers_elem.status;

      // modifiers
      Read(&modifiers, &buf);

      // status
      uint8_t tmp87;
      Read(&tmp87, &buf);
      status = static_cast<GrabStatus>(tmp87);

      // pad0
      Pad(&buf, 3);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::XIPassiveUngrabDevice(
    const Input::XIPassiveUngrabDeviceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& grab_window = request.grab_window;
  auto& detail = request.detail;
  auto& deviceid = request.deviceid;
  uint16_t num_modifiers{};
  auto& grab_type = request.grab_type;
  auto& modifiers = request.modifiers;
  size_t modifiers_len = modifiers.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 55;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // grab_window
  buf.Write(&grab_window);

  // detail
  buf.Write(&detail);

  // deviceid
  buf.Write(&deviceid);

  // num_modifiers
  num_modifiers = modifiers.size();
  buf.Write(&num_modifiers);

  // grab_type
  uint8_t tmp88;
  tmp88 = static_cast<uint8_t>(grab_type);
  buf.Write(&tmp88);

  // pad0
  Pad(&buf, 3);

  // modifiers
  CHECK_EQ(static_cast<size_t>(num_modifiers), modifiers.size());
  for (auto& modifiers_elem : modifiers) {
    // modifiers_elem
    buf.Write(&modifiers_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XIPassiveUngrabDevice",
                                        false);
}

Future<void> Input::XIPassiveUngrabDevice(
    const Window& grab_window,
    const uint32_t& detail,
    const DeviceId& deviceid,
    const GrabType& grab_type,
    const std::vector<uint32_t>& modifiers) {
  return Input::XIPassiveUngrabDevice(Input::XIPassiveUngrabDeviceRequest{
      grab_window, detail, deviceid, grab_type, modifiers});
}

Future<Input::XIListPropertiesReply> Input::XIListProperties(
    const Input::XIListPropertiesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceid = request.deviceid;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 56;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceid
  buf.Write(&deviceid);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<Input::XIListPropertiesReply>(
      &buf, "Input::XIListProperties", false);
}

Future<Input::XIListPropertiesReply> Input::XIListProperties(
    const DeviceId& deviceid) {
  return Input::XIListProperties(Input::XIListPropertiesRequest{deviceid});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::XIListPropertiesReply> detail::ReadReply<
    Input::XIListPropertiesReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::XIListPropertiesReply>();

  auto& sequence = (*reply).sequence;
  uint16_t num_properties{};
  auto& properties = (*reply).properties;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_properties
  Read(&num_properties, &buf);

  // pad1
  Pad(&buf, 22);

  // properties
  properties.resize(num_properties);
  for (auto& properties_elem : properties) {
    // properties_elem
    Read(&properties_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::XIChangeProperty(
    const Input::XIChangePropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceid = request.deviceid;
  auto& mode = request.mode;
  PropertyFormat format{};
  auto& property = request.property;
  auto& type = request.type;
  auto& num_items = request.num_items;
  auto& items = request;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 57;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceid
  buf.Write(&deviceid);

  // mode
  uint8_t tmp89;
  tmp89 = static_cast<uint8_t>(mode);
  buf.Write(&tmp89);

  // format
  SwitchVar(PropertyFormat::c_8Bits, items.data8.has_value(), false, &format);
  SwitchVar(PropertyFormat::c_16Bits, items.data16.has_value(), false, &format);
  SwitchVar(PropertyFormat::c_32Bits, items.data32.has_value(), false, &format);
  uint8_t tmp90;
  tmp90 = static_cast<uint8_t>(format);
  buf.Write(&tmp90);

  // property
  buf.Write(&property);

  // type
  buf.Write(&type);

  // num_items
  buf.Write(&num_items);

  // items
  auto items_expr = format;
  if (CaseEq(items_expr, PropertyFormat::c_8Bits)) {
    auto& data8 = *items.data8;

    // data8
    CHECK_EQ(static_cast<size_t>(num_items), data8.size());
    for (auto& data8_elem : data8) {
      // data8_elem
      buf.Write(&data8_elem);
    }

    // pad0
    Align(&buf, 4);
  }
  if (CaseEq(items_expr, PropertyFormat::c_16Bits)) {
    auto& data16 = *items.data16;

    // data16
    CHECK_EQ(static_cast<size_t>(num_items), data16.size());
    for (auto& data16_elem : data16) {
      // data16_elem
      buf.Write(&data16_elem);
    }

    // pad1
    Align(&buf, 4);
  }
  if (CaseEq(items_expr, PropertyFormat::c_32Bits)) {
    auto& data32 = *items.data32;

    // data32
    CHECK_EQ(static_cast<size_t>(num_items), data32.size());
    for (auto& data32_elem : data32) {
      // data32_elem
      buf.Write(&data32_elem);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XIChangeProperty", false);
}

Future<void> Input::XIChangeProperty(
    const DeviceId& deviceid,
    const PropMode& mode,
    const Atom& property,
    const Atom& type,
    const uint32_t& num_items,
    const std::optional<std::vector<uint8_t>>& data8,
    const std::optional<std::vector<uint16_t>>& data16,
    const std::optional<std::vector<uint32_t>>& data32) {
  return Input::XIChangeProperty(Input::XIChangePropertyRequest{
      deviceid, mode, property, type, num_items, data8, data16, data32});
}

Future<void> Input::XIDeleteProperty(
    const Input::XIDeletePropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceid = request.deviceid;
  auto& property = request.property;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 58;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceid
  buf.Write(&deviceid);

  // pad0
  Pad(&buf, 2);

  // property
  buf.Write(&property);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XIDeleteProperty", false);
}

Future<void> Input::XIDeleteProperty(const DeviceId& deviceid,
                                     const Atom& property) {
  return Input::XIDeleteProperty(
      Input::XIDeletePropertyRequest{deviceid, property});
}

Future<Input::XIGetPropertyReply> Input::XIGetProperty(
    const Input::XIGetPropertyRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceid = request.deviceid;
  auto& c_delete = request.c_delete;
  auto& property = request.property;
  auto& type = request.type;
  auto& offset = request.offset;
  auto& len = request.len;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 59;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceid
  buf.Write(&deviceid);

  // c_delete
  buf.Write(&c_delete);

  // pad0
  Pad(&buf, 1);

  // property
  buf.Write(&property);

  // type
  buf.Write(&type);

  // offset
  buf.Write(&offset);

  // len
  buf.Write(&len);

  Align(&buf, 4);

  return connection_->SendRequest<Input::XIGetPropertyReply>(
      &buf, "Input::XIGetProperty", false);
}

Future<Input::XIGetPropertyReply> Input::XIGetProperty(const DeviceId& deviceid,
                                                       const uint8_t& c_delete,
                                                       const Atom& property,
                                                       const Atom& type,
                                                       const uint32_t& offset,
                                                       const uint32_t& len) {
  return Input::XIGetProperty(Input::XIGetPropertyRequest{
      deviceid, c_delete, property, type, offset, len});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::XIGetPropertyReply> detail::ReadReply<
    Input::XIGetPropertyReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::XIGetPropertyReply>();

  auto& sequence = (*reply).sequence;
  auto& type = (*reply).type;
  auto& bytes_after = (*reply).bytes_after;
  auto& num_items = (*reply).num_items;
  Input::PropertyFormat format{};
  auto& items = (*reply);

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // type
  Read(&type, &buf);

  // bytes_after
  Read(&bytes_after, &buf);

  // num_items
  Read(&num_items, &buf);

  // format
  uint8_t tmp91;
  Read(&tmp91, &buf);
  format = static_cast<Input::PropertyFormat>(tmp91);

  // pad1
  Pad(&buf, 11);

  // items
  auto items_expr = format;
  if (CaseEq(items_expr, Input::PropertyFormat::c_8Bits)) {
    items.data8.emplace(decltype(items.data8)::value_type());
    auto& data8 = *items.data8;

    // data8
    data8.resize(num_items);
    for (auto& data8_elem : data8) {
      // data8_elem
      Read(&data8_elem, &buf);
    }

    // pad2
    Align(&buf, 4);
  }
  if (CaseEq(items_expr, Input::PropertyFormat::c_16Bits)) {
    items.data16.emplace(decltype(items.data16)::value_type());
    auto& data16 = *items.data16;

    // data16
    data16.resize(num_items);
    for (auto& data16_elem : data16) {
      // data16_elem
      Read(&data16_elem, &buf);
    }

    // pad3
    Align(&buf, 4);
  }
  if (CaseEq(items_expr, Input::PropertyFormat::c_32Bits)) {
    items.data32.emplace(decltype(items.data32)::value_type());
    auto& data32 = *items.data32;

    // data32
    data32.resize(num_items);
    for (auto& data32_elem : data32) {
      // data32_elem
      Read(&data32_elem, &buf);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Input::XIGetSelectedEventsReply> Input::XIGetSelectedEvents(
    const Input::XIGetSelectedEventsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 60;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<Input::XIGetSelectedEventsReply>(
      &buf, "Input::XIGetSelectedEvents", false);
}

Future<Input::XIGetSelectedEventsReply> Input::XIGetSelectedEvents(
    const Window& window) {
  return Input::XIGetSelectedEvents(Input::XIGetSelectedEventsRequest{window});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Input::XIGetSelectedEventsReply> detail::ReadReply<
    Input::XIGetSelectedEventsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Input::XIGetSelectedEventsReply>();

  auto& sequence = (*reply).sequence;
  uint16_t num_masks{};
  auto& masks = (*reply).masks;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // num_masks
  Read(&num_masks, &buf);

  // pad1
  Pad(&buf, 22);

  // masks
  masks.resize(num_masks);
  for (auto& masks_elem : masks) {
    // masks_elem
    {
      auto& deviceid = masks_elem.deviceid;
      uint16_t mask_len{};
      auto& mask = masks_elem.mask;

      // deviceid
      Read(&deviceid, &buf);

      // mask_len
      Read(&mask_len, &buf);

      // mask
      mask.resize(mask_len);
      for (auto& mask_elem : mask) {
        // mask_elem
        uint32_t tmp92;
        Read(&tmp92, &buf);
        mask_elem = static_cast<Input::XIEventMask>(tmp92);
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Input::XIBarrierReleasePointer(
    const Input::XIBarrierReleasePointerRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint32_t num_barriers{};
  auto& barriers = request.barriers;
  size_t barriers_len = barriers.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 61;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // num_barriers
  num_barriers = barriers.size();
  buf.Write(&num_barriers);

  // barriers
  CHECK_EQ(static_cast<size_t>(num_barriers), barriers.size());
  for (auto& barriers_elem : barriers) {
    // barriers_elem
    {
      auto& deviceid = barriers_elem.deviceid;
      auto& barrier = barriers_elem.barrier;
      auto& eventid = barriers_elem.eventid;

      // deviceid
      buf.Write(&deviceid);

      // pad0
      Pad(&buf, 2);

      // barrier
      buf.Write(&barrier);

      // eventid
      buf.Write(&eventid);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::XIBarrierReleasePointer",
                                        false);
}

Future<void> Input::XIBarrierReleasePointer(
    const std::vector<BarrierReleasePointerInfo>& barriers) {
  return Input::XIBarrierReleasePointer(
      Input::XIBarrierReleasePointerRequest{barriers});
}

Future<void> Input::SendExtensionEvent(
    const Input::SendExtensionEventRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& destination = request.destination;
  auto& device_id = request.device_id;
  auto& propagate = request.propagate;
  uint16_t num_classes{};
  uint8_t num_events{};
  auto& events = request.events;
  size_t events_len = events.size();
  auto& classes = request.classes;
  size_t classes_len = classes.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 31;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // destination
  buf.Write(&destination);

  // device_id
  buf.Write(&device_id);

  // propagate
  buf.Write(&propagate);

  // num_classes
  num_classes = classes.size();
  buf.Write(&num_classes);

  // num_events
  num_events = events.size();
  buf.Write(&num_events);

  // pad0
  Pad(&buf, 3);

  // events
  CHECK_EQ(static_cast<size_t>(num_events), events.size());
  for (auto& events_elem : events) {
    // events_elem
    buf.Write(&events_elem);
  }

  // classes
  CHECK_EQ(static_cast<size_t>(num_classes), classes.size());
  for (auto& classes_elem : classes) {
    // classes_elem
    buf.Write(&classes_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Input::SendExtensionEvent",
                                        false);
}

Future<void> Input::SendExtensionEvent(const Window& destination,
                                       const uint8_t& device_id,
                                       const uint8_t& propagate,
                                       const std::vector<EventForSend>& events,
                                       const std::vector<EventClass>& classes) {
  return Input::SendExtensionEvent(Input::SendExtensionEventRequest{
      destination, device_id, propagate, events, classes});
}

}  // namespace x11
