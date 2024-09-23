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

#include "xkb.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Xkb::Xkb(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

std::string Xkb::KeyboardError::ToString() const {
  std::stringstream ss_;
  ss_ << "Xkb::KeyboardError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".value = " << static_cast<uint64_t>(value) << ", ";
  ss_ << ".minorOpcode = " << static_cast<uint64_t>(minorOpcode) << ", ";
  ss_ << ".majorOpcode = " << static_cast<uint64_t>(majorOpcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Xkb::KeyboardError>(Xkb::KeyboardError* error_,
                                   ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& value = (*error_).value;
  auto& minorOpcode = (*error_).minorOpcode;
  auto& majorOpcode = (*error_).majorOpcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // value
  Read(&value, &buf);

  // minorOpcode
  Read(&minorOpcode, &buf);

  // majorOpcode
  Read(&majorOpcode, &buf);

  // pad0
  Pad(&buf, 21);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::NewKeyboardNotifyEvent>(Xkb::NewKeyboardNotifyEvent* event_,
                                            ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& oldDeviceID = (*event_).oldDeviceID;
  auto& minKeyCode = (*event_).minKeyCode;
  auto& maxKeyCode = (*event_).maxKeyCode;
  auto& oldMinKeyCode = (*event_).oldMinKeyCode;
  auto& oldMaxKeyCode = (*event_).oldMaxKeyCode;
  auto& requestMajor = (*event_).requestMajor;
  auto& requestMinor = (*event_).requestMinor;
  auto& changed = (*event_).changed;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // oldDeviceID
  Read(&oldDeviceID, &buf);

  // minKeyCode
  Read(&minKeyCode, &buf);

  // maxKeyCode
  Read(&maxKeyCode, &buf);

  // oldMinKeyCode
  Read(&oldMinKeyCode, &buf);

  // oldMaxKeyCode
  Read(&oldMaxKeyCode, &buf);

  // requestMajor
  Read(&requestMajor, &buf);

  // requestMinor
  Read(&requestMinor, &buf);

  // changed
  uint16_t tmp0;
  Read(&tmp0, &buf);
  changed = static_cast<Xkb::NKNDetail>(tmp0);

  // pad0
  Pad(&buf, 14);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::MapNotifyEvent>(Xkb::MapNotifyEvent* event_,
                                    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& ptrBtnActions = (*event_).ptrBtnActions;
  auto& changed = (*event_).changed;
  auto& minKeyCode = (*event_).minKeyCode;
  auto& maxKeyCode = (*event_).maxKeyCode;
  auto& firstType = (*event_).firstType;
  auto& nTypes = (*event_).nTypes;
  auto& firstKeySym = (*event_).firstKeySym;
  auto& nKeySyms = (*event_).nKeySyms;
  auto& firstKeyAct = (*event_).firstKeyAct;
  auto& nKeyActs = (*event_).nKeyActs;
  auto& firstKeyBehavior = (*event_).firstKeyBehavior;
  auto& nKeyBehavior = (*event_).nKeyBehavior;
  auto& firstKeyExplicit = (*event_).firstKeyExplicit;
  auto& nKeyExplicit = (*event_).nKeyExplicit;
  auto& firstModMapKey = (*event_).firstModMapKey;
  auto& nModMapKeys = (*event_).nModMapKeys;
  auto& firstVModMapKey = (*event_).firstVModMapKey;
  auto& nVModMapKeys = (*event_).nVModMapKeys;
  auto& virtualMods = (*event_).virtualMods;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // ptrBtnActions
  Read(&ptrBtnActions, &buf);

  // changed
  uint16_t tmp1;
  Read(&tmp1, &buf);
  changed = static_cast<Xkb::MapPart>(tmp1);

  // minKeyCode
  Read(&minKeyCode, &buf);

  // maxKeyCode
  Read(&maxKeyCode, &buf);

  // firstType
  Read(&firstType, &buf);

  // nTypes
  Read(&nTypes, &buf);

  // firstKeySym
  Read(&firstKeySym, &buf);

  // nKeySyms
  Read(&nKeySyms, &buf);

  // firstKeyAct
  Read(&firstKeyAct, &buf);

  // nKeyActs
  Read(&nKeyActs, &buf);

  // firstKeyBehavior
  Read(&firstKeyBehavior, &buf);

  // nKeyBehavior
  Read(&nKeyBehavior, &buf);

  // firstKeyExplicit
  Read(&firstKeyExplicit, &buf);

  // nKeyExplicit
  Read(&nKeyExplicit, &buf);

  // firstModMapKey
  Read(&firstModMapKey, &buf);

  // nModMapKeys
  Read(&nModMapKeys, &buf);

  // firstVModMapKey
  Read(&firstVModMapKey, &buf);

  // nVModMapKeys
  Read(&nVModMapKeys, &buf);

  // virtualMods
  uint16_t tmp2;
  Read(&tmp2, &buf);
  virtualMods = static_cast<Xkb::VMod>(tmp2);

  // pad0
  Pad(&buf, 2);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::StateNotifyEvent>(Xkb::StateNotifyEvent* event_,
                                      ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& mods = (*event_).mods;
  auto& baseMods = (*event_).baseMods;
  auto& latchedMods = (*event_).latchedMods;
  auto& lockedMods = (*event_).lockedMods;
  auto& group = (*event_).group;
  auto& baseGroup = (*event_).baseGroup;
  auto& latchedGroup = (*event_).latchedGroup;
  auto& lockedGroup = (*event_).lockedGroup;
  auto& compatState = (*event_).compatState;
  auto& grabMods = (*event_).grabMods;
  auto& compatGrabMods = (*event_).compatGrabMods;
  auto& lookupMods = (*event_).lookupMods;
  auto& compatLoockupMods = (*event_).compatLoockupMods;
  auto& ptrBtnState = (*event_).ptrBtnState;
  auto& changed = (*event_).changed;
  auto& keycode = (*event_).keycode;
  auto& eventType = (*event_).eventType;
  auto& requestMajor = (*event_).requestMajor;
  auto& requestMinor = (*event_).requestMinor;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // mods
  uint8_t tmp3;
  Read(&tmp3, &buf);
  mods = static_cast<ModMask>(tmp3);

  // baseMods
  uint8_t tmp4;
  Read(&tmp4, &buf);
  baseMods = static_cast<ModMask>(tmp4);

  // latchedMods
  uint8_t tmp5;
  Read(&tmp5, &buf);
  latchedMods = static_cast<ModMask>(tmp5);

  // lockedMods
  uint8_t tmp6;
  Read(&tmp6, &buf);
  lockedMods = static_cast<ModMask>(tmp6);

  // group
  uint8_t tmp7;
  Read(&tmp7, &buf);
  group = static_cast<Xkb::Group>(tmp7);

  // baseGroup
  Read(&baseGroup, &buf);

  // latchedGroup
  Read(&latchedGroup, &buf);

  // lockedGroup
  uint8_t tmp8;
  Read(&tmp8, &buf);
  lockedGroup = static_cast<Xkb::Group>(tmp8);

  // compatState
  uint8_t tmp9;
  Read(&tmp9, &buf);
  compatState = static_cast<ModMask>(tmp9);

  // grabMods
  uint8_t tmp10;
  Read(&tmp10, &buf);
  grabMods = static_cast<ModMask>(tmp10);

  // compatGrabMods
  uint8_t tmp11;
  Read(&tmp11, &buf);
  compatGrabMods = static_cast<ModMask>(tmp11);

  // lookupMods
  uint8_t tmp12;
  Read(&tmp12, &buf);
  lookupMods = static_cast<ModMask>(tmp12);

  // compatLoockupMods
  uint8_t tmp13;
  Read(&tmp13, &buf);
  compatLoockupMods = static_cast<ModMask>(tmp13);

  // ptrBtnState
  uint16_t tmp14;
  Read(&tmp14, &buf);
  ptrBtnState = static_cast<KeyButMask>(tmp14);

  // changed
  uint16_t tmp15;
  Read(&tmp15, &buf);
  changed = static_cast<Xkb::StatePart>(tmp15);

  // keycode
  Read(&keycode, &buf);

  // eventType
  Read(&eventType, &buf);

  // requestMajor
  Read(&requestMajor, &buf);

  // requestMinor
  Read(&requestMinor, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::ControlsNotifyEvent>(Xkb::ControlsNotifyEvent* event_,
                                         ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& numGroups = (*event_).numGroups;
  auto& changedControls = (*event_).changedControls;
  auto& enabledControls = (*event_).enabledControls;
  auto& enabledControlChanges = (*event_).enabledControlChanges;
  auto& keycode = (*event_).keycode;
  auto& eventType = (*event_).eventType;
  auto& requestMajor = (*event_).requestMajor;
  auto& requestMinor = (*event_).requestMinor;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // numGroups
  Read(&numGroups, &buf);

  // pad0
  Pad(&buf, 2);

  // changedControls
  uint32_t tmp16;
  Read(&tmp16, &buf);
  changedControls = static_cast<Xkb::Control>(tmp16);

  // enabledControls
  uint32_t tmp17;
  Read(&tmp17, &buf);
  enabledControls = static_cast<Xkb::BoolCtrl>(tmp17);

  // enabledControlChanges
  uint32_t tmp18;
  Read(&tmp18, &buf);
  enabledControlChanges = static_cast<Xkb::BoolCtrl>(tmp18);

  // keycode
  Read(&keycode, &buf);

  // eventType
  Read(&eventType, &buf);

  // requestMajor
  Read(&requestMajor, &buf);

  // requestMinor
  Read(&requestMinor, &buf);

  // pad1
  Pad(&buf, 4);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::IndicatorStateNotifyEvent>(
    Xkb::IndicatorStateNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& state = (*event_).state;
  auto& stateChanged = (*event_).stateChanged;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // pad0
  Pad(&buf, 3);

  // state
  Read(&state, &buf);

  // stateChanged
  Read(&stateChanged, &buf);

  // pad1
  Pad(&buf, 12);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::IndicatorMapNotifyEvent>(
    Xkb::IndicatorMapNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& state = (*event_).state;
  auto& mapChanged = (*event_).mapChanged;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // pad0
  Pad(&buf, 3);

  // state
  Read(&state, &buf);

  // mapChanged
  Read(&mapChanged, &buf);

  // pad1
  Pad(&buf, 12);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::NamesNotifyEvent>(Xkb::NamesNotifyEvent* event_,
                                      ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& changed = (*event_).changed;
  auto& firstType = (*event_).firstType;
  auto& nTypes = (*event_).nTypes;
  auto& firstLevelName = (*event_).firstLevelName;
  auto& nLevelNames = (*event_).nLevelNames;
  auto& nRadioGroups = (*event_).nRadioGroups;
  auto& nKeyAliases = (*event_).nKeyAliases;
  auto& changedGroupNames = (*event_).changedGroupNames;
  auto& changedVirtualMods = (*event_).changedVirtualMods;
  auto& firstKey = (*event_).firstKey;
  auto& nKeys = (*event_).nKeys;
  auto& changedIndicators = (*event_).changedIndicators;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // pad0
  Pad(&buf, 1);

  // changed
  uint16_t tmp19;
  Read(&tmp19, &buf);
  changed = static_cast<Xkb::NameDetail>(tmp19);

  // firstType
  Read(&firstType, &buf);

  // nTypes
  Read(&nTypes, &buf);

  // firstLevelName
  Read(&firstLevelName, &buf);

  // nLevelNames
  Read(&nLevelNames, &buf);

  // pad1
  Pad(&buf, 1);

  // nRadioGroups
  Read(&nRadioGroups, &buf);

  // nKeyAliases
  Read(&nKeyAliases, &buf);

  // changedGroupNames
  uint8_t tmp20;
  Read(&tmp20, &buf);
  changedGroupNames = static_cast<Xkb::SetOfGroup>(tmp20);

  // changedVirtualMods
  uint16_t tmp21;
  Read(&tmp21, &buf);
  changedVirtualMods = static_cast<Xkb::VMod>(tmp21);

  // firstKey
  Read(&firstKey, &buf);

  // nKeys
  Read(&nKeys, &buf);

  // changedIndicators
  Read(&changedIndicators, &buf);

  // pad2
  Pad(&buf, 4);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::CompatMapNotifyEvent>(Xkb::CompatMapNotifyEvent* event_,
                                          ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& changedGroups = (*event_).changedGroups;
  auto& firstSI = (*event_).firstSI;
  auto& nSI = (*event_).nSI;
  auto& nTotalSI = (*event_).nTotalSI;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // changedGroups
  uint8_t tmp22;
  Read(&tmp22, &buf);
  changedGroups = static_cast<Xkb::SetOfGroup>(tmp22);

  // firstSI
  Read(&firstSI, &buf);

  // nSI
  Read(&nSI, &buf);

  // nTotalSI
  Read(&nTotalSI, &buf);

  // pad0
  Pad(&buf, 16);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::BellNotifyEvent>(Xkb::BellNotifyEvent* event_,
                                     ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& bellClass = (*event_).bellClass;
  auto& bellID = (*event_).bellID;
  auto& percent = (*event_).percent;
  auto& pitch = (*event_).pitch;
  auto& duration = (*event_).duration;
  auto& name = (*event_).name;
  auto& window = (*event_).window;
  auto& eventOnly = (*event_).eventOnly;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // bellClass
  uint8_t tmp23;
  Read(&tmp23, &buf);
  bellClass = static_cast<Xkb::BellClassResult>(tmp23);

  // bellID
  Read(&bellID, &buf);

  // percent
  Read(&percent, &buf);

  // pitch
  Read(&pitch, &buf);

  // duration
  Read(&duration, &buf);

  // name
  Read(&name, &buf);

  // window
  Read(&window, &buf);

  // eventOnly
  Read(&eventOnly, &buf);

  // pad0
  Pad(&buf, 7);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::ActionMessageEvent>(Xkb::ActionMessageEvent* event_,
                                        ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& keycode = (*event_).keycode;
  auto& press = (*event_).press;
  auto& keyEventFollows = (*event_).keyEventFollows;
  auto& mods = (*event_).mods;
  auto& group = (*event_).group;
  auto& message = (*event_).message;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // keycode
  Read(&keycode, &buf);

  // press
  Read(&press, &buf);

  // keyEventFollows
  Read(&keyEventFollows, &buf);

  // mods
  uint8_t tmp24;
  Read(&tmp24, &buf);
  mods = static_cast<ModMask>(tmp24);

  // group
  uint8_t tmp25;
  Read(&tmp25, &buf);
  group = static_cast<Xkb::Group>(tmp25);

  // message
  for (auto& message_elem : message) {
    // message_elem
    Read(&message_elem, &buf);
  }

  // pad0
  Pad(&buf, 10);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::AccessXNotifyEvent>(Xkb::AccessXNotifyEvent* event_,
                                        ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& keycode = (*event_).keycode;
  auto& detailt = (*event_).detailt;
  auto& slowKeysDelay = (*event_).slowKeysDelay;
  auto& debounceDelay = (*event_).debounceDelay;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // keycode
  Read(&keycode, &buf);

  // detailt
  uint16_t tmp26;
  Read(&tmp26, &buf);
  detailt = static_cast<Xkb::AXNDetail>(tmp26);

  // slowKeysDelay
  Read(&slowKeysDelay, &buf);

  // debounceDelay
  Read(&debounceDelay, &buf);

  // pad0
  Pad(&buf, 16);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Xkb::ExtensionDeviceNotifyEvent>(
    Xkb::ExtensionDeviceNotifyEvent* event_,
    ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& xkbType = (*event_).xkbType;
  auto& sequence = (*event_).sequence;
  auto& time = (*event_).time;
  auto& deviceID = (*event_).deviceID;
  auto& reason = (*event_).reason;
  auto& ledClass = (*event_).ledClass;
  auto& ledID = (*event_).ledID;
  auto& ledsDefined = (*event_).ledsDefined;
  auto& ledState = (*event_).ledState;
  auto& firstButton = (*event_).firstButton;
  auto& nButtons = (*event_).nButtons;
  auto& supported = (*event_).supported;
  auto& unsupported = (*event_).unsupported;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // xkbType
  Read(&xkbType, &buf);

  // sequence
  Read(&sequence, &buf);

  // time
  Read(&time, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // pad0
  Pad(&buf, 1);

  // reason
  uint16_t tmp27;
  Read(&tmp27, &buf);
  reason = static_cast<Xkb::XIFeature>(tmp27);

  // ledClass
  uint16_t tmp28;
  Read(&tmp28, &buf);
  ledClass = static_cast<Xkb::LedClassResult>(tmp28);

  // ledID
  Read(&ledID, &buf);

  // ledsDefined
  Read(&ledsDefined, &buf);

  // ledState
  Read(&ledState, &buf);

  // firstButton
  Read(&firstButton, &buf);

  // nButtons
  Read(&nButtons, &buf);

  // supported
  uint16_t tmp29;
  Read(&tmp29, &buf);
  supported = static_cast<Xkb::XIFeature>(tmp29);

  // unsupported
  uint16_t tmp30;
  Read(&tmp30, &buf);
  unsupported = static_cast<Xkb::XIFeature>(tmp30);

  // pad1
  Pad(&buf, 2);

  CHECK_LE(buf.offset, 32ul);
}

Future<Xkb::UseExtensionReply> Xkb::UseExtension(
    const Xkb::UseExtensionRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& wantedMajor = request.wantedMajor;
  auto& wantedMinor = request.wantedMinor;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 0;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // wantedMajor
  buf.Write(&wantedMajor);

  // wantedMinor
  buf.Write(&wantedMinor);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::UseExtensionReply>(
      &buf, "Xkb::UseExtension", false);
}

Future<Xkb::UseExtensionReply> Xkb::UseExtension(const uint16_t& wantedMajor,
                                                 const uint16_t& wantedMinor) {
  return Xkb::UseExtension(Xkb::UseExtensionRequest{wantedMajor, wantedMinor});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::UseExtensionReply> detail::ReadReply<
    Xkb::UseExtensionReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::UseExtensionReply>();

  auto& supported = (*reply).supported;
  auto& sequence = (*reply).sequence;
  auto& serverMajor = (*reply).serverMajor;
  auto& serverMinor = (*reply).serverMinor;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // supported
  Read(&supported, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // serverMajor
  Read(&serverMajor, &buf);

  // serverMinor
  Read(&serverMinor, &buf);

  // pad0
  Pad(&buf, 20);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xkb::SelectEvents(const Xkb::SelectEventsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& affectWhich = request.affectWhich;
  auto& clear = request.clear;
  auto& selectAll = request.selectAll;
  auto& affectMap = request.affectMap;
  auto& map = request.map;
  auto& details = request;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // affectWhich
  uint16_t tmp31;
  tmp31 = static_cast<uint16_t>(affectWhich);
  buf.Write(&tmp31);

  // clear
  uint16_t tmp32;
  tmp32 = static_cast<uint16_t>(clear);
  buf.Write(&tmp32);

  // selectAll
  uint16_t tmp33;
  tmp33 = static_cast<uint16_t>(selectAll);
  buf.Write(&tmp33);

  // affectMap
  uint16_t tmp34;
  tmp34 = static_cast<uint16_t>(affectMap);
  buf.Write(&tmp34);

  // map
  uint16_t tmp35;
  tmp35 = static_cast<uint16_t>(map);
  buf.Write(&tmp35);

  // details
  auto details_expr =
      BitAnd(affectWhich, BitAnd(BitNot(clear), BitNot(selectAll)));
  if (CaseAnd(details_expr, EventType::NewKeyboardNotify)) {
    auto& affectNewKeyboard = *details.affectNewKeyboard;
    auto& newKeyboardDetails = *details.newKeyboardDetails;

    // affectNewKeyboard
    uint16_t tmp36;
    tmp36 = static_cast<uint16_t>(affectNewKeyboard);
    buf.Write(&tmp36);

    // newKeyboardDetails
    uint16_t tmp37;
    tmp37 = static_cast<uint16_t>(newKeyboardDetails);
    buf.Write(&tmp37);
  }
  if (CaseAnd(details_expr, EventType::StateNotify)) {
    auto& affectState = *details.affectState;
    auto& stateDetails = *details.stateDetails;

    // affectState
    uint16_t tmp38;
    tmp38 = static_cast<uint16_t>(affectState);
    buf.Write(&tmp38);

    // stateDetails
    uint16_t tmp39;
    tmp39 = static_cast<uint16_t>(stateDetails);
    buf.Write(&tmp39);
  }
  if (CaseAnd(details_expr, EventType::ControlsNotify)) {
    auto& affectCtrls = *details.affectCtrls;
    auto& ctrlDetails = *details.ctrlDetails;

    // affectCtrls
    uint32_t tmp40;
    tmp40 = static_cast<uint32_t>(affectCtrls);
    buf.Write(&tmp40);

    // ctrlDetails
    uint32_t tmp41;
    tmp41 = static_cast<uint32_t>(ctrlDetails);
    buf.Write(&tmp41);
  }
  if (CaseAnd(details_expr, EventType::IndicatorStateNotify)) {
    auto& affectIndicatorState = *details.affectIndicatorState;
    auto& indicatorStateDetails = *details.indicatorStateDetails;

    // affectIndicatorState
    buf.Write(&affectIndicatorState);

    // indicatorStateDetails
    buf.Write(&indicatorStateDetails);
  }
  if (CaseAnd(details_expr, EventType::IndicatorMapNotify)) {
    auto& affectIndicatorMap = *details.affectIndicatorMap;
    auto& indicatorMapDetails = *details.indicatorMapDetails;

    // affectIndicatorMap
    buf.Write(&affectIndicatorMap);

    // indicatorMapDetails
    buf.Write(&indicatorMapDetails);
  }
  if (CaseAnd(details_expr, EventType::NamesNotify)) {
    auto& affectNames = *details.affectNames;
    auto& namesDetails = *details.namesDetails;

    // affectNames
    uint16_t tmp42;
    tmp42 = static_cast<uint16_t>(affectNames);
    buf.Write(&tmp42);

    // namesDetails
    uint16_t tmp43;
    tmp43 = static_cast<uint16_t>(namesDetails);
    buf.Write(&tmp43);
  }
  if (CaseAnd(details_expr, EventType::CompatMapNotify)) {
    auto& affectCompat = *details.affectCompat;
    auto& compatDetails = *details.compatDetails;

    // affectCompat
    uint8_t tmp44;
    tmp44 = static_cast<uint8_t>(affectCompat);
    buf.Write(&tmp44);

    // compatDetails
    uint8_t tmp45;
    tmp45 = static_cast<uint8_t>(compatDetails);
    buf.Write(&tmp45);
  }
  if (CaseAnd(details_expr, EventType::BellNotify)) {
    auto& affectBell = *details.affectBell;
    auto& bellDetails = *details.bellDetails;

    // affectBell
    buf.Write(&affectBell);

    // bellDetails
    buf.Write(&bellDetails);
  }
  if (CaseAnd(details_expr, EventType::ActionMessage)) {
    auto& affectMsgDetails = *details.affectMsgDetails;
    auto& msgDetails = *details.msgDetails;

    // affectMsgDetails
    buf.Write(&affectMsgDetails);

    // msgDetails
    buf.Write(&msgDetails);
  }
  if (CaseAnd(details_expr, EventType::AccessXNotify)) {
    auto& affectAccessX = *details.affectAccessX;
    auto& accessXDetails = *details.accessXDetails;

    // affectAccessX
    uint16_t tmp46;
    tmp46 = static_cast<uint16_t>(affectAccessX);
    buf.Write(&tmp46);

    // accessXDetails
    uint16_t tmp47;
    tmp47 = static_cast<uint16_t>(accessXDetails);
    buf.Write(&tmp47);
  }
  if (CaseAnd(details_expr, EventType::ExtensionDeviceNotify)) {
    auto& affectExtDev = *details.affectExtDev;
    auto& extdevDetails = *details.extdevDetails;

    // affectExtDev
    uint16_t tmp48;
    tmp48 = static_cast<uint16_t>(affectExtDev);
    buf.Write(&tmp48);

    // extdevDetails
    uint16_t tmp49;
    tmp49 = static_cast<uint16_t>(extdevDetails);
    buf.Write(&tmp49);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xkb::SelectEvents", false);
}

Future<void> Xkb::SelectEvents(
    const DeviceSpec& deviceSpec,
    const EventType& affectWhich,
    const EventType& clear,
    const EventType& selectAll,
    const MapPart& affectMap,
    const MapPart& map,
    const std::optional<NKNDetail>& affectNewKeyboard,
    const std::optional<NKNDetail>& newKeyboardDetails,
    const std::optional<StatePart>& affectState,
    const std::optional<StatePart>& stateDetails,
    const std::optional<Control>& affectCtrls,
    const std::optional<Control>& ctrlDetails,
    const std::optional<uint32_t>& affectIndicatorState,
    const std::optional<uint32_t>& indicatorStateDetails,
    const std::optional<uint32_t>& affectIndicatorMap,
    const std::optional<uint32_t>& indicatorMapDetails,
    const std::optional<NameDetail>& affectNames,
    const std::optional<NameDetail>& namesDetails,
    const std::optional<CMDetail>& affectCompat,
    const std::optional<CMDetail>& compatDetails,
    const std::optional<uint8_t>& affectBell,
    const std::optional<uint8_t>& bellDetails,
    const std::optional<uint8_t>& affectMsgDetails,
    const std::optional<uint8_t>& msgDetails,
    const std::optional<AXNDetail>& affectAccessX,
    const std::optional<AXNDetail>& accessXDetails,
    const std::optional<XIFeature>& affectExtDev,
    const std::optional<XIFeature>& extdevDetails) {
  return Xkb::SelectEvents(Xkb::SelectEventsRequest{deviceSpec,
                                                    affectWhich,
                                                    clear,
                                                    selectAll,
                                                    affectMap,
                                                    map,
                                                    affectNewKeyboard,
                                                    newKeyboardDetails,
                                                    affectState,
                                                    stateDetails,
                                                    affectCtrls,
                                                    ctrlDetails,
                                                    affectIndicatorState,
                                                    indicatorStateDetails,
                                                    affectIndicatorMap,
                                                    indicatorMapDetails,
                                                    affectNames,
                                                    namesDetails,
                                                    affectCompat,
                                                    compatDetails,
                                                    affectBell,
                                                    bellDetails,
                                                    affectMsgDetails,
                                                    msgDetails,
                                                    affectAccessX,
                                                    accessXDetails,
                                                    affectExtDev,
                                                    extdevDetails});
}

Future<void> Xkb::Bell(const Xkb::BellRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& bellClass = request.bellClass;
  auto& bellID = request.bellID;
  auto& percent = request.percent;
  auto& forceSound = request.forceSound;
  auto& eventOnly = request.eventOnly;
  auto& pitch = request.pitch;
  auto& duration = request.duration;
  auto& name = request.name;
  auto& window = request.window;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // bellClass
  buf.Write(&bellClass);

  // bellID
  buf.Write(&bellID);

  // percent
  buf.Write(&percent);

  // forceSound
  buf.Write(&forceSound);

  // eventOnly
  buf.Write(&eventOnly);

  // pad0
  Pad(&buf, 1);

  // pitch
  buf.Write(&pitch);

  // duration
  buf.Write(&duration);

  // pad1
  Pad(&buf, 2);

  // name
  buf.Write(&name);

  // window
  buf.Write(&window);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xkb::Bell", false);
}

Future<void> Xkb::Bell(const DeviceSpec& deviceSpec,
                       const BellClassSpec& bellClass,
                       const IDSpec& bellID,
                       const int8_t& percent,
                       const uint8_t& forceSound,
                       const uint8_t& eventOnly,
                       const int16_t& pitch,
                       const int16_t& duration,
                       const Atom& name,
                       const Window& window) {
  return Xkb::Bell(Xkb::BellRequest{deviceSpec, bellClass, bellID, percent,
                                    forceSound, eventOnly, pitch, duration,
                                    name, window});
}

Future<Xkb::GetStateReply> Xkb::GetState(const Xkb::GetStateRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::GetStateReply>(&buf, "Xkb::GetState",
                                                      false);
}

Future<Xkb::GetStateReply> Xkb::GetState(const DeviceSpec& deviceSpec) {
  return Xkb::GetState(Xkb::GetStateRequest{deviceSpec});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::GetStateReply> detail::ReadReply<Xkb::GetStateReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::GetStateReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  auto& mods = (*reply).mods;
  auto& baseMods = (*reply).baseMods;
  auto& latchedMods = (*reply).latchedMods;
  auto& lockedMods = (*reply).lockedMods;
  auto& group = (*reply).group;
  auto& lockedGroup = (*reply).lockedGroup;
  auto& baseGroup = (*reply).baseGroup;
  auto& latchedGroup = (*reply).latchedGroup;
  auto& compatState = (*reply).compatState;
  auto& grabMods = (*reply).grabMods;
  auto& compatGrabMods = (*reply).compatGrabMods;
  auto& lookupMods = (*reply).lookupMods;
  auto& compatLookupMods = (*reply).compatLookupMods;
  auto& ptrBtnState = (*reply).ptrBtnState;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // mods
  uint8_t tmp50;
  Read(&tmp50, &buf);
  mods = static_cast<ModMask>(tmp50);

  // baseMods
  uint8_t tmp51;
  Read(&tmp51, &buf);
  baseMods = static_cast<ModMask>(tmp51);

  // latchedMods
  uint8_t tmp52;
  Read(&tmp52, &buf);
  latchedMods = static_cast<ModMask>(tmp52);

  // lockedMods
  uint8_t tmp53;
  Read(&tmp53, &buf);
  lockedMods = static_cast<ModMask>(tmp53);

  // group
  uint8_t tmp54;
  Read(&tmp54, &buf);
  group = static_cast<Xkb::Group>(tmp54);

  // lockedGroup
  uint8_t tmp55;
  Read(&tmp55, &buf);
  lockedGroup = static_cast<Xkb::Group>(tmp55);

  // baseGroup
  Read(&baseGroup, &buf);

  // latchedGroup
  Read(&latchedGroup, &buf);

  // compatState
  uint8_t tmp56;
  Read(&tmp56, &buf);
  compatState = static_cast<ModMask>(tmp56);

  // grabMods
  uint8_t tmp57;
  Read(&tmp57, &buf);
  grabMods = static_cast<ModMask>(tmp57);

  // compatGrabMods
  uint8_t tmp58;
  Read(&tmp58, &buf);
  compatGrabMods = static_cast<ModMask>(tmp58);

  // lookupMods
  uint8_t tmp59;
  Read(&tmp59, &buf);
  lookupMods = static_cast<ModMask>(tmp59);

  // compatLookupMods
  uint8_t tmp60;
  Read(&tmp60, &buf);
  compatLookupMods = static_cast<ModMask>(tmp60);

  // pad0
  Pad(&buf, 1);

  // ptrBtnState
  uint16_t tmp61;
  Read(&tmp61, &buf);
  ptrBtnState = static_cast<KeyButMask>(tmp61);

  // pad1
  Pad(&buf, 6);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xkb::LatchLockState(const Xkb::LatchLockStateRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& affectModLocks = request.affectModLocks;
  auto& modLocks = request.modLocks;
  auto& lockGroup = request.lockGroup;
  auto& groupLock = request.groupLock;
  auto& affectModLatches = request.affectModLatches;
  auto& latchGroup = request.latchGroup;
  auto& groupLatch = request.groupLatch;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // affectModLocks
  uint8_t tmp62;
  tmp62 = static_cast<uint8_t>(affectModLocks);
  buf.Write(&tmp62);

  // modLocks
  uint8_t tmp63;
  tmp63 = static_cast<uint8_t>(modLocks);
  buf.Write(&tmp63);

  // lockGroup
  buf.Write(&lockGroup);

  // groupLock
  uint8_t tmp64;
  tmp64 = static_cast<uint8_t>(groupLock);
  buf.Write(&tmp64);

  // affectModLatches
  uint8_t tmp65;
  tmp65 = static_cast<uint8_t>(affectModLatches);
  buf.Write(&tmp65);

  // pad0
  Pad(&buf, 1);

  // pad1
  Pad(&buf, 1);

  // latchGroup
  buf.Write(&latchGroup);

  // groupLatch
  buf.Write(&groupLatch);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xkb::LatchLockState", false);
}

Future<void> Xkb::LatchLockState(const DeviceSpec& deviceSpec,
                                 const ModMask& affectModLocks,
                                 const ModMask& modLocks,
                                 const uint8_t& lockGroup,
                                 const Group& groupLock,
                                 const ModMask& affectModLatches,
                                 const uint8_t& latchGroup,
                                 const uint16_t& groupLatch) {
  return Xkb::LatchLockState(Xkb::LatchLockStateRequest{
      deviceSpec, affectModLocks, modLocks, lockGroup, groupLock,
      affectModLatches, latchGroup, groupLatch});
}

Future<Xkb::GetControlsReply> Xkb::GetControls(
    const Xkb::GetControlsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::GetControlsReply>(
      &buf, "Xkb::GetControls", false);
}

Future<Xkb::GetControlsReply> Xkb::GetControls(const DeviceSpec& deviceSpec) {
  return Xkb::GetControls(Xkb::GetControlsRequest{deviceSpec});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::GetControlsReply> detail::ReadReply<Xkb::GetControlsReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::GetControlsReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  auto& mouseKeysDfltBtn = (*reply).mouseKeysDfltBtn;
  auto& numGroups = (*reply).numGroups;
  auto& groupsWrap = (*reply).groupsWrap;
  auto& internalModsMask = (*reply).internalModsMask;
  auto& ignoreLockModsMask = (*reply).ignoreLockModsMask;
  auto& internalModsRealMods = (*reply).internalModsRealMods;
  auto& ignoreLockModsRealMods = (*reply).ignoreLockModsRealMods;
  auto& internalModsVmods = (*reply).internalModsVmods;
  auto& ignoreLockModsVmods = (*reply).ignoreLockModsVmods;
  auto& repeatDelay = (*reply).repeatDelay;
  auto& repeatInterval = (*reply).repeatInterval;
  auto& slowKeysDelay = (*reply).slowKeysDelay;
  auto& debounceDelay = (*reply).debounceDelay;
  auto& mouseKeysDelay = (*reply).mouseKeysDelay;
  auto& mouseKeysInterval = (*reply).mouseKeysInterval;
  auto& mouseKeysTimeToMax = (*reply).mouseKeysTimeToMax;
  auto& mouseKeysMaxSpeed = (*reply).mouseKeysMaxSpeed;
  auto& mouseKeysCurve = (*reply).mouseKeysCurve;
  auto& accessXOption = (*reply).accessXOption;
  auto& accessXTimeout = (*reply).accessXTimeout;
  auto& accessXTimeoutOptionsMask = (*reply).accessXTimeoutOptionsMask;
  auto& accessXTimeoutOptionsValues = (*reply).accessXTimeoutOptionsValues;
  auto& accessXTimeoutMask = (*reply).accessXTimeoutMask;
  auto& accessXTimeoutValues = (*reply).accessXTimeoutValues;
  auto& enabledControls = (*reply).enabledControls;
  auto& perKeyRepeat = (*reply).perKeyRepeat;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // mouseKeysDfltBtn
  Read(&mouseKeysDfltBtn, &buf);

  // numGroups
  Read(&numGroups, &buf);

  // groupsWrap
  Read(&groupsWrap, &buf);

  // internalModsMask
  uint8_t tmp66;
  Read(&tmp66, &buf);
  internalModsMask = static_cast<ModMask>(tmp66);

  // ignoreLockModsMask
  uint8_t tmp67;
  Read(&tmp67, &buf);
  ignoreLockModsMask = static_cast<ModMask>(tmp67);

  // internalModsRealMods
  uint8_t tmp68;
  Read(&tmp68, &buf);
  internalModsRealMods = static_cast<ModMask>(tmp68);

  // ignoreLockModsRealMods
  uint8_t tmp69;
  Read(&tmp69, &buf);
  ignoreLockModsRealMods = static_cast<ModMask>(tmp69);

  // pad0
  Pad(&buf, 1);

  // internalModsVmods
  uint16_t tmp70;
  Read(&tmp70, &buf);
  internalModsVmods = static_cast<Xkb::VMod>(tmp70);

  // ignoreLockModsVmods
  uint16_t tmp71;
  Read(&tmp71, &buf);
  ignoreLockModsVmods = static_cast<Xkb::VMod>(tmp71);

  // repeatDelay
  Read(&repeatDelay, &buf);

  // repeatInterval
  Read(&repeatInterval, &buf);

  // slowKeysDelay
  Read(&slowKeysDelay, &buf);

  // debounceDelay
  Read(&debounceDelay, &buf);

  // mouseKeysDelay
  Read(&mouseKeysDelay, &buf);

  // mouseKeysInterval
  Read(&mouseKeysInterval, &buf);

  // mouseKeysTimeToMax
  Read(&mouseKeysTimeToMax, &buf);

  // mouseKeysMaxSpeed
  Read(&mouseKeysMaxSpeed, &buf);

  // mouseKeysCurve
  Read(&mouseKeysCurve, &buf);

  // accessXOption
  uint16_t tmp72;
  Read(&tmp72, &buf);
  accessXOption = static_cast<Xkb::AXOption>(tmp72);

  // accessXTimeout
  Read(&accessXTimeout, &buf);

  // accessXTimeoutOptionsMask
  uint16_t tmp73;
  Read(&tmp73, &buf);
  accessXTimeoutOptionsMask = static_cast<Xkb::AXOption>(tmp73);

  // accessXTimeoutOptionsValues
  uint16_t tmp74;
  Read(&tmp74, &buf);
  accessXTimeoutOptionsValues = static_cast<Xkb::AXOption>(tmp74);

  // pad1
  Pad(&buf, 2);

  // accessXTimeoutMask
  uint32_t tmp75;
  Read(&tmp75, &buf);
  accessXTimeoutMask = static_cast<Xkb::BoolCtrl>(tmp75);

  // accessXTimeoutValues
  uint32_t tmp76;
  Read(&tmp76, &buf);
  accessXTimeoutValues = static_cast<Xkb::BoolCtrl>(tmp76);

  // enabledControls
  uint32_t tmp77;
  Read(&tmp77, &buf);
  enabledControls = static_cast<Xkb::BoolCtrl>(tmp77);

  // perKeyRepeat
  for (auto& perKeyRepeat_elem : perKeyRepeat) {
    // perKeyRepeat_elem
    Read(&perKeyRepeat_elem, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xkb::SetControls(const Xkb::SetControlsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& affectInternalRealMods = request.affectInternalRealMods;
  auto& internalRealMods = request.internalRealMods;
  auto& affectIgnoreLockRealMods = request.affectIgnoreLockRealMods;
  auto& ignoreLockRealMods = request.ignoreLockRealMods;
  auto& affectInternalVirtualMods = request.affectInternalVirtualMods;
  auto& internalVirtualMods = request.internalVirtualMods;
  auto& affectIgnoreLockVirtualMods = request.affectIgnoreLockVirtualMods;
  auto& ignoreLockVirtualMods = request.ignoreLockVirtualMods;
  auto& mouseKeysDfltBtn = request.mouseKeysDfltBtn;
  auto& groupsWrap = request.groupsWrap;
  auto& accessXOptions = request.accessXOptions;
  auto& affectEnabledControls = request.affectEnabledControls;
  auto& enabledControls = request.enabledControls;
  auto& changeControls = request.changeControls;
  auto& repeatDelay = request.repeatDelay;
  auto& repeatInterval = request.repeatInterval;
  auto& slowKeysDelay = request.slowKeysDelay;
  auto& debounceDelay = request.debounceDelay;
  auto& mouseKeysDelay = request.mouseKeysDelay;
  auto& mouseKeysInterval = request.mouseKeysInterval;
  auto& mouseKeysTimeToMax = request.mouseKeysTimeToMax;
  auto& mouseKeysMaxSpeed = request.mouseKeysMaxSpeed;
  auto& mouseKeysCurve = request.mouseKeysCurve;
  auto& accessXTimeout = request.accessXTimeout;
  auto& accessXTimeoutMask = request.accessXTimeoutMask;
  auto& accessXTimeoutValues = request.accessXTimeoutValues;
  auto& accessXTimeoutOptionsMask = request.accessXTimeoutOptionsMask;
  auto& accessXTimeoutOptionsValues = request.accessXTimeoutOptionsValues;
  auto& perKeyRepeat = request.perKeyRepeat;
  size_t perKeyRepeat_len = perKeyRepeat.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // affectInternalRealMods
  uint8_t tmp78;
  tmp78 = static_cast<uint8_t>(affectInternalRealMods);
  buf.Write(&tmp78);

  // internalRealMods
  uint8_t tmp79;
  tmp79 = static_cast<uint8_t>(internalRealMods);
  buf.Write(&tmp79);

  // affectIgnoreLockRealMods
  uint8_t tmp80;
  tmp80 = static_cast<uint8_t>(affectIgnoreLockRealMods);
  buf.Write(&tmp80);

  // ignoreLockRealMods
  uint8_t tmp81;
  tmp81 = static_cast<uint8_t>(ignoreLockRealMods);
  buf.Write(&tmp81);

  // affectInternalVirtualMods
  uint16_t tmp82;
  tmp82 = static_cast<uint16_t>(affectInternalVirtualMods);
  buf.Write(&tmp82);

  // internalVirtualMods
  uint16_t tmp83;
  tmp83 = static_cast<uint16_t>(internalVirtualMods);
  buf.Write(&tmp83);

  // affectIgnoreLockVirtualMods
  uint16_t tmp84;
  tmp84 = static_cast<uint16_t>(affectIgnoreLockVirtualMods);
  buf.Write(&tmp84);

  // ignoreLockVirtualMods
  uint16_t tmp85;
  tmp85 = static_cast<uint16_t>(ignoreLockVirtualMods);
  buf.Write(&tmp85);

  // mouseKeysDfltBtn
  buf.Write(&mouseKeysDfltBtn);

  // groupsWrap
  buf.Write(&groupsWrap);

  // accessXOptions
  uint16_t tmp86;
  tmp86 = static_cast<uint16_t>(accessXOptions);
  buf.Write(&tmp86);

  // pad0
  Pad(&buf, 2);

  // affectEnabledControls
  uint32_t tmp87;
  tmp87 = static_cast<uint32_t>(affectEnabledControls);
  buf.Write(&tmp87);

  // enabledControls
  uint32_t tmp88;
  tmp88 = static_cast<uint32_t>(enabledControls);
  buf.Write(&tmp88);

  // changeControls
  uint32_t tmp89;
  tmp89 = static_cast<uint32_t>(changeControls);
  buf.Write(&tmp89);

  // repeatDelay
  buf.Write(&repeatDelay);

  // repeatInterval
  buf.Write(&repeatInterval);

  // slowKeysDelay
  buf.Write(&slowKeysDelay);

  // debounceDelay
  buf.Write(&debounceDelay);

  // mouseKeysDelay
  buf.Write(&mouseKeysDelay);

  // mouseKeysInterval
  buf.Write(&mouseKeysInterval);

  // mouseKeysTimeToMax
  buf.Write(&mouseKeysTimeToMax);

  // mouseKeysMaxSpeed
  buf.Write(&mouseKeysMaxSpeed);

  // mouseKeysCurve
  buf.Write(&mouseKeysCurve);

  // accessXTimeout
  buf.Write(&accessXTimeout);

  // accessXTimeoutMask
  uint32_t tmp90;
  tmp90 = static_cast<uint32_t>(accessXTimeoutMask);
  buf.Write(&tmp90);

  // accessXTimeoutValues
  uint32_t tmp91;
  tmp91 = static_cast<uint32_t>(accessXTimeoutValues);
  buf.Write(&tmp91);

  // accessXTimeoutOptionsMask
  uint16_t tmp92;
  tmp92 = static_cast<uint16_t>(accessXTimeoutOptionsMask);
  buf.Write(&tmp92);

  // accessXTimeoutOptionsValues
  uint16_t tmp93;
  tmp93 = static_cast<uint16_t>(accessXTimeoutOptionsValues);
  buf.Write(&tmp93);

  // perKeyRepeat
  for (auto& perKeyRepeat_elem : perKeyRepeat) {
    // perKeyRepeat_elem
    buf.Write(&perKeyRepeat_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xkb::SetControls", false);
}

Future<void> Xkb::SetControls(const DeviceSpec& deviceSpec,
                              const ModMask& affectInternalRealMods,
                              const ModMask& internalRealMods,
                              const ModMask& affectIgnoreLockRealMods,
                              const ModMask& ignoreLockRealMods,
                              const VMod& affectInternalVirtualMods,
                              const VMod& internalVirtualMods,
                              const VMod& affectIgnoreLockVirtualMods,
                              const VMod& ignoreLockVirtualMods,
                              const uint8_t& mouseKeysDfltBtn,
                              const uint8_t& groupsWrap,
                              const AXOption& accessXOptions,
                              const BoolCtrl& affectEnabledControls,
                              const BoolCtrl& enabledControls,
                              const Control& changeControls,
                              const uint16_t& repeatDelay,
                              const uint16_t& repeatInterval,
                              const uint16_t& slowKeysDelay,
                              const uint16_t& debounceDelay,
                              const uint16_t& mouseKeysDelay,
                              const uint16_t& mouseKeysInterval,
                              const uint16_t& mouseKeysTimeToMax,
                              const uint16_t& mouseKeysMaxSpeed,
                              const int16_t& mouseKeysCurve,
                              const uint16_t& accessXTimeout,
                              const BoolCtrl& accessXTimeoutMask,
                              const BoolCtrl& accessXTimeoutValues,
                              const AXOption& accessXTimeoutOptionsMask,
                              const AXOption& accessXTimeoutOptionsValues,
                              const std::array<uint8_t, 32>& perKeyRepeat) {
  return Xkb::SetControls(Xkb::SetControlsRequest{deviceSpec,
                                                  affectInternalRealMods,
                                                  internalRealMods,
                                                  affectIgnoreLockRealMods,
                                                  ignoreLockRealMods,
                                                  affectInternalVirtualMods,
                                                  internalVirtualMods,
                                                  affectIgnoreLockVirtualMods,
                                                  ignoreLockVirtualMods,
                                                  mouseKeysDfltBtn,
                                                  groupsWrap,
                                                  accessXOptions,
                                                  affectEnabledControls,
                                                  enabledControls,
                                                  changeControls,
                                                  repeatDelay,
                                                  repeatInterval,
                                                  slowKeysDelay,
                                                  debounceDelay,
                                                  mouseKeysDelay,
                                                  mouseKeysInterval,
                                                  mouseKeysTimeToMax,
                                                  mouseKeysMaxSpeed,
                                                  mouseKeysCurve,
                                                  accessXTimeout,
                                                  accessXTimeoutMask,
                                                  accessXTimeoutValues,
                                                  accessXTimeoutOptionsMask,
                                                  accessXTimeoutOptionsValues,
                                                  perKeyRepeat});
}

Future<Xkb::GetMapReply> Xkb::GetMap(const Xkb::GetMapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& full = request.full;
  auto& partial = request.partial;
  auto& firstType = request.firstType;
  auto& nTypes = request.nTypes;
  auto& firstKeySym = request.firstKeySym;
  auto& nKeySyms = request.nKeySyms;
  auto& firstKeyAction = request.firstKeyAction;
  auto& nKeyActions = request.nKeyActions;
  auto& firstKeyBehavior = request.firstKeyBehavior;
  auto& nKeyBehaviors = request.nKeyBehaviors;
  auto& virtualMods = request.virtualMods;
  auto& firstKeyExplicit = request.firstKeyExplicit;
  auto& nKeyExplicit = request.nKeyExplicit;
  auto& firstModMapKey = request.firstModMapKey;
  auto& nModMapKeys = request.nModMapKeys;
  auto& firstVModMapKey = request.firstVModMapKey;
  auto& nVModMapKeys = request.nVModMapKeys;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // full
  uint16_t tmp94;
  tmp94 = static_cast<uint16_t>(full);
  buf.Write(&tmp94);

  // partial
  uint16_t tmp95;
  tmp95 = static_cast<uint16_t>(partial);
  buf.Write(&tmp95);

  // firstType
  buf.Write(&firstType);

  // nTypes
  buf.Write(&nTypes);

  // firstKeySym
  buf.Write(&firstKeySym);

  // nKeySyms
  buf.Write(&nKeySyms);

  // firstKeyAction
  buf.Write(&firstKeyAction);

  // nKeyActions
  buf.Write(&nKeyActions);

  // firstKeyBehavior
  buf.Write(&firstKeyBehavior);

  // nKeyBehaviors
  buf.Write(&nKeyBehaviors);

  // virtualMods
  uint16_t tmp96;
  tmp96 = static_cast<uint16_t>(virtualMods);
  buf.Write(&tmp96);

  // firstKeyExplicit
  buf.Write(&firstKeyExplicit);

  // nKeyExplicit
  buf.Write(&nKeyExplicit);

  // firstModMapKey
  buf.Write(&firstModMapKey);

  // nModMapKeys
  buf.Write(&nModMapKeys);

  // firstVModMapKey
  buf.Write(&firstVModMapKey);

  // nVModMapKeys
  buf.Write(&nVModMapKeys);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::GetMapReply>(&buf, "Xkb::GetMap", false);
}

Future<Xkb::GetMapReply> Xkb::GetMap(const DeviceSpec& deviceSpec,
                                     const MapPart& full,
                                     const MapPart& partial,
                                     const uint8_t& firstType,
                                     const uint8_t& nTypes,
                                     const KeyCode& firstKeySym,
                                     const uint8_t& nKeySyms,
                                     const KeyCode& firstKeyAction,
                                     const uint8_t& nKeyActions,
                                     const KeyCode& firstKeyBehavior,
                                     const uint8_t& nKeyBehaviors,
                                     const VMod& virtualMods,
                                     const KeyCode& firstKeyExplicit,
                                     const uint8_t& nKeyExplicit,
                                     const KeyCode& firstModMapKey,
                                     const uint8_t& nModMapKeys,
                                     const KeyCode& firstVModMapKey,
                                     const uint8_t& nVModMapKeys) {
  return Xkb::GetMap(Xkb::GetMapRequest{
      deviceSpec, full, partial, firstType, nTypes, firstKeySym, nKeySyms,
      firstKeyAction, nKeyActions, firstKeyBehavior, nKeyBehaviors, virtualMods,
      firstKeyExplicit, nKeyExplicit, firstModMapKey, nModMapKeys,
      firstVModMapKey, nVModMapKeys});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::GetMapReply> detail::ReadReply<Xkb::GetMapReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::GetMapReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  auto& minKeyCode = (*reply).minKeyCode;
  auto& maxKeyCode = (*reply).maxKeyCode;
  Xkb::MapPart present{};
  auto& firstType = (*reply).firstType;
  auto& nTypes = (*reply).nTypes;
  auto& totalTypes = (*reply).totalTypes;
  auto& firstKeySym = (*reply).firstKeySym;
  auto& totalSyms = (*reply).totalSyms;
  auto& nKeySyms = (*reply).nKeySyms;
  auto& firstKeyAction = (*reply).firstKeyAction;
  auto& totalActions = (*reply).totalActions;
  auto& nKeyActions = (*reply).nKeyActions;
  auto& firstKeyBehavior = (*reply).firstKeyBehavior;
  auto& nKeyBehaviors = (*reply).nKeyBehaviors;
  auto& totalKeyBehaviors = (*reply).totalKeyBehaviors;
  auto& firstKeyExplicit = (*reply).firstKeyExplicit;
  auto& nKeyExplicit = (*reply).nKeyExplicit;
  auto& totalKeyExplicit = (*reply).totalKeyExplicit;
  auto& firstModMapKey = (*reply).firstModMapKey;
  auto& nModMapKeys = (*reply).nModMapKeys;
  auto& totalModMapKeys = (*reply).totalModMapKeys;
  auto& firstVModMapKey = (*reply).firstVModMapKey;
  auto& nVModMapKeys = (*reply).nVModMapKeys;
  auto& totalVModMapKeys = (*reply).totalVModMapKeys;
  auto& virtualMods = (*reply).virtualMods;
  auto& map = (*reply);

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // pad0
  Pad(&buf, 2);

  // minKeyCode
  Read(&minKeyCode, &buf);

  // maxKeyCode
  Read(&maxKeyCode, &buf);

  // present
  uint16_t tmp97;
  Read(&tmp97, &buf);
  present = static_cast<Xkb::MapPart>(tmp97);

  // firstType
  Read(&firstType, &buf);

  // nTypes
  Read(&nTypes, &buf);

  // totalTypes
  Read(&totalTypes, &buf);

  // firstKeySym
  Read(&firstKeySym, &buf);

  // totalSyms
  Read(&totalSyms, &buf);

  // nKeySyms
  Read(&nKeySyms, &buf);

  // firstKeyAction
  Read(&firstKeyAction, &buf);

  // totalActions
  Read(&totalActions, &buf);

  // nKeyActions
  Read(&nKeyActions, &buf);

  // firstKeyBehavior
  Read(&firstKeyBehavior, &buf);

  // nKeyBehaviors
  Read(&nKeyBehaviors, &buf);

  // totalKeyBehaviors
  Read(&totalKeyBehaviors, &buf);

  // firstKeyExplicit
  Read(&firstKeyExplicit, &buf);

  // nKeyExplicit
  Read(&nKeyExplicit, &buf);

  // totalKeyExplicit
  Read(&totalKeyExplicit, &buf);

  // firstModMapKey
  Read(&firstModMapKey, &buf);

  // nModMapKeys
  Read(&nModMapKeys, &buf);

  // totalModMapKeys
  Read(&totalModMapKeys, &buf);

  // firstVModMapKey
  Read(&firstVModMapKey, &buf);

  // nVModMapKeys
  Read(&nVModMapKeys, &buf);

  // totalVModMapKeys
  Read(&totalVModMapKeys, &buf);

  // pad1
  Pad(&buf, 1);

  // virtualMods
  uint16_t tmp98;
  Read(&tmp98, &buf);
  virtualMods = static_cast<Xkb::VMod>(tmp98);

  // map
  auto map_expr = present;
  if (CaseAnd(map_expr, Xkb::MapPart::KeyTypes)) {
    map.types_rtrn.emplace(decltype(map.types_rtrn)::value_type());
    auto& types_rtrn = *map.types_rtrn;

    // types_rtrn
    types_rtrn.resize(nTypes);
    for (auto& types_rtrn_elem : types_rtrn) {
      // types_rtrn_elem
      {
        auto& mods_mask = types_rtrn_elem.mods_mask;
        auto& mods_mods = types_rtrn_elem.mods_mods;
        auto& mods_vmods = types_rtrn_elem.mods_vmods;
        auto& numLevels = types_rtrn_elem.numLevels;
        uint8_t nMapEntries{};
        auto& hasPreserve = types_rtrn_elem.hasPreserve;
        auto& map = types_rtrn_elem.map;
        auto& preserve = types_rtrn_elem.preserve;

        // mods_mask
        uint8_t tmp99;
        Read(&tmp99, &buf);
        mods_mask = static_cast<ModMask>(tmp99);

        // mods_mods
        uint8_t tmp100;
        Read(&tmp100, &buf);
        mods_mods = static_cast<ModMask>(tmp100);

        // mods_vmods
        uint16_t tmp101;
        Read(&tmp101, &buf);
        mods_vmods = static_cast<Xkb::VMod>(tmp101);

        // numLevels
        Read(&numLevels, &buf);

        // nMapEntries
        Read(&nMapEntries, &buf);

        // hasPreserve
        Read(&hasPreserve, &buf);

        // pad0
        Pad(&buf, 1);

        // map
        map.resize(nMapEntries);
        for (auto& map_elem : map) {
          // map_elem
          {
            auto& active = map_elem.active;
            auto& mods_mask = map_elem.mods_mask;
            auto& level = map_elem.level;
            auto& mods_mods = map_elem.mods_mods;
            auto& mods_vmods = map_elem.mods_vmods;

            // active
            Read(&active, &buf);

            // mods_mask
            uint8_t tmp102;
            Read(&tmp102, &buf);
            mods_mask = static_cast<ModMask>(tmp102);

            // level
            Read(&level, &buf);

            // mods_mods
            uint8_t tmp103;
            Read(&tmp103, &buf);
            mods_mods = static_cast<ModMask>(tmp103);

            // mods_vmods
            uint16_t tmp104;
            Read(&tmp104, &buf);
            mods_vmods = static_cast<Xkb::VMod>(tmp104);

            // pad0
            Pad(&buf, 2);
          }
        }

        // preserve
        preserve.resize((hasPreserve) * (nMapEntries));
        for (auto& preserve_elem : preserve) {
          // preserve_elem
          {
            auto& mask = preserve_elem.mask;
            auto& realMods = preserve_elem.realMods;
            auto& vmods = preserve_elem.vmods;

            // mask
            uint8_t tmp105;
            Read(&tmp105, &buf);
            mask = static_cast<ModMask>(tmp105);

            // realMods
            uint8_t tmp106;
            Read(&tmp106, &buf);
            realMods = static_cast<ModMask>(tmp106);

            // vmods
            uint16_t tmp107;
            Read(&tmp107, &buf);
            vmods = static_cast<Xkb::VMod>(tmp107);
          }
        }
      }
    }
  }
  if (CaseAnd(map_expr, Xkb::MapPart::KeySyms)) {
    map.syms_rtrn.emplace(decltype(map.syms_rtrn)::value_type());
    auto& syms_rtrn = *map.syms_rtrn;

    // syms_rtrn
    syms_rtrn.resize(nKeySyms);
    for (auto& syms_rtrn_elem : syms_rtrn) {
      // syms_rtrn_elem
      {
        auto& kt_index = syms_rtrn_elem.kt_index;
        auto& groupInfo = syms_rtrn_elem.groupInfo;
        auto& width = syms_rtrn_elem.width;
        uint16_t nSyms{};
        auto& syms = syms_rtrn_elem.syms;

        // kt_index
        for (auto& kt_index_elem : kt_index) {
          // kt_index_elem
          Read(&kt_index_elem, &buf);
        }

        // groupInfo
        Read(&groupInfo, &buf);

        // width
        Read(&width, &buf);

        // nSyms
        Read(&nSyms, &buf);

        // syms
        syms.resize(nSyms);
        for (auto& syms_elem : syms) {
          // syms_elem
          Read(&syms_elem, &buf);
        }
      }
    }
  }
  if (CaseAnd(map_expr, Xkb::MapPart::KeyActions)) {
    map.acts_rtrn_count.emplace(decltype(map.acts_rtrn_count)::value_type());
    map.acts_rtrn_acts.emplace(decltype(map.acts_rtrn_acts)::value_type());
    auto& acts_rtrn_count = *map.acts_rtrn_count;
    auto& acts_rtrn_acts = *map.acts_rtrn_acts;

    // acts_rtrn_count
    acts_rtrn_count.resize(nKeyActions);
    for (auto& acts_rtrn_count_elem : acts_rtrn_count) {
      // acts_rtrn_count_elem
      Read(&acts_rtrn_count_elem, &buf);
    }

    // pad2
    Align(&buf, 4);

    // acts_rtrn_acts
    acts_rtrn_acts.resize(totalActions);
    for (auto& acts_rtrn_acts_elem : acts_rtrn_acts) {
      // acts_rtrn_acts_elem
      Read(&acts_rtrn_acts_elem, &buf);
    }
  }
  if (CaseAnd(map_expr, Xkb::MapPart::KeyBehaviors)) {
    map.behaviors_rtrn.emplace(decltype(map.behaviors_rtrn)::value_type());
    auto& behaviors_rtrn = *map.behaviors_rtrn;

    // behaviors_rtrn
    behaviors_rtrn.resize(totalKeyBehaviors);
    for (auto& behaviors_rtrn_elem : behaviors_rtrn) {
      // behaviors_rtrn_elem
      {
        auto& keycode = behaviors_rtrn_elem.keycode;
        auto& behavior = behaviors_rtrn_elem.behavior;

        // keycode
        Read(&keycode, &buf);

        // behavior
        Read(&behavior, &buf);

        // pad0
        Pad(&buf, 1);
      }
    }
  }
  if (CaseAnd(map_expr, Xkb::MapPart::VirtualMods)) {
    map.vmods_rtrn.emplace(decltype(map.vmods_rtrn)::value_type());
    auto& vmods_rtrn = *map.vmods_rtrn;

    // vmods_rtrn
    vmods_rtrn.resize(PopCount(virtualMods));
    for (auto& vmods_rtrn_elem : vmods_rtrn) {
      // vmods_rtrn_elem
      uint8_t tmp108;
      Read(&tmp108, &buf);
      vmods_rtrn_elem = static_cast<ModMask>(tmp108);
    }

    // pad3
    Align(&buf, 4);
  }
  if (CaseAnd(map_expr, Xkb::MapPart::ExplicitComponents)) {
    map.explicit_rtrn.emplace(decltype(map.explicit_rtrn)::value_type());
    auto& explicit_rtrn = *map.explicit_rtrn;

    // explicit_rtrn
    explicit_rtrn.resize(totalKeyExplicit);
    for (auto& explicit_rtrn_elem : explicit_rtrn) {
      // explicit_rtrn_elem
      {
        auto& keycode = explicit_rtrn_elem.keycode;
        auto& c_explicit = explicit_rtrn_elem.c_explicit;

        // keycode
        Read(&keycode, &buf);

        // c_explicit
        uint8_t tmp109;
        Read(&tmp109, &buf);
        c_explicit = static_cast<Xkb::Explicit>(tmp109);
      }
    }

    // pad4
    Align(&buf, 4);
  }
  if (CaseAnd(map_expr, Xkb::MapPart::ModifierMap)) {
    map.modmap_rtrn.emplace(decltype(map.modmap_rtrn)::value_type());
    auto& modmap_rtrn = *map.modmap_rtrn;

    // modmap_rtrn
    modmap_rtrn.resize(totalModMapKeys);
    for (auto& modmap_rtrn_elem : modmap_rtrn) {
      // modmap_rtrn_elem
      {
        auto& keycode = modmap_rtrn_elem.keycode;
        auto& mods = modmap_rtrn_elem.mods;

        // keycode
        Read(&keycode, &buf);

        // mods
        uint8_t tmp110;
        Read(&tmp110, &buf);
        mods = static_cast<ModMask>(tmp110);
      }
    }

    // pad5
    Align(&buf, 4);
  }
  if (CaseAnd(map_expr, Xkb::MapPart::VirtualModMap)) {
    map.vmodmap_rtrn.emplace(decltype(map.vmodmap_rtrn)::value_type());
    auto& vmodmap_rtrn = *map.vmodmap_rtrn;

    // vmodmap_rtrn
    vmodmap_rtrn.resize(totalVModMapKeys);
    for (auto& vmodmap_rtrn_elem : vmodmap_rtrn) {
      // vmodmap_rtrn_elem
      {
        auto& keycode = vmodmap_rtrn_elem.keycode;
        auto& vmods = vmodmap_rtrn_elem.vmods;

        // keycode
        Read(&keycode, &buf);

        // pad0
        Pad(&buf, 1);

        // vmods
        uint16_t tmp111;
        Read(&tmp111, &buf);
        vmods = static_cast<Xkb::VMod>(tmp111);
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xkb::SetMap(const Xkb::SetMapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  MapPart present{};
  auto& flags = request.flags;
  auto& minKeyCode = request.minKeyCode;
  auto& maxKeyCode = request.maxKeyCode;
  auto& firstType = request.firstType;
  auto& nTypes = request.nTypes;
  auto& firstKeySym = request.firstKeySym;
  auto& nKeySyms = request.nKeySyms;
  auto& totalSyms = request.totalSyms;
  auto& firstKeyAction = request.firstKeyAction;
  auto& nKeyActions = request.nKeyActions;
  auto& totalActions = request.totalActions;
  auto& firstKeyBehavior = request.firstKeyBehavior;
  auto& nKeyBehaviors = request.nKeyBehaviors;
  auto& totalKeyBehaviors = request.totalKeyBehaviors;
  auto& firstKeyExplicit = request.firstKeyExplicit;
  auto& nKeyExplicit = request.nKeyExplicit;
  auto& totalKeyExplicit = request.totalKeyExplicit;
  auto& firstModMapKey = request.firstModMapKey;
  auto& nModMapKeys = request.nModMapKeys;
  auto& totalModMapKeys = request.totalModMapKeys;
  auto& firstVModMapKey = request.firstVModMapKey;
  auto& nVModMapKeys = request.nVModMapKeys;
  auto& totalVModMapKeys = request.totalVModMapKeys;
  auto& virtualMods = request.virtualMods;
  auto& values = request;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 9;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // present
  SwitchVar(MapPart::KeyTypes, values.types.has_value(), true, &present);
  SwitchVar(MapPart::KeySyms, values.syms.has_value(), true, &present);
  SwitchVar(MapPart::KeyActions, values.actionsCount.has_value(), true,
            &present);
  SwitchVar(MapPart::KeyBehaviors, values.behaviors.has_value(), true,
            &present);
  SwitchVar(MapPart::VirtualMods, values.vmods.has_value(), true, &present);
  SwitchVar(MapPart::ExplicitComponents, values.c_explicit.has_value(), true,
            &present);
  SwitchVar(MapPart::ModifierMap, values.modmap.has_value(), true, &present);
  SwitchVar(MapPart::VirtualModMap, values.vmodmap.has_value(), true, &present);
  uint16_t tmp112;
  tmp112 = static_cast<uint16_t>(present);
  buf.Write(&tmp112);

  // flags
  uint16_t tmp113;
  tmp113 = static_cast<uint16_t>(flags);
  buf.Write(&tmp113);

  // minKeyCode
  buf.Write(&minKeyCode);

  // maxKeyCode
  buf.Write(&maxKeyCode);

  // firstType
  buf.Write(&firstType);

  // nTypes
  buf.Write(&nTypes);

  // firstKeySym
  buf.Write(&firstKeySym);

  // nKeySyms
  buf.Write(&nKeySyms);

  // totalSyms
  buf.Write(&totalSyms);

  // firstKeyAction
  buf.Write(&firstKeyAction);

  // nKeyActions
  buf.Write(&nKeyActions);

  // totalActions
  buf.Write(&totalActions);

  // firstKeyBehavior
  buf.Write(&firstKeyBehavior);

  // nKeyBehaviors
  buf.Write(&nKeyBehaviors);

  // totalKeyBehaviors
  buf.Write(&totalKeyBehaviors);

  // firstKeyExplicit
  buf.Write(&firstKeyExplicit);

  // nKeyExplicit
  buf.Write(&nKeyExplicit);

  // totalKeyExplicit
  buf.Write(&totalKeyExplicit);

  // firstModMapKey
  buf.Write(&firstModMapKey);

  // nModMapKeys
  buf.Write(&nModMapKeys);

  // totalModMapKeys
  buf.Write(&totalModMapKeys);

  // firstVModMapKey
  buf.Write(&firstVModMapKey);

  // nVModMapKeys
  buf.Write(&nVModMapKeys);

  // totalVModMapKeys
  buf.Write(&totalVModMapKeys);

  // virtualMods
  uint16_t tmp114;
  tmp114 = static_cast<uint16_t>(virtualMods);
  buf.Write(&tmp114);

  // values
  auto values_expr = present;
  if (CaseAnd(values_expr, MapPart::KeyTypes)) {
    auto& types = *values.types;

    // types
    CHECK_EQ(static_cast<size_t>(nTypes), types.size());
    for (auto& types_elem : types) {
      // types_elem
      {
        auto& mask = types_elem.mask;
        auto& realMods = types_elem.realMods;
        auto& virtualMods = types_elem.virtualMods;
        auto& numLevels = types_elem.numLevels;
        uint8_t nMapEntries{};
        auto& preserve = types_elem.preserve;
        auto& entries = types_elem.entries;
        auto& preserve_entries = types_elem.preserve_entries;

        // mask
        uint8_t tmp115;
        tmp115 = static_cast<uint8_t>(mask);
        buf.Write(&tmp115);

        // realMods
        uint8_t tmp116;
        tmp116 = static_cast<uint8_t>(realMods);
        buf.Write(&tmp116);

        // virtualMods
        uint16_t tmp117;
        tmp117 = static_cast<uint16_t>(virtualMods);
        buf.Write(&tmp117);

        // numLevels
        buf.Write(&numLevels);

        // nMapEntries
        nMapEntries = entries.size();
        buf.Write(&nMapEntries);

        // preserve
        buf.Write(&preserve);

        // pad0
        Pad(&buf, 1);

        // entries
        CHECK_EQ(static_cast<size_t>(nMapEntries), entries.size());
        for (auto& entries_elem : entries) {
          // entries_elem
          {
            auto& level = entries_elem.level;
            auto& realMods = entries_elem.realMods;
            auto& virtualMods = entries_elem.virtualMods;

            // level
            buf.Write(&level);

            // realMods
            uint8_t tmp118;
            tmp118 = static_cast<uint8_t>(realMods);
            buf.Write(&tmp118);

            // virtualMods
            uint16_t tmp119;
            tmp119 = static_cast<uint16_t>(virtualMods);
            buf.Write(&tmp119);
          }
        }

        // preserve_entries
        CHECK_EQ(static_cast<size_t>((preserve) * (nMapEntries)),
                 preserve_entries.size());
        for (auto& preserve_entries_elem : preserve_entries) {
          // preserve_entries_elem
          {
            auto& level = preserve_entries_elem.level;
            auto& realMods = preserve_entries_elem.realMods;
            auto& virtualMods = preserve_entries_elem.virtualMods;

            // level
            buf.Write(&level);

            // realMods
            uint8_t tmp120;
            tmp120 = static_cast<uint8_t>(realMods);
            buf.Write(&tmp120);

            // virtualMods
            uint16_t tmp121;
            tmp121 = static_cast<uint16_t>(virtualMods);
            buf.Write(&tmp121);
          }
        }
      }
    }
  }
  if (CaseAnd(values_expr, MapPart::KeySyms)) {
    auto& syms = *values.syms;

    // syms
    CHECK_EQ(static_cast<size_t>(nKeySyms), syms.size());
    for (auto& syms_elem : syms) {
      // syms_elem
      {
        auto& kt_index = syms_elem.kt_index;
        auto& groupInfo = syms_elem.groupInfo;
        auto& width = syms_elem.width;
        uint16_t nSyms{};
        auto& syms = syms_elem.syms;

        // kt_index
        for (auto& kt_index_elem : kt_index) {
          // kt_index_elem
          buf.Write(&kt_index_elem);
        }

        // groupInfo
        buf.Write(&groupInfo);

        // width
        buf.Write(&width);

        // nSyms
        nSyms = syms.size();
        buf.Write(&nSyms);

        // syms
        CHECK_EQ(static_cast<size_t>(nSyms), syms.size());
        for (auto& syms_elem : syms) {
          // syms_elem
          buf.Write(&syms_elem);
        }
      }
    }
  }
  if (CaseAnd(values_expr, MapPart::KeyActions)) {
    auto& actionsCount = *values.actionsCount;
    auto& actions = *values.actions;

    // actionsCount
    CHECK_EQ(static_cast<size_t>(nKeyActions), actionsCount.size());
    for (auto& actionsCount_elem : actionsCount) {
      // actionsCount_elem
      buf.Write(&actionsCount_elem);
    }

    // pad0
    Align(&buf, 4);

    // actions
    CHECK_EQ(static_cast<size_t>(totalActions), actions.size());
    for (auto& actions_elem : actions) {
      // actions_elem
      buf.Write(&actions_elem);
    }
  }
  if (CaseAnd(values_expr, MapPart::KeyBehaviors)) {
    auto& behaviors = *values.behaviors;

    // behaviors
    CHECK_EQ(static_cast<size_t>(totalKeyBehaviors), behaviors.size());
    for (auto& behaviors_elem : behaviors) {
      // behaviors_elem
      {
        auto& keycode = behaviors_elem.keycode;
        auto& behavior = behaviors_elem.behavior;

        // keycode
        buf.Write(&keycode);

        // behavior
        buf.Write(&behavior);

        // pad0
        Pad(&buf, 1);
      }
    }
  }
  if (CaseAnd(values_expr, MapPart::VirtualMods)) {
    auto& vmods = *values.vmods;

    // vmods
    CHECK_EQ(static_cast<size_t>(PopCount(virtualMods)), vmods.size());
    for (auto& vmods_elem : vmods) {
      // vmods_elem
      buf.Write(&vmods_elem);
    }

    // pad1
    Align(&buf, 4);
  }
  if (CaseAnd(values_expr, MapPart::ExplicitComponents)) {
    auto& c_explicit = *values.c_explicit;

    // c_explicit
    CHECK_EQ(static_cast<size_t>(totalKeyExplicit), c_explicit.size());
    for (auto& c_explicit_elem : c_explicit) {
      // c_explicit_elem
      {
        auto& keycode = c_explicit_elem.keycode;
        auto& c_explicit = c_explicit_elem.c_explicit;

        // keycode
        buf.Write(&keycode);

        // c_explicit
        uint8_t tmp122;
        tmp122 = static_cast<uint8_t>(c_explicit);
        buf.Write(&tmp122);
      }
    }
  }
  if (CaseAnd(values_expr, MapPart::ModifierMap)) {
    auto& modmap = *values.modmap;

    // modmap
    CHECK_EQ(static_cast<size_t>(totalModMapKeys), modmap.size());
    for (auto& modmap_elem : modmap) {
      // modmap_elem
      {
        auto& keycode = modmap_elem.keycode;
        auto& mods = modmap_elem.mods;

        // keycode
        buf.Write(&keycode);

        // mods
        uint8_t tmp123;
        tmp123 = static_cast<uint8_t>(mods);
        buf.Write(&tmp123);
      }
    }
  }
  if (CaseAnd(values_expr, MapPart::VirtualModMap)) {
    auto& vmodmap = *values.vmodmap;

    // vmodmap
    CHECK_EQ(static_cast<size_t>(totalVModMapKeys), vmodmap.size());
    for (auto& vmodmap_elem : vmodmap) {
      // vmodmap_elem
      {
        auto& keycode = vmodmap_elem.keycode;
        auto& vmods = vmodmap_elem.vmods;

        // keycode
        buf.Write(&keycode);

        // pad0
        Pad(&buf, 1);

        // vmods
        uint16_t tmp124;
        tmp124 = static_cast<uint16_t>(vmods);
        buf.Write(&tmp124);
      }
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xkb::SetMap", false);
}

Future<void> Xkb::SetMap(
    const DeviceSpec& deviceSpec,
    const SetMapFlags& flags,
    const KeyCode& minKeyCode,
    const KeyCode& maxKeyCode,
    const uint8_t& firstType,
    const uint8_t& nTypes,
    const KeyCode& firstKeySym,
    const uint8_t& nKeySyms,
    const uint16_t& totalSyms,
    const KeyCode& firstKeyAction,
    const uint8_t& nKeyActions,
    const uint16_t& totalActions,
    const KeyCode& firstKeyBehavior,
    const uint8_t& nKeyBehaviors,
    const uint8_t& totalKeyBehaviors,
    const KeyCode& firstKeyExplicit,
    const uint8_t& nKeyExplicit,
    const uint8_t& totalKeyExplicit,
    const KeyCode& firstModMapKey,
    const uint8_t& nModMapKeys,
    const uint8_t& totalModMapKeys,
    const KeyCode& firstVModMapKey,
    const uint8_t& nVModMapKeys,
    const uint8_t& totalVModMapKeys,
    const VMod& virtualMods,
    const std::optional<std::vector<SetKeyType>>& types,
    const std::optional<std::vector<KeySymMap>>& syms,
    const std::optional<std::vector<uint8_t>>& actionsCount,
    const std::optional<std::vector<Action>>& actions,
    const std::optional<std::vector<SetBehavior>>& behaviors,
    const std::optional<std::vector<uint8_t>>& vmods,
    const std::optional<std::vector<SetExplicit>>& c_explicit,
    const std::optional<std::vector<KeyModMap>>& modmap,
    const std::optional<std::vector<KeyVModMap>>& vmodmap) {
  return Xkb::SetMap(Xkb::SetMapRequest{deviceSpec,
                                        flags,
                                        minKeyCode,
                                        maxKeyCode,
                                        firstType,
                                        nTypes,
                                        firstKeySym,
                                        nKeySyms,
                                        totalSyms,
                                        firstKeyAction,
                                        nKeyActions,
                                        totalActions,
                                        firstKeyBehavior,
                                        nKeyBehaviors,
                                        totalKeyBehaviors,
                                        firstKeyExplicit,
                                        nKeyExplicit,
                                        totalKeyExplicit,
                                        firstModMapKey,
                                        nModMapKeys,
                                        totalModMapKeys,
                                        firstVModMapKey,
                                        nVModMapKeys,
                                        totalVModMapKeys,
                                        virtualMods,
                                        types,
                                        syms,
                                        actionsCount,
                                        actions,
                                        behaviors,
                                        vmods,
                                        c_explicit,
                                        modmap,
                                        vmodmap});
}

Future<Xkb::GetCompatMapReply> Xkb::GetCompatMap(
    const Xkb::GetCompatMapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& groups = request.groups;
  auto& getAllSI = request.getAllSI;
  auto& firstSI = request.firstSI;
  auto& nSI = request.nSI;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // groups
  uint8_t tmp125;
  tmp125 = static_cast<uint8_t>(groups);
  buf.Write(&tmp125);

  // getAllSI
  buf.Write(&getAllSI);

  // firstSI
  buf.Write(&firstSI);

  // nSI
  buf.Write(&nSI);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::GetCompatMapReply>(
      &buf, "Xkb::GetCompatMap", false);
}

Future<Xkb::GetCompatMapReply> Xkb::GetCompatMap(const DeviceSpec& deviceSpec,
                                                 const SetOfGroup& groups,
                                                 const uint8_t& getAllSI,
                                                 const uint16_t& firstSI,
                                                 const uint16_t& nSI) {
  return Xkb::GetCompatMap(
      Xkb::GetCompatMapRequest{deviceSpec, groups, getAllSI, firstSI, nSI});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::GetCompatMapReply> detail::ReadReply<
    Xkb::GetCompatMapReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::GetCompatMapReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  auto& groupsRtrn = (*reply).groupsRtrn;
  auto& firstSIRtrn = (*reply).firstSIRtrn;
  uint16_t nSIRtrn{};
  auto& nTotalSI = (*reply).nTotalSI;
  auto& si_rtrn = (*reply).si_rtrn;
  auto& group_rtrn = (*reply).group_rtrn;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // groupsRtrn
  uint8_t tmp126;
  Read(&tmp126, &buf);
  groupsRtrn = static_cast<Xkb::SetOfGroup>(tmp126);

  // pad0
  Pad(&buf, 1);

  // firstSIRtrn
  Read(&firstSIRtrn, &buf);

  // nSIRtrn
  Read(&nSIRtrn, &buf);

  // nTotalSI
  Read(&nTotalSI, &buf);

  // pad1
  Pad(&buf, 16);

  // si_rtrn
  si_rtrn.resize(nSIRtrn);
  for (auto& si_rtrn_elem : si_rtrn) {
    // si_rtrn_elem
    {
      auto& sym = si_rtrn_elem.sym;
      auto& mods = si_rtrn_elem.mods;
      auto& match = si_rtrn_elem.match;
      auto& virtualMod = si_rtrn_elem.virtualMod;
      auto& flags = si_rtrn_elem.flags;
      auto& action = si_rtrn_elem.action;

      // sym
      Read(&sym, &buf);

      // mods
      uint8_t tmp127;
      Read(&tmp127, &buf);
      mods = static_cast<ModMask>(tmp127);

      // match
      Read(&match, &buf);

      // virtualMod
      uint8_t tmp128;
      Read(&tmp128, &buf);
      virtualMod = static_cast<Xkb::VModsLow>(tmp128);

      // flags
      Read(&flags, &buf);

      // action
      {
        auto& type = action.type;
        auto& data = action.data;

        // type
        uint8_t tmp129;
        Read(&tmp129, &buf);
        type = static_cast<Xkb::SAType>(tmp129);

        // data
        for (auto& data_elem : data) {
          // data_elem
          Read(&data_elem, &buf);
        }
      }
    }
  }

  // group_rtrn
  group_rtrn.resize(PopCount(groupsRtrn));
  for (auto& group_rtrn_elem : group_rtrn) {
    // group_rtrn_elem
    {
      auto& mask = group_rtrn_elem.mask;
      auto& realMods = group_rtrn_elem.realMods;
      auto& vmods = group_rtrn_elem.vmods;

      // mask
      uint8_t tmp130;
      Read(&tmp130, &buf);
      mask = static_cast<ModMask>(tmp130);

      // realMods
      uint8_t tmp131;
      Read(&tmp131, &buf);
      realMods = static_cast<ModMask>(tmp131);

      // vmods
      uint16_t tmp132;
      Read(&tmp132, &buf);
      vmods = static_cast<Xkb::VMod>(tmp132);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xkb::SetCompatMap(const Xkb::SetCompatMapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& recomputeActions = request.recomputeActions;
  auto& truncateSI = request.truncateSI;
  auto& groups = request.groups;
  auto& firstSI = request.firstSI;
  uint16_t nSI{};
  auto& si = request.si;
  size_t si_len = si.size();
  auto& groupMaps = request.groupMaps;
  size_t groupMaps_len = groupMaps.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // pad0
  Pad(&buf, 1);

  // recomputeActions
  buf.Write(&recomputeActions);

  // truncateSI
  buf.Write(&truncateSI);

  // groups
  uint8_t tmp133;
  tmp133 = static_cast<uint8_t>(groups);
  buf.Write(&tmp133);

  // firstSI
  buf.Write(&firstSI);

  // nSI
  nSI = si.size();
  buf.Write(&nSI);

  // pad1
  Pad(&buf, 2);

  // si
  CHECK_EQ(static_cast<size_t>(nSI), si.size());
  for (auto& si_elem : si) {
    // si_elem
    {
      auto& sym = si_elem.sym;
      auto& mods = si_elem.mods;
      auto& match = si_elem.match;
      auto& virtualMod = si_elem.virtualMod;
      auto& flags = si_elem.flags;
      auto& action = si_elem.action;

      // sym
      buf.Write(&sym);

      // mods
      uint8_t tmp134;
      tmp134 = static_cast<uint8_t>(mods);
      buf.Write(&tmp134);

      // match
      buf.Write(&match);

      // virtualMod
      uint8_t tmp135;
      tmp135 = static_cast<uint8_t>(virtualMod);
      buf.Write(&tmp135);

      // flags
      buf.Write(&flags);

      // action
      {
        auto& type = action.type;
        auto& data = action.data;

        // type
        uint8_t tmp136;
        tmp136 = static_cast<uint8_t>(type);
        buf.Write(&tmp136);

        // data
        for (auto& data_elem : data) {
          // data_elem
          buf.Write(&data_elem);
        }
      }
    }
  }

  // groupMaps
  CHECK_EQ(static_cast<size_t>(PopCount(groups)), groupMaps.size());
  for (auto& groupMaps_elem : groupMaps) {
    // groupMaps_elem
    {
      auto& mask = groupMaps_elem.mask;
      auto& realMods = groupMaps_elem.realMods;
      auto& vmods = groupMaps_elem.vmods;

      // mask
      uint8_t tmp137;
      tmp137 = static_cast<uint8_t>(mask);
      buf.Write(&tmp137);

      // realMods
      uint8_t tmp138;
      tmp138 = static_cast<uint8_t>(realMods);
      buf.Write(&tmp138);

      // vmods
      uint16_t tmp139;
      tmp139 = static_cast<uint16_t>(vmods);
      buf.Write(&tmp139);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xkb::SetCompatMap", false);
}

Future<void> Xkb::SetCompatMap(const DeviceSpec& deviceSpec,
                               const uint8_t& recomputeActions,
                               const uint8_t& truncateSI,
                               const SetOfGroup& groups,
                               const uint16_t& firstSI,
                               const std::vector<SymInterpret>& si,
                               const std::vector<ModDef>& groupMaps) {
  return Xkb::SetCompatMap(
      Xkb::SetCompatMapRequest{deviceSpec, recomputeActions, truncateSI, groups,
                               firstSI, si, groupMaps});
}

Future<Xkb::GetIndicatorStateReply> Xkb::GetIndicatorState(
    const Xkb::GetIndicatorStateRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // pad0
  Pad(&buf, 2);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::GetIndicatorStateReply>(
      &buf, "Xkb::GetIndicatorState", false);
}

Future<Xkb::GetIndicatorStateReply> Xkb::GetIndicatorState(
    const DeviceSpec& deviceSpec) {
  return Xkb::GetIndicatorState(Xkb::GetIndicatorStateRequest{deviceSpec});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::GetIndicatorStateReply> detail::ReadReply<
    Xkb::GetIndicatorStateReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::GetIndicatorStateReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  auto& state = (*reply).state;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // state
  Read(&state, &buf);

  // pad0
  Pad(&buf, 20);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xkb::GetIndicatorMapReply> Xkb::GetIndicatorMap(
    const Xkb::GetIndicatorMapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& which = request.which;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 13;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // pad0
  Pad(&buf, 2);

  // which
  buf.Write(&which);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::GetIndicatorMapReply>(
      &buf, "Xkb::GetIndicatorMap", false);
}

Future<Xkb::GetIndicatorMapReply> Xkb::GetIndicatorMap(
    const DeviceSpec& deviceSpec,
    const uint32_t& which) {
  return Xkb::GetIndicatorMap(Xkb::GetIndicatorMapRequest{deviceSpec, which});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::GetIndicatorMapReply> detail::ReadReply<
    Xkb::GetIndicatorMapReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::GetIndicatorMapReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  auto& which = (*reply).which;
  auto& realIndicators = (*reply).realIndicators;
  auto& nIndicators = (*reply).nIndicators;
  auto& maps = (*reply).maps;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // which
  Read(&which, &buf);

  // realIndicators
  Read(&realIndicators, &buf);

  // nIndicators
  Read(&nIndicators, &buf);

  // pad0
  Pad(&buf, 15);

  // maps
  maps.resize(PopCount(which));
  for (auto& maps_elem : maps) {
    // maps_elem
    {
      auto& flags = maps_elem.flags;
      auto& whichGroups = maps_elem.whichGroups;
      auto& groups = maps_elem.groups;
      auto& whichMods = maps_elem.whichMods;
      auto& mods = maps_elem.mods;
      auto& realMods = maps_elem.realMods;
      auto& vmods = maps_elem.vmods;
      auto& ctrls = maps_elem.ctrls;

      // flags
      uint8_t tmp140;
      Read(&tmp140, &buf);
      flags = static_cast<Xkb::IMFlag>(tmp140);

      // whichGroups
      uint8_t tmp141;
      Read(&tmp141, &buf);
      whichGroups = static_cast<Xkb::IMGroupsWhich>(tmp141);

      // groups
      uint8_t tmp142;
      Read(&tmp142, &buf);
      groups = static_cast<Xkb::SetOfGroup>(tmp142);

      // whichMods
      uint8_t tmp143;
      Read(&tmp143, &buf);
      whichMods = static_cast<Xkb::IMModsWhich>(tmp143);

      // mods
      uint8_t tmp144;
      Read(&tmp144, &buf);
      mods = static_cast<ModMask>(tmp144);

      // realMods
      uint8_t tmp145;
      Read(&tmp145, &buf);
      realMods = static_cast<ModMask>(tmp145);

      // vmods
      uint16_t tmp146;
      Read(&tmp146, &buf);
      vmods = static_cast<Xkb::VMod>(tmp146);

      // ctrls
      uint32_t tmp147;
      Read(&tmp147, &buf);
      ctrls = static_cast<Xkb::BoolCtrl>(tmp147);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xkb::SetIndicatorMap(const Xkb::SetIndicatorMapRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& which = request.which;
  auto& maps = request.maps;
  size_t maps_len = maps.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 14;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // pad0
  Pad(&buf, 2);

  // which
  buf.Write(&which);

  // maps
  CHECK_EQ(static_cast<size_t>(PopCount(which)), maps.size());
  for (auto& maps_elem : maps) {
    // maps_elem
    {
      auto& flags = maps_elem.flags;
      auto& whichGroups = maps_elem.whichGroups;
      auto& groups = maps_elem.groups;
      auto& whichMods = maps_elem.whichMods;
      auto& mods = maps_elem.mods;
      auto& realMods = maps_elem.realMods;
      auto& vmods = maps_elem.vmods;
      auto& ctrls = maps_elem.ctrls;

      // flags
      uint8_t tmp148;
      tmp148 = static_cast<uint8_t>(flags);
      buf.Write(&tmp148);

      // whichGroups
      uint8_t tmp149;
      tmp149 = static_cast<uint8_t>(whichGroups);
      buf.Write(&tmp149);

      // groups
      uint8_t tmp150;
      tmp150 = static_cast<uint8_t>(groups);
      buf.Write(&tmp150);

      // whichMods
      uint8_t tmp151;
      tmp151 = static_cast<uint8_t>(whichMods);
      buf.Write(&tmp151);

      // mods
      uint8_t tmp152;
      tmp152 = static_cast<uint8_t>(mods);
      buf.Write(&tmp152);

      // realMods
      uint8_t tmp153;
      tmp153 = static_cast<uint8_t>(realMods);
      buf.Write(&tmp153);

      // vmods
      uint16_t tmp154;
      tmp154 = static_cast<uint16_t>(vmods);
      buf.Write(&tmp154);

      // ctrls
      uint32_t tmp155;
      tmp155 = static_cast<uint32_t>(ctrls);
      buf.Write(&tmp155);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xkb::SetIndicatorMap", false);
}

Future<void> Xkb::SetIndicatorMap(const DeviceSpec& deviceSpec,
                                  const uint32_t& which,
                                  const std::vector<IndicatorMap>& maps) {
  return Xkb::SetIndicatorMap(
      Xkb::SetIndicatorMapRequest{deviceSpec, which, maps});
}

Future<Xkb::GetNamedIndicatorReply> Xkb::GetNamedIndicator(
    const Xkb::GetNamedIndicatorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& ledClass = request.ledClass;
  auto& ledID = request.ledID;
  auto& indicator = request.indicator;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 15;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // ledClass
  uint16_t tmp156;
  tmp156 = static_cast<uint16_t>(ledClass);
  buf.Write(&tmp156);

  // ledID
  buf.Write(&ledID);

  // pad0
  Pad(&buf, 2);

  // indicator
  buf.Write(&indicator);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::GetNamedIndicatorReply>(
      &buf, "Xkb::GetNamedIndicator", false);
}

Future<Xkb::GetNamedIndicatorReply> Xkb::GetNamedIndicator(
    const DeviceSpec& deviceSpec,
    const LedClass& ledClass,
    const IDSpec& ledID,
    const Atom& indicator) {
  return Xkb::GetNamedIndicator(
      Xkb::GetNamedIndicatorRequest{deviceSpec, ledClass, ledID, indicator});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::GetNamedIndicatorReply> detail::ReadReply<
    Xkb::GetNamedIndicatorReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::GetNamedIndicatorReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  auto& indicator = (*reply).indicator;
  auto& found = (*reply).found;
  auto& on = (*reply).on;
  auto& realIndicator = (*reply).realIndicator;
  auto& ndx = (*reply).ndx;
  auto& map_flags = (*reply).map_flags;
  auto& map_whichGroups = (*reply).map_whichGroups;
  auto& map_groups = (*reply).map_groups;
  auto& map_whichMods = (*reply).map_whichMods;
  auto& map_mods = (*reply).map_mods;
  auto& map_realMods = (*reply).map_realMods;
  auto& map_vmod = (*reply).map_vmod;
  auto& map_ctrls = (*reply).map_ctrls;
  auto& supported = (*reply).supported;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // indicator
  Read(&indicator, &buf);

  // found
  Read(&found, &buf);

  // on
  Read(&on, &buf);

  // realIndicator
  Read(&realIndicator, &buf);

  // ndx
  Read(&ndx, &buf);

  // map_flags
  uint8_t tmp157;
  Read(&tmp157, &buf);
  map_flags = static_cast<Xkb::IMFlag>(tmp157);

  // map_whichGroups
  uint8_t tmp158;
  Read(&tmp158, &buf);
  map_whichGroups = static_cast<Xkb::IMGroupsWhich>(tmp158);

  // map_groups
  uint8_t tmp159;
  Read(&tmp159, &buf);
  map_groups = static_cast<Xkb::SetOfGroups>(tmp159);

  // map_whichMods
  uint8_t tmp160;
  Read(&tmp160, &buf);
  map_whichMods = static_cast<Xkb::IMModsWhich>(tmp160);

  // map_mods
  uint8_t tmp161;
  Read(&tmp161, &buf);
  map_mods = static_cast<ModMask>(tmp161);

  // map_realMods
  uint8_t tmp162;
  Read(&tmp162, &buf);
  map_realMods = static_cast<ModMask>(tmp162);

  // map_vmod
  uint16_t tmp163;
  Read(&tmp163, &buf);
  map_vmod = static_cast<Xkb::VMod>(tmp163);

  // map_ctrls
  uint32_t tmp164;
  Read(&tmp164, &buf);
  map_ctrls = static_cast<Xkb::BoolCtrl>(tmp164);

  // supported
  Read(&supported, &buf);

  // pad0
  Pad(&buf, 3);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xkb::SetNamedIndicator(
    const Xkb::SetNamedIndicatorRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& ledClass = request.ledClass;
  auto& ledID = request.ledID;
  auto& indicator = request.indicator;
  auto& setState = request.setState;
  auto& on = request.on;
  auto& setMap = request.setMap;
  auto& createMap = request.createMap;
  auto& map_flags = request.map_flags;
  auto& map_whichGroups = request.map_whichGroups;
  auto& map_groups = request.map_groups;
  auto& map_whichMods = request.map_whichMods;
  auto& map_realMods = request.map_realMods;
  auto& map_vmods = request.map_vmods;
  auto& map_ctrls = request.map_ctrls;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 16;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // ledClass
  uint16_t tmp165;
  tmp165 = static_cast<uint16_t>(ledClass);
  buf.Write(&tmp165);

  // ledID
  buf.Write(&ledID);

  // pad0
  Pad(&buf, 2);

  // indicator
  buf.Write(&indicator);

  // setState
  buf.Write(&setState);

  // on
  buf.Write(&on);

  // setMap
  buf.Write(&setMap);

  // createMap
  buf.Write(&createMap);

  // pad1
  Pad(&buf, 1);

  // map_flags
  uint8_t tmp166;
  tmp166 = static_cast<uint8_t>(map_flags);
  buf.Write(&tmp166);

  // map_whichGroups
  uint8_t tmp167;
  tmp167 = static_cast<uint8_t>(map_whichGroups);
  buf.Write(&tmp167);

  // map_groups
  uint8_t tmp168;
  tmp168 = static_cast<uint8_t>(map_groups);
  buf.Write(&tmp168);

  // map_whichMods
  uint8_t tmp169;
  tmp169 = static_cast<uint8_t>(map_whichMods);
  buf.Write(&tmp169);

  // map_realMods
  uint8_t tmp170;
  tmp170 = static_cast<uint8_t>(map_realMods);
  buf.Write(&tmp170);

  // map_vmods
  uint16_t tmp171;
  tmp171 = static_cast<uint16_t>(map_vmods);
  buf.Write(&tmp171);

  // map_ctrls
  uint32_t tmp172;
  tmp172 = static_cast<uint32_t>(map_ctrls);
  buf.Write(&tmp172);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xkb::SetNamedIndicator", false);
}

Future<void> Xkb::SetNamedIndicator(const DeviceSpec& deviceSpec,
                                    const LedClass& ledClass,
                                    const IDSpec& ledID,
                                    const Atom& indicator,
                                    const uint8_t& setState,
                                    const uint8_t& on,
                                    const uint8_t& setMap,
                                    const uint8_t& createMap,
                                    const IMFlag& map_flags,
                                    const IMGroupsWhich& map_whichGroups,
                                    const SetOfGroups& map_groups,
                                    const IMModsWhich& map_whichMods,
                                    const ModMask& map_realMods,
                                    const VMod& map_vmods,
                                    const BoolCtrl& map_ctrls) {
  return Xkb::SetNamedIndicator(Xkb::SetNamedIndicatorRequest{
      deviceSpec, ledClass, ledID, indicator, setState, on, setMap, createMap,
      map_flags, map_whichGroups, map_groups, map_whichMods, map_realMods,
      map_vmods, map_ctrls});
}

Future<Xkb::GetNamesReply> Xkb::GetNames(const Xkb::GetNamesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& which = request.which;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 17;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // pad0
  Pad(&buf, 2);

  // which
  uint32_t tmp173;
  tmp173 = static_cast<uint32_t>(which);
  buf.Write(&tmp173);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::GetNamesReply>(&buf, "Xkb::GetNames",
                                                      false);
}

Future<Xkb::GetNamesReply> Xkb::GetNames(const DeviceSpec& deviceSpec,
                                         const NameDetail& which) {
  return Xkb::GetNames(Xkb::GetNamesRequest{deviceSpec, which});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::GetNamesReply> detail::ReadReply<Xkb::GetNamesReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::GetNamesReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  Xkb::NameDetail which{};
  auto& minKeyCode = (*reply).minKeyCode;
  auto& maxKeyCode = (*reply).maxKeyCode;
  auto& nTypes = (*reply).nTypes;
  auto& groupNames = (*reply).groupNames;
  auto& virtualMods = (*reply).virtualMods;
  auto& firstKey = (*reply).firstKey;
  auto& nKeys = (*reply).nKeys;
  auto& indicators = (*reply).indicators;
  auto& nRadioGroups = (*reply).nRadioGroups;
  auto& nKeyAliases = (*reply).nKeyAliases;
  auto& nKTLevels = (*reply).nKTLevels;
  auto& valueList = (*reply);

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // which
  uint32_t tmp174;
  Read(&tmp174, &buf);
  which = static_cast<Xkb::NameDetail>(tmp174);

  // minKeyCode
  Read(&minKeyCode, &buf);

  // maxKeyCode
  Read(&maxKeyCode, &buf);

  // nTypes
  Read(&nTypes, &buf);

  // groupNames
  uint8_t tmp175;
  Read(&tmp175, &buf);
  groupNames = static_cast<Xkb::SetOfGroup>(tmp175);

  // virtualMods
  uint16_t tmp176;
  Read(&tmp176, &buf);
  virtualMods = static_cast<Xkb::VMod>(tmp176);

  // firstKey
  Read(&firstKey, &buf);

  // nKeys
  Read(&nKeys, &buf);

  // indicators
  Read(&indicators, &buf);

  // nRadioGroups
  Read(&nRadioGroups, &buf);

  // nKeyAliases
  Read(&nKeyAliases, &buf);

  // nKTLevels
  Read(&nKTLevels, &buf);

  // pad0
  Pad(&buf, 4);

  // valueList
  auto valueList_expr = which;
  if (CaseAnd(valueList_expr, Xkb::NameDetail::Keycodes)) {
    valueList.keycodesName.emplace(
        decltype(valueList.keycodesName)::value_type());
    auto& keycodesName = *valueList.keycodesName;

    // keycodesName
    Read(&keycodesName, &buf);
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::Geometry)) {
    valueList.geometryName.emplace(
        decltype(valueList.geometryName)::value_type());
    auto& geometryName = *valueList.geometryName;

    // geometryName
    Read(&geometryName, &buf);
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::Symbols)) {
    valueList.symbolsName.emplace(
        decltype(valueList.symbolsName)::value_type());
    auto& symbolsName = *valueList.symbolsName;

    // symbolsName
    Read(&symbolsName, &buf);
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::PhysSymbols)) {
    valueList.physSymbolsName.emplace(
        decltype(valueList.physSymbolsName)::value_type());
    auto& physSymbolsName = *valueList.physSymbolsName;

    // physSymbolsName
    Read(&physSymbolsName, &buf);
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::Types)) {
    valueList.typesName.emplace(decltype(valueList.typesName)::value_type());
    auto& typesName = *valueList.typesName;

    // typesName
    Read(&typesName, &buf);
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::Compat)) {
    valueList.compatName.emplace(decltype(valueList.compatName)::value_type());
    auto& compatName = *valueList.compatName;

    // compatName
    Read(&compatName, &buf);
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::KeyTypeNames)) {
    valueList.typeNames.emplace(decltype(valueList.typeNames)::value_type());
    auto& typeNames = *valueList.typeNames;

    // typeNames
    typeNames.resize(nTypes);
    for (auto& typeNames_elem : typeNames) {
      // typeNames_elem
      Read(&typeNames_elem, &buf);
    }
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::KTLevelNames)) {
    valueList.nLevelsPerType.emplace(
        decltype(valueList.nLevelsPerType)::value_type());
    valueList.ktLevelNames.emplace(
        decltype(valueList.ktLevelNames)::value_type());
    auto& nLevelsPerType = *valueList.nLevelsPerType;
    auto& ktLevelNames = *valueList.ktLevelNames;

    // nLevelsPerType
    nLevelsPerType.resize(nTypes);
    for (auto& nLevelsPerType_elem : nLevelsPerType) {
      // nLevelsPerType_elem
      Read(&nLevelsPerType_elem, &buf);
    }

    // pad1
    Align(&buf, 4);

    // ktLevelNames
    auto sum177_ =
        SumOf([](auto& listelem_ref) { return listelem_ref; }, nLevelsPerType);
    ktLevelNames.resize(sum177_);
    for (auto& ktLevelNames_elem : ktLevelNames) {
      // ktLevelNames_elem
      Read(&ktLevelNames_elem, &buf);
    }
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::IndicatorNames)) {
    valueList.indicatorNames.emplace(
        decltype(valueList.indicatorNames)::value_type());
    auto& indicatorNames = *valueList.indicatorNames;

    // indicatorNames
    indicatorNames.resize(PopCount(indicators));
    for (auto& indicatorNames_elem : indicatorNames) {
      // indicatorNames_elem
      Read(&indicatorNames_elem, &buf);
    }
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::VirtualModNames)) {
    valueList.virtualModNames.emplace(
        decltype(valueList.virtualModNames)::value_type());
    auto& virtualModNames = *valueList.virtualModNames;

    // virtualModNames
    virtualModNames.resize(PopCount(virtualMods));
    for (auto& virtualModNames_elem : virtualModNames) {
      // virtualModNames_elem
      Read(&virtualModNames_elem, &buf);
    }
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::GroupNames)) {
    valueList.groups.emplace(decltype(valueList.groups)::value_type());
    auto& groups = *valueList.groups;

    // groups
    groups.resize(PopCount(groupNames));
    for (auto& groups_elem : groups) {
      // groups_elem
      Read(&groups_elem, &buf);
    }
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::KeyNames)) {
    valueList.keyNames.emplace(decltype(valueList.keyNames)::value_type());
    auto& keyNames = *valueList.keyNames;

    // keyNames
    keyNames.resize(nKeys);
    for (auto& keyNames_elem : keyNames) {
      // keyNames_elem
      {
        auto& name = keyNames_elem.name;

        // name
        for (auto& name_elem : name) {
          // name_elem
          Read(&name_elem, &buf);
        }
      }
    }
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::KeyAliases)) {
    valueList.keyAliases.emplace(decltype(valueList.keyAliases)::value_type());
    auto& keyAliases = *valueList.keyAliases;

    // keyAliases
    keyAliases.resize(nKeyAliases);
    for (auto& keyAliases_elem : keyAliases) {
      // keyAliases_elem
      {
        auto& real = keyAliases_elem.real;
        auto& alias = keyAliases_elem.alias;

        // real
        for (auto& real_elem : real) {
          // real_elem
          Read(&real_elem, &buf);
        }

        // alias
        for (auto& alias_elem : alias) {
          // alias_elem
          Read(&alias_elem, &buf);
        }
      }
    }
  }
  if (CaseAnd(valueList_expr, Xkb::NameDetail::RGNames)) {
    valueList.radioGroupNames.emplace(
        decltype(valueList.radioGroupNames)::value_type());
    auto& radioGroupNames = *valueList.radioGroupNames;

    // radioGroupNames
    radioGroupNames.resize(nRadioGroups);
    for (auto& radioGroupNames_elem : radioGroupNames) {
      // radioGroupNames_elem
      Read(&radioGroupNames_elem, &buf);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xkb::SetNames(const Xkb::SetNamesRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& virtualMods = request.virtualMods;
  NameDetail which{};
  auto& firstType = request.firstType;
  auto& nTypes = request.nTypes;
  auto& firstKTLevelt = request.firstKTLevelt;
  auto& nKTLevels = request.nKTLevels;
  auto& indicators = request.indicators;
  auto& groupNames = request.groupNames;
  auto& nRadioGroups = request.nRadioGroups;
  auto& firstKey = request.firstKey;
  auto& nKeys = request.nKeys;
  auto& nKeyAliases = request.nKeyAliases;
  auto& totalKTLevelNames = request.totalKTLevelNames;
  auto& values = request;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 18;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // virtualMods
  uint16_t tmp178;
  tmp178 = static_cast<uint16_t>(virtualMods);
  buf.Write(&tmp178);

  // which
  SwitchVar(NameDetail::Keycodes, values.keycodesName.has_value(), true,
            &which);
  SwitchVar(NameDetail::Geometry, values.geometryName.has_value(), true,
            &which);
  SwitchVar(NameDetail::Symbols, values.symbolsName.has_value(), true, &which);
  SwitchVar(NameDetail::PhysSymbols, values.physSymbolsName.has_value(), true,
            &which);
  SwitchVar(NameDetail::Types, values.typesName.has_value(), true, &which);
  SwitchVar(NameDetail::Compat, values.compatName.has_value(), true, &which);
  SwitchVar(NameDetail::KeyTypeNames, values.typeNames.has_value(), true,
            &which);
  SwitchVar(NameDetail::KTLevelNames, values.nLevelsPerType.has_value(), true,
            &which);
  SwitchVar(NameDetail::IndicatorNames, values.indicatorNames.has_value(), true,
            &which);
  SwitchVar(NameDetail::VirtualModNames, values.virtualModNames.has_value(),
            true, &which);
  SwitchVar(NameDetail::GroupNames, values.groups.has_value(), true, &which);
  SwitchVar(NameDetail::KeyNames, values.keyNames.has_value(), true, &which);
  SwitchVar(NameDetail::KeyAliases, values.keyAliases.has_value(), true,
            &which);
  SwitchVar(NameDetail::RGNames, values.radioGroupNames.has_value(), true,
            &which);
  uint32_t tmp179;
  tmp179 = static_cast<uint32_t>(which);
  buf.Write(&tmp179);

  // firstType
  buf.Write(&firstType);

  // nTypes
  buf.Write(&nTypes);

  // firstKTLevelt
  buf.Write(&firstKTLevelt);

  // nKTLevels
  buf.Write(&nKTLevels);

  // indicators
  buf.Write(&indicators);

  // groupNames
  uint8_t tmp180;
  tmp180 = static_cast<uint8_t>(groupNames);
  buf.Write(&tmp180);

  // nRadioGroups
  buf.Write(&nRadioGroups);

  // firstKey
  buf.Write(&firstKey);

  // nKeys
  buf.Write(&nKeys);

  // nKeyAliases
  buf.Write(&nKeyAliases);

  // pad0
  Pad(&buf, 1);

  // totalKTLevelNames
  buf.Write(&totalKTLevelNames);

  // values
  auto values_expr = which;
  if (CaseAnd(values_expr, NameDetail::Keycodes)) {
    auto& keycodesName = *values.keycodesName;

    // keycodesName
    buf.Write(&keycodesName);
  }
  if (CaseAnd(values_expr, NameDetail::Geometry)) {
    auto& geometryName = *values.geometryName;

    // geometryName
    buf.Write(&geometryName);
  }
  if (CaseAnd(values_expr, NameDetail::Symbols)) {
    auto& symbolsName = *values.symbolsName;

    // symbolsName
    buf.Write(&symbolsName);
  }
  if (CaseAnd(values_expr, NameDetail::PhysSymbols)) {
    auto& physSymbolsName = *values.physSymbolsName;

    // physSymbolsName
    buf.Write(&physSymbolsName);
  }
  if (CaseAnd(values_expr, NameDetail::Types)) {
    auto& typesName = *values.typesName;

    // typesName
    buf.Write(&typesName);
  }
  if (CaseAnd(values_expr, NameDetail::Compat)) {
    auto& compatName = *values.compatName;

    // compatName
    buf.Write(&compatName);
  }
  if (CaseAnd(values_expr, NameDetail::KeyTypeNames)) {
    auto& typeNames = *values.typeNames;

    // typeNames
    CHECK_EQ(static_cast<size_t>(nTypes), typeNames.size());
    for (auto& typeNames_elem : typeNames) {
      // typeNames_elem
      buf.Write(&typeNames_elem);
    }
  }
  if (CaseAnd(values_expr, NameDetail::KTLevelNames)) {
    auto& nLevelsPerType = *values.nLevelsPerType;
    auto& ktLevelNames = *values.ktLevelNames;

    // nLevelsPerType
    CHECK_EQ(static_cast<size_t>(nTypes), nLevelsPerType.size());
    for (auto& nLevelsPerType_elem : nLevelsPerType) {
      // nLevelsPerType_elem
      buf.Write(&nLevelsPerType_elem);
    }

    // pad1
    Align(&buf, 4);

    // ktLevelNames
    auto sum181_ = SumOf([](const auto& listelem_ref) { return listelem_ref; },
                         nLevelsPerType);
    CHECK_EQ(static_cast<size_t>(sum181_), ktLevelNames.size());
    for (auto& ktLevelNames_elem : ktLevelNames) {
      // ktLevelNames_elem
      buf.Write(&ktLevelNames_elem);
    }
  }
  if (CaseAnd(values_expr, NameDetail::IndicatorNames)) {
    auto& indicatorNames = *values.indicatorNames;

    // indicatorNames
    CHECK_EQ(static_cast<size_t>(PopCount(indicators)), indicatorNames.size());
    for (auto& indicatorNames_elem : indicatorNames) {
      // indicatorNames_elem
      buf.Write(&indicatorNames_elem);
    }
  }
  if (CaseAnd(values_expr, NameDetail::VirtualModNames)) {
    auto& virtualModNames = *values.virtualModNames;

    // virtualModNames
    CHECK_EQ(static_cast<size_t>(PopCount(virtualMods)),
             virtualModNames.size());
    for (auto& virtualModNames_elem : virtualModNames) {
      // virtualModNames_elem
      buf.Write(&virtualModNames_elem);
    }
  }
  if (CaseAnd(values_expr, NameDetail::GroupNames)) {
    auto& groups = *values.groups;

    // groups
    CHECK_EQ(static_cast<size_t>(PopCount(groupNames)), groups.size());
    for (auto& groups_elem : groups) {
      // groups_elem
      buf.Write(&groups_elem);
    }
  }
  if (CaseAnd(values_expr, NameDetail::KeyNames)) {
    auto& keyNames = *values.keyNames;

    // keyNames
    CHECK_EQ(static_cast<size_t>(nKeys), keyNames.size());
    for (auto& keyNames_elem : keyNames) {
      // keyNames_elem
      {
        auto& name = keyNames_elem.name;

        // name
        for (auto& name_elem : name) {
          // name_elem
          buf.Write(&name_elem);
        }
      }
    }
  }
  if (CaseAnd(values_expr, NameDetail::KeyAliases)) {
    auto& keyAliases = *values.keyAliases;

    // keyAliases
    CHECK_EQ(static_cast<size_t>(nKeyAliases), keyAliases.size());
    for (auto& keyAliases_elem : keyAliases) {
      // keyAliases_elem
      {
        auto& real = keyAliases_elem.real;
        auto& alias = keyAliases_elem.alias;

        // real
        for (auto& real_elem : real) {
          // real_elem
          buf.Write(&real_elem);
        }

        // alias
        for (auto& alias_elem : alias) {
          // alias_elem
          buf.Write(&alias_elem);
        }
      }
    }
  }
  if (CaseAnd(values_expr, NameDetail::RGNames)) {
    auto& radioGroupNames = *values.radioGroupNames;

    // radioGroupNames
    CHECK_EQ(static_cast<size_t>(nRadioGroups), radioGroupNames.size());
    for (auto& radioGroupNames_elem : radioGroupNames) {
      // radioGroupNames_elem
      buf.Write(&radioGroupNames_elem);
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xkb::SetNames", false);
}

Future<void> Xkb::SetNames(
    const DeviceSpec& deviceSpec,
    const VMod& virtualMods,
    const uint8_t& firstType,
    const uint8_t& nTypes,
    const uint8_t& firstKTLevelt,
    const uint8_t& nKTLevels,
    const uint32_t& indicators,
    const SetOfGroup& groupNames,
    const uint8_t& nRadioGroups,
    const KeyCode& firstKey,
    const uint8_t& nKeys,
    const uint8_t& nKeyAliases,
    const uint16_t& totalKTLevelNames,
    const std::optional<Atom>& keycodesName,
    const std::optional<Atom>& geometryName,
    const std::optional<Atom>& symbolsName,
    const std::optional<Atom>& physSymbolsName,
    const std::optional<Atom>& typesName,
    const std::optional<Atom>& compatName,
    const std::optional<std::vector<Atom>>& typeNames,
    const std::optional<std::vector<uint8_t>>& nLevelsPerType,
    const std::optional<std::vector<Atom>>& ktLevelNames,
    const std::optional<std::vector<Atom>>& indicatorNames,
    const std::optional<std::vector<Atom>>& virtualModNames,
    const std::optional<std::vector<Atom>>& groups,
    const std::optional<std::vector<KeyName>>& keyNames,
    const std::optional<std::vector<KeyAlias>>& keyAliases,
    const std::optional<std::vector<Atom>>& radioGroupNames) {
  return Xkb::SetNames(Xkb::SetNamesRequest{deviceSpec,
                                            virtualMods,
                                            firstType,
                                            nTypes,
                                            firstKTLevelt,
                                            nKTLevels,
                                            indicators,
                                            groupNames,
                                            nRadioGroups,
                                            firstKey,
                                            nKeys,
                                            nKeyAliases,
                                            totalKTLevelNames,
                                            keycodesName,
                                            geometryName,
                                            symbolsName,
                                            physSymbolsName,
                                            typesName,
                                            compatName,
                                            typeNames,
                                            nLevelsPerType,
                                            ktLevelNames,
                                            indicatorNames,
                                            virtualModNames,
                                            groups,
                                            keyNames,
                                            keyAliases,
                                            radioGroupNames});
}

Future<Xkb::PerClientFlagsReply> Xkb::PerClientFlags(
    const Xkb::PerClientFlagsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& change = request.change;
  auto& value = request.value;
  auto& ctrlsToChange = request.ctrlsToChange;
  auto& autoCtrls = request.autoCtrls;
  auto& autoCtrlsValues = request.autoCtrlsValues;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 21;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // pad0
  Pad(&buf, 2);

  // change
  uint32_t tmp182;
  tmp182 = static_cast<uint32_t>(change);
  buf.Write(&tmp182);

  // value
  uint32_t tmp183;
  tmp183 = static_cast<uint32_t>(value);
  buf.Write(&tmp183);

  // ctrlsToChange
  uint32_t tmp184;
  tmp184 = static_cast<uint32_t>(ctrlsToChange);
  buf.Write(&tmp184);

  // autoCtrls
  uint32_t tmp185;
  tmp185 = static_cast<uint32_t>(autoCtrls);
  buf.Write(&tmp185);

  // autoCtrlsValues
  uint32_t tmp186;
  tmp186 = static_cast<uint32_t>(autoCtrlsValues);
  buf.Write(&tmp186);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::PerClientFlagsReply>(
      &buf, "Xkb::PerClientFlags", false);
}

Future<Xkb::PerClientFlagsReply> Xkb::PerClientFlags(
    const DeviceSpec& deviceSpec,
    const PerClientFlag& change,
    const PerClientFlag& value,
    const BoolCtrl& ctrlsToChange,
    const BoolCtrl& autoCtrls,
    const BoolCtrl& autoCtrlsValues) {
  return Xkb::PerClientFlags(Xkb::PerClientFlagsRequest{
      deviceSpec, change, value, ctrlsToChange, autoCtrls, autoCtrlsValues});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::PerClientFlagsReply> detail::ReadReply<
    Xkb::PerClientFlagsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::PerClientFlagsReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  auto& supported = (*reply).supported;
  auto& value = (*reply).value;
  auto& autoCtrls = (*reply).autoCtrls;
  auto& autoCtrlsValues = (*reply).autoCtrlsValues;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // supported
  uint32_t tmp187;
  Read(&tmp187, &buf);
  supported = static_cast<Xkb::PerClientFlag>(tmp187);

  // value
  uint32_t tmp188;
  Read(&tmp188, &buf);
  value = static_cast<Xkb::PerClientFlag>(tmp188);

  // autoCtrls
  uint32_t tmp189;
  Read(&tmp189, &buf);
  autoCtrls = static_cast<Xkb::BoolCtrl>(tmp189);

  // autoCtrlsValues
  uint32_t tmp190;
  Read(&tmp190, &buf);
  autoCtrlsValues = static_cast<Xkb::BoolCtrl>(tmp190);

  // pad0
  Pad(&buf, 8);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xkb::ListComponentsReply> Xkb::ListComponents(
    const Xkb::ListComponentsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& maxNames = request.maxNames;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 22;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // maxNames
  buf.Write(&maxNames);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::ListComponentsReply>(
      &buf, "Xkb::ListComponents", false);
}

Future<Xkb::ListComponentsReply> Xkb::ListComponents(
    const DeviceSpec& deviceSpec,
    const uint16_t& maxNames) {
  return Xkb::ListComponents(Xkb::ListComponentsRequest{deviceSpec, maxNames});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::ListComponentsReply> detail::ReadReply<
    Xkb::ListComponentsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::ListComponentsReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  uint16_t nKeymaps{};
  uint16_t nKeycodes{};
  uint16_t nTypes{};
  uint16_t nCompatMaps{};
  uint16_t nSymbols{};
  uint16_t nGeometries{};
  auto& extra = (*reply).extra;
  auto& keymaps = (*reply).keymaps;
  auto& keycodes = (*reply).keycodes;
  auto& types = (*reply).types;
  auto& compatMaps = (*reply).compatMaps;
  auto& symbols = (*reply).symbols;
  auto& geometries = (*reply).geometries;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // nKeymaps
  Read(&nKeymaps, &buf);

  // nKeycodes
  Read(&nKeycodes, &buf);

  // nTypes
  Read(&nTypes, &buf);

  // nCompatMaps
  Read(&nCompatMaps, &buf);

  // nSymbols
  Read(&nSymbols, &buf);

  // nGeometries
  Read(&nGeometries, &buf);

  // extra
  Read(&extra, &buf);

  // pad0
  Pad(&buf, 10);

  // keymaps
  keymaps.resize(nKeymaps);
  for (auto& keymaps_elem : keymaps) {
    // keymaps_elem
    {
      auto& flags = keymaps_elem.flags;
      uint16_t length{};
      auto& string = keymaps_elem.string;

      // flags
      Read(&flags, &buf);

      // length
      Read(&length, &buf);

      // string
      string.resize(length);
      for (auto& string_elem : string) {
        // string_elem
        Read(&string_elem, &buf);
      }

      // pad0
      Align(&buf, 2);
    }
  }

  // keycodes
  keycodes.resize(nKeycodes);
  for (auto& keycodes_elem : keycodes) {
    // keycodes_elem
    {
      auto& flags = keycodes_elem.flags;
      uint16_t length{};
      auto& string = keycodes_elem.string;

      // flags
      Read(&flags, &buf);

      // length
      Read(&length, &buf);

      // string
      string.resize(length);
      for (auto& string_elem : string) {
        // string_elem
        Read(&string_elem, &buf);
      }

      // pad0
      Align(&buf, 2);
    }
  }

  // types
  types.resize(nTypes);
  for (auto& types_elem : types) {
    // types_elem
    {
      auto& flags = types_elem.flags;
      uint16_t length{};
      auto& string = types_elem.string;

      // flags
      Read(&flags, &buf);

      // length
      Read(&length, &buf);

      // string
      string.resize(length);
      for (auto& string_elem : string) {
        // string_elem
        Read(&string_elem, &buf);
      }

      // pad0
      Align(&buf, 2);
    }
  }

  // compatMaps
  compatMaps.resize(nCompatMaps);
  for (auto& compatMaps_elem : compatMaps) {
    // compatMaps_elem
    {
      auto& flags = compatMaps_elem.flags;
      uint16_t length{};
      auto& string = compatMaps_elem.string;

      // flags
      Read(&flags, &buf);

      // length
      Read(&length, &buf);

      // string
      string.resize(length);
      for (auto& string_elem : string) {
        // string_elem
        Read(&string_elem, &buf);
      }

      // pad0
      Align(&buf, 2);
    }
  }

  // symbols
  symbols.resize(nSymbols);
  for (auto& symbols_elem : symbols) {
    // symbols_elem
    {
      auto& flags = symbols_elem.flags;
      uint16_t length{};
      auto& string = symbols_elem.string;

      // flags
      Read(&flags, &buf);

      // length
      Read(&length, &buf);

      // string
      string.resize(length);
      for (auto& string_elem : string) {
        // string_elem
        Read(&string_elem, &buf);
      }

      // pad0
      Align(&buf, 2);
    }
  }

  // geometries
  geometries.resize(nGeometries);
  for (auto& geometries_elem : geometries) {
    // geometries_elem
    {
      auto& flags = geometries_elem.flags;
      uint16_t length{};
      auto& string = geometries_elem.string;

      // flags
      Read(&flags, &buf);

      // length
      Read(&length, &buf);

      // string
      string.resize(length);
      for (auto& string_elem : string) {
        // string_elem
        Read(&string_elem, &buf);
      }

      // pad0
      Align(&buf, 2);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xkb::GetKbdByNameReply> Xkb::GetKbdByName(
    const Xkb::GetKbdByNameRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& need = request.need;
  auto& want = request.want;
  auto& load = request.load;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 23;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // need
  uint16_t tmp191;
  tmp191 = static_cast<uint16_t>(need);
  buf.Write(&tmp191);

  // want
  uint16_t tmp192;
  tmp192 = static_cast<uint16_t>(want);
  buf.Write(&tmp192);

  // load
  buf.Write(&load);

  // pad0
  Pad(&buf, 1);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::GetKbdByNameReply>(
      &buf, "Xkb::GetKbdByName", false);
}

Future<Xkb::GetKbdByNameReply> Xkb::GetKbdByName(const DeviceSpec& deviceSpec,
                                                 const GBNDetail& need,
                                                 const GBNDetail& want,
                                                 const uint8_t& load) {
  return Xkb::GetKbdByName(
      Xkb::GetKbdByNameRequest{deviceSpec, need, want, load});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::GetKbdByNameReply> detail::ReadReply<
    Xkb::GetKbdByNameReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::GetKbdByNameReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  auto& minKeyCode = (*reply).minKeyCode;
  auto& maxKeyCode = (*reply).maxKeyCode;
  auto& loaded = (*reply).loaded;
  auto& newKeyboard = (*reply).newKeyboard;
  auto& found = (*reply).found;
  Xkb::GBNDetail reported{};
  auto& replies = (*reply);

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // minKeyCode
  Read(&minKeyCode, &buf);

  // maxKeyCode
  Read(&maxKeyCode, &buf);

  // loaded
  Read(&loaded, &buf);

  // newKeyboard
  Read(&newKeyboard, &buf);

  // found
  uint16_t tmp193;
  Read(&tmp193, &buf);
  found = static_cast<Xkb::GBNDetail>(tmp193);

  // reported
  uint16_t tmp194;
  Read(&tmp194, &buf);
  reported = static_cast<Xkb::GBNDetail>(tmp194);

  // pad0
  Pad(&buf, 16);

  // replies
  auto replies_expr = reported;
  if (CaseAnd(replies_expr, Xkb::GBNDetail::Types) ||
      CaseAnd(replies_expr, Xkb::GBNDetail::ClientSymbols) ||
      CaseAnd(replies_expr, Xkb::GBNDetail::ServerSymbols)) {
    replies.types.emplace(decltype(replies.types)::value_type());
    auto& getmap_type = (*replies.types).getmap_type;
    auto& typeDeviceID = (*replies.types).typeDeviceID;
    auto& getmap_sequence = (*replies.types).getmap_sequence;
    auto& getmap_length = (*replies.types).getmap_length;
    auto& typeMinKeyCode = (*replies.types).typeMinKeyCode;
    auto& typeMaxKeyCode = (*replies.types).typeMaxKeyCode;
    Xkb::MapPart present{};
    auto& firstType = (*replies.types).firstType;
    auto& nTypes = (*replies.types).nTypes;
    auto& totalTypes = (*replies.types).totalTypes;
    auto& firstKeySym = (*replies.types).firstKeySym;
    auto& totalSyms = (*replies.types).totalSyms;
    auto& nKeySyms = (*replies.types).nKeySyms;
    auto& firstKeyAction = (*replies.types).firstKeyAction;
    auto& totalActions = (*replies.types).totalActions;
    auto& nKeyActions = (*replies.types).nKeyActions;
    auto& firstKeyBehavior = (*replies.types).firstKeyBehavior;
    auto& nKeyBehaviors = (*replies.types).nKeyBehaviors;
    auto& totalKeyBehaviors = (*replies.types).totalKeyBehaviors;
    auto& firstKeyExplicit = (*replies.types).firstKeyExplicit;
    auto& nKeyExplicit = (*replies.types).nKeyExplicit;
    auto& totalKeyExplicit = (*replies.types).totalKeyExplicit;
    auto& firstModMapKey = (*replies.types).firstModMapKey;
    auto& nModMapKeys = (*replies.types).nModMapKeys;
    auto& totalModMapKeys = (*replies.types).totalModMapKeys;
    auto& firstVModMapKey = (*replies.types).firstVModMapKey;
    auto& nVModMapKeys = (*replies.types).nVModMapKeys;
    auto& totalVModMapKeys = (*replies.types).totalVModMapKeys;
    auto& virtualMods = (*replies.types).virtualMods;
    auto& map = (*replies.types);

    // getmap_type
    Read(&getmap_type, &buf);

    // typeDeviceID
    Read(&typeDeviceID, &buf);

    // getmap_sequence
    Read(&getmap_sequence, &buf);

    // getmap_length
    Read(&getmap_length, &buf);

    // pad1
    Pad(&buf, 2);

    // typeMinKeyCode
    Read(&typeMinKeyCode, &buf);

    // typeMaxKeyCode
    Read(&typeMaxKeyCode, &buf);

    // present
    uint16_t tmp195;
    Read(&tmp195, &buf);
    present = static_cast<Xkb::MapPart>(tmp195);

    // firstType
    Read(&firstType, &buf);

    // nTypes
    Read(&nTypes, &buf);

    // totalTypes
    Read(&totalTypes, &buf);

    // firstKeySym
    Read(&firstKeySym, &buf);

    // totalSyms
    Read(&totalSyms, &buf);

    // nKeySyms
    Read(&nKeySyms, &buf);

    // firstKeyAction
    Read(&firstKeyAction, &buf);

    // totalActions
    Read(&totalActions, &buf);

    // nKeyActions
    Read(&nKeyActions, &buf);

    // firstKeyBehavior
    Read(&firstKeyBehavior, &buf);

    // nKeyBehaviors
    Read(&nKeyBehaviors, &buf);

    // totalKeyBehaviors
    Read(&totalKeyBehaviors, &buf);

    // firstKeyExplicit
    Read(&firstKeyExplicit, &buf);

    // nKeyExplicit
    Read(&nKeyExplicit, &buf);

    // totalKeyExplicit
    Read(&totalKeyExplicit, &buf);

    // firstModMapKey
    Read(&firstModMapKey, &buf);

    // nModMapKeys
    Read(&nModMapKeys, &buf);

    // totalModMapKeys
    Read(&totalModMapKeys, &buf);

    // firstVModMapKey
    Read(&firstVModMapKey, &buf);

    // nVModMapKeys
    Read(&nVModMapKeys, &buf);

    // totalVModMapKeys
    Read(&totalVModMapKeys, &buf);

    // pad2
    Pad(&buf, 1);

    // virtualMods
    uint16_t tmp196;
    Read(&tmp196, &buf);
    virtualMods = static_cast<Xkb::VMod>(tmp196);

    // map
    auto map_expr = present;
    if (CaseAnd(map_expr, Xkb::MapPart::KeyTypes)) {
      map.types_rtrn.emplace(decltype(map.types_rtrn)::value_type());
      auto& types_rtrn = *map.types_rtrn;

      // types_rtrn
      types_rtrn.resize(nTypes);
      for (auto& types_rtrn_elem : types_rtrn) {
        // types_rtrn_elem
        {
          auto& mods_mask = types_rtrn_elem.mods_mask;
          auto& mods_mods = types_rtrn_elem.mods_mods;
          auto& mods_vmods = types_rtrn_elem.mods_vmods;
          auto& numLevels = types_rtrn_elem.numLevels;
          uint8_t nMapEntries{};
          auto& hasPreserve = types_rtrn_elem.hasPreserve;
          auto& map = types_rtrn_elem.map;
          auto& preserve = types_rtrn_elem.preserve;

          // mods_mask
          uint8_t tmp197;
          Read(&tmp197, &buf);
          mods_mask = static_cast<ModMask>(tmp197);

          // mods_mods
          uint8_t tmp198;
          Read(&tmp198, &buf);
          mods_mods = static_cast<ModMask>(tmp198);

          // mods_vmods
          uint16_t tmp199;
          Read(&tmp199, &buf);
          mods_vmods = static_cast<Xkb::VMod>(tmp199);

          // numLevels
          Read(&numLevels, &buf);

          // nMapEntries
          Read(&nMapEntries, &buf);

          // hasPreserve
          Read(&hasPreserve, &buf);

          // pad0
          Pad(&buf, 1);

          // map
          map.resize(nMapEntries);
          for (auto& map_elem : map) {
            // map_elem
            {
              auto& active = map_elem.active;
              auto& mods_mask = map_elem.mods_mask;
              auto& level = map_elem.level;
              auto& mods_mods = map_elem.mods_mods;
              auto& mods_vmods = map_elem.mods_vmods;

              // active
              Read(&active, &buf);

              // mods_mask
              uint8_t tmp200;
              Read(&tmp200, &buf);
              mods_mask = static_cast<ModMask>(tmp200);

              // level
              Read(&level, &buf);

              // mods_mods
              uint8_t tmp201;
              Read(&tmp201, &buf);
              mods_mods = static_cast<ModMask>(tmp201);

              // mods_vmods
              uint16_t tmp202;
              Read(&tmp202, &buf);
              mods_vmods = static_cast<Xkb::VMod>(tmp202);

              // pad0
              Pad(&buf, 2);
            }
          }

          // preserve
          preserve.resize((hasPreserve) * (nMapEntries));
          for (auto& preserve_elem : preserve) {
            // preserve_elem
            {
              auto& mask = preserve_elem.mask;
              auto& realMods = preserve_elem.realMods;
              auto& vmods = preserve_elem.vmods;

              // mask
              uint8_t tmp203;
              Read(&tmp203, &buf);
              mask = static_cast<ModMask>(tmp203);

              // realMods
              uint8_t tmp204;
              Read(&tmp204, &buf);
              realMods = static_cast<ModMask>(tmp204);

              // vmods
              uint16_t tmp205;
              Read(&tmp205, &buf);
              vmods = static_cast<Xkb::VMod>(tmp205);
            }
          }
        }
      }
    }
    if (CaseAnd(map_expr, Xkb::MapPart::KeySyms)) {
      map.syms_rtrn.emplace(decltype(map.syms_rtrn)::value_type());
      auto& syms_rtrn = *map.syms_rtrn;

      // syms_rtrn
      syms_rtrn.resize(nKeySyms);
      for (auto& syms_rtrn_elem : syms_rtrn) {
        // syms_rtrn_elem
        {
          auto& kt_index = syms_rtrn_elem.kt_index;
          auto& groupInfo = syms_rtrn_elem.groupInfo;
          auto& width = syms_rtrn_elem.width;
          uint16_t nSyms{};
          auto& syms = syms_rtrn_elem.syms;

          // kt_index
          for (auto& kt_index_elem : kt_index) {
            // kt_index_elem
            Read(&kt_index_elem, &buf);
          }

          // groupInfo
          Read(&groupInfo, &buf);

          // width
          Read(&width, &buf);

          // nSyms
          Read(&nSyms, &buf);

          // syms
          syms.resize(nSyms);
          for (auto& syms_elem : syms) {
            // syms_elem
            Read(&syms_elem, &buf);
          }
        }
      }
    }
    if (CaseAnd(map_expr, Xkb::MapPart::KeyActions)) {
      map.acts_rtrn_count.emplace(decltype(map.acts_rtrn_count)::value_type());
      map.acts_rtrn_acts.emplace(decltype(map.acts_rtrn_acts)::value_type());
      auto& acts_rtrn_count = *map.acts_rtrn_count;
      auto& acts_rtrn_acts = *map.acts_rtrn_acts;

      // acts_rtrn_count
      acts_rtrn_count.resize(nKeyActions);
      for (auto& acts_rtrn_count_elem : acts_rtrn_count) {
        // acts_rtrn_count_elem
        Read(&acts_rtrn_count_elem, &buf);
      }

      // pad3
      Align(&buf, 4);

      // acts_rtrn_acts
      acts_rtrn_acts.resize(totalActions);
      for (auto& acts_rtrn_acts_elem : acts_rtrn_acts) {
        // acts_rtrn_acts_elem
        Read(&acts_rtrn_acts_elem, &buf);
      }
    }
    if (CaseAnd(map_expr, Xkb::MapPart::KeyBehaviors)) {
      map.behaviors_rtrn.emplace(decltype(map.behaviors_rtrn)::value_type());
      auto& behaviors_rtrn = *map.behaviors_rtrn;

      // behaviors_rtrn
      behaviors_rtrn.resize(totalKeyBehaviors);
      for (auto& behaviors_rtrn_elem : behaviors_rtrn) {
        // behaviors_rtrn_elem
        {
          auto& keycode = behaviors_rtrn_elem.keycode;
          auto& behavior = behaviors_rtrn_elem.behavior;

          // keycode
          Read(&keycode, &buf);

          // behavior
          Read(&behavior, &buf);

          // pad0
          Pad(&buf, 1);
        }
      }
    }
    if (CaseAnd(map_expr, Xkb::MapPart::VirtualMods)) {
      map.vmods_rtrn.emplace(decltype(map.vmods_rtrn)::value_type());
      auto& vmods_rtrn = *map.vmods_rtrn;

      // vmods_rtrn
      vmods_rtrn.resize(PopCount(virtualMods));
      for (auto& vmods_rtrn_elem : vmods_rtrn) {
        // vmods_rtrn_elem
        uint8_t tmp206;
        Read(&tmp206, &buf);
        vmods_rtrn_elem = static_cast<ModMask>(tmp206);
      }

      // pad4
      Align(&buf, 4);
    }
    if (CaseAnd(map_expr, Xkb::MapPart::ExplicitComponents)) {
      map.explicit_rtrn.emplace(decltype(map.explicit_rtrn)::value_type());
      auto& explicit_rtrn = *map.explicit_rtrn;

      // explicit_rtrn
      explicit_rtrn.resize(totalKeyExplicit);
      for (auto& explicit_rtrn_elem : explicit_rtrn) {
        // explicit_rtrn_elem
        {
          auto& keycode = explicit_rtrn_elem.keycode;
          auto& c_explicit = explicit_rtrn_elem.c_explicit;

          // keycode
          Read(&keycode, &buf);

          // c_explicit
          uint8_t tmp207;
          Read(&tmp207, &buf);
          c_explicit = static_cast<Xkb::Explicit>(tmp207);
        }
      }

      // pad5
      Align(&buf, 4);
    }
    if (CaseAnd(map_expr, Xkb::MapPart::ModifierMap)) {
      map.modmap_rtrn.emplace(decltype(map.modmap_rtrn)::value_type());
      auto& modmap_rtrn = *map.modmap_rtrn;

      // modmap_rtrn
      modmap_rtrn.resize(totalModMapKeys);
      for (auto& modmap_rtrn_elem : modmap_rtrn) {
        // modmap_rtrn_elem
        {
          auto& keycode = modmap_rtrn_elem.keycode;
          auto& mods = modmap_rtrn_elem.mods;

          // keycode
          Read(&keycode, &buf);

          // mods
          uint8_t tmp208;
          Read(&tmp208, &buf);
          mods = static_cast<ModMask>(tmp208);
        }
      }

      // pad6
      Align(&buf, 4);
    }
    if (CaseAnd(map_expr, Xkb::MapPart::VirtualModMap)) {
      map.vmodmap_rtrn.emplace(decltype(map.vmodmap_rtrn)::value_type());
      auto& vmodmap_rtrn = *map.vmodmap_rtrn;

      // vmodmap_rtrn
      vmodmap_rtrn.resize(totalVModMapKeys);
      for (auto& vmodmap_rtrn_elem : vmodmap_rtrn) {
        // vmodmap_rtrn_elem
        {
          auto& keycode = vmodmap_rtrn_elem.keycode;
          auto& vmods = vmodmap_rtrn_elem.vmods;

          // keycode
          Read(&keycode, &buf);

          // pad0
          Pad(&buf, 1);

          // vmods
          uint16_t tmp209;
          Read(&tmp209, &buf);
          vmods = static_cast<Xkb::VMod>(tmp209);
        }
      }
    }
  }
  if (CaseAnd(replies_expr, Xkb::GBNDetail::CompatMap)) {
    replies.compat_map.emplace(decltype(replies.compat_map)::value_type());
    auto& compatmap_type = (*replies.compat_map).compatmap_type;
    auto& compatDeviceID = (*replies.compat_map).compatDeviceID;
    auto& compatmap_sequence = (*replies.compat_map).compatmap_sequence;
    auto& compatmap_length = (*replies.compat_map).compatmap_length;
    auto& groupsRtrn = (*replies.compat_map).groupsRtrn;
    auto& firstSIRtrn = (*replies.compat_map).firstSIRtrn;
    uint16_t nSIRtrn{};
    auto& nTotalSI = (*replies.compat_map).nTotalSI;
    auto& si_rtrn = (*replies.compat_map).si_rtrn;
    auto& group_rtrn = (*replies.compat_map).group_rtrn;

    // compatmap_type
    Read(&compatmap_type, &buf);

    // compatDeviceID
    Read(&compatDeviceID, &buf);

    // compatmap_sequence
    Read(&compatmap_sequence, &buf);

    // compatmap_length
    Read(&compatmap_length, &buf);

    // groupsRtrn
    uint8_t tmp210;
    Read(&tmp210, &buf);
    groupsRtrn = static_cast<Xkb::SetOfGroup>(tmp210);

    // pad7
    Pad(&buf, 1);

    // firstSIRtrn
    Read(&firstSIRtrn, &buf);

    // nSIRtrn
    Read(&nSIRtrn, &buf);

    // nTotalSI
    Read(&nTotalSI, &buf);

    // pad8
    Pad(&buf, 16);

    // si_rtrn
    si_rtrn.resize(nSIRtrn);
    for (auto& si_rtrn_elem : si_rtrn) {
      // si_rtrn_elem
      {
        auto& sym = si_rtrn_elem.sym;
        auto& mods = si_rtrn_elem.mods;
        auto& match = si_rtrn_elem.match;
        auto& virtualMod = si_rtrn_elem.virtualMod;
        auto& flags = si_rtrn_elem.flags;
        auto& action = si_rtrn_elem.action;

        // sym
        Read(&sym, &buf);

        // mods
        uint8_t tmp211;
        Read(&tmp211, &buf);
        mods = static_cast<ModMask>(tmp211);

        // match
        Read(&match, &buf);

        // virtualMod
        uint8_t tmp212;
        Read(&tmp212, &buf);
        virtualMod = static_cast<Xkb::VModsLow>(tmp212);

        // flags
        Read(&flags, &buf);

        // action
        {
          auto& type = action.type;
          auto& data = action.data;

          // type
          uint8_t tmp213;
          Read(&tmp213, &buf);
          type = static_cast<Xkb::SAType>(tmp213);

          // data
          for (auto& data_elem : data) {
            // data_elem
            Read(&data_elem, &buf);
          }
        }
      }
    }

    // group_rtrn
    group_rtrn.resize(PopCount(groupsRtrn));
    for (auto& group_rtrn_elem : group_rtrn) {
      // group_rtrn_elem
      {
        auto& mask = group_rtrn_elem.mask;
        auto& realMods = group_rtrn_elem.realMods;
        auto& vmods = group_rtrn_elem.vmods;

        // mask
        uint8_t tmp214;
        Read(&tmp214, &buf);
        mask = static_cast<ModMask>(tmp214);

        // realMods
        uint8_t tmp215;
        Read(&tmp215, &buf);
        realMods = static_cast<ModMask>(tmp215);

        // vmods
        uint16_t tmp216;
        Read(&tmp216, &buf);
        vmods = static_cast<Xkb::VMod>(tmp216);
      }
    }
  }
  if (CaseAnd(replies_expr, Xkb::GBNDetail::IndicatorMaps)) {
    replies.indicator_maps.emplace(
        decltype(replies.indicator_maps)::value_type());
    auto& indicatormap_type = (*replies.indicator_maps).indicatormap_type;
    auto& indicatorDeviceID = (*replies.indicator_maps).indicatorDeviceID;
    auto& indicatormap_sequence =
        (*replies.indicator_maps).indicatormap_sequence;
    auto& indicatormap_length = (*replies.indicator_maps).indicatormap_length;
    auto& which = (*replies.indicator_maps).which;
    auto& realIndicators = (*replies.indicator_maps).realIndicators;
    uint8_t nIndicators{};
    auto& maps = (*replies.indicator_maps).maps;

    // indicatormap_type
    Read(&indicatormap_type, &buf);

    // indicatorDeviceID
    Read(&indicatorDeviceID, &buf);

    // indicatormap_sequence
    Read(&indicatormap_sequence, &buf);

    // indicatormap_length
    Read(&indicatormap_length, &buf);

    // which
    Read(&which, &buf);

    // realIndicators
    Read(&realIndicators, &buf);

    // nIndicators
    Read(&nIndicators, &buf);

    // pad9
    Pad(&buf, 15);

    // maps
    maps.resize(nIndicators);
    for (auto& maps_elem : maps) {
      // maps_elem
      {
        auto& flags = maps_elem.flags;
        auto& whichGroups = maps_elem.whichGroups;
        auto& groups = maps_elem.groups;
        auto& whichMods = maps_elem.whichMods;
        auto& mods = maps_elem.mods;
        auto& realMods = maps_elem.realMods;
        auto& vmods = maps_elem.vmods;
        auto& ctrls = maps_elem.ctrls;

        // flags
        uint8_t tmp217;
        Read(&tmp217, &buf);
        flags = static_cast<Xkb::IMFlag>(tmp217);

        // whichGroups
        uint8_t tmp218;
        Read(&tmp218, &buf);
        whichGroups = static_cast<Xkb::IMGroupsWhich>(tmp218);

        // groups
        uint8_t tmp219;
        Read(&tmp219, &buf);
        groups = static_cast<Xkb::SetOfGroup>(tmp219);

        // whichMods
        uint8_t tmp220;
        Read(&tmp220, &buf);
        whichMods = static_cast<Xkb::IMModsWhich>(tmp220);

        // mods
        uint8_t tmp221;
        Read(&tmp221, &buf);
        mods = static_cast<ModMask>(tmp221);

        // realMods
        uint8_t tmp222;
        Read(&tmp222, &buf);
        realMods = static_cast<ModMask>(tmp222);

        // vmods
        uint16_t tmp223;
        Read(&tmp223, &buf);
        vmods = static_cast<Xkb::VMod>(tmp223);

        // ctrls
        uint32_t tmp224;
        Read(&tmp224, &buf);
        ctrls = static_cast<Xkb::BoolCtrl>(tmp224);
      }
    }
  }
  if (CaseAnd(replies_expr, Xkb::GBNDetail::KeyNames) ||
      CaseAnd(replies_expr, Xkb::GBNDetail::OtherNames)) {
    replies.key_names.emplace(decltype(replies.key_names)::value_type());
    auto& keyname_type = (*replies.key_names).keyname_type;
    auto& keyDeviceID = (*replies.key_names).keyDeviceID;
    auto& keyname_sequence = (*replies.key_names).keyname_sequence;
    auto& keyname_length = (*replies.key_names).keyname_length;
    Xkb::NameDetail which{};
    auto& keyMinKeyCode = (*replies.key_names).keyMinKeyCode;
    auto& keyMaxKeyCode = (*replies.key_names).keyMaxKeyCode;
    auto& nTypes = (*replies.key_names).nTypes;
    auto& groupNames = (*replies.key_names).groupNames;
    auto& virtualMods = (*replies.key_names).virtualMods;
    auto& firstKey = (*replies.key_names).firstKey;
    auto& nKeys = (*replies.key_names).nKeys;
    auto& indicators = (*replies.key_names).indicators;
    auto& nRadioGroups = (*replies.key_names).nRadioGroups;
    auto& nKeyAliases = (*replies.key_names).nKeyAliases;
    auto& nKTLevels = (*replies.key_names).nKTLevels;
    auto& valueList = (*replies.key_names);

    // keyname_type
    Read(&keyname_type, &buf);

    // keyDeviceID
    Read(&keyDeviceID, &buf);

    // keyname_sequence
    Read(&keyname_sequence, &buf);

    // keyname_length
    Read(&keyname_length, &buf);

    // which
    uint32_t tmp225;
    Read(&tmp225, &buf);
    which = static_cast<Xkb::NameDetail>(tmp225);

    // keyMinKeyCode
    Read(&keyMinKeyCode, &buf);

    // keyMaxKeyCode
    Read(&keyMaxKeyCode, &buf);

    // nTypes
    Read(&nTypes, &buf);

    // groupNames
    uint8_t tmp226;
    Read(&tmp226, &buf);
    groupNames = static_cast<Xkb::SetOfGroup>(tmp226);

    // virtualMods
    uint16_t tmp227;
    Read(&tmp227, &buf);
    virtualMods = static_cast<Xkb::VMod>(tmp227);

    // firstKey
    Read(&firstKey, &buf);

    // nKeys
    Read(&nKeys, &buf);

    // indicators
    Read(&indicators, &buf);

    // nRadioGroups
    Read(&nRadioGroups, &buf);

    // nKeyAliases
    Read(&nKeyAliases, &buf);

    // nKTLevels
    Read(&nKTLevels, &buf);

    // pad10
    Pad(&buf, 4);

    // valueList
    auto valueList_expr = which;
    if (CaseAnd(valueList_expr, Xkb::NameDetail::Keycodes)) {
      valueList.keycodesName.emplace(
          decltype(valueList.keycodesName)::value_type());
      auto& keycodesName = *valueList.keycodesName;

      // keycodesName
      Read(&keycodesName, &buf);
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::Geometry)) {
      valueList.geometryName.emplace(
          decltype(valueList.geometryName)::value_type());
      auto& geometryName = *valueList.geometryName;

      // geometryName
      Read(&geometryName, &buf);
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::Symbols)) {
      valueList.symbolsName.emplace(
          decltype(valueList.symbolsName)::value_type());
      auto& symbolsName = *valueList.symbolsName;

      // symbolsName
      Read(&symbolsName, &buf);
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::PhysSymbols)) {
      valueList.physSymbolsName.emplace(
          decltype(valueList.physSymbolsName)::value_type());
      auto& physSymbolsName = *valueList.physSymbolsName;

      // physSymbolsName
      Read(&physSymbolsName, &buf);
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::Types)) {
      valueList.typesName.emplace(decltype(valueList.typesName)::value_type());
      auto& typesName = *valueList.typesName;

      // typesName
      Read(&typesName, &buf);
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::Compat)) {
      valueList.compatName.emplace(
          decltype(valueList.compatName)::value_type());
      auto& compatName = *valueList.compatName;

      // compatName
      Read(&compatName, &buf);
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::KeyTypeNames)) {
      valueList.typeNames.emplace(decltype(valueList.typeNames)::value_type());
      auto& typeNames = *valueList.typeNames;

      // typeNames
      typeNames.resize(nTypes);
      for (auto& typeNames_elem : typeNames) {
        // typeNames_elem
        Read(&typeNames_elem, &buf);
      }
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::KTLevelNames)) {
      valueList.nLevelsPerType.emplace(
          decltype(valueList.nLevelsPerType)::value_type());
      valueList.ktLevelNames.emplace(
          decltype(valueList.ktLevelNames)::value_type());
      auto& nLevelsPerType = *valueList.nLevelsPerType;
      auto& ktLevelNames = *valueList.ktLevelNames;

      // nLevelsPerType
      nLevelsPerType.resize(nTypes);
      for (auto& nLevelsPerType_elem : nLevelsPerType) {
        // nLevelsPerType_elem
        Read(&nLevelsPerType_elem, &buf);
      }

      // pad11
      Align(&buf, 4);

      // ktLevelNames
      auto sum228_ = SumOf([](auto& listelem_ref) { return listelem_ref; },
                           nLevelsPerType);
      ktLevelNames.resize(sum228_);
      for (auto& ktLevelNames_elem : ktLevelNames) {
        // ktLevelNames_elem
        Read(&ktLevelNames_elem, &buf);
      }
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::IndicatorNames)) {
      valueList.indicatorNames.emplace(
          decltype(valueList.indicatorNames)::value_type());
      auto& indicatorNames = *valueList.indicatorNames;

      // indicatorNames
      indicatorNames.resize(PopCount(indicators));
      for (auto& indicatorNames_elem : indicatorNames) {
        // indicatorNames_elem
        Read(&indicatorNames_elem, &buf);
      }
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::VirtualModNames)) {
      valueList.virtualModNames.emplace(
          decltype(valueList.virtualModNames)::value_type());
      auto& virtualModNames = *valueList.virtualModNames;

      // virtualModNames
      virtualModNames.resize(PopCount(virtualMods));
      for (auto& virtualModNames_elem : virtualModNames) {
        // virtualModNames_elem
        Read(&virtualModNames_elem, &buf);
      }
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::GroupNames)) {
      valueList.groups.emplace(decltype(valueList.groups)::value_type());
      auto& groups = *valueList.groups;

      // groups
      groups.resize(PopCount(groupNames));
      for (auto& groups_elem : groups) {
        // groups_elem
        Read(&groups_elem, &buf);
      }
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::KeyNames)) {
      valueList.keyNames.emplace(decltype(valueList.keyNames)::value_type());
      auto& keyNames = *valueList.keyNames;

      // keyNames
      keyNames.resize(nKeys);
      for (auto& keyNames_elem : keyNames) {
        // keyNames_elem
        {
          auto& name = keyNames_elem.name;

          // name
          for (auto& name_elem : name) {
            // name_elem
            Read(&name_elem, &buf);
          }
        }
      }
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::KeyAliases)) {
      valueList.keyAliases.emplace(
          decltype(valueList.keyAliases)::value_type());
      auto& keyAliases = *valueList.keyAliases;

      // keyAliases
      keyAliases.resize(nKeyAliases);
      for (auto& keyAliases_elem : keyAliases) {
        // keyAliases_elem
        {
          auto& real = keyAliases_elem.real;
          auto& alias = keyAliases_elem.alias;

          // real
          for (auto& real_elem : real) {
            // real_elem
            Read(&real_elem, &buf);
          }

          // alias
          for (auto& alias_elem : alias) {
            // alias_elem
            Read(&alias_elem, &buf);
          }
        }
      }
    }
    if (CaseAnd(valueList_expr, Xkb::NameDetail::RGNames)) {
      valueList.radioGroupNames.emplace(
          decltype(valueList.radioGroupNames)::value_type());
      auto& radioGroupNames = *valueList.radioGroupNames;

      // radioGroupNames
      radioGroupNames.resize(nRadioGroups);
      for (auto& radioGroupNames_elem : radioGroupNames) {
        // radioGroupNames_elem
        Read(&radioGroupNames_elem, &buf);
      }
    }
  }
  if (CaseAnd(replies_expr, Xkb::GBNDetail::Geometry)) {
    replies.geometry.emplace(decltype(replies.geometry)::value_type());
    auto& geometry_type = (*replies.geometry).geometry_type;
    auto& geometryDeviceID = (*replies.geometry).geometryDeviceID;
    auto& geometry_sequence = (*replies.geometry).geometry_sequence;
    auto& geometry_length = (*replies.geometry).geometry_length;
    auto& name = (*replies.geometry).name;
    auto& geometryFound = (*replies.geometry).geometryFound;
    auto& widthMM = (*replies.geometry).widthMM;
    auto& heightMM = (*replies.geometry).heightMM;
    auto& nProperties = (*replies.geometry).nProperties;
    auto& nColors = (*replies.geometry).nColors;
    auto& nShapes = (*replies.geometry).nShapes;
    auto& nSections = (*replies.geometry).nSections;
    auto& nDoodads = (*replies.geometry).nDoodads;
    auto& nKeyAliases = (*replies.geometry).nKeyAliases;
    auto& baseColorNdx = (*replies.geometry).baseColorNdx;
    auto& labelColorNdx = (*replies.geometry).labelColorNdx;
    auto& labelFont = (*replies.geometry).labelFont;

    // geometry_type
    Read(&geometry_type, &buf);

    // geometryDeviceID
    Read(&geometryDeviceID, &buf);

    // geometry_sequence
    Read(&geometry_sequence, &buf);

    // geometry_length
    Read(&geometry_length, &buf);

    // name
    Read(&name, &buf);

    // geometryFound
    Read(&geometryFound, &buf);

    // pad12
    Pad(&buf, 1);

    // widthMM
    Read(&widthMM, &buf);

    // heightMM
    Read(&heightMM, &buf);

    // nProperties
    Read(&nProperties, &buf);

    // nColors
    Read(&nColors, &buf);

    // nShapes
    Read(&nShapes, &buf);

    // nSections
    Read(&nSections, &buf);

    // nDoodads
    Read(&nDoodads, &buf);

    // nKeyAliases
    Read(&nKeyAliases, &buf);

    // baseColorNdx
    Read(&baseColorNdx, &buf);

    // labelColorNdx
    Read(&labelColorNdx, &buf);

    // labelFont
    {
      uint16_t length{};
      auto& string = labelFont.string;
      auto& alignment_pad = labelFont.alignment_pad;

      // length
      Read(&length, &buf);

      // string
      string.resize(length);
      for (auto& string_elem : string) {
        // string_elem
        Read(&string_elem, &buf);
      }

      // alignment_pad
      alignment_pad = buffer->ReadAndAdvance(
          (BitAnd((length) + (5), BitNot(3))) - ((length) + (2)));
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Xkb::GetDeviceInfoReply> Xkb::GetDeviceInfo(
    const Xkb::GetDeviceInfoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& wanted = request.wanted;
  auto& allButtons = request.allButtons;
  auto& firstButton = request.firstButton;
  auto& nButtons = request.nButtons;
  auto& ledClass = request.ledClass;
  auto& ledID = request.ledID;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 24;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // wanted
  uint16_t tmp229;
  tmp229 = static_cast<uint16_t>(wanted);
  buf.Write(&tmp229);

  // allButtons
  buf.Write(&allButtons);

  // firstButton
  buf.Write(&firstButton);

  // nButtons
  buf.Write(&nButtons);

  // pad0
  Pad(&buf, 1);

  // ledClass
  uint16_t tmp230;
  tmp230 = static_cast<uint16_t>(ledClass);
  buf.Write(&tmp230);

  // ledID
  buf.Write(&ledID);

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::GetDeviceInfoReply>(
      &buf, "Xkb::GetDeviceInfo", false);
}

Future<Xkb::GetDeviceInfoReply> Xkb::GetDeviceInfo(const DeviceSpec& deviceSpec,
                                                   const XIFeature& wanted,
                                                   const uint8_t& allButtons,
                                                   const uint8_t& firstButton,
                                                   const uint8_t& nButtons,
                                                   const LedClass& ledClass,
                                                   const IDSpec& ledID) {
  return Xkb::GetDeviceInfo(Xkb::GetDeviceInfoRequest{
      deviceSpec, wanted, allButtons, firstButton, nButtons, ledClass, ledID});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::GetDeviceInfoReply> detail::ReadReply<
    Xkb::GetDeviceInfoReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::GetDeviceInfoReply>();

  auto& deviceID = (*reply).deviceID;
  auto& sequence = (*reply).sequence;
  auto& present = (*reply).present;
  auto& supported = (*reply).supported;
  auto& unsupported = (*reply).unsupported;
  uint16_t nDeviceLedFBs{};
  auto& firstBtnWanted = (*reply).firstBtnWanted;
  auto& nBtnsWanted = (*reply).nBtnsWanted;
  auto& firstBtnRtrn = (*reply).firstBtnRtrn;
  uint8_t nBtnsRtrn{};
  auto& totalBtns = (*reply).totalBtns;
  auto& hasOwnState = (*reply).hasOwnState;
  auto& dfltKbdFB = (*reply).dfltKbdFB;
  auto& dfltLedFB = (*reply).dfltLedFB;
  auto& devType = (*reply).devType;
  uint16_t nameLen{};
  auto& name = (*reply).name;
  auto& btnActions = (*reply).btnActions;
  auto& leds = (*reply).leds;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // deviceID
  Read(&deviceID, &buf);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // present
  uint16_t tmp231;
  Read(&tmp231, &buf);
  present = static_cast<Xkb::XIFeature>(tmp231);

  // supported
  uint16_t tmp232;
  Read(&tmp232, &buf);
  supported = static_cast<Xkb::XIFeature>(tmp232);

  // unsupported
  uint16_t tmp233;
  Read(&tmp233, &buf);
  unsupported = static_cast<Xkb::XIFeature>(tmp233);

  // nDeviceLedFBs
  Read(&nDeviceLedFBs, &buf);

  // firstBtnWanted
  Read(&firstBtnWanted, &buf);

  // nBtnsWanted
  Read(&nBtnsWanted, &buf);

  // firstBtnRtrn
  Read(&firstBtnRtrn, &buf);

  // nBtnsRtrn
  Read(&nBtnsRtrn, &buf);

  // totalBtns
  Read(&totalBtns, &buf);

  // hasOwnState
  Read(&hasOwnState, &buf);

  // dfltKbdFB
  Read(&dfltKbdFB, &buf);

  // dfltLedFB
  Read(&dfltLedFB, &buf);

  // pad0
  Pad(&buf, 2);

  // devType
  Read(&devType, &buf);

  // nameLen
  Read(&nameLen, &buf);

  // name
  name.resize(nameLen);
  for (auto& name_elem : name) {
    // name_elem
    Read(&name_elem, &buf);
  }

  // pad1
  Align(&buf, 4);

  // btnActions
  btnActions.resize(nBtnsRtrn);
  for (auto& btnActions_elem : btnActions) {
    // btnActions_elem
    Read(&btnActions_elem, &buf);
  }

  // leds
  leds.resize(nDeviceLedFBs);
  for (auto& leds_elem : leds) {
    // leds_elem
    {
      auto& ledClass = leds_elem.ledClass;
      auto& ledID = leds_elem.ledID;
      auto& namesPresent = leds_elem.namesPresent;
      auto& mapsPresent = leds_elem.mapsPresent;
      auto& physIndicators = leds_elem.physIndicators;
      auto& state = leds_elem.state;
      auto& names = leds_elem.names;
      auto& maps = leds_elem.maps;

      // ledClass
      uint16_t tmp234;
      Read(&tmp234, &buf);
      ledClass = static_cast<Xkb::LedClass>(tmp234);

      // ledID
      Read(&ledID, &buf);

      // namesPresent
      Read(&namesPresent, &buf);

      // mapsPresent
      Read(&mapsPresent, &buf);

      // physIndicators
      Read(&physIndicators, &buf);

      // state
      Read(&state, &buf);

      // names
      names.resize(PopCount(namesPresent));
      for (auto& names_elem : names) {
        // names_elem
        Read(&names_elem, &buf);
      }

      // maps
      maps.resize(PopCount(mapsPresent));
      for (auto& maps_elem : maps) {
        // maps_elem
        {
          auto& flags = maps_elem.flags;
          auto& whichGroups = maps_elem.whichGroups;
          auto& groups = maps_elem.groups;
          auto& whichMods = maps_elem.whichMods;
          auto& mods = maps_elem.mods;
          auto& realMods = maps_elem.realMods;
          auto& vmods = maps_elem.vmods;
          auto& ctrls = maps_elem.ctrls;

          // flags
          uint8_t tmp235;
          Read(&tmp235, &buf);
          flags = static_cast<Xkb::IMFlag>(tmp235);

          // whichGroups
          uint8_t tmp236;
          Read(&tmp236, &buf);
          whichGroups = static_cast<Xkb::IMGroupsWhich>(tmp236);

          // groups
          uint8_t tmp237;
          Read(&tmp237, &buf);
          groups = static_cast<Xkb::SetOfGroup>(tmp237);

          // whichMods
          uint8_t tmp238;
          Read(&tmp238, &buf);
          whichMods = static_cast<Xkb::IMModsWhich>(tmp238);

          // mods
          uint8_t tmp239;
          Read(&tmp239, &buf);
          mods = static_cast<ModMask>(tmp239);

          // realMods
          uint8_t tmp240;
          Read(&tmp240, &buf);
          realMods = static_cast<ModMask>(tmp240);

          // vmods
          uint16_t tmp241;
          Read(&tmp241, &buf);
          vmods = static_cast<Xkb::VMod>(tmp241);

          // ctrls
          uint32_t tmp242;
          Read(&tmp242, &buf);
          ctrls = static_cast<Xkb::BoolCtrl>(tmp242);
        }
      }
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Xkb::SetDeviceInfo(const Xkb::SetDeviceInfoRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& deviceSpec = request.deviceSpec;
  auto& firstBtn = request.firstBtn;
  uint8_t nBtns{};
  auto& change = request.change;
  uint16_t nDeviceLedFBs{};
  auto& btnActions = request.btnActions;
  size_t btnActions_len = btnActions.size();
  auto& leds = request.leds;
  size_t leds_len = leds.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 25;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // deviceSpec
  buf.Write(&deviceSpec);

  // firstBtn
  buf.Write(&firstBtn);

  // nBtns
  nBtns = btnActions.size();
  buf.Write(&nBtns);

  // change
  uint16_t tmp243;
  tmp243 = static_cast<uint16_t>(change);
  buf.Write(&tmp243);

  // nDeviceLedFBs
  nDeviceLedFBs = leds.size();
  buf.Write(&nDeviceLedFBs);

  // btnActions
  CHECK_EQ(static_cast<size_t>(nBtns), btnActions.size());
  for (auto& btnActions_elem : btnActions) {
    // btnActions_elem
    buf.Write(&btnActions_elem);
  }

  // leds
  CHECK_EQ(static_cast<size_t>(nDeviceLedFBs), leds.size());
  for (auto& leds_elem : leds) {
    // leds_elem
    {
      auto& ledClass = leds_elem.ledClass;
      auto& ledID = leds_elem.ledID;
      auto& namesPresent = leds_elem.namesPresent;
      auto& mapsPresent = leds_elem.mapsPresent;
      auto& physIndicators = leds_elem.physIndicators;
      auto& state = leds_elem.state;
      auto& names = leds_elem.names;
      auto& maps = leds_elem.maps;

      // ledClass
      uint16_t tmp244;
      tmp244 = static_cast<uint16_t>(ledClass);
      buf.Write(&tmp244);

      // ledID
      buf.Write(&ledID);

      // namesPresent
      buf.Write(&namesPresent);

      // mapsPresent
      buf.Write(&mapsPresent);

      // physIndicators
      buf.Write(&physIndicators);

      // state
      buf.Write(&state);

      // names
      CHECK_EQ(static_cast<size_t>(PopCount(namesPresent)), names.size());
      for (auto& names_elem : names) {
        // names_elem
        buf.Write(&names_elem);
      }

      // maps
      CHECK_EQ(static_cast<size_t>(PopCount(mapsPresent)), maps.size());
      for (auto& maps_elem : maps) {
        // maps_elem
        {
          auto& flags = maps_elem.flags;
          auto& whichGroups = maps_elem.whichGroups;
          auto& groups = maps_elem.groups;
          auto& whichMods = maps_elem.whichMods;
          auto& mods = maps_elem.mods;
          auto& realMods = maps_elem.realMods;
          auto& vmods = maps_elem.vmods;
          auto& ctrls = maps_elem.ctrls;

          // flags
          uint8_t tmp245;
          tmp245 = static_cast<uint8_t>(flags);
          buf.Write(&tmp245);

          // whichGroups
          uint8_t tmp246;
          tmp246 = static_cast<uint8_t>(whichGroups);
          buf.Write(&tmp246);

          // groups
          uint8_t tmp247;
          tmp247 = static_cast<uint8_t>(groups);
          buf.Write(&tmp247);

          // whichMods
          uint8_t tmp248;
          tmp248 = static_cast<uint8_t>(whichMods);
          buf.Write(&tmp248);

          // mods
          uint8_t tmp249;
          tmp249 = static_cast<uint8_t>(mods);
          buf.Write(&tmp249);

          // realMods
          uint8_t tmp250;
          tmp250 = static_cast<uint8_t>(realMods);
          buf.Write(&tmp250);

          // vmods
          uint16_t tmp251;
          tmp251 = static_cast<uint16_t>(vmods);
          buf.Write(&tmp251);

          // ctrls
          uint32_t tmp252;
          tmp252 = static_cast<uint32_t>(ctrls);
          buf.Write(&tmp252);
        }
      }
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Xkb::SetDeviceInfo", false);
}

Future<void> Xkb::SetDeviceInfo(const DeviceSpec& deviceSpec,
                                const uint8_t& firstBtn,
                                const XIFeature& change,
                                const std::vector<Action>& btnActions,
                                const std::vector<DeviceLedInfo>& leds) {
  return Xkb::SetDeviceInfo(Xkb::SetDeviceInfoRequest{
      deviceSpec, firstBtn, change, btnActions, leds});
}

Future<Xkb::SetDebuggingFlagsReply> Xkb::SetDebuggingFlags(
    const Xkb::SetDebuggingFlagsRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  uint16_t msgLength{};
  auto& affectFlags = request.affectFlags;
  auto& flags = request.flags;
  auto& affectCtrls = request.affectCtrls;
  auto& ctrls = request.ctrls;
  auto& message = request.message;
  size_t message_len = message.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 101;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // msgLength
  msgLength = message.size();
  buf.Write(&msgLength);

  // pad0
  Pad(&buf, 2);

  // affectFlags
  buf.Write(&affectFlags);

  // flags
  buf.Write(&flags);

  // affectCtrls
  buf.Write(&affectCtrls);

  // ctrls
  buf.Write(&ctrls);

  // message
  CHECK_EQ(static_cast<size_t>(msgLength), message.size());
  for (auto& message_elem : message) {
    // message_elem
    buf.Write(&message_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<Xkb::SetDebuggingFlagsReply>(
      &buf, "Xkb::SetDebuggingFlags", false);
}

Future<Xkb::SetDebuggingFlagsReply> Xkb::SetDebuggingFlags(
    const uint32_t& affectFlags,
    const uint32_t& flags,
    const uint32_t& affectCtrls,
    const uint32_t& ctrls,
    const std::vector<String8>& message) {
  return Xkb::SetDebuggingFlags(Xkb::SetDebuggingFlagsRequest{
      affectFlags, flags, affectCtrls, ctrls, message});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Xkb::SetDebuggingFlagsReply> detail::ReadReply<
    Xkb::SetDebuggingFlagsReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Xkb::SetDebuggingFlagsReply>();

  auto& sequence = (*reply).sequence;
  auto& currentFlags = (*reply).currentFlags;
  auto& currentCtrls = (*reply).currentCtrls;
  auto& supportedFlags = (*reply).supportedFlags;
  auto& supportedCtrls = (*reply).supportedCtrls;

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

  // currentFlags
  Read(&currentFlags, &buf);

  // currentCtrls
  Read(&currentCtrls, &buf);

  // supportedFlags
  Read(&supportedFlags, &buf);

  // supportedCtrls
  Read(&supportedCtrls, &buf);

  // pad1
  Pad(&buf, 8);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

}  // namespace x11
