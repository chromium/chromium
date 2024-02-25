// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/keyboard_state.h"

#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/keysyms/keysyms.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

namespace {

constexpr KeySym kNoSymbol = static_cast<KeySym>(0);

void ConvertCaseImpl(uint32_t sym, uint32_t* lower, uint32_t* upper);

void ConvertCase(KeySym sym, KeySym* lower, KeySym* upper) {
  uint32_t lower32;
  uint32_t upper32;
  ConvertCaseImpl(static_cast<uint32_t>(sym), &lower32, &upper32);
  *lower = static_cast<KeySym>(lower32);
  *upper = static_cast<KeySym>(upper32);
}

bool IsPublicOrPrivateKeypadKey(KeySym keysym) {
  auto key = static_cast<uint32_t>(keysym);
  return (key >= XK_KP_Space && key <= XK_KP_Equal) ||
         (key >= 0x11000000 && key <= 0x1100FFFF);
}

int GetXkbGroupFromState(int state) {
  return (state >> 13) & 0x3;
}

#include "third_party/libx11/src/KeyBind.c"
#include "third_party/libx11/src/xkb/XKBBind.c"
#include "third_party/libxcb-keysyms/keysyms/keysyms.c"

}  // namespace

KeyboardState::KeyboardState() = default;

KeyboardState::~KeyboardState() = default;

// Non-XKB (core protocol) implementation of KeysymToKeycode and
// KeycodeToKeysym.
class CoreKeyboardState : public KeyboardState {
 public:
  explicit CoreKeyboardState(Connection* connection) : connection_(connection) {
    UpdateMapping();
  }

  ~CoreKeyboardState() override = default;

  KeyCode KeysymToKeycode(uint32_t keysym) const override {
    auto min_keycode = static_cast<uint8_t>(connection_->setup().min_keycode);
    auto max_keycode = static_cast<uint8_t>(connection_->setup().max_keycode);
    int count = max_keycode - min_keycode + 1;
    CHECK_EQ(count * keyboard_mapping_.keysyms_per_keycode,
             static_cast<int>(keyboard_mapping_.keysyms.size()));
    for (size_t i = 0; i < keyboard_mapping_.keysyms.size(); i++) {
      auto keycode = min_keycode + i / keyboard_mapping_.keysyms_per_keycode;
      if (keyboard_mapping_.keysyms[i] == static_cast<KeySym>(keysym))
        return static_cast<KeyCode>(keycode);
    }
    return {};
  }

  uint32_t KeycodeToKeysym(KeyCode keycode, uint32_t modifiers) const override {
    auto sym = static_cast<uint32_t>(KeycodeToKeysymCoreImpl(
        keycode, modifiers, connection_, keyboard_mapping_, lock_meaning_,
        mode_switch_, num_lock_));
    return sym == XK_VoidSymbol ? 0 : sym;
  }

 private:
  void UpdateMapping() override {
    UpdateMappingImpl(connection_, &keyboard_mapping_, &lock_meaning_,
                      &mode_switch_, &num_lock_);
  }

  const raw_ptr<Connection> connection_;
  GetKeyboardMappingReply keyboard_mapping_;
  uint16_t lock_meaning_ = 0;
  uint8_t mode_switch_ = 0;
  uint8_t num_lock_ = 0;
};

// XKB implementation of KeysymToKeycode and KeycodeToKeysym.
class XkbKeyboardState : public KeyboardState {
 public:
  explicit XkbKeyboardState(Connection* connection) : connection_(connection) {
    UpdateMapping();
  }

  ~XkbKeyboardState() override = default;

  KeyCode KeysymToKeycode(uint32_t keysym) const override {
    int first_keycode = static_cast<int>(map_.firstKeySym);
    for (int keycode = 0; keycode < map_.nKeySyms; keycode++) {
      for (auto sym : map_.syms_rtrn->at(keycode).syms) {
        if (static_cast<uint32_t>(sym) == keysym)
          return static_cast<KeyCode>(keycode + first_keycode);
      }
    }
    return {};
  }

  uint32_t KeycodeToKeysym(KeyCode key, uint32_t modifiers) const override {
    return KeycodeToKeysymXkbImpl(key, modifiers, map_);
  }

 private:
  void UpdateMapping() override {
    auto future = connection_->xkb().GetMap(
        {static_cast<Xkb::DeviceSpec>(Xkb::Id::UseCoreKbd),
         Xkb::MapPart::KeyTypes | Xkb::MapPart::KeySyms});
    if (auto response = future.Sync())
      map_ = std::move(*response.reply);
  }

  const raw_ptr<Connection> connection_;
  Xkb::GetMapReply map_;
};

std::unique_ptr<KeyboardState> CreateKeyboardState(Connection* connection) {
  if (connection->xkb().present())
    return std::make_unique<XkbKeyboardState>(connection);
  return std::make_unique<CoreKeyboardState>(connection);
}

}  // namespace x11
