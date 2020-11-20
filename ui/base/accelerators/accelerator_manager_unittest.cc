// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator_manager.h"

#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
namespace test {
namespace {

Accelerator GetAccelerator(KeyboardCode code, int mask) {
  return Accelerator(code, mask);
}

// Possible flags used for accelerators.
const int kAcceleratorModifiers[] = {EF_SHIFT_DOWN, EF_CONTROL_DOWN,
                                     EF_ALT_DOWN, EF_COMMAND_DOWN};

// Returns a set of flags from id, where id is a bitmask into
// kAcceleratorModifiers used to determine which flags are set.
int BuildAcceleratorModifier(int id) {
  int result = 0;
  for (size_t i = 0; i < base::size(kAcceleratorModifiers); ++i) {
    if (((1 << i) & id) != 0)
      result |= kAcceleratorModifiers[i];
  }
  return result;
}

class AcceleratorManagerTest : public testing::Test {
 public:
  AcceleratorManagerTest() = default;
  ~AcceleratorManagerTest() override = default;

 protected:
  AcceleratorManager manager_;
};

TEST_F(AcceleratorManagerTest, Register) {
  TestAcceleratorTarget target;
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  const Accelerator accelerator_b(VKEY_B, EF_NONE);
  const Accelerator accelerator_c(VKEY_C, EF_NONE);
  const Accelerator accelerator_d(VKEY_D, EF_NONE);
  manager_.Register(
      {accelerator_a, accelerator_b, accelerator_c, accelerator_d},
      AcceleratorManager::kNormalPriority, &target);

  // The registered accelerators are processed.
  EXPECT_TRUE(manager_.Process(accelerator_a));
  EXPECT_TRUE(manager_.Process(accelerator_b));
  EXPECT_TRUE(manager_.Process(accelerator_c));
  EXPECT_TRUE(manager_.Process(accelerator_d));
  EXPECT_EQ(4, target.accelerator_count());
}

TEST_F(AcceleratorManagerTest, RegisterMultipleTarget) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  TestAcceleratorTarget target1;
  manager_.Register({accelerator_a}, AcceleratorManager::kNormalPriority,
                    &target1);
  TestAcceleratorTarget target2;
  manager_.Register({accelerator_a}, AcceleratorManager::kNormalPriority,
                    &target2);

  // If multiple targets are registered with the same accelerator, the target
  // registered later processes the accelerator.
  EXPECT_TRUE(manager_.Process(accelerator_a));
  EXPECT_EQ(0, target1.accelerator_count());
  EXPECT_EQ(1, target2.accelerator_count());
}

TEST_F(AcceleratorManagerTest, Unregister) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  TestAcceleratorTarget target;
  const Accelerator accelerator_b(VKEY_B, EF_NONE);
  manager_.Register({accelerator_a, accelerator_b},
                    AcceleratorManager::kNormalPriority, &target);

  // Unregistering a different accelerator does not affect the other
  // accelerator.
  manager_.Unregister(accelerator_b, &target);
  EXPECT_TRUE(manager_.Process(accelerator_a));
  EXPECT_EQ(1, target.accelerator_count());

  // The unregistered accelerator is no longer processed.
  target.ResetCounts();
  manager_.Unregister(accelerator_a, &target);
  EXPECT_FALSE(manager_.Process(accelerator_a));
  EXPECT_EQ(0, target.accelerator_count());
}

TEST_F(AcceleratorManagerTest, UnregisterAll) {
  const Accelerator accelerator_a(VKEY_A, EF_NONE);
  TestAcceleratorTarget target1;
  const Accelerator accelerator_b(VKEY_B, EF_NONE);
  manager_.Register({accelerator_a, accelerator_b},
                    AcceleratorManager::kNormalPriority, &target1);

  const Accelerator accelerator_c(VKEY_C, EF_NONE);
  TestAcceleratorTarget target2;
  manager_.Register({accelerator_c}, AcceleratorManager::kNormalPriority,
                    &target2);

  manager_.UnregisterAll(&target1);

  // All the accelerators registered for |target1| are no longer processed.
  EXPECT_FALSE(manager_.Process(accelerator_a));
  EXPECT_FALSE(manager_.Process(accelerator_b));
  EXPECT_EQ(0, target1.accelerator_count());

  // UnregisterAll with a different target does not affect the other target.
  EXPECT_TRUE(manager_.Process(accelerator_c));
  EXPECT_EQ(1, target2.accelerator_count());
}

