// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  std::unordered_map<base::string16, base::string16> data;
  WriteCustomDataToPickle(data, pickle);
}

void PrepareTestData(base::Pickle* pickle) {
  std::unordered_map<base::string16, base::string16> data = {
      {ASCIIToUTF16("abc"), base::string16()},
      {ASCIIToUTF16("de"), ASCIIToUTF16("1")},
      {ASCIIToUTF16("f"), ASCIIToUTF16("23")}};
  WriteCustomDataToPickle(data, pickle);
}

TEST(CustomDataHelperTest, EmptyReadTypes) {
  base::Pickle pickle;
  PrepareEmptyTestData(&pickle);

  std::vector<base::string16> types;
  ReadCustomDataTypes(pickle.data(), pickle.size(), &types);
  EXPECT_EQ(0u, types.size());
}

TEST(CustomDataHelperTest, EmptyReadSingleType) {
  base::Pickle pickle;
  PrepareEmptyTestData(&pickle);

  base::string16 result;
  ReadCustomDataForType(pickle.data(),
                        pickle.size(),
                        ASCIIToUTF16("f"),
                        &result);
  EXPECT_EQ(base::string16(), result);
}

TEST(CustomDataHelperTest, EmptyReadMap) {
  base::Pickle pickle;
  PrepareEmptyTestData(&pickle);

  std::unordered_map<base::string16, base::string16> result;
  ReadCustomDataIntoMap(pickle.data(), pickle.size(), &result);
  EXPECT_EQ(0u, result.size());
}

TEST(CustomDataHelperTest, ReadTypes) {
  base::Pickle pickle;
  PrepareTestData(&pickle);

  std::vector<base::string16> types;
  ReadCustomDataTypes(pickle.data(), pickle.size(), &types);

  std::vector<base::string16> expected = {
      ASCIIToUTF16("abc"), ASCIIToUTF16("de"), ASCIIToUTF16("f")};
  // We need to sort to compare equality, as the underlying custom data is
  // unordered
  std::sort(types.begin(), types.end());
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(expected, types);
}

TEST(CustomDataHelperTest, ReadSingleType) {
  base::Pickle pickle;
  PrepareTestData(&pickle);

  base::string16 result;
  ReadCustomDataForType(pickle.data(),
                        pickle.size(),
                        ASCIIToUTF16("abc"),
                        &result);
  EXPECT_EQ(base::string16(), result);

  ReadCustomDataForType(pickle.data(),
                        pickle.size(),
                        ASCIIToUTF16("de"),
                        &result);
  EXPECT_EQ(ASCIIToUTF16("1"), result);

  ReadCustomDataForType(pickle.data(),
                        pickle.size(),
                        ASCIIToUTF16("f"),
                        &result);
  EXPECT_EQ(ASCIIToUTF16("23"), result);
}

TEST(CustomDataHelperTest, ReadMap) {
  base::Pickle pickle;
  PrepareTestData(&pickle);

  std::unordered_map<base::string16, base::string16> result;
  ReadCustomDataIntoMap(pickle.data(), pickle.size(), &result);

  std::unordered_map<base::string16, base::string16> expected = {
      {ASCIIToUTF16("abc"), base::string16()},
      {ASCIIToUTF16("de"), ASCIIToUTF16("1")},
      {ASCIIToUTF16("f"), ASCIIToUTF16("23")}};
  EXPECT_EQ(expected, result);
}

TEST(CustomDataHelperTest, BadReadTypes) {
  // ReadCustomDataTypes makes the additional guarantee that the contents of the
  // result vector will not change if the input is malformed.
  std::vector<base::string16> expected = {
      ASCIIToUTF16("abc"), ASCIIToUTF16("de"), ASCIIToUTF16("f")};

  base::Pickle malformed;
  malformed.WriteUInt32(1000);
  malformed.WriteString16(ASCIIToUTF16("hello"));
  malformed.WriteString16(ASCIIToUTF16("world"));
  std::vector<base::string16> actual(expected);
  ReadCustomDataTypes(malformed.data(), malformed.size(), &actual);
  EXPECT_EQ(expected, actual);

  base::Pickle malformed2;
  malformed2.WriteUInt32(1);
  malformed2.WriteString16(ASCIIToUTF16("hello"));
  std::vector<base::string16> actual2(expected);
  ReadCustomDataTypes(malformed2.data(), malformed2.size(), &actual2);
  EXPECT_EQ(expected, actual2);
}

TEST(CustomDataHelperTest, BadPickle) {
  base::string16 result_data;
  std::unordered_map<base::string16, base::string16> result_map;

  base::Pickle malformed;
  malformed.WriteUInt32(1000);
  malformed.WriteString16(ASCIIToUTF16("hello"));
  malformed.WriteString16(ASCIIToUTF16("world"));

  ReadCustomDataForType(malformed.data(),
                        malformed.size(),
                        ASCIIToUTF16("f"),
                        &result_data);
  ReadCustomDataIntoMap(malformed.data(), malformed.size(), &result_map);
  EXPECT_EQ(0u, result_data.size());
  EXPECT_EQ(0u, result_map.size());

  base::Pickle malformed2;
  malformed2.WriteUInt32(1);
  malformed2.WriteString16(ASCIIToUTF16("hello"));

  ReadCustomDataForType(malformed2.data(),
                        malformed2.size(),
                        ASCIIToUTF16("f"),
                        &result_data);
  ReadCustomDataIntoMap(malformed2.data(), malformed2.size(), &result_map);
  EXPECT_EQ(0u, result_data.size());
  EXPECT_EQ(0u, result_map.size());
}

}  // namespace

}  // namespace ui
