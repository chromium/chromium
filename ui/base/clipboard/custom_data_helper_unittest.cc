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
  ReadCustomDataTypes(pickle.data(), pickle.size(), &types);
  EXPECT_EQ(0u, types.size());
}

TEST(CustomDataHelperTest, EmptyReadSingleType) {
  base::Pickle pickle;
  PrepareEmptyTestData(&pickle);

  std::u16string result;
  ReadCustomDataForType(pickle.data(), pickle.size(), u"f", &result);
  EXPECT_EQ(std::u16string(), result);
}

TEST(CustomDataHelperTest, EmptyReadMap) {
  base::Pickle pickle;
  PrepareEmptyTestData(&pickle);

  std::unordered_map<std::u16string, std::u16string> result;
  ReadCustomDataIntoMap(pickle.data(), pickle.size(), &result);
  EXPECT_EQ(0u, result.size());
}

TEST(CustomDataHelperTest, ReadTypes) {
  base::Pickle pickle;
  PrepareTestData(&pickle);

  std::vector<std::u16string> types;
  ReadCustomDataTypes(pickle.data(), pickle.size(), &types);

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

  std::u16string result;
  ReadCustomDataForType(pickle.data(), pickle.size(), u"abc", &result);
  EXPECT_EQ(std::u16string(), result);

  ReadCustomDataForType(pickle.data(), pickle.size(), u"de", &result);
  EXPECT_EQ(u"1", result);

  ReadCustomDataForType(pickle.data(), pickle.size(), u"f", &result);
  EXPECT_EQ(u"23", result);
}

TEST(CustomDataHelperTest, ReadMap) {
  base::Pickle pickle;
  PrepareTestData(&pickle);

  std::unordered_map<std::u16string, std::u16string> result;
  ReadCustomDataIntoMap(pickle.data(), pickle.size(), &result);

  std::unordered_map<std::u16string, std::u16string> expected = {
      {u"abc", std::u16string()}, {u"de", u"1"}, {u"f", u"23"}};
  EXPECT_EQ(expected, result);
}

TEST(CustomDataHelperTest, BadReadTypes) {
  // ReadCustomDataTypes makes the additional guarantee that the contents of the
  // result vector will not change if the input is malformed.
  std::vector<std::u16string> expected = {u"abc", u"de", u"f"};

  base::Pickle malformed;
  malformed.WriteUInt32(1000);
  malformed.WriteString16(u"hello");
  malformed.WriteString16(u"world");
  std::vector<std::u16string> actual(expected);
  ReadCustomDataTypes(malformed.data(), malformed.size(), &actual);
  EXPECT_EQ(expected, actual);

  base::Pickle malformed2;
  malformed2.WriteUInt32(1);
  malformed2.WriteString16(u"hello");
  std::vector<std::u16string> actual2(expected);
  ReadCustomDataTypes(malformed2.data(), malformed2.size(), &actual2);
  EXPECT_EQ(expected, actual2);
}

TEST(CustomDataHelperTest, BadPickle) {
  std::u16string result_data;
  std::unordered_map<std::u16string, std::u16string> result_map;

  base::Pickle malformed;
  malformed.WriteUInt32(1000);
  malformed.WriteString16(u"hello");
  malformed.WriteString16(u"world");

  ReadCustomDataForType(malformed.data(), malformed.size(), u"f", &result_data);
  ReadCustomDataIntoMap(malformed.data(), malformed.size(), &result_map);
  EXPECT_EQ(0u, result_data.size());
  EXPECT_EQ(0u, result_map.size());

  base::Pickle malformed2;
  malformed2.WriteUInt32(1);
  malformed2.WriteString16(u"hello");

  ReadCustomDataForType(malformed2.data(), malformed2.size(), u"f",
                        &result_data);
  ReadCustomDataIntoMap(malformed2.data(), malformed2.size(), &result_map);
  EXPECT_EQ(0u, result_data.size());
  EXPECT_EQ(0u, result_map.size());
}

}  // namespace

}  // namespace ui