TEST_F(AcceleratorManagerTest, Process) {
  TestAcceleratorTarget target;

  // Test all cases of possible modifiers.
  for (size_t i = 0; i < (1 << base::size(kAcceleratorModifiers)); ++i) {
    const int modifiers = BuildAcceleratorModifier(i);
    Accelerator accelerator(GetAccelerator(VKEY_A, modifiers));
    manager_.Register({accelerator}, AcceleratorManager::kNormalPriority,
                      &target);

    // The registered accelerator is processed.
    const int last_count = target.accelerator_count();
    EXPECT_TRUE(manager_.Process(accelerator)) << i;
    EXPECT_EQ(last_count + 1, target.accelerator_count()) << i;

    // The non-registered accelerators are not processed.
    accelerator.set_key_state(Accelerator::KeyState::RELEASED);
    EXPECT_FALSE(manager_.Process(accelerator)) << i;  // different type

    EXPECT_FALSE(manager_.Process(GetAccelerator(VKEY_UNKNOWN, modifiers)))
        << i;  // different vkey
    EXPECT_FALSE(manager_.Process(GetAccelerator(VKEY_B, modifiers)))
        << i;  // different vkey
    EXPECT_FALSE(manager_.Process(GetAccelerator(VKEY_SHIFT, modifiers)))
        << i;  // different vkey

    for (size_t test_i = 0; test_i < (1 << base::size(kAcceleratorModifiers));
         ++test_i) {
      if (test_i == i)
        continue;
      const int test_modifiers = BuildAcceleratorModifier(test_i);
      const Accelerator test_accelerator(
          GetAccelerator(VKEY_A, test_modifiers));
      EXPECT_FALSE(manager_.Process(test_accelerator)) << " i=" << i
                                                       << " test_i=" << test_i;
    }

    EXPECT_EQ(last_count + 1, target.accelerator_count()) << i;
    manager_.UnregisterAll(&target);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(AcceleratorManagerTest, NewMapping) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNewShortcutMapping);

  // Test new mapping with a ASCII punctuation shortcut that doesn't involve
  // shift.
  TestAcceleratorTarget target;
  {
    // ']' + ctrl
    const Accelerator accelerator(VKEY_OEM_6, EF_CONTROL_DOWN);
    manager_.Register({accelerator}, AcceleratorManager::kNormalPriority,
                      &target);
    KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_1, ui::DomCode::NONE,
                   ui::EF_CONTROL_DOWN, ui::DomKey::Constant<']'>::Character,
                   base::TimeTicks());
    const Accelerator trigger(event);
    EXPECT_TRUE(manager_.IsRegistered(trigger));
    EXPECT_TRUE(manager_.Process(trigger));
  }

  // Test new mapping with a ASCII punctuation shortcut that involves shift.
  {
    // ']' + ctrl + shift, which produces '}' on US layout.
    const Accelerator accelerator(VKEY_OEM_6, EF_CONTROL_DOWN | EF_SHIFT_DOWN);
    manager_.Register({accelerator}, AcceleratorManager::kNormalPriority,
                      &target);
    KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_1, ui::DomCode::NONE,
                   ui::EF_CONTROL_DOWN, ui::DomKey::Constant<'}'>::Character,
                   base::TimeTicks());
    const Accelerator trigger(event);
    EXPECT_TRUE(manager_.IsRegistered(trigger));
    EXPECT_TRUE(manager_.Process(trigger));
  }
}
#endif

}  // namespace
}  // namespace test
}  // namespace ui
