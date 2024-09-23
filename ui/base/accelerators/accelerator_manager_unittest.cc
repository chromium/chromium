// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/accelerators/accelerator_manager.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
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
  for (size_t i = 0; i < std::size(kAcceleratorModifiers); ++i) {
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
  for (size_t i = 0; i < (1 << std::size(kAcceleratorModifiers)); ++i) {
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

    for (size_t test_i = 0; test_i < (1 << std::size(kAcceleratorModifiers));
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

#if BUILDFLAG(IS_CHROMEOS)

TEST_F(AcceleratorManagerTest, PositionalShortcuts_AllEqual) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kImprovedKeyboardShortcuts);

  // Use a local instance so that the feature is enabled during construction.
  AcceleratorManager manager;
  manager.SetUsePositionalLookup(true);

  // Test what would be ctrl + ']' (VKEY_OEM_6) on a US keyboard. This
  // should match.
  TestAcceleratorTarget target;
  const Accelerator accelerator(VKEY_OEM_6, EF_CONTROL_DOWN);
  manager.Register({accelerator}, AcceleratorManager::kNormalPriority, &target);
  KeyEvent event(ui::EventType::kKeyPressed, VKEY_OEM_6,
                 ui::DomCode::BRACKET_RIGHT, ui::EF_CONTROL_DOWN,
                 ui::DomKey::FromCharacter(']'), base::TimeTicks());
  const Accelerator trigger(event);
  EXPECT_TRUE(manager.IsRegistered(trigger));
  EXPECT_TRUE(manager.Process(trigger));
}

TEST_F(AcceleratorManagerTest, PositionalShortcuts_MatchingDomCode) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kImprovedKeyboardShortcuts);

  // Use a local instance so that the feature is enabled during construction.
  AcceleratorManager manager;
  manager.SetUsePositionalLookup(true);

  // Test what would be ctrl + ']' on a US keyboard with matching DomCode
  // and different VKEY (eg. '+'). This is the use case of a positional key
  // on the German keyboard. Since the DomCode matches, this should match.
  TestAcceleratorTarget target;
  const Accelerator accelerator(VKEY_OEM_6, EF_CONTROL_DOWN);
  manager.Register({accelerator}, AcceleratorManager::kNormalPriority, &target);
  KeyEvent event(ui::EventType::kKeyPressed, VKEY_OEM_PLUS,
                 ui::DomCode::BRACKET_RIGHT, ui::EF_CONTROL_DOWN,
                 ui::DomKey::FromCharacter(']'), base::TimeTicks());
  const Accelerator trigger(event);
  EXPECT_TRUE(manager.IsRegistered(trigger));
  EXPECT_TRUE(manager.Process(trigger));
}

TEST_F(AcceleratorManagerTest, PositionalShortcuts_NotMatchingDomCode) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kImprovedKeyboardShortcuts);

  // Use a local instance so that the feature is enabled during construction.
  AcceleratorManager manager;
  manager.SetUsePositionalLookup(true);

  // Test what would be ctrl + ']' on a US keyboard using positional mapping
  // for a German layout. The accelerator is registered using the US VKEY and
  // triggered with a KeyEvent with the US VKEY but a mismatched DomCode. This
  // should not match. This prevents ghost shortcuts on non-US layouts.
  TestAcceleratorTarget target;
  const Accelerator accelerator(VKEY_OEM_6, EF_CONTROL_DOWN);
  manager.Register({accelerator}, AcceleratorManager::kNormalPriority, &target);
  KeyEvent event(ui::EventType::kKeyPressed, VKEY_OEM_6,
                 ui::DomCode::BRACKET_LEFT, ui::EF_CONTROL_DOWN,
                 ui::DomKey::FromCharacter(']'), base::TimeTicks());
  const Accelerator trigger(event);
  EXPECT_FALSE(manager.IsRegistered(trigger));
  EXPECT_FALSE(manager.Process(trigger));
}

TEST_F(AcceleratorManagerTest, PositionalShortcuts_NonPositionalMatch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kImprovedKeyboardShortcuts);

  // Use a local instance so that the feature is enabled during construction.
  AcceleratorManager manager;
  manager.SetUsePositionalLookup(true);

  // Test ctrl + 'Z' for the German layout. Since 'Z' is not a positional
  // key it should match based on the VKEY, regardless of the DomCode. In this
  // case the 'Z' has DomCode US_Y (ie. QWERTZ keyboard), but it should still
  // match.
  TestAcceleratorTarget target;
  const Accelerator accelerator(VKEY_Z, EF_CONTROL_DOWN);
  manager.Register({accelerator}, AcceleratorManager::kNormalPriority, &target);
  KeyEvent event(ui::EventType::kKeyPressed, VKEY_Z, ui::DomCode::US_Y,
                 ui::EF_CONTROL_DOWN, ui::DomKey::FromCharacter(']'),
                 base::TimeTicks());
  const Accelerator trigger(event);
  EXPECT_TRUE(manager.IsRegistered(trigger));
  EXPECT_TRUE(manager.Process(trigger));
}

TEST_F(AcceleratorManagerTest, PositionalShortcuts_NonPositionalNonMatch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kImprovedKeyboardShortcuts);

  // Use a local instance so that the feature is enabled during construction.
  AcceleratorManager manager;
  manager.SetUsePositionalLookup(true);

  // Test ctrl + 'Z' for the German layout. The 'Y' key (in the US_Z position),
  // should not match. Alphanumeric keys are not positional, and pressing the
  // key with DomCode::US_Z should not match when it's mapped to VKEY_Y.
  TestAcceleratorTarget target;
  const Accelerator accelerator(VKEY_Z, EF_CONTROL_DOWN);
  manager.Register({accelerator}, AcceleratorManager::kNormalPriority, &target);
  KeyEvent event(ui::EventType::kKeyPressed, VKEY_Y, ui::DomCode::US_Z,
                 ui::EF_CONTROL_DOWN, ui::DomKey::FromCharacter(']'),
                 base::TimeTicks());
  const Accelerator trigger(event);
  EXPECT_FALSE(manager.IsRegistered(trigger));
  EXPECT_FALSE(manager.Process(trigger));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace test
}  // namespace ui
