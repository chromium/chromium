// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_keyboard_layout_engine.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_map.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/layout/scoped_keyboard_layout_engine.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xkb.h"

namespace ui {

namespace {

class X11KeyboardLayoutEngineTest : public testing::Test {
 protected:
  void SetUp() override {
    connection_ = x11::Connection::Get();
    ASSERT_TRUE(connection_);
    ASSERT_TRUE(connection_->Ready());
    ASSERT_TRUE(connection_->xkb().present());

    // Grab the X server before reading or modifying the shared XKB keymap and
    // locked group so parallel test workers do not observe or overwrite the
    // temporary XKB state before TearDown() restores the original state.
    grab_server_ = std::make_unique<x11::ScopedXGrabServer>(connection_);

    const auto device =
        static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd);
    constexpr auto kAllMapParts = static_cast<x11::Xkb::MapPart>(0xff);
    connection_->xkb().SelectEvents(x11::Xkb::SelectEventsRequest{
        .deviceSpec = device,
        .affectWhich = x11::Xkb::EventType::NewKeyboardNotify |
                       x11::Xkb::EventType::MapNotify,
        .selectAll = x11::Xkb::EventType::NewKeyboardNotify |
                     x11::Xkb::EventType::MapNotify,
        .affectMap = kAllMapParts,
        .map = kAllMapParts,
    });

    auto map_reply = connection_->xkb()
                         .GetMap({device, x11::Xkb::MapPart::KeyTypes |
                                              x11::Xkb::MapPart::KeySyms})
                         .Sync();
    ASSERT_TRUE(map_reply);
    orig_map_ = std::move(*map_reply.reply);

    auto state_reply = connection_->xkb().GetState({device}).Sync();
    ASSERT_TRUE(state_reply);
    orig_locked_group_ = state_reply->lockedGroup;

    scoped_engine_ = std::make_unique<ScopedKeyboardLayoutEngine>(
        std::make_unique<X11KeyboardLayoutEngine>());
  }

  void TearDown() override {
    scoped_engine_.reset();
    if (connection_ && orig_map_.syms_rtrn.has_value()) {
      ApplyKeySyms(*orig_map_.syms_rtrn);
      LockGroup(orig_locked_group_);
    }
    grab_server_.reset();
    if (connection_) {
      connection_->Sync();
    }
  }

  void ApplyKeySyms(const std::vector<x11::Xkb::KeySymMap>& syms) {
    uint16_t total_syms = 0;
    for (const auto& entry : syms) {
      total_syms += entry.syms.size();
    }
    const auto device =
        static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd);
    auto set_future = connection_->xkb().SetMap(x11::Xkb::SetMapRequest{
        .deviceSpec = device,
        .flags = x11::Xkb::SetMapFlags::RecomputeActions,
        .minKeyCode = orig_map_.minKeyCode,
        .maxKeyCode = orig_map_.maxKeyCode,
        .firstKeySym = orig_map_.firstKeySym,
        .nKeySyms = orig_map_.nKeySyms,
        .totalSyms = total_syms,
        .syms = syms,
    });
    EXPECT_FALSE(set_future.Sync().error);
    connection_->Sync();
    connection_->DispatchAll();
  }

