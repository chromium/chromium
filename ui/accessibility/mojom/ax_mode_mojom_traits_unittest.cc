// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_mode_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/mojom/ax_mode.mojom.h"

namespace {

using mojo::test::SerializeAndDeserialize;

// Helper to define the domain of valid AXMode flags.
auto AnyAXModeFlag() {
  // LINT.IfChange
  return fuzztest::BitFlagCombinationOf({
      ui::AXMode::kNativeAPIs,
      ui::AXMode::kWebContents,
      ui::AXMode::kInlineTextBoxes,
      ui::AXMode::kExtendedProperties,
      ui::AXMode::kHTML,
      ui::AXMode::kHTMLMetadata,
      ui::AXMode::kLabelImages,
      ui::AXMode::kPDFPrinting,
      ui::AXMode::kAnnotateMainNode,
      ui::AXMode::kFromPlatform,
      ui::AXMode::kScreenReader,
      ui::AXMode::kNativeAdaptedWebContents,
  });
  // LINT.ThenChange(//ui/accessibility/ax_mode.h)
}

// Helper to define the domain of valid AXMode filter flags.
auto AnyAXModeFilterFlag() {
  // LINT.IfChange(ax_mode_filters)
  return fuzztest::BitFlagCombinationOf({
      ui::AXMode::kFormsAndLabelsOnly,
      ui::AXMode::kOnScreenOnly,
  });
  // LINT.ThenChange(//ui/accessibility/ax_mode.h:ax_mode_filters)
}

// Verifies that VALID AXModes always roundtrip successfully and remain equal.
void RoundtripEquality(uint32_t flags, uint32_t filter_flags) {
  ui::AXMode input(flags, filter_flags);
  ui::AXMode output;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<ax::mojom::AXMode>(input, output));
  EXPECT_EQ(input, output);
}

FUZZ_TEST(AXModeMojomTraitsFuzzTest, RoundtripEquality)
    .WithDomains(AnyAXModeFlag(), AnyAXModeFilterFlag());

// Verifies that ARBITRARY (potentially invalid) integers do not crash the
// deserializer/validation logic.
void DeserializationSafety(uint32_t flags, uint32_t filter_flags) {
  ui::AXMode input(flags, filter_flags);
  ui::AXMode output;

  SerializeAndDeserialize<ax::mojom::AXMode>(input, output);
}

FUZZ_TEST(AXModeMojomTraitsFuzzTest, DeserializationSafety)
    .WithDomains(fuzztest::Arbitrary<uint32_t>(),
                 fuzztest::Arbitrary<uint32_t>());

TEST(AXModeMojomTraitsTest, TestSerializeAndDeserializeAXModeData) {
  ui::AXMode input(ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents);
  ui::AXMode output;
  EXPECT_TRUE(SerializeAndDeserialize<ax::mojom::AXMode>(input, output));
  EXPECT_EQ(ui::kAXModeBasic, output);
}

}  // namespace
