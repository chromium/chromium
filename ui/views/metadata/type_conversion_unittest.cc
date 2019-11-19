// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/gfx/geometry/rect.h"

namespace MD = views::metadata;

using TypeConversionTest = PlatformTest;

TEST_F(TypeConversionTest, TestConversion_IntToString) {
  int from_int = 5;
  base::string16 to_string = MD::TypeConverter<int>::ToString(from_int);

  EXPECT_EQ(to_string, base::ASCIIToUTF16("5"));
}

TEST_F(TypeConversionTest, TestConversion_StringToInt) {
  base::string16 from_string = base::ASCIIToUTF16("10");
  EXPECT_EQ(MD::TypeConverter<int>::FromString(from_string), 10);
}

// This tests whether the converter handles a bogus input string, in which case
// the return value should be nullopt.
TEST_F(TypeConversionTest, TestConversion_BogusStringToInt) {
  base::string16 from_string = base::ASCIIToUTF16("Foo");
  EXPECT_EQ(MD::TypeConverter<int>::FromString(from_string), base::nullopt);
}

TEST_F(TypeConversionTest, TestConversion_BogusStringToFloat) {
  base::string16 from_string = base::ASCIIToUTF16("1.2");
  EXPECT_EQ(MD::TypeConverter<float>::FromString(from_string), 1.2f);
}

TEST_F(TypeConversionTest, TestConversion_OptionalIntToString) {
  base::Optional<int> src;
  base::string16 to_string =
      MD::TypeConverter<base::Optional<int>>::ToString(src);
  EXPECT_EQ(to_string, MD::GetNullOptStr());

  src = 5;
  to_string = MD::TypeConverter<base::Optional<int>>::ToString(src);
  EXPECT_EQ(to_string, base::ASCIIToUTF16("5"));
}

TEST_F(TypeConversionTest, TestConversion_StringToOptionalInt) {
  base::Optional<int> ret;
  EXPECT_EQ(
      MD::TypeConverter<base::Optional<int>>::FromString(MD::GetNullOptStr()),
      base::make_optional(ret));

  EXPECT_EQ(MD::TypeConverter<base::Optional<int>>::FromString(
                base::ASCIIToUTF16("10")),
            10);

  EXPECT_EQ(MD::TypeConverter<base::Optional<int>>::FromString(
                base::ASCIIToUTF16("ab0")),
            base::nullopt);
}

TEST_F(TypeConversionTest, TestConversion_ShadowValuesToString) {
  gfx::ShadowValues shadow_values;
  shadow_values.emplace_back(gfx::Vector2d(1, 2), .3,
                             SkColorSetARGB(128, 255, 0, 0));

  EXPECT_EQ(MD::TypeConverter<gfx::ShadowValues>::ToString(shadow_values),
            base::ASCIIToUTF16("[ (1,2),0.30,rgba(255,0,0,128) ]"));

  shadow_values.emplace_back(gfx::Vector2d(9, 8), .76,
                             SkColorSetARGB(20, 0, 64, 255));

  EXPECT_EQ(
      MD::TypeConverter<gfx::ShadowValues>::ToString(shadow_values),
      base::ASCIIToUTF16(
          "[ (1,2),0.30,rgba(255,0,0,128); (9,8),0.76,rgba(0,64,255,20) ]"));
}

TEST_F(TypeConversionTest, TestConversion_StringToShadowValues) {
  base::Optional<gfx::ShadowValues> opt_result =
      MD::TypeConverter<gfx::ShadowValues>::FromString(base::ASCIIToUTF16(
          "[ (6,4),0.53,rgba(23,44,0,255); (93,83),4.33,rgba(10,20,0,15) ]"));

  EXPECT_EQ(opt_result.has_value(), true);
  gfx::ShadowValues result = opt_result.value();
  EXPECT_EQ(result.size(), 2U);

  EXPECT_EQ(result[0].color(), SkColorSetARGB(255, 23, 44, 0));
  EXPECT_EQ(result[1].color(), SkColorSetARGB(15, 10, 20, 0));

  EXPECT_EQ(result[0].x(), 6);
  EXPECT_EQ(result[1].x(), 93);

  EXPECT_EQ(result[0].y(), 4);
  EXPECT_EQ(result[1].y(), 83);

  EXPECT_EQ(result[0].blur(), 0.53);
  EXPECT_EQ(result[1].blur(), 4.33);
}