  void LockGroup(x11::Xkb::Group group) {
    const auto device =
        static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd);
    auto future = connection_->xkb().LatchLockState({
        .deviceSpec = device,
        .lockGroup = 1,
        .groupLock = group,
    });
    EXPECT_FALSE(future.Sync().error);
    connection_->Sync();
    connection_->DispatchAll();
  }

  // Builds a 3-group KeySymMap table based on `orig_map_` where:
  // - DomCode::US_A maps to `a_syms[g]` in group `g`
  // - DomCode::US_Q maps to `q_syms[g]` in group `g`
  // - DomCode::INTL_BACKSLASH stays a 1-group key (`XK_less`) to test XKB
  //   group wrapping in groups > 0.
  std::vector<x11::Xkb::KeySymMap> BuildThreeGroupSyms(
      std::array<uint32_t, 3> a_syms,
      std::array<uint32_t, 3> q_syms) const {
    auto syms = *orig_map_.syms_rtrn;
    const int first = static_cast<int>(orig_map_.firstKeySym);

    auto set_three_group_key = [&](DomCode code,
                                   std::array<uint32_t, 3> group_syms) {
      const int kc = KeycodeConverter::DomCodeToNativeKeycode(code);
      ASSERT_GE(kc, first);
      ASSERT_LT(kc - first, static_cast<int>(syms.size()));
      auto& entry = syms[kc - first];
      entry.kt_index = {0, 0, 0, 0};  // ONE_LEVEL per group
      entry.groupInfo = 3;            // 3 groups, WrapIntoRange (0)
      entry.width = 1;
      entry.syms = {
          static_cast<x11::KeySym>(group_syms[0]),
          static_cast<x11::KeySym>(group_syms[1]),
          static_cast<x11::KeySym>(group_syms[2]),
      };
    };

    set_three_group_key(DomCode::US_A, a_syms);
    set_three_group_key(DomCode::US_Q, q_syms);

    const int intl_kc =
        KeycodeConverter::DomCodeToNativeKeycode(DomCode::INTL_BACKSLASH);
    if (intl_kc >= first && intl_kc - first < static_cast<int>(syms.size())) {
      auto& intl = syms[intl_kc - first];
      intl.kt_index = {0, 0, 0, 0};
      intl.groupInfo = 1;  // Single group; wraps in groups 1 and 2.
      intl.width = 1;
      intl.syms = {static_cast<x11::KeySym>('<')};
    }
    return syms;
  }

  raw_ptr<x11::Connection> connection_ = nullptr;
  std::unique_ptr<x11::ScopedXGrabServer> grab_server_;
  x11::Xkb::GetMapReply orig_map_;
  x11::Xkb::Group orig_locked_group_ = x11::Xkb::Group::c_1;
  std::unique_ptr<ScopedKeyboardLayoutEngine> scoped_engine_;
};

}  // namespace

TEST_F(X11KeyboardLayoutEngineTest, UsesActiveGroupWithAsciiFallback) {
  // Group 0: US ('a', 'q'), Group 1: Dvorak ('a', '\''),
  // Group 2: Cyrillic/non-ASCII (0x06c6 = Cyrillic_ef, 0x06ca = shorti).
  ApplyKeySyms(BuildThreeGroupSyms({'a', 'a', 0x06c6}, {'q', '\'', 0x06ca}));

  LockGroup(x11::Xkb::Group::c_1);
  auto map = GenerateDomKeyboardLayoutMap();
  EXPECT_EQ("q", map["KeyQ"]);
  EXPECT_EQ("<", map["IntlBackslash"]);

  // Switching active group to Dvorak returns "'" for KeyQ and still wraps
  // single-group IntlBackslash to "<".
  LockGroup(x11::Xkb::Group::c_2);
  map = GenerateDomKeyboardLayoutMap();
  EXPECT_EQ("'", map["KeyQ"]);
  EXPECT_EQ("<", map["IntlBackslash"]);

  // Reordering to [Dvorak, US, Cyrillic] while keeping group 1 (now US) active
  // updates the cached XKB map via MapNotify and returns "q".
  ApplyKeySyms(BuildThreeGroupSyms({'a', 'a', 0x06c6}, {'\'', 'q', 0x06ca}));
  LockGroup(x11::Xkb::Group::c_2);
  map = GenerateDomKeyboardLayoutMap();
  EXPECT_EQ("q", map["KeyQ"]);

  // Switching active group to non-ASCII group 2 falls back to group 0 (Dvorak).
  LockGroup(x11::Xkb::Group::c_3);
  map = GenerateDomKeyboardLayoutMap();
  EXPECT_EQ("'", map["KeyQ"]);
}

}  // namespace ui
