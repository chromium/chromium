// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_manager.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_test_ids.h"
#include "ui/gfx/color_palette.h"

namespace ui {

namespace {

class ColorProviderManagerTest : public testing::Test {
 public:
  ColorProviderManagerTest() { ColorProviderManager::ResetForTesting(); }
  ColorProviderManagerTest(const ColorProviderManagerTest&) = delete;
  ColorProviderManagerTest& operator=(const ColorProviderManagerTest&) = delete;
  ~ColorProviderManagerTest() override {
    ColorProviderManager::ResetForTesting();
  }
};

ColorProvider* GetLightNormalColorProvider() {
  return ColorProviderManager::GetForTesting().GetColorProviderFor(
      ui::ColorProviderKey());
}

class TestInitializerSupplier : public ColorProviderKey::InitializerSupplier {
  void AddColorMixers(ColorProvider* provider,
                      const ColorProviderKey& key) const override {}
};

}  // namespace

// Verifies that color providers endure for each call to GetColorProviderFor().
TEST_F(ColorProviderManagerTest, Persistence) {
  base::HistogramTester histogram_tester;
  ColorProvider* provider = GetLightNormalColorProvider();
  ASSERT_NE(nullptr, provider);
  EXPECT_EQ(provider, GetLightNormalColorProvider());
  histogram_tester.ExpectTotalCount(
      "Views.Browser.TimeSpentInitializingColorProvider", 1);
}

// Verifies that the initializer is called for each newly created color
// provider.
TEST_F(ColorProviderManagerTest, SetInitializer) {
  ColorProviderManager::GetForTesting().AppendColorProviderInitializer(
      base::BindRepeating([](ColorProvider* provider, const ColorProviderKey&) {
        provider->AddMixer()[kColorTest0] = {SK_ColorBLUE};
      }));

  base::HistogramTester histogram_tester;
  ColorProvider* provider = GetLightNormalColorProvider();
  ASSERT_NE(nullptr, provider);
  EXPECT_EQ(SK_ColorBLUE, provider->GetColor(kColorTest0));
  histogram_tester.ExpectTotalCount(
      "Views.Browser.TimeSpentInitializingColorProvider", 1);
}

// Verifies resetting the manager clears the provider. This is useful to keep
// unit tests isolated from each other.
TEST_F(ColorProviderManagerTest, Reset) {
  ColorProviderManager::GetForTesting().AppendColorProviderInitializer(
      base::BindRepeating([](ColorProvider* provider, const ColorProviderKey&) {
        provider->AddMixer()[kColorTest0] = {SK_ColorBLUE};
      }));

  base::HistogramTester histogram_tester;
  ColorProvider* provider = GetLightNormalColorProvider();
  ASSERT_NE(nullptr, provider);
  EXPECT_EQ(SK_ColorBLUE, provider->GetColor(kColorTest0));
  ColorProviderManager::ResetForTesting();
  EXPECT_EQ(gfx::kPlaceholderColor,
            GetLightNormalColorProvider()->GetColor(kColorTest0));
  histogram_tester.ExpectTotalCount(
      "Views.Browser.TimeSpentInitializingColorProvider", 2);
}

TEST_F(ColorProviderManagerTest, LookupWithDeletedMember) {
  ColorProviderManager& manager = ColorProviderManager::GetForTesting();
  ColorProviderKey key;

  {
    TestInitializerSupplier supplier;
    key.app_controller = &supplier;

    EXPECT_TRUE(manager.GetColorProviderFor(key));
    key.app_controller = &supplier;
  }

  // key.app_controller is now invalid but shouldn't be dereferenced so the key
  // is still safe to use.
  EXPECT_TRUE(manager.GetColorProviderFor(key));
}

TEST_F(ColorProviderManagerTest, KeyOrderIsStable) {
  ColorProviderKey keys[2];

  // Allocate two suppliers.
  std::vector<TestInitializerSupplier> supplier(2);
  keys[0].app_controller = &supplier[0];
  keys[1].app_controller = &supplier[1];

  // Validate order.
  ASSERT_LT(keys[0], keys[1]);

  // Delete the higher of the two suppliers.
  supplier.pop_back();

  // Verify that the order hasn't changed.
  EXPECT_LT(keys[0], keys[1]);
}

}  // namespace ui
