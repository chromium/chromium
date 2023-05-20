// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/accelerators/platform_accelerator_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "ui/base/accelerators/accelerator.h"

TEST(PlatformAcceleratorCocoaTest,
     GetKeyEquivalentAndModifierMaskFromAccelerator) {
  static const struct {
    ui::Accelerator accelerator;
    NSString* expected_key_equivalent;
    NSUInteger expected_modifier_mask;
  } kTestCases[] = {
      {{ui::VKEY_OEM_PLUS, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN},
       @"+",
       NSEventModifierFlagCommand},
      {{ui::VKEY_T, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN},
       @"T",
       NSEventModifierFlagCommand},
      {{ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN},
       // \x08 is kBackspaceCharCode in Carbon, which NSMenuItem will translate
       // into U+232B.
       @"\x08",
       NSEventModifierFlagCommand | NSEventModifierFlagShift},
      {{ui::VKEY_F,
        ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN | ui::EF_FUNCTION_DOWN},
       @"f",
       NSEventModifierFlagCommand | NSEventModifierFlagControl |
           NSEventModifierFlagFunction},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(base::StringPrintf("key_code='%c', modifiers=0x%x",
                                    test.accelerator.key_code(),
                                    test.accelerator.modifiers()));
    KeyEquivalentAndModifierMask* equivalent =
        ui::GetKeyEquivalentAndModifierMaskFromAccelerator(test.accelerator);
    EXPECT_NSEQ(test.expected_key_equivalent, equivalent.keyEquivalent);
    EXPECT_EQ(test.expected_modifier_mask, equivalent.modifierMask);
  }
}
