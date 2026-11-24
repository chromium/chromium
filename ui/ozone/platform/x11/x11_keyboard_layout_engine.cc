// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_keyboard_layout_engine.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"
#include "ui/events/keycodes/keysym_to_unicode.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"

namespace ui {

namespace {

constexpr int kXkbGroupShift = 13;

constexpr auto kDevice =
    static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd);

x11::Connection* GetXkbConnection() {
  auto* connection = x11::Connection::Get();
  if (!connection || !connection->Ready() || !connection->xkb().present()) {
    return nullptr;
  }
  return connection;
}

}  // namespace

X11KeyboardLayoutEngine::X11KeyboardLayoutEngine()
    : connection_(GetXkbConnection()) {
  if (!connection_) {
    return;
  }
  connection_observation_.Observe(connection_);

  // Only group changes are of interest in StateNotify events, which are
  // otherwise sent for every modifier change.
  constexpr auto kAllEvents =
      x11::Xkb::EventType::NewKeyboardNotify | x11::Xkb::EventType::MapNotify |
      x11::Xkb::EventType::StateNotify | x11::Xkb::EventType::ControlsNotify;
  constexpr auto kSelectAllEvents = x11::Xkb::EventType::NewKeyboardNotify |
                                    x11::Xkb::EventType::MapNotify |
                                    x11::Xkb::EventType::ControlsNotify;
  constexpr auto kAllMapParts = static_cast<x11::Xkb::MapPart>(0xff);
  connection_->xkb().SelectEvents(x11::Xkb::SelectEventsRequest{
      .deviceSpec = kDevice,
      .affectWhich = kAllEvents,
      .selectAll = kSelectAllEvents,
      .affectMap = kAllMapParts,
      .map = kAllMapParts,
      .affectState = x11::Xkb::StatePart::GroupState,
      .stateDetails = x11::Xkb::StatePart::GroupState,
  });
  FetchGroupState();
}

X11KeyboardLayoutEngine::~X11KeyboardLayoutEngine() = default;

uint32_t X11KeyboardLayoutEngine::GetKeyboardLayoutCount() const {
  if (!connection_) {
    return StubKeyboardLayoutEngine::GetKeyboardLayoutCount();
  }
  return num_groups_;
}

DomKey X11KeyboardLayoutEngine::LookupForLayout(DomCode dom_code,
                                                uint32_t layout_index) const {
  if (!connection_) {
    return StubKeyboardLayoutEngine::LookupForLayout(dom_code, layout_index);
  }
  if (layout_index >= num_groups_) {
    return DomKey::NONE;
  }

  const int native_keycode = KeycodeConverter::DomCodeToNativeKeycode(dom_code);
  if (native_keycode == KeycodeConverter::InvalidNativeKeycode()) {
    return DomKey::NONE;
  }

  // Index 0 is the active group; the remaining indices are the other groups
  // in system priority order.
  const uint32_t active = active_group_ < num_groups_ ? active_group_ : 0;
  const uint32_t group =
      layout_index == 0
          ? active
          : (layout_index <= active ? layout_index - 1 : layout_index);
  const uint32_t keysym = connection_->KeycodeToKeysym(
      static_cast<x11::KeyCode>(native_keycode), group << kXkbGroupShift);
  if (keysym == 0) {
    return DomKey::NONE;
  }
  return XKeySymToDomKey(keysym, GetUnicodeCharacterFromXKeySym(keysym));
}

void X11KeyboardLayoutEngine::OnEvent(const x11::Event& event) {
  if (auto* state = event.As<x11::Xkb::StateNotifyEvent>()) {
    active_group_ = static_cast<uint8_t>(state->group);
  } else if (auto* controls = event.As<x11::Xkb::ControlsNotifyEvent>()) {
    num_groups_ = std::max<uint8_t>(1, controls->numGroups);
  } else if (event.As<x11::Xkb::MapNotifyEvent>() ||
             event.As<x11::Xkb::NewKeyboardNotifyEvent>()) {
    FetchGroupState();
  }
}

void X11KeyboardLayoutEngine::FetchGroupState() {
  connection_->xkb().GetState({kDevice}).OnResponse(base::BindOnce(
      &X11KeyboardLayoutEngine::OnGetState, weak_factory_.GetWeakPtr()));
  connection_->xkb().GetControls({kDevice}).OnResponse(base::BindOnce(
      &X11KeyboardLayoutEngine::OnGetControls, weak_factory_.GetWeakPtr()));
  connection_->Flush();
}

void X11KeyboardLayoutEngine::OnGetState(x11::Xkb::GetStateResponse response) {
  if (response) {
    active_group_ = static_cast<uint8_t>(response->group);
  }
}

void X11KeyboardLayoutEngine::OnGetControls(
    x11::Xkb::GetControlsResponse response) {
  if (response) {
    num_groups_ = std::max<uint8_t>(1, response->numGroups);
  }
}

}  // namespace ui
