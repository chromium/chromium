// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/util/display_manager_test_util.h"

#include <stdint.h>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_features.h"

namespace display {

namespace {

// Use larger than max int to catch overflow early. This should be equal to the
// same name constant in display_util.cc.
constexpr int64_t kSynthesizedDisplayIdStart = 2200000000LL;

}  // namespace

TEST(DisplayManagerTestUtilTest, TestResetDisplayIdForTest) {
  ResetDisplayIdForTest();

  const int64_t kFirstId = GetASynthesizedDisplayId();
  EXPECT_EQ(kSynthesizedDisplayIdStart, kFirstId);
  const int64_t kFirstEdidConnectorIndex =
      GetNextSynthesizedEdidDisplayConnectorIndex();
  EXPECT_EQ(1L, kFirstEdidConnectorIndex);

  // Synthesize a hundred different display IDs and EDID-based display connector
  // indices.
  for (size_t i = 0; i < 100; ++i) {
    GetASynthesizedDisplayId();
    GetNextSynthesizedEdidDisplayConnectorIndex();
  }
  ResetDisplayIdForTest();

  EXPECT_EQ(kFirstId, GetASynthesizedDisplayId());
  EXPECT_EQ(kFirstEdidConnectorIndex,
            GetNextSynthesizedEdidDisplayConnectorIndex());
}

TEST(DisplayManagerTestUtilTest, TestGetASynthesizedDisplayId) {
  ResetDisplayIdForTest();

  // Test port-based synthesized IDs, which increment by 1 after the initial
  // value of kSynthesizedDisplayIdStart.
  EXPECT_EQ(kSynthesizedDisplayIdStart, GetASynthesizedDisplayId());
  EXPECT_EQ(2200000257, GetASynthesizedDisplayId());
  EXPECT_EQ(2200000258, GetASynthesizedDisplayId());
  EXPECT_EQ(2200000259, GetASynthesizedDisplayId());
  EXPECT_EQ(2200000260, GetASynthesizedDisplayId());
  EXPECT_EQ(2200000261, GetASynthesizedDisplayId());
  EXPECT_EQ(2200000262, GetASynthesizedDisplayId());

  // Test EDID-based synthesized IDs, which are incremented by 0x100 after the
  // initial value of kSynthesizedDisplayIdStart.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      display::features::kEnableEdidBasedDisplayIds);
  ResetDisplayIdForTest();

  EXPECT_EQ(kSynthesizedDisplayIdStart, GetASynthesizedDisplayId());
  EXPECT_EQ(2200000256, GetASynthesizedDisplayId());
  EXPECT_EQ(2200000512, GetASynthesizedDisplayId());
  EXPECT_EQ(2200000768, GetASynthesizedDisplayId());
  EXPECT_EQ(2200001024, GetASynthesizedDisplayId());
  EXPECT_EQ(2200001280, GetASynthesizedDisplayId());
  EXPECT_EQ(2200001536, GetASynthesizedDisplayId());
}

TEST(DisplayManagerTestUtilTest, TestSynthesizeDisplayIdFromSeed) {
  constexpr int64_t kFirstIdBase = 1000000000;
  constexpr int64_t kFirstIdConnectorIndex = 1;
  constexpr int64_t kFirstId = kFirstIdBase + kFirstIdConnectorIndex;

  const int64_t kSecondId = SynthesizeDisplayIdFromSeed(kFirstId);
  EXPECT_EQ(1000000002, kSecondId);

  // Connector index is stored in the first 8 bits for port-base display IDs.
  const int64_t kSecondIdConnectorIndex = kSecondId & 0xFF;
  EXPECT_EQ(kFirstIdConnectorIndex + 1, kSecondIdConnectorIndex);

  const int64_t kSecondIdBase =
      static_cast<int64_t>(~static_cast<uint64_t>(0xFF) & kSecondId);
  EXPECT_EQ(kFirstIdBase, kSecondIdBase);

  // Make sure the returned synthesized ID doesn't change for the same seed.
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_EQ(kSecondId, SynthesizeDisplayIdFromSeed(kFirstId));
  }

  // Test when the seed is equal to initial synthesized display ID
  // kSynthesizedDisplayIdStart.
  constexpr int64_t kExpectedIdAfterInitialValue =
      kSynthesizedDisplayIdStart + 0x101;
  EXPECT_EQ(kExpectedIdAfterInitialValue,
            SynthesizeDisplayIdFromSeed(kSynthesizedDisplayIdStart));
}

TEST(DisplayManagerTestUtilTest, TestProduceAlternativeSchemeIdForId) {
  EXPECT_EQ(1200000000,
            ProduceAlternativeSchemeIdForId(kSynthesizedDisplayIdStart));
  EXPECT_EQ(1200000257, ProduceAlternativeSchemeIdForId(2200000257));
  EXPECT_EQ(1000000000, ProduceAlternativeSchemeIdForId(2000000000));

  EXPECT_EQ(234, ProduceAlternativeSchemeIdForId(1234));
  EXPECT_EQ(111, ProduceAlternativeSchemeIdForId(1111));
  EXPECT_EQ(100, ProduceAlternativeSchemeIdForId(1100));

  EXPECT_EQ(20, ProduceAlternativeSchemeIdForId(10));
  EXPECT_EQ(17, ProduceAlternativeSchemeIdForId(7));
}

}  // namespace display
