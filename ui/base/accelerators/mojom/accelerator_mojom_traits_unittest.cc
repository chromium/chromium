// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/mojom/accelerator_mojom_traits.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/mojom/accelerator.mojom.h"
#include "ui/events/event_constants.h"

namespace ui {

TEST(AcceleratorStructTraitsTest, SerializeAndDeserialize1) {
  Accelerator accelerator(
      KeyboardCode::VKEY_TAB, EF_NUM_LOCK_ON,
      ui::Accelerator::KeyState::RELEASED,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
  Accelerator deserialized;
  ASSERT_TRUE(mojom::Accelerator::Deserialize(
      mojom::Accelerator::Serialize(&accelerator), &deserialized));
  EXPECT_EQ(accelerator, deserialized);
}

TEST(AcceleratorStructTraitsTest, SerializeAndDeserialize2) {
  const Accelerator accelerator(KeyboardCode::VKEY_SPACE, EF_SHIFT_DOWN);
  Accelerator deserialized;
  ASSERT_TRUE(mojom::Accelerator::Deserialize(
      mojom::Accelerator::Serialize(&accelerator), &deserialized));
  EXPECT_EQ(accelerator, deserialized);
}

}  // namespace ui
