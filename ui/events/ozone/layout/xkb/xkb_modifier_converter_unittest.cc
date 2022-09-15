// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/xkb/xkb_modifier_converter.h"

#include <xkbcommon/xkbcommon.h>

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/scoped_xkb.h"

namespace ui {

TEST(XkbModifierConverterTest, Basic) {
  XkbModifierConverter converter({
      XKB_MOD_NAME_SHIFT,
      XKB_MOD_NAME_CAPS,
  });

  // Conversion from ui::EventFlags to xkb_mod_mask_t.
  {
    EXPECT_EQ(1u, converter.MaskFromUiFlags(ui::EF_SHIFT_DOWN));
    EXPECT_EQ(2u, converter.MaskFromUiFlags(ui::EF_CAPS_LOCK_ON));

    // Bit-or mask is returned for bit-or ui::EventFlags.
    EXPECT_EQ(
        3u, converter.MaskFromUiFlags(ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_ON));

    // Unknown ui::EventFlags are just ignored.
    EXPECT_EQ(0u, converter.MaskFromUiFlags(ui::EF_LEFT_MOUSE_BUTTON));
    EXPECT_EQ(1u, converter.MaskFromUiFlags(ui::EF_SHIFT_DOWN |
                                            ui::EF_LEFT_MOUSE_BUTTON));
  }

  // Conversion from xkb_mod_mask_t to ui::EventFlags.
  {
    EXPECT_EQ(ui::EF_SHIFT_DOWN, converter.UiFlagsFromMask(1));
    EXPECT_EQ(ui::EF_CAPS_LOCK_ON, converter.UiFlagsFromMask(2));

    // Bit-or ui::EventFlags is returned for bit-or mask.
    EXPECT_EQ(ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_ON,
              converter.UiFlagsFromMask(3));

    // Unknown mask is just ignored.
    EXPECT_EQ(0, converter.UiFlagsFromMask(8));
    EXPECT_EQ(ui::EF_SHIFT_DOWN, converter.UiFlagsFromMask(9));
  }

  // Look up by names.
  EXPECT_EQ(0u, converter.MaskFromNames({}));
  EXPECT_EQ(1u, converter.MaskFromNames({XKB_MOD_NAME_SHIFT}));
  EXPECT_EQ(2u, converter.MaskFromNames({XKB_MOD_NAME_CAPS}));
  EXPECT_EQ(3u,
            converter.MaskFromNames({XKB_MOD_NAME_SHIFT, XKB_MOD_NAME_CAPS}));

  // Unknown is ignored.
  EXPECT_EQ(0u, converter.MaskFromNames({XKB_MOD_NAME_NUM}));
}

TEST(XkbModifierConverterTest, FromKeymap) {
  std::unique_ptr<xkb_context, ui::XkbContextDeleter> xkb_context(
      xkb_context_new(XKB_CONTEXT_NO_FLAGS));
  xkb_rule_names names = {
      .rules = nullptr,
      .model = "pc101",
      .layout = "us",
      .variant = "",
      .options = "",
  };
  std::unique_ptr<xkb_keymap, ui::XkbKeymapDeleter> xkb_keymap(
      xkb_keymap_new_from_names(xkb_context.get(), &names,
                                XKB_KEYMAP_COMPILE_NO_FLAGS));

  XkbModifierConverter converter =
      XkbModifierConverter::CreateFromKeymap(xkb_keymap.get());

  // Making sure the data is consistent with X.h.
  EXPECT_EQ(1u << 0, converter.MaskFromUiFlags(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(1u << 1, converter.MaskFromUiFlags(ui::EF_CAPS_LOCK_ON));
  EXPECT_EQ(1u << 2, converter.MaskFromUiFlags(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(1u << 3, converter.MaskFromUiFlags(ui::EF_ALT_DOWN));
  EXPECT_EQ(1u << 4, converter.MaskFromUiFlags(ui::EF_NUM_LOCK_ON));
  EXPECT_EQ(1u << 5, converter.MaskFromUiFlags(ui::EF_MOD3_DOWN));
  EXPECT_EQ(1u << 6, converter.MaskFromUiFlags(ui::EF_COMMAND_DOWN));
  EXPECT_EQ(1u << 7, converter.MaskFromUiFlags(ui::EF_ALTGR_DOWN));
}

}  // namespace ui
