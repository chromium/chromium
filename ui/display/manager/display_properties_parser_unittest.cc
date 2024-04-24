// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_properties_parser.h"

#include <string_view>

#include "base/json/json_reader.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

namespace display {
namespace {

std::optional<base::Value> ReadJsonString(std::string_view json) {
  return base::JSONReader::Read(json);
}

using DisplayPropertiesParserTest = ::testing::Test;

TEST(DisplayPropertiesParserTest, Valid_SingleDisplay) {
  auto result = ReadJsonString(
      "[{\"connector-type\": 14, \"rounded-corners\": {\"bottom-left\": 15, "
      "\"bottom-right\": 15, \"top-left\": 16, \"top-right\": 16}}]");

  const auto radii = ParseDisplayPanelRadii(&result.value());
  ASSERT_TRUE(radii.has_value());
  EXPECT_EQ(radii, gfx::RoundedCornersF(16, 16, 15, 15));
}

TEST(DisplayPropertiesParserTest, Invalid_MultipleDisplays) {
  auto result = ReadJsonString(
      "[{\"connector-type\": 14, \"rounded-corners\": {\"bottom-left\": 15, "
      "\"bottom-right\": 15, \"top-left\": 16, \"top-right\": 16}}, "
      "{\"connector-type\": 19, \"rounded-corners\": {\"bottom-left\": 15, "
      "\"bottom-right\": 15, \"top-left\": 15, \"top-right\": 15}}]");

  const auto radii = ParseDisplayPanelRadii(&result.value());
  ASSERT_FALSE(radii.has_value());
}

TEST(DisplayPropertiesParserTest, InValidRadii) {
  auto result = ReadJsonString(
      "[{\"connector-type\": 5, \"rounded-corners\": {\"bottom-left\": 15, "
      "\"bottom-right\": -15, \"top-left\": 16, \"top-right\": 16}}]");

  const auto radii = ParseDisplayPanelRadii(&result.value());
  ASSERT_FALSE(radii.has_value());
}

TEST(DisplayPropertiesParserTest, InValidField_WrongKey) {
  auto result = ReadJsonString(
      "[{\"connector-type\": 5, \"round-corners\": {\"bottom-left\": 15, "
      "\"bottom-right\": -15, \"top-left\": 16, \"top-right\": 16}}]");

  const auto radii = ParseDisplayPanelRadii(&result.value());
  ASSERT_FALSE(radii.has_value());
}

TEST(DisplayPropertiesParserTest, InValidField_MissingValuePair) {
  auto result = ReadJsonString(
      "[{\"connector-type\": 5, \"rounded-corners\": {\"bottom-left\": 15, "
      "\"bottom-right\": -15, \"top-left\": 16}}]");

  const auto radii = ParseDisplayPanelRadii(&result.value());
  ASSERT_FALSE(radii.has_value());
}

}  // namespace
}  // namespace display
