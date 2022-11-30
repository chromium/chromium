// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator_map.h"

#include <utility>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

namespace {

bool IsValidMatch(AcceleratorMap<int>* map,
                  const Accelerator& pressed,
                  int expected) {
  return nullptr != map->Find(pressed) && expected == *map->Find(pressed) &&
         expected == map->Get(pressed);
}

TEST(AcceleratorMapTest, MapIsEmpty) {
  AcceleratorMap<int> m;
  EXPECT_EQ(0U, m.size());
  EXPECT_TRUE(m.empty());
}

TEST(AcceleratorMapTest, EmptyFind) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);
  EXPECT_EQ(nullptr, m.Find(accelerator));

  // Still empty after lookup.
  EXPECT_TRUE(m.empty());
}

TEST(AcceleratorMapTest, FindExists) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);

  const int expected = 77;
  m.InsertNew(std::make_pair(accelerator, expected));
  EXPECT_TRUE(IsValidMatch(&m, accelerator, expected));

  // Still single entry.
  EXPECT_EQ(1U, m.size());
}

TEST(AcceleratorMapTest, FindDoesNotExist) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);
  Accelerator other(VKEY_Y, EF_SHIFT_DOWN);

  const int expected = 77;
  m.InsertNew(std::make_pair(accelerator, expected));
  EXPECT_EQ(nullptr, m.Find(other));

  // Still single entry.
  EXPECT_EQ(1U, m.size());
}

TEST(AcceleratorMapTest, InsertDefaultCreatedNew) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);
  int& value_ref = m.GetOrInsertDefault(accelerator);
  EXPECT_EQ(int(), value_ref);
  EXPECT_EQ(1U, m.size());
}

TEST(AcceleratorMapTest, ChangeValueViaReturnedRef) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);
  int& value_ref = m.GetOrInsertDefault(accelerator);

  const int expected = 77;
  value_ref = expected;
  EXPECT_EQ(expected, m.Get(accelerator));
}

TEST(AcceleratorMapTest, SetValueDirect) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);

  const int expected = 77;
  m.InsertNew(std::make_pair(accelerator, expected));
  EXPECT_EQ(expected, m.Get(accelerator));
}

TEST(AcceleratorMapTest, Iterate) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);
  const int expected = 77;
  m.InsertNew(std::make_pair(accelerator, expected));

  auto iter = m.begin();
  EXPECT_NE(m.end(), iter);
  EXPECT_EQ(accelerator, iter->first);
  EXPECT_EQ(expected, iter->second);
  ++iter;
  EXPECT_EQ(m.end(), iter);
}

// Chrome OS specific tests.
// Only Chrome OS supports positional shortcuts.
#if BUILDFLAG(IS_CHROMEOS)

// Even with positional lookup enabled, if both the stored and lookup
// accelerator have no DomCode then the behavior is as if there was no
// positional lookup.
TEST(AcceleratorMapTest, PositionalLookupExistsVkeyOnly) {
  AcceleratorMap<int> m;
  m.set_use_positional_lookup(true);
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);
  EXPECT_EQ(DomCode::NONE, accelerator.code());

  const int expected = 77;
  m.InsertNew(std::make_pair(accelerator, expected));
  EXPECT_TRUE(IsValidMatch(&m, accelerator, expected));
}

// Both the VKEY and DomCode match, so this is always a match. This scenario
// happens when positional shortcuts are enabled, and the layout has a VKEY
// mapping consistent with the US layout.
TEST(AcceleratorMapTest, PositionalLookupExistsFullMatch) {
  AcceleratorMap<int> m;
  m.set_use_positional_lookup(true);

  Accelerator registered(VKEY_OEM_6, EF_SHIFT_DOWN);
  const int expected = 77;
  m.InsertNew(std::make_pair(registered, expected));

  Accelerator pressed(VKEY_OEM_6, DomCode::BRACKET_RIGHT, EF_SHIFT_DOWN);
  EXPECT_TRUE(IsValidMatch(&m, pressed, expected));
}

