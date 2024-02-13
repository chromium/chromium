// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/metadata/base_type_conversion.h"

#include "base/ranges/ranges.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

using TypeConversionTest = PlatformTest;

// Used in CheckIsSerializable test case.
enum TestResult {
  TEST_TRUE,
  TEST_FALSE,
};
DEFINE_ENUM_CONVERTERS(TestResult, {TEST_TRUE, u"TRUE"}, {TEST_FALSE, u"FALSE"})

TEST_F(TypeConversionTest, TestConversion_IntToString) {
  int from_int = 5;
  std::u16string to_string =
      ui::metadata::TypeConverter<int>::ToString(from_int);

  EXPECT_EQ(to_string, u"5");
}

TEST_F(TypeConversionTest, TestConversion_StringToInt) {
  std::u16string from_string = u"10";
  EXPECT_EQ(ui::metadata::TypeConverter<int>::FromString(from_string), 10);
}

// This tests whether the converter handles a bogus input string, in which case
// the return value should be nullopt.
TEST_F(TypeConversionTest, TestConversion_BogusStringToInt) {
  std::u16string from_string = u"Foo";
  EXPECT_EQ(ui::metadata::TypeConverter<int>::FromString(from_string),
            std::nullopt);
}

TEST_F(TypeConversionTest, TestConversion_BogusStringToFloat) {
  std::u16string from_string = u"1.2";
  EXPECT_EQ(ui::metadata::TypeConverter<float>::FromString(from_string), 1.2f);
}

TEST_F(TypeConversionTest, TestConversion_OptionalIntToString) {
  std::optional<int> src;
  std::u16string to_string =
      ui::metadata::TypeConverter<std::optional<int>>::ToString(src);
  EXPECT_EQ(to_string, ui::metadata::GetNullOptStr());

  src = 5;
  to_string = ui::metadata::TypeConverter<std::optional<int>>::ToString(src);
  EXPECT_EQ(to_string, u"5");
}

TEST_F(TypeConversionTest, TestConversion_StringToOptionalInt) {
  std::optional<int> ret;
  EXPECT_EQ(ui::metadata::TypeConverter<std::optional<int>>::FromString(
                ui::metadata::GetNullOptStr()),
            std::make_optional(ret));

  EXPECT_EQ(ui::metadata::TypeConverter<std::optional<int>>::FromString(u"10"),
            10);

  EXPECT_EQ(ui::metadata::TypeConverter<std::optional<int>>::FromString(u"ab0"),
            std::nullopt);
}

TEST_F(TypeConversionTest, TestConversion_ShadowValuesToString) {
  gfx::ShadowValues shadow_values;
  shadow_values.emplace_back(gfx::Vector2d(1, 2), .3,
                             SkColorSetARGB(128, 255, 0, 0));

  EXPECT_EQ(
      ui::metadata::TypeConverter<gfx::ShadowValues>::ToString(shadow_values),
      u"[ (1,2),0.30,rgba(255,0,0,128) ]");

  shadow_values.emplace_back(gfx::Vector2d(9, 8), .76,
                             SkColorSetARGB(20, 0, 64, 255));

  EXPECT_EQ(
      ui::metadata::TypeConverter<gfx::ShadowValues>::ToString(shadow_values),
      u"[ (1,2),0.30,rgba(255,0,0,128); (9,8),0.76,rgba(0,64,255,20) ]");
}

TEST_F(TypeConversionTest, TestConversion_StringToShadowValues) {
  std::optional<gfx::ShadowValues> opt_result =
      ui::metadata::TypeConverter<gfx::ShadowValues>::FromString(
          u"[ (6,4),0.53,rgba(23,44,0,1); (93,83),4.33,rgba(10,20,0,0.059) ]");

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
  std::optional<SkColor> result =
      ui::metadata::SkColorConverter::FromString(u"0x112233");
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetRGB(0x11, 0x22, 0x33));

  // Check conversion from argb hex string
  result = ui::metadata::SkColorConverter::FromString(u"0x7F112233");
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetARGB(0x7F, 0x11, 0x22, 0x33));

  // Check conversion from rgb(r,g,b) string
  result = ui::metadata::SkColorConverter::FromString(u"rgb(0, 128, 192)");
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetRGB(0, 128, 192));

  // Check conversion from rgba(r,g,b,a) string
  result =
      ui::metadata::SkColorConverter::FromString(u"rgba(0, 128, 192, 0.5)");
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetARGB(128, 0, 128, 192));

  // Check conversion from hsl(h,s,l) string
  result = ui::metadata::SkColorConverter::FromString(u"hsl(195, 100%, 50%)");
  EXPECT_TRUE(result);
  const SkScalar hsv[3] = {195.0, 1.0, 0.5};
  EXPECT_EQ(result.value(), SkHSVToColor(hsv));

  // Check conversion from hsla(h,s,l,a) string
  result =
      ui::metadata::SkColorConverter::FromString(u"hsl(195, 100%, 50%, 0.5)");
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkHSVToColor(128, hsv));

  // Check conversion from a decimal integer value
  result = ui::metadata::SkColorConverter::FromString(u"4278239231");
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetARGB(255, 0, 191, 255));

  // Check without commas.
  result = ui::metadata::SkColorConverter::FromString(u"rgba(92 92 92 1)");
  EXPECT_TRUE(result);
  EXPECT_EQ(result.value(), SkColorSetARGB(255, 92, 92, 92));

  // Don't support the CSS hash color style
  result = ui::metadata::SkColorConverter::FromString(u"#03254");
  EXPECT_FALSE(result);

  // Don't support some common invalid values
  result = ui::metadata::SkColorConverter::FromString(u"rgba(1,2,3,4)");
  EXPECT_FALSE(result);

  result = ui::metadata::SkColorConverter::FromString(u"rgba(1,2,3,4");
  EXPECT_FALSE(result);

  result = ui::metadata::SkColorConverter::FromString(u"hsla(1,2,3,4)");
  EXPECT_FALSE(result);
}

