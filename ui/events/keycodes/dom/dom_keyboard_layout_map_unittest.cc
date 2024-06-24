// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_manager.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_map_base.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

namespace {

// Represents a keyboard layout where every key is ASCII capable.
constexpr uint32_t kKeyboardLayoutWithAllValidKeys = 1;
// Layout contains all valid keys so this number represents a complete layout.
constexpr uint32_t kKeyboardLayoutWithAllValidKeysCount = 50;

// Represents a keyboard layout where every alpha key is ASCII capable.
constexpr uint32_t kKeyboardLayoutWithAllValidAlphaKeys = 2;
// Layout contains all valid alpha keys so this number represents a complete
// layout minus two INTL keys which are subbed.
constexpr uint32_t kKeyboardLayoutWithAllValidAlphaKeysCount = 48;

// Represents a keyboard layout where some alpha keys are ASCII capable.
constexpr uint32_t kKeyboardLayoutWithSomeValidKeys = 3;
// The mostly valid keyboard does not pass the 'IsAsciiCapable()' test however
// the DomKeyboardLayoutManager will pass the first layout it has if it doesn't
// find an ASCII capable layout first.
constexpr uint32_t kKeyboardLayoutWithSomeValidKeysCount = 47;

// Represents a keyboard layout where no alpha keys are ASCII capable.
constexpr uint32_t kKeyboardLayoutWithNoValidKeys = 4;
constexpr uint32_t kKeyboardLayoutWithNoValidKeysCount = 0;

DomKey GetKeyFromFullLookupTable(DomCode dom_code) {
  // Includes all DomCodes used in |writing_system_key_domcodes|.  Tests can
  // use this table to retrieve valid DomKeys for each DomCode to simulate a
  // 'complete' keyboard layout.  They can also use this to construct a partial
  // layout with valid DomKey values for the populated entries.
  static base::flat_map<DomCode, DomKey> kFullLookupTable(
      {{DomCode::DIGIT0, DomKey::FromCharacter('0')},
       {DomCode::DIGIT1, DomKey::FromCharacter('1')},
       {DomCode::DIGIT2, DomKey::FromCharacter('2')},
       {DomCode::DIGIT3, DomKey::FromCharacter('3')},
       {DomCode::DIGIT4, DomKey::FromCharacter('4')},
       {DomCode::DIGIT5, DomKey::FromCharacter('5')},
       {DomCode::DIGIT6, DomKey::FromCharacter('6')},
       {DomCode::DIGIT7, DomKey::FromCharacter('7')},
       {DomCode::DIGIT8, DomKey::FromCharacter('8')},
       {DomCode::DIGIT9, DomKey::FromCharacter('9')},
       {DomCode::US_A, DomKey::FromCharacter('a')},
       {DomCode::US_B, DomKey::FromCharacter('b')},
       {DomCode::US_C, DomKey::FromCharacter('c')},
       {DomCode::US_D, DomKey::FromCharacter('d')},
       {DomCode::US_E, DomKey::FromCharacter('e')},
       {DomCode::US_F, DomKey::FromCharacter('f')},
       {DomCode::US_G, DomKey::FromCharacter('g')},
       {DomCode::US_H, DomKey::FromCharacter('h')},
       {DomCode::US_I, DomKey::FromCharacter('i')},
       {DomCode::US_J, DomKey::FromCharacter('j')},
       {DomCode::US_K, DomKey::FromCharacter('k')},
       {DomCode::US_L, DomKey::FromCharacter('l')},
       {DomCode::US_M, DomKey::FromCharacter('m')},
       {DomCode::US_N, DomKey::FromCharacter('n')},
       {DomCode::US_O, DomKey::FromCharacter('o')},
       {DomCode::US_P, DomKey::FromCharacter('p')},
       {DomCode::US_Q, DomKey::FromCharacter('q')},
       {DomCode::US_R, DomKey::FromCharacter('r')},
       {DomCode::US_S, DomKey::FromCharacter('s')},
       {DomCode::US_T, DomKey::FromCharacter('t')},
       {DomCode::US_U, DomKey::FromCharacter('u')},
       {DomCode::US_V, DomKey::FromCharacter('v')},
       {DomCode::US_W, DomKey::FromCharacter('w')},
       {DomCode::US_X, DomKey::FromCharacter('x')},
       {DomCode::US_Y, DomKey::FromCharacter('y')},
       {DomCode::US_Z, DomKey::FromCharacter('z')},
       {DomCode::BACKQUOTE, DomKey::FromCharacter('`')},
       {DomCode::MINUS, DomKey::FromCharacter('-')},
       {DomCode::EQUAL, DomKey::FromCharacter('=')},
       {DomCode::INTL_YEN, DomKey::DeadKeyFromCombiningCharacter(0x00A5)},
       {DomCode::BRACKET_LEFT, DomKey::FromCharacter('{')},
       {DomCode::BRACKET_RIGHT, DomKey::FromCharacter('}')},
       {DomCode::BACKSLASH, DomKey::FromCharacter('\\')},
       {DomCode::SEMICOLON, DomKey::FromCharacter(';')},
       {DomCode::QUOTE, DomKey::FromCharacter('\'')},
       {DomCode::INTL_BACKSLASH, DomKey::FromCharacter('/')},
       {DomCode::COMMA, DomKey::FromCharacter(',')},
       {DomCode::PERIOD, DomKey::FromCharacter('.')},
       {DomCode::SLASH, DomKey::FromCharacter('/')},
       {DomCode::INTL_RO, DomKey::DeadKeyFromCombiningCharacter(0x308D)}});

  // Ensure the 'full' lookup table contains the same number of elements as the
  // writing system table used by the class under test.  Ideally this would be a
  // static assert however that doesn't work since the other table is in a
  // different compilation unit.
  DCHECK_EQ(std::size(kFullLookupTable), kWritingSystemKeyDomCodeEntries);

  if (kFullLookupTable.count(dom_code) == 0)
    return DomKey::NONE;

  return kFullLookupTable[dom_code];
}

DomKey GetKeyFromCombiningLayoutTable(DomCode dom_code) {
  // Used for testing combining keys in both printable and combining forms.
  static base::flat_map<DomCode, DomKey> kCombiningLayoutTable({
      // Grave, combining.
      {DomCode::DIGIT0, DomKey::DeadKeyFromCombiningCharacter(0x0300)},
      // Grave, printable.
      {DomCode::DIGIT1, DomKey::FromCharacter(0x0060)},
      // Acute, combining.
      {DomCode::DIGIT2, DomKey::DeadKeyFromCombiningCharacter(0x0301)},
      // Acute, printable.
      {DomCode::DIGIT3, DomKey::FromCharacter(0x0027)},
      // Circumflex, combining.
      {DomCode::DIGIT4, DomKey::DeadKeyFromCombiningCharacter(0x0302)},
      // Circumflex, printable.
      {DomCode::DIGIT5, DomKey::FromCharacter(0x005e)},
      // Tilde, combining.
      {DomCode::DIGIT6, DomKey::DeadKeyFromCombiningCharacter(0x0303)},
      // Tilde, printable.
      {DomCode::DIGIT7, DomKey::FromCharacter(0x007e)},
      // Diaeresis, combining.
      {DomCode::DIGIT8, DomKey::DeadKeyFromCombiningCharacter(0x0308)},
      // Diaeresis, printable.
      {DomCode::DIGIT9, DomKey::FromCharacter(0x00a8)},
  });

  if (kCombiningLayoutTable.count(dom_code) == 0)
    return DomKey::NONE;

  return kCombiningLayoutTable[dom_code];
}

}  // namespace

