// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/decoration_util.h"

#include <optional>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/decoration/decoration_details.h"
#include "ui/decoration/highlight_border_value.h"
#include "ui/decoration/shadow.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/shadow_value.h"

namespace ui::decoration {
namespace {

constexpr SkColor kHighlight = SK_ColorGREEN;
constexpr SkColor kBorder = SK_ColorRED;

// Tests the ShadowDetailsKey works properly for shadow details cache.
TEST(ShadowUtilTest, ShadowDetailsKey) {
  // Make a cache for the generated details such that they will not be removed
  // from the shadow details cache.
  std::vector<ShadowDetails> details;
  // Add first shadow details.
  details.emplace_back(ShadowDetails::Get(
      gfx::RoundedCornersF(2), Shadow::MakeShadowValues(/*elevation=*/4)));
  EXPECT_EQ(1u, ShadowDetails::GetDetailsCacheSizeForTest());
  EXPECT_EQ(details[0].aperture_insets,
            ShadowGenerator::GetNineboxApertureInsets(details[0].spec,
                                                      gfx::RoundedCornersF(2)));
  EXPECT_EQ(details[0].margins, ShadowGenerator::GetMargins(details[0].spec));

  // Add second shadow details with a different elevation.
  details.emplace_back(ShadowDetails::Get(
      /*rounded_corners=*/gfx::RoundedCornersF(2),
      Shadow::MakeShadowValues(/*elevation=*/5)));
  EXPECT_EQ(2u, ShadowDetails::GetDetailsCacheSizeForTest());

  // Add third shadow details with a different rounded corner radius.
  details.emplace_back(ShadowDetails::Get(
      /*rounded_corners=*/gfx::RoundedCornersF(3),
      Shadow::MakeShadowValues(/*elevation=*/5)));
  EXPECT_EQ(3u, ShadowDetails::GetDetailsCacheSizeForTest());

  // Add a same shadow details will not increase the cache.
  details.emplace_back(ShadowDetails::Get(
      /*rounded_corners=*/gfx::RoundedCornersF(2),
      Shadow::MakeShadowValues(/*elevation=*/4)));
  EXPECT_EQ(3u, ShadowDetails::GetDetailsCacheSizeForTest());

  // Add fourth shadow details with variable rounded corner radii.
  details.emplace_back(ShadowDetails::Get(
      /*rounded_corners=*/gfx::RoundedCornersF(1, 2, 3, 4),
      Shadow::MakeShadowValues(/*elevation=*/5)));
  EXPECT_EQ(4u, ShadowDetails::GetDetailsCacheSizeForTest());

  // Add fifth shadow details with a different key shadow blur than the first
  // details.
  const gfx::ShadowValues& values_1 = details[0].spec;
  gfx::ShadowValues new_blur_values = {
      gfx::ShadowValue(values_1[0].offset(), /*blur=*/20, values_1[0].color()),
      values_1[1]};
  details.emplace_back(ShadowDetails::Get(
      /*rounded_corners=*/gfx::RoundedCornersF(2), new_blur_values));
  EXPECT_EQ(5u, ShadowDetails::GetDetailsCacheSizeForTest());

  // Add sixth shadow details with a different ambient color than the second
  // details.
  const gfx::ShadowValues& values_2 = details[1].spec;
  gfx::ShadowValues new_color_values = {
      gfx::ShadowValue(values_2[0].offset(), values_2[0].blur(), SK_ColorBLUE),
      values_2[1]};
  details.emplace_back(ShadowDetails::Get(
      /*rounded_corners=*/gfx::RoundedCornersF(2), new_color_values));
  EXPECT_EQ(6u, ShadowDetails::GetDetailsCacheSizeForTest());

  // Add seventh shadow details with a different is_pill_shaped value than the
  // first details.
  details.emplace_back(ShadowDetails::Get(
      /*rounded_corners=*/gfx::RoundedCornersF(2),
      Shadow::MakeShadowValues(
          /*elevation=*/4, Shadow::Style::kMaterialDesign,
          /*colors=*/std::nullopt,
          /*is_pill_shaped=*/true)));
  EXPECT_EQ(7u, ShadowDetails::GetDetailsCacheSizeForTest());
}

TEST(HighlightBorderGeneratorTest, Insets) {
  const HighlightBorderValue value(kHighlight, kBorder);
  EXPECT_EQ(gfx::Outsets(), HighlightBorderGenerator::GetMargins(value));
  EXPECT_EQ(gfx::Insets(2), HighlightBorderGenerator::GetNineboxApertureInsets(
                                value, gfx::RoundedCornersF()));
  EXPECT_EQ(gfx::Insets(10), HighlightBorderGenerator::GetNineboxApertureInsets(
                                 value, gfx::RoundedCornersF(8)));
  EXPECT_EQ(gfx::Insets::TLBR(10, 18, 18, 14),
            HighlightBorderGenerator::GetNineboxApertureInsets(
                value, gfx::RoundedCornersF(/*upper_left=*/8,
                                            /*upper_right=*/4,
                                            /*lower_right=*/12,
                                            /*lower_left=*/16)));

  const HighlightBorderValue thick(kHighlight, kBorder, /*thickness=*/2);
  EXPECT_EQ(gfx::Outsets(), HighlightBorderGenerator::GetMargins(thick));
  EXPECT_EQ(gfx::Insets(12), HighlightBorderGenerator::GetNineboxApertureInsets(
                                 thick, gfx::RoundedCornersF(8)));

  const HighlightBorderValue pill(kHighlight, kBorder, /*thickness=*/1,
                                  /*is_pill_shaped=*/true);
  EXPECT_EQ(gfx::Outsets(), HighlightBorderGenerator::GetMargins(pill));
  EXPECT_EQ(gfx::Insets(18), HighlightBorderGenerator::GetNineboxApertureInsets(
                                 pill, gfx::RoundedCornersF(18)));
  EXPECT_EQ(gfx::Insets::TLBR(18, 18, 18, 2),
            HighlightBorderGenerator::GetNineboxApertureInsets(
                pill, gfx::RoundedCornersF(18, 0, 0, 18)));
}

// Tests that the details cache is keyed on the corner radii and every field of
// the HighlightBorderValue.
TEST(HighlightBorderGeneratorTest, DetailsCacheKey) {
  // Colors no other test uses, so that the first lookup misses and evicts any
  // unowned entries left behind by other tests; the cache growth is then
  // measured relative to that first insertion.
  constexpr SkColor kUniqueHighlight = SkColorSetRGB(0x12, 0x34, 0x56);
  constexpr SkColor kUniqueBorder = SkColorSetRGB(0x65, 0x43, 0x21);
  const gfx::RoundedCornersF radii(4);
  const HighlightBorderValue value(kUniqueHighlight, kUniqueBorder);

  // Hold on to the details so that they are not evicted from the cache.
  std::vector<HighlightBorderDetails> details;
  details.push_back(HighlightBorderDetails::Get(radii, value));
  const size_t base_size = HighlightBorderDetails::GetDetailsCacheSizeForTest();
  EXPECT_EQ(gfx::Size(13, 13), details[0].nine_patch_image.size());
  EXPECT_EQ(HighlightBorderGenerator::GetNineboxApertureInsets(value, radii),
            details[0].aperture_insets);
  EXPECT_EQ(HighlightBorderGenerator::GetMargins(value), details[0].margins);

  // The same key returns the same image.
  details.push_back(HighlightBorderDetails::Get(radii, value));
  EXPECT_EQ(base_size, HighlightBorderDetails::GetDetailsCacheSizeForTest());
  EXPECT_TRUE(details[1].nine_patch_image.BackedBySameObjectAs(
      details[0].nine_patch_image));

  details.push_back(
      HighlightBorderDetails::Get(gfx::RoundedCornersF(6), value));
  EXPECT_EQ(base_size + 1,
            HighlightBorderDetails::GetDetailsCacheSizeForTest());

  details.push_back(HighlightBorderDetails::Get(
      radii, HighlightBorderValue(SK_ColorBLUE, kUniqueBorder)));
  EXPECT_EQ(base_size + 2,
            HighlightBorderDetails::GetDetailsCacheSizeForTest());

  details.push_back(HighlightBorderDetails::Get(
      radii, HighlightBorderValue(kUniqueHighlight, SK_ColorBLUE)));
  EXPECT_EQ(base_size + 3,
            HighlightBorderDetails::GetDetailsCacheSizeForTest());

  details.push_back(HighlightBorderDetails::Get(
      radii,
      HighlightBorderValue(kUniqueHighlight, kUniqueBorder, /*thickness=*/2)));
  EXPECT_EQ(base_size + 4,
            HighlightBorderDetails::GetDetailsCacheSizeForTest());

  details.push_back(HighlightBorderDetails::Get(
      radii, HighlightBorderValue(kUniqueHighlight, kUniqueBorder,
                                  /*thickness=*/1, /*is_pill_shaped=*/true)));
  EXPECT_EQ(base_size + 5,
            HighlightBorderDetails::GetDetailsCacheSizeForTest());
}

using TestCompositeValue =
    CompositeValue<gfx::ShadowValues, HighlightBorderValue>;
using TestCompositeGenerator =
    CompositeGenerator<ShadowDetails, HighlightBorderDetails>;
using TestCompositeDetails =
    DecorationDetails<TestCompositeValue, TestCompositeGenerator>;

TEST(CompositeGeneratorTest, Insets) {
  const gfx::RoundedCornersF radii(8);
  const gfx::ShadowValues shadows = Shadow::MakeShadowValues(/*elevation=*/6);
  const HighlightBorderValue border(kHighlight, kBorder, /*thickness=*/1);

  // Shadow only.
  const TestCompositeValue shadow_only(shadows, std::nullopt);
  EXPECT_EQ(ShadowGenerator::GetMargins(shadows),
            TestCompositeGenerator::GetMargins(shadow_only));
  EXPECT_EQ(
      ShadowGenerator::GetNineboxApertureInsets(shadows, radii),
      TestCompositeGenerator::GetNineboxApertureInsets(shadow_only, radii));

  // Highlight border only.
  const TestCompositeValue border_only(std::nullopt, border);
  EXPECT_EQ(HighlightBorderGenerator::GetMargins(border),
            TestCompositeGenerator::GetMargins(border_only));
  EXPECT_EQ(
      HighlightBorderGenerator::GetNineboxApertureInsets(border, radii),
      TestCompositeGenerator::GetNineboxApertureInsets(border_only, radii));

  // Combined (shadow margins/aperture larger than 1px border).
  const TestCompositeValue combined(shadows, border);
  EXPECT_EQ(ShadowGenerator::GetMargins(shadows),
            TestCompositeGenerator::GetMargins(combined));
  EXPECT_EQ(ShadowGenerator::GetNineboxApertureInsets(shadows, radii),
            TestCompositeGenerator::GetNineboxApertureInsets(combined, radii));

  // Combined where thick border's inward reach exceeds a small elevation-1
  // shadow.
  const gfx::ShadowValues small_shadows =
      Shadow::MakeShadowValues(/*elevation=*/1);
  const HighlightBorderValue thick_border(kHighlight, kBorder, /*thickness=*/3);
  const TestCompositeValue thick_combined(small_shadows, thick_border);
  EXPECT_EQ(ShadowGenerator::GetMargins(small_shadows),
            TestCompositeGenerator::GetMargins(thick_combined));
  EXPECT_EQ(
      gfx::Insets::TLBR(15, 16, 17, 16),
      TestCompositeGenerator::GetNineboxApertureInsets(thick_combined, radii));
}

TEST(CompositeGeneratorTest, DetailsCacheKey) {
  constexpr SkColor kUniqueHighlight = SkColorSetRGB(0x23, 0x45, 0x67);
  constexpr SkColor kUniqueBorder = SkColorSetRGB(0x76, 0x54, 0x32);
  const gfx::RoundedCornersF radii(4);
  const gfx::ShadowValues shadows = Shadow::MakeShadowValues(/*elevation=*/4);
  const HighlightBorderValue border(kUniqueHighlight, kUniqueBorder);
  const TestCompositeValue value(shadows, border);

  std::vector<TestCompositeDetails> details;
  details.push_back(TestCompositeDetails::Get(radii, value));
  const size_t base_size = TestCompositeDetails::GetDetailsCacheSizeForTest();

  // Same key returns the same cached image.
  details.push_back(TestCompositeDetails::Get(radii, value));
  EXPECT_EQ(base_size, TestCompositeDetails::GetDetailsCacheSizeForTest());
  EXPECT_TRUE(details[1].nine_patch_image.BackedBySameObjectAs(
      details[0].nine_patch_image));

  // Different corner radii.
  details.push_back(TestCompositeDetails::Get(gfx::RoundedCornersF(6), value));
  EXPECT_EQ(base_size + 1, TestCompositeDetails::GetDetailsCacheSizeForTest());

  // Different shadow elevation.
  details.push_back(TestCompositeDetails::Get(
      radii,
      TestCompositeValue(Shadow::MakeShadowValues(/*elevation=*/8), border)));
  EXPECT_EQ(base_size + 2, TestCompositeDetails::GetDetailsCacheSizeForTest());

  // Shadow omitted.
  details.push_back(TestCompositeDetails::Get(
      radii, TestCompositeValue(std::nullopt, border)));
  EXPECT_EQ(base_size + 3, TestCompositeDetails::GetDetailsCacheSizeForTest());

  // Border omitted.
  details.push_back(TestCompositeDetails::Get(
      radii, TestCompositeValue(shadows, std::nullopt)));
  EXPECT_EQ(base_size + 4, TestCompositeDetails::GetDetailsCacheSizeForTest());
}

}  // namespace
}  // namespace ui::decoration