TEST_F(TypeConversionTest, TestConversion_ColorParserTest) {
  using converter = ui::metadata::SkColorConverter;
  std::u16string color;
  const std::u16string source =
      u"rgb(0, 128, 192), hsl(90, 100%, 30%), rgba(128, 128, 128, 0.5), "
      u"hsla(240, 100%, 50%, 0.5)";
  auto start_pos = source.cbegin();
  EXPECT_TRUE(
      converter::GetNextColor(start_pos, source.cend(), color, start_pos));
  EXPECT_EQ(color, u"rgb(0, 128, 192)");
  EXPECT_TRUE(
      converter::GetNextColor(start_pos, source.cend(), color, start_pos));
  EXPECT_EQ(color, u"hsl(90, 100%, 30%)");
  EXPECT_TRUE(
      converter::GetNextColor(start_pos, source.cend(), color, start_pos));
  EXPECT_EQ(color, u"rgba(128, 128, 128, 0.5)");
  EXPECT_TRUE(converter::GetNextColor(start_pos, source.cend(), color));
  EXPECT_EQ(color, u"hsla(240, 100%, 50%, 0.5)");
}

TEST_F(TypeConversionTest, TestConversion_InsetsToString) {
  constexpr auto kInsets = gfx::Insets::TLBR(3, 5, 7, 9);

  std::u16string to_string =
      ui::metadata::TypeConverter<gfx::Insets>::ToString(kInsets);

  EXPECT_EQ(to_string, u"3,5,7,9");
}

TEST_F(TypeConversionTest, TestConversion_StringToInsets) {
  std::u16string from_string = u"2,3,4,5";
  EXPECT_EQ(ui::metadata::TypeConverter<gfx::Insets>::FromString(from_string),
            gfx::Insets::TLBR(2, 3, 4, 5));
}

TEST_F(TypeConversionTest, TestConversion_VectorToString) {
  const std::vector<int> kVector{3, 5, 7, 9};

  std::u16string to_string =
      ui::metadata::TypeConverter<std::vector<int>>::ToString(kVector);

  EXPECT_EQ(to_string, u"{3,5,7,9}");
}

TEST_F(TypeConversionTest, TestConversion_StringToVector) {
  std::u16string from_string = u"{2,3,4,5}";
  EXPECT_EQ(
      ui::metadata::TypeConverter<std::vector<int>>::FromString(from_string),
      std::vector<int>({2, 3, 4, 5}));
}

TEST_F(TypeConversionTest, CheckIsSerializable) {
  // Test types with explicitly added converters.
  EXPECT_TRUE(ui::metadata::TypeConverter<int8_t>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<int16_t>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<int32_t>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<int64_t>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<uint8_t>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<uint16_t>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<uint32_t>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<uint64_t>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<float>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<double>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<bool>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<const char*>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<std::u16string>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<gfx::ShadowValues>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<gfx::Size>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<gfx::Range>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<gfx::Insets>::IsSerializable());

  // Test enum type.
  EXPECT_TRUE(ui::metadata::TypeConverter<TestResult>::IsSerializable());

  // Test aliased types.
  EXPECT_TRUE(ui::metadata::TypeConverter<int>::IsSerializable());
  EXPECT_TRUE(ui::metadata::TypeConverter<SkColor>::IsSerializable());

  // Test std::optional type.
  EXPECT_TRUE(ui::metadata::TypeConverter<
              std::optional<const char*>>::IsSerializable());
  EXPECT_TRUE(
      ui::metadata::TypeConverter<std::optional<int>>::IsSerializable());
}
