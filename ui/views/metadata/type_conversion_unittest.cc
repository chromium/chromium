// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include "base/ranges/ranges.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/button.h"

using TypeConversionTest = PlatformTest;

namespace views {

// Used in CheckIsSerializable test case.
enum TestResult {
  TEST_TRUE,
  TEST_FALSE,
};
DEFINE_ENUM_CONVERTERS(TestResult,
                       {TEST_TRUE, base::ASCIIToUTF16("TRUE")},
                       {TEST_FALSE, base::ASCIIToUTF16("FALSE")})

TEST_F(TypeConversionTest, TestConversion_IntToString) {
  int from_int = 5;
  base::string16 to_string = metadata::TypeConverter<int>::ToString(from_int);

  EXPECT_EQ(to_string, base::ASCIIToUTF16("5"));
}

TEST_F(TypeConversionTest, TestConversion_StringToInt) {
  base::string16 from_string = base::ASCIIToUTF16("10");
  EXPECT_EQ(metadata::TypeConverter<int>::FromString(from_string), 10);
}

// This tests whether the converter handles a bogus input string, in which case
// the return value should be nullopt.
TEST_F(TypeConversionTest, TestConversion_BogusStringToInt) {
  base::string16 from_string = base::ASCIIToUTF16("Foo");
  EXPECT_EQ(metadata::TypeConverter<int>::FromString(from_string),
            base::nullopt);
}

TEST_F(TypeConversionTest, TestConversion_BogusStringToFloat) {
  base::string16 from_string = base::ASCIIToUTF16("1.2");
  EXPECT_EQ(metadata::TypeConverter<float>::FromString(from_string), 1.2f);
}

TEST_F(TypeConversionTest, TestConversion_OptionalIntToString) {
  base::Optional<int> src;
  base::string16 to_string =
      metadata::TypeConverter<base::Optional<int>>::ToString(src);
  EXPECT_EQ(to_string, metadata::GetNullOptStr());

  src = 5;
  to_string = metadata::TypeConverter<base::Optional<int>>::ToString(src);
  EXPECT_EQ(to_string, base::ASCIIToUTF16("5"));
}

TEST_F(TypeConversionTest, TestConversion_StringToOptionalInt) {
  base::Optional<int> ret;
  EXPECT_EQ(metadata::TypeConverter<base::Optional<int>>::FromString(
                metadata::GetNullOptStr()),
            base::make_optional(ret));

  EXPECT_EQ(metadata::TypeConverter<base::Optional<int>>::FromString(
                base::ASCIIToUTF16("10")),
            10);

  EXPECT_EQ(metadata::TypeConverter<base::Optional<int>>::FromString(
                base::ASCIIToUTF16("ab0")),
            base::nullopt);
}

TEST_F(TypeConversionTest, TestConversion_ShadowValuesToString) {
  gfx::ShadowValues shadow_values;
  shadow_values.emplace_back(gfx::Vector2d(1, 2), .3,
                             SkColorSetARGB(128, 255, 0, 0));

  EXPECT_EQ(metadata::TypeConverter<gfx::ShadowValues>::ToString(shadow_values),
            base::ASCIIToUTF16("[ (1,2),0.30,rgba(255,0,0,128) ]"));

  shadow_values.emplace_back(gfx::Vector2d(9, 8), .76,
                             SkColorSetARGB(20, 0, 64, 255));

  EXPECT_EQ(
      metadata::TypeConverter<gfx::ShadowValues>::ToString(shadow_values),
      base::ASCIIToUTF16(
          "[ (1,2),0.30,rgba(255,0,0,128); (9,8),0.76,rgba(0,64,255,20) ]"));
}

TEST_F(TypeConversionTest, TestConversion_StringToShadowValues) {
  base::Optional<gfx::ShadowValues> opt_result =
      metadata::TypeConverter<gfx::ShadowValues>::FromString(base::ASCIIToUTF16(
          "[ (6,4),0.53,rgba(23,44,0,1); (93,83),4.33,rgba(10,20,0,0.059) ]"));

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

TEST_F(TypeConversionTest, TestConversion_SkColorConversions) {
  // Check conversion from rgb hex string
  base::Optional<SkColor> result =
      metadata::SkColorConverter::FromString(base::ASCIIToUTF16("0x112233"));
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetRGB(0x11, 0x22, 0x33));

  // Check conversion from argb hex string
  result =
      metadata::SkColorConverter::FromString(base::ASCIIToUTF16("0x7F112233"));
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetARGB(0x7F, 0x11, 0x22, 0x33));

