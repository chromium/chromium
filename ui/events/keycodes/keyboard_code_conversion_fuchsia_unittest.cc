// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion_fuchsia.h"

#include <fidl/fuchsia.ui.input3/cpp/fidl.h>
#include <cstdint>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace {

using fuchsia_input::Key;
using fuchsia_ui_input3::KeyMeaning;
using fuchsia_ui_input3::NonPrintableKey;

#define EXPECT_CODEPOINT_MAPS(codepoint)                                     \
  EXPECT_EQ(                                                                 \
      ui::DomKey::FromCharacter(codepoint),                                  \
      ui::DomKeyFromFuchsiaKeyMeaning(KeyMeaning::WithCodepoint(codepoint))) \
      << " for codepoint " << codepoint;

// For |size| Unicode values beginning with code |start|, check that they are
// converted to the correct DomKey.
void CheckConversionsInRange(int start, int size) {
  for (int i = 0; i < size; i++)
    EXPECT_CODEPOINT_MAPS(start + i);
}

// Check that the for the below ranges, Fuchsia Keys are correctly converted to
// DomKeys.
TEST(FuchsiaKeyboardCodeConversion, FuchsiaKeyToDomKeyConversionRanges) {
  // Check digits 1 through 0.
  CheckConversionsInRange(0x0030, 10);

  // Check capital letters.
  CheckConversionsInRange(0x0041, 26);

  // Check lower case letters.
  CheckConversionsInRange(0x0061, 26);
}

TEST(FuchsiaKeyboardCodeConversion, FuchsiaKeyToDomKeySpecificValues) {
  // Yen sign.
  EXPECT_EQ(ui::DomKey::FromCharacter(0x00a5),
            ui::DomKeyFromFuchsiaKeyMeaning(KeyMeaning::WithCodepoint(165)));

  // Right brace.
  EXPECT_EQ(ui::DomKey::FromCharacter(0x007d),
            ui::DomKeyFromFuchsiaKeyMeaning(KeyMeaning::WithCodepoint(125)));

  // Lower case c with circumflex.
  EXPECT_EQ(ui::DomKey::FromCharacter(0x0109),
            ui::DomKeyFromFuchsiaKeyMeaning(KeyMeaning::WithCodepoint(265)));

  // Check that NonPrintableKeys are converted correctly.
  EXPECT_EQ(ui::DomKey::ENTER,
            ui::DomKeyFromFuchsiaKeyMeaning(
                KeyMeaning::WithNonPrintableKey(NonPrintableKey::kEnter)));
}

}  // namespace