class TestDomKeyboardLayoutMap : public DomKeyboardLayoutMapBase {
 public:
  TestDomKeyboardLayoutMap();

  TestDomKeyboardLayoutMap(const TestDomKeyboardLayoutMap&) = delete;
  TestDomKeyboardLayoutMap& operator=(const TestDomKeyboardLayoutMap&) = delete;

  ~TestDomKeyboardLayoutMap() override;

  // DomKeyboardLayoutMapBase overrides.
  uint32_t GetKeyboardLayoutCount() override;
  DomKey GetDomKeyFromDomCodeForLayout(DomCode dom_code,
                                       uint32_t keyboard_layout_id) override;

  // Adds a new keyboard layout in FIFO order.
  void AddKeyboardLayout(uint32_t test_layout_id);

 private:
  // Helper methods used to populate a layout for testing.
  DomKey GetDomKeyForLayoutWithAllValidKeys(DomCode dom_code);
  DomKey GetDomKeyForLayoutWithAllValidAlphaKeys(DomCode dom_code);
  DomKey GetDomKeyForLayoutWithSomeValidKeys(DomCode dom_code);
  DomKey GetDomKeyForLayoutWithNoValidKeys();

  std::vector<uint32_t> test_keyboard_layouts_;
};

TestDomKeyboardLayoutMap::TestDomKeyboardLayoutMap() = default;

