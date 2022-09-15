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
      {{DomCode::DIGIT0, DomKey::Constant<'0'>::Character},
       {DomCode::DIGIT1, DomKey::Constant<'1'>::Character},
       {DomCode::DIGIT2, DomKey::Constant<'2'>::Character},
       {DomCode::DIGIT3, DomKey::Constant<'3'>::Character},
       {DomCode::DIGIT4, DomKey::Constant<'4'>::Character},
       {DomCode::DIGIT5, DomKey::Constant<'5'>::Character},
       {DomCode::DIGIT6, DomKey::Constant<'6'>::Character},
       {DomCode::DIGIT7, DomKey::Constant<'7'>::Character},
       {DomCode::DIGIT8, DomKey::Constant<'8'>::Character},
       {DomCode::DIGIT9, DomKey::Constant<'9'>::Character},
       {DomCode::US_A, DomKey::Constant<'a'>::Character},
       {DomCode::US_B, DomKey::Constant<'b'>::Character},
       {DomCode::US_C, DomKey::Constant<'c'>::Character},
       {DomCode::US_D, DomKey::Constant<'d'>::Character},
       {DomCode::US_E, DomKey::Constant<'e'>::Character},
       {DomCode::US_F, DomKey::Constant<'f'>::Character},
       {DomCode::US_G, DomKey::Constant<'g'>::Character},
       {DomCode::US_H, DomKey::Constant<'h'>::Character},
       {DomCode::US_I, DomKey::Constant<'i'>::Character},
       {DomCode::US_J, DomKey::Constant<'j'>::Character},
       {DomCode::US_K, DomKey::Constant<'k'>::Character},
       {DomCode::US_L, DomKey::Constant<'l'>::Character},
       {DomCode::US_M, DomKey::Constant<'m'>::Character},
       {DomCode::US_N, DomKey::Constant<'n'>::Character},
       {DomCode::US_O, DomKey::Constant<'o'>::Character},
       {DomCode::US_P, DomKey::Constant<'p'>::Character},
       {DomCode::US_Q, DomKey::Constant<'q'>::Character},
       {DomCode::US_R, DomKey::Constant<'r'>::Character},
       {DomCode::US_S, DomKey::Constant<'s'>::Character},
       {DomCode::US_T, DomKey::Constant<'t'>::Character},
       {DomCode::US_U, DomKey::Constant<'u'>::Character},
       {DomCode::US_V, DomKey::Constant<'v'>::Character},
       {DomCode::US_W, DomKey::Constant<'w'>::Character},
       {DomCode::US_X, DomKey::Constant<'x'>::Character},
       {DomCode::US_Y, DomKey::Constant<'y'>::Character},
       {DomCode::US_Z, DomKey::Constant<'z'>::Character},
       {DomCode::BACKQUOTE, DomKey::Constant<'`'>::Character},
       {DomCode::MINUS, DomKey::Constant<'-'>::Character},
       {DomCode::EQUAL, DomKey::Constant<'='>::Character},
       {DomCode::INTL_YEN, DomKey::Constant<0x00A5>::Dead},
       {DomCode::BRACKET_LEFT, DomKey::Constant<'{'>::Character},
       {DomCode::BRACKET_RIGHT, DomKey::Constant<'}'>::Character},
       {DomCode::BACKSLASH, DomKey::Constant<'\\'>::Character},
       {DomCode::SEMICOLON, DomKey::Constant<';'>::Character},
       {DomCode::QUOTE, DomKey::Constant<'\''>::Character},
       {DomCode::INTL_BACKSLASH, DomKey::Constant<'/'>::Character},
       {DomCode::COMMA, DomKey::Constant<','>::Character},
       {DomCode::PERIOD, DomKey::Constant<'.'>::Character},
       {DomCode::SLASH, DomKey::Constant<'/'>::Character},
       {DomCode::INTL_RO, DomKey::Constant<0x308D>::Dead}});

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
      {DomCode::DIGIT0, DomKey::Constant<0x0300>::Dead},
      // Grave, printable.
      {DomCode::DIGIT1, DomKey::Constant<0x0060>::Character},
      // Acute, combining.
      {DomCode::DIGIT2, DomKey::Constant<0x0301>::Dead},
      // Acute, printable.
      {DomCode::DIGIT3, DomKey::Constant<0x0027>::Character},
      // Circumflex, combining.
      {DomCode::DIGIT4, DomKey::Constant<0x0302>::Dead},
      // Circumflex, printable.
      {DomCode::DIGIT5, DomKey::Constant<0x005e>::Character},
      // Tilde, combining.
      {DomCode::DIGIT6, DomKey::Constant<0x0303>::Dead},
      // Tilde, printable.
      {DomCode::DIGIT7, DomKey::Constant<0x007e>::Character},
      // Diaeresis, combining.
      {DomCode::DIGIT8, DomKey::Constant<0x0308>::Dead},
      // Diaeresis, printable.
      {DomCode::DIGIT9, DomKey::Constant<0x00a8>::Character},
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

  NOTREACHED();
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
