// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/custom_data_helper.h"

#include <utility>

#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace ui {

namespace {

void PrepareEmptyTestData(base::Pickle* pickle) {
  std::unordered_map<std::u16string, std::u16string> data;
  WriteCustomDataToPickle(data, pickle);
}

void PrepareTestData(base::Pickle* pickle) {
  std::unordered_map<std::u16string, std::u16string> data = {
      {u"abc", std::u16string()}, {u"de", u"1"}, {u"f", u"23"}};
  WriteCustomDataToPickle(data, pickle);
}

TEST(CustomDataHelperTest, EmptyReadTypes) {
  base::Pickle pickle;
  PrepareEmptyTestData(&pickle);

  std::vector<std::u16string> types;
  ReadCustomDataTypes(pickle, &types);
  EXPECT_EQ(0u, types.size());
}

TEST(CustomDataHelperTest, EmptyReadSingleType) {
  base::Pickle pickle;
  PrepareEmptyTestData(&pickle);

  EXPECT_EQ(std::nullopt, ReadCustomDataForType(pickle, u"f"));
}

TEST(CustomDataHelperTest, EmptyReadMap) {
  base::Pickle pickle;
  PrepareEmptyTestData(&pickle);

  EXPECT_EQ((std::unordered_map<std::u16string, std::u16string>()),
            ReadCustomDataIntoMap(pickle));
}

TEST(CustomDataHelperTest, ReadTypes) {
  base::Pickle pickle;
  PrepareTestData(&pickle);

  std::vector<std::u16string> types;
  ReadCustomDataTypes(pickle, &types);

  std::vector<std::u16string> expected = {u"abc", u"de", u"f"};
  // We need to sort to compare equality, as the underlying custom data is
  // unordered
  std::sort(types.begin(), types.end());
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(expected, types);
}

TEST(CustomDataHelperTest, ReadSingleType) {
  base::Pickle pickle;
  PrepareTestData(&pickle);

  EXPECT_EQ(u"", ReadCustomDataForType(pickle, u"abc"));
  EXPECT_EQ(u"1", ReadCustomDataForType(pickle, u"de"));
  EXPECT_EQ(u"23", ReadCustomDataForType(pickle, u"f"));
}

TEST(CustomDataHelperTest, ReadMap) {
  base::Pickle pickle;
  PrepareTestData(&pickle);

  const std::unordered_map<std::u16string, std::u16string> expected = {
      {u"abc", std::u16string()}, {u"de", u"1"}, {u"f", u"23"}};
  EXPECT_EQ(expected, ReadCustomDataIntoMap(pickle));
}

TEST(CustomDataHelperTest, BadReadTypes) {
  // ReadCustomDataTypes makes the additional guarantee that the contents of the
  // result vector will not change if the input is malformed.
  const std::vector<std::u16string> expected = {u"abc", u"de", u"f"};

  {
    base::Pickle malformed;
    malformed.WriteUInt32(1000);
    malformed.WriteString16(u"hello");
    malformed.WriteString16(u"world");
    std::vector<std::u16string> actual = expected;
    ReadCustomDataTypes(malformed, &actual);
    EXPECT_EQ(expected, actual);
  }

  {
    base::Pickle malformed;
    malformed.WriteUInt32(1);
    malformed.WriteString16(u"hello");
    std::vector<std::u16string> actual = expected;
    ReadCustomDataTypes(malformed, &actual);
    EXPECT_EQ(expected, actual);
  }
}

TEST(CustomDataHelperTest, BadPickle) {
  std::unordered_map<std::u16string, std::u16string> result_map;

  {
    base::Pickle malformed;
    malformed.WriteUInt32(1000);
    malformed.WriteString16(u"hello");
    malformed.WriteString16(u"world");

    EXPECT_EQ(std::nullopt, ReadCustomDataForType(malformed, u"f"));
    EXPECT_EQ(std::nullopt, ReadCustomDataIntoMap(malformed));
  }

  {
    base::Pickle malformed;
    malformed.WriteUInt32(1);
    malformed.WriteString16(u"hello");

    EXPECT_EQ(std::nullopt, ReadCustomDataForType(malformed, u"f"));
    EXPECT_EQ(std::nullopt, ReadCustomDataIntoMap(malformed));
  }
}

}  // namespace

}  // namespace ui