  // Check conversion from rgb(r,g,b) string
  result = metadata::SkColorConverter::FromString(
      base::ASCIIToUTF16("rgb(0, 128, 192)"));
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetRGB(0, 128, 192));

  // Check conversion from rgba(r,g,b,a) string
  result = metadata::SkColorConverter::FromString(
      base::ASCIIToUTF16("rgba(0, 128, 192, 0.5)"));
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetARGB(128, 0, 128, 192));

  // Check conversion from hsl(h,s,l) string
  result = metadata::SkColorConverter::FromString(
      base::ASCIIToUTF16("hsl(195, 100%, 50%)"));
  EXPECT_TRUE(result);
  const SkScalar hsv[3] = {195.0, 1.0, 0.5};
  EXPECT_EQ(result.value(), SkHSVToColor(hsv));

  // Check conversion from hsla(h,s,l,a) string
  result = metadata::SkColorConverter::FromString(
      base::ASCIIToUTF16("hsl(195, 100%, 50%, 0.5)"));
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkHSVToColor(128, hsv));

  // Check conversion from a decimal integer value
  result =
      metadata::SkColorConverter::FromString(base::ASCIIToUTF16("4278239231"));
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetARGB(255, 0, 191, 255));

  // Check without commas.
  result = metadata::SkColorConverter::FromString(
      base::ASCIIToUTF16("rgba(92 92 92 1)"));
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetARGB(255, 92, 92, 92));

  // Don't support the CSS hash color style
  result = metadata::SkColorConverter::FromString(base::ASCIIToUTF16("#03254"));
  EXPECT_FALSE(result);

  // Don't support some common invalid values
  result = metadata::SkColorConverter::FromString(
      base::ASCIIToUTF16("rgba(1,2,3,4)"));
  EXPECT_FALSE(result);

  result = metadata::SkColorConverter::FromString(
      base::ASCIIToUTF16("rgba(1,2,3,4"));
  EXPECT_FALSE(result);

  result = metadata::SkColorConverter::FromString(
      base::ASCIIToUTF16("hsla(1,2,3,4)"));
  EXPECT_FALSE(result);
}

TEST_F(TypeConversionTest, TestConversion_ColorParserTest) {
  using converter = metadata::SkColorConverter;
  base::string16 color;
  const base::string16 source = base::ASCIIToUTF16(
      "rgb(0, 128, 192), hsl(90, 100%, 30%), rgba(128, 128, 128, 0.5), "
      "hsla(240, 100%, 50%, 0.5)");
  auto start_pos = source.cbegin();
  EXPECT_TRUE(
      converter::GetNextColor(start_pos, source.cend(), color, start_pos));
  EXPECT_EQ(color, base::ASCIIToUTF16("rgb(0, 128, 192)"));
  EXPECT_TRUE(
      converter::GetNextColor(start_pos, source.cend(), color, start_pos));
  EXPECT_EQ(color, base::ASCIIToUTF16("hsl(90, 100%, 30%)"));
  EXPECT_TRUE(
      converter::GetNextColor(start_pos, source.cend(), color, start_pos));
  EXPECT_EQ(color, base::ASCIIToUTF16("rgba(128, 128, 128, 0.5)"));
  EXPECT_TRUE(converter::GetNextColor(start_pos, source.cend(), color));
  EXPECT_EQ(color, base::ASCIIToUTF16("hsla(240, 100%, 50%, 0.5)"));
}

TEST_F(TypeConversionTest, TestConversion_InsetsToString) {
  constexpr gfx::Insets kInsets(3, 5, 7, 9);

  base::string16 to_string =
      metadata::TypeConverter<gfx::Insets>::ToString(kInsets);

  EXPECT_EQ(to_string, base::ASCIIToUTF16(kInsets.ToString()));
}

TEST_F(TypeConversionTest, TestConversion_StringToInsets) {
  base::string16 from_string = base::ASCIIToUTF16("2,3,4,5");
  EXPECT_EQ(metadata::TypeConverter<gfx::Insets>::FromString(from_string),
            gfx::Insets(2, 3, 4, 5));
}

TEST_F(TypeConversionTest, TestConversion_VectorToString) {
  const std::vector<int> kVector{3, 5, 7, 9};

  base::string16 to_string =
      metadata::TypeConverter<std::vector<int>>::ToString(kVector);

  EXPECT_EQ(to_string, STRING16_LITERAL("{3,5,7,9}"));
}

TEST_F(TypeConversionTest, TestConversion_StringToVector) {
  base::string16 from_string = base::ASCIIToUTF16("{2,3,4,5}");
  EXPECT_EQ(metadata::TypeConverter<std::vector<int>>::FromString(from_string),
            std::vector<int>({2, 3, 4, 5}));
}

TEST_F(TypeConversionTest, CheckIsSerializable) {
  // Test types with explicitly added converters.
  EXPECT_TRUE(metadata::TypeConverter<int8_t>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<int16_t>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<int32_t>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<int64_t>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<uint8_t>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<uint16_t>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<uint32_t>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<uint64_t>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<float>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<double>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<bool>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<const char*>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<base::string16>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<gfx::ShadowValues>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<gfx::Size>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<gfx::Range>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<gfx::Insets>::IsSerializable());

  // Test enum type.
  EXPECT_TRUE(metadata::TypeConverter<TestResult>::IsSerializable());

  // Test aliased types.
  EXPECT_TRUE(metadata::TypeConverter<int>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<SkColor>::IsSerializable());

  // Test types with no explicit or aliased converters.
  EXPECT_FALSE(metadata::TypeConverter<
               views::Button::PressedCallback>::IsSerializable());
  EXPECT_FALSE(metadata::TypeConverter<views::FocusRing*>::IsSerializable());

  // Test base::Optional type.
  EXPECT_TRUE(
      metadata::TypeConverter<base::Optional<const char*>>::IsSerializable());
  EXPECT_TRUE(metadata::TypeConverter<base::Optional<int>>::IsSerializable());
  EXPECT_FALSE(metadata::TypeConverter<
               base::Optional<views::FocusRing*>>::IsSerializable());
}

}  // namespace views
