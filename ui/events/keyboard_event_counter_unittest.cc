// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keyboard_event_counter.h"

#include <memory>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(KeyboardEventCounterTest, KeyPressCounter) {
  KeyboardEventCounter counter;

  EXPECT_EQ(0u, counter.GetKeyPressCount());

  counter.OnKeyboardEvent(ui::EventType::kKeyPressed, ui::VKEY_0);
  EXPECT_EQ(1u, counter.GetKeyPressCount());

  // Holding the same key without releasing it does not increase the count.
  counter.OnKeyboardEvent(ui::EventType::kKeyPressed, ui::VKEY_0);
  EXPECT_EQ(1u, counter.GetKeyPressCount());

  // Releasing the key does not affect the total count.
  counter.OnKeyboardEvent(ui::EventType::kKeyReleased, ui::VKEY_0);
  EXPECT_EQ(1u, counter.GetKeyPressCount());

  counter.OnKeyboardEvent(ui::EventType::kKeyPressed, ui::VKEY_0);
  counter.OnKeyboardEvent(ui::EventType::kKeyReleased, ui::VKEY_0);
  EXPECT_EQ(2u, counter.GetKeyPressCount());
}

}  // namespace ui