// The DomCode matches, but the VKEY does not - this is a positional match.
// This scenario happens on eg. German and Spanish keyboards.
TEST(AcceleratorMapTest, PositionalLookupDomCodeMatchOnly) {
  AcceleratorMap<int> m;
  m.set_use_positional_lookup(true);

  Accelerator registered(VKEY_OEM_6, EF_SHIFT_DOWN);
  const int expected = 77;
  m.InsertNew(std::make_pair(registered, expected));

  Accelerator pressed(VKEY_OEM_PLUS, DomCode::BRACKET_RIGHT, EF_SHIFT_DOWN);
  EXPECT_TRUE(IsValidMatch(&m, pressed, expected));
}

// With positional mapping enabled this first press is like the ']' key
// on a German keyboard, and it matches. When positional mapping is disabled
// it no longer matches because the VKEYs are different.
//
// Disabling positional lookup also used for special layouts like Dvorak which
// are designed to intentionally reposition certain punctuation keys. These
// layouts already work with US-like VKEY mapping, albeit to keys in different
// positions.
TEST(AcceleratorMapTest, PositionalLookupDisabled) {
  AcceleratorMap<int> m;

  // NOTE: The state of use_positional_lookup_ has no effect on insertion.
  Accelerator registered(VKEY_OEM_6, EF_SHIFT_DOWN);
  const int expected = 77;
  m.InsertNew(std::make_pair(registered, expected));

  // This lookup with succeed with positional lookup enabled.
  m.set_use_positional_lookup(true);
  Accelerator pressed(VKEY_OEM_PLUS, DomCode::BRACKET_RIGHT, EF_SHIFT_DOWN);
  EXPECT_TRUE(IsValidMatch(&m, pressed, expected));

  // Switch to a non-positional layout before testing the pressed keys and
  // this lookup with fail.
  m.set_use_positional_lookup(false);
  EXPECT_FALSE(IsValidMatch(&m, pressed, expected));
}

// The VKEY matches, and both the registered and pressed accelerator supply a
// positional DomCode - this is not a match. This prevents ghost or conflicting
// shortcuts on the key that has the matching VKEY. This is a scenario on a
// Spanish keyboard.
TEST(AcceleratorMapTest, PositionalLookupVkeyMatchOnlyBothDomCodesSpecified) {
  AcceleratorMap<int> m;
  m.set_use_positional_lookup(true);
  Accelerator registered(VKEY_OEM_6, EF_SHIFT_DOWN);
  const int expected = 77;
  m.InsertNew(std::make_pair(registered, expected));

  Accelerator pressed(VKEY_OEM_6, DomCode::EQUAL, EF_SHIFT_DOWN);
  EXPECT_FALSE(IsValidMatch(&m, pressed, expected));
}

// The VKEY matches, and the registered accelerator has no DomCode (ie. it's
// non-positional) - this is a match. This scenario is to allow non-positional
// shortcuts to continue to work regardless of the DomCode. This is a scenario
// for the Z key on a German or other QWERTZ layout.
TEST(AcceleratorMapTest, PositionalLookupVkeyMatchOnlyRegisteredDomCodeIsNone) {
  AcceleratorMap<int> m;
  m.set_use_positional_lookup(true);
  Accelerator registered(VKEY_Z, EF_SHIFT_DOWN);
  const int expected = 77;
  m.InsertNew(std::make_pair(registered, expected));

  Accelerator pressed(VKEY_Z, DomCode::US_Y, EF_SHIFT_DOWN);
  EXPECT_TRUE(IsValidMatch(&m, pressed, expected));
}

// When an accelerator is inserted to the map, if it contains a DomCode it
// should be stripped out.
TEST(AcceleratorMapTest, DomCodesStrippedWhenInserted) {
  AcceleratorMap<int> m;
  m.set_use_positional_lookup(true);

  // Verify the accelerator has a DomCode and insert it.
  Accelerator accelerator(ui::VKEY_F, DomCode::US_F,
                          ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);
  EXPECT_EQ(accelerator.code(), DomCode::US_F);
  const int expected = 77;
  m.InsertNew(std::make_pair(accelerator, expected));

  // Reset the DomCode on the accelerator and perform a lookup and verify
  // that it can still be found.
  accelerator.reset_code();
  auto* value = m.Find(accelerator);
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, expected);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

}  // namespace ui