TestDomKeyboardLayoutMap::~TestDomKeyboardLayoutMap() = default;

uint32_t TestDomKeyboardLayoutMap::GetKeyboardLayoutCount() {
  return test_keyboard_layouts_.size();
}

DomKey TestDomKeyboardLayoutMap::GetDomKeyFromDomCodeForLayout(
    DomCode dom_code,
    uint32_t keyboard_layout_id) {
  uint32_t test_layout_id = test_keyboard_layouts_[keyboard_layout_id];
  if (test_layout_id == kKeyboardLayoutWithAllValidKeys)
    return GetDomKeyForLayoutWithAllValidKeys(dom_code);
  if (test_layout_id == kKeyboardLayoutWithAllValidAlphaKeys)
    return GetDomKeyForLayoutWithAllValidAlphaKeys(dom_code);
  if (test_layout_id == kKeyboardLayoutWithSomeValidKeys)
    return GetDomKeyForLayoutWithSomeValidKeys(dom_code);
  if (test_layout_id == kKeyboardLayoutWithNoValidKeys)
    return GetDomKeyForLayoutWithNoValidKeys();

  NOTREACHED_IN_MIGRATION();
  return DomKey::NONE;
}

void TestDomKeyboardLayoutMap::AddKeyboardLayout(uint32_t test_layout_id) {
  test_keyboard_layouts_.push_back(test_layout_id);
}

DomKey TestDomKeyboardLayoutMap::GetDomKeyForLayoutWithAllValidKeys(
    DomCode dom_code) {
  return GetKeyFromFullLookupTable(dom_code);
}

DomKey TestDomKeyboardLayoutMap::GetDomKeyForLayoutWithAllValidAlphaKeys(
    DomCode dom_code) {
  // If the number of excluded keys changes, please modify
  // |kKeyboardLayoutWithAllValidAlphaKeysCount| to match the new value.
  if (dom_code == DomCode::INTL_RO || dom_code == DomCode::INTL_YEN)
    return DomKey::NONE;

  // DIGIT 0 - 9 are overridden for combining char tests so use those here since
  // this method only ensures the alpha keys are valid.
  if (GetKeyFromCombiningLayoutTable(dom_code) != DomKey::NONE)
    return GetKeyFromCombiningLayoutTable(dom_code);

  return GetKeyFromFullLookupTable(dom_code);
}

DomKey TestDomKeyboardLayoutMap::GetDomKeyForLayoutWithSomeValidKeys(
    DomCode dom_code) {
  if (dom_code == DomCode::US_A || dom_code == DomCode::US_Z ||
      dom_code == DomCode::BACKQUOTE)
    return DomKey::NONE;
  return GetKeyFromFullLookupTable(dom_code);
}

DomKey TestDomKeyboardLayoutMap::GetDomKeyForLayoutWithNoValidKeys() {
  return DomKey::NONE;
}

TEST(DomKeyboardLayoutMapTest, MapGenerationWithZeroLayouts) {
  TestDomKeyboardLayoutMap test_keyboard_layout_map;
  auto map = test_keyboard_layout_map.Generate();
  ASSERT_EQ(0UL, map.size());
}

TEST(DomKeyboardLayoutMapTest, MapGenerationWithCompletelyValidLayout) {
  TestDomKeyboardLayoutMap test_keyboard_layout_map;
  test_keyboard_layout_map.AddKeyboardLayout(kKeyboardLayoutWithAllValidKeys);

  auto map = test_keyboard_layout_map.Generate();
  ASSERT_EQ(kKeyboardLayoutWithAllValidKeysCount, map.size());
}

TEST(DomKeyboardLayoutMapTest, MapGenerationWithValidAlphaKeys) {
  TestDomKeyboardLayoutMap test_keyboard_layout_map;
  test_keyboard_layout_map.AddKeyboardLayout(
      kKeyboardLayoutWithAllValidAlphaKeys);

  auto map = test_keyboard_layout_map.Generate();
  ASSERT_EQ(kKeyboardLayoutWithAllValidAlphaKeysCount, map.size());
}

TEST(DomKeyboardLayoutMapTest, MapGenerationWithMostlyValidAlphaKeys) {
  TestDomKeyboardLayoutMap test_keyboard_layout_map;
  test_keyboard_layout_map.AddKeyboardLayout(kKeyboardLayoutWithSomeValidKeys);

  auto map = test_keyboard_layout_map.Generate();
  ASSERT_EQ(kKeyboardLayoutWithSomeValidKeysCount, map.size());
}

TEST(DomKeyboardLayoutMapTest, MapGenerationWithNoValidKeys) {
  TestDomKeyboardLayoutMap test_keyboard_layout_map;
  test_keyboard_layout_map.AddKeyboardLayout(kKeyboardLayoutWithNoValidKeys);

  auto map = test_keyboard_layout_map.Generate();
  ASSERT_EQ(kKeyboardLayoutWithNoValidKeysCount, map.size());
}

TEST(DomKeyboardLayoutMapTest, MapGenerationWithValidLayoutFirst) {
  TestDomKeyboardLayoutMap test_keyboard_layout_map;
  test_keyboard_layout_map.AddKeyboardLayout(kKeyboardLayoutWithAllValidKeys);
  test_keyboard_layout_map.AddKeyboardLayout(kKeyboardLayoutWithSomeValidKeys);
  test_keyboard_layout_map.AddKeyboardLayout(kKeyboardLayoutWithNoValidKeys);

  auto map = test_keyboard_layout_map.Generate();
  ASSERT_EQ(kKeyboardLayoutWithAllValidKeysCount, map.size());
}

TEST(DomKeyboardLayoutMapTest, MapGenerationWithValidLayoutLast) {
  TestDomKeyboardLayoutMap test_keyboard_layout_map;
  test_keyboard_layout_map.AddKeyboardLayout(kKeyboardLayoutWithSomeValidKeys);
  test_keyboard_layout_map.AddKeyboardLayout(kKeyboardLayoutWithNoValidKeys);
  test_keyboard_layout_map.AddKeyboardLayout(kKeyboardLayoutWithAllValidKeys);

  auto map = test_keyboard_layout_map.Generate();
  ASSERT_EQ(kKeyboardLayoutWithAllValidKeysCount, map.size());
}

TEST(DomKeyboardLayoutMapTest, MapGenerationWithTwoValidLayouts) {
  TestDomKeyboardLayoutMap test_keyboard_layout_map_1;
  test_keyboard_layout_map_1.AddKeyboardLayout(kKeyboardLayoutWithAllValidKeys);
  test_keyboard_layout_map_1.AddKeyboardLayout(
      kKeyboardLayoutWithAllValidAlphaKeys);

  auto map_1 = test_keyboard_layout_map_1.Generate();
  EXPECT_EQ(kKeyboardLayoutWithAllValidKeysCount, map_1.size());

  TestDomKeyboardLayoutMap test_keyboard_layout_map_2;
  test_keyboard_layout_map_2.AddKeyboardLayout(
      kKeyboardLayoutWithAllValidAlphaKeys);
  test_keyboard_layout_map_2.AddKeyboardLayout(kKeyboardLayoutWithAllValidKeys);

  auto map_2 = test_keyboard_layout_map_2.Generate();
  EXPECT_EQ(kKeyboardLayoutWithAllValidAlphaKeysCount, map_2.size());
}

}  // namespace ui
