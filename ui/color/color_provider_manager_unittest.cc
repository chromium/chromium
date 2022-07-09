// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_manager.h"

#include <vector>

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
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
      {ColorProviderManager::ColorMode::kLight,
       ColorProviderManager::ContrastMode::kNormal,
       ColorProviderManager::SystemTheme::kDefault,
       ColorProviderManager::FrameType::kChromium, absl::nullopt, nullptr});
}

// Returns a Key where |color| is the user_color value.
ColorProviderManager::Key UserColorKey(SkColor color) {
  return ColorProviderManager::Key(ColorProviderManager::ColorMode::kLight,
                                   ColorProviderManager::ContrastMode::kNormal,
                                   ColorProviderManager::SystemTheme::kDefault,
                                   ColorProviderManager::FrameType::kChromium,
                                   color, nullptr);
}

class TestInitializerSupplier
    : public ColorProviderManager::InitializerSupplier {
  void AddColorMixers(ColorProvider* provider,
                      const ColorProviderManager::Key& key) const override {}
};

}  // namespace

// Verifies that color providers endure for each call to GetColorProviderFor().
TEST_F(ColorProviderManagerTest, Persistence) {
  ColorProvider* provider = GetLightNormalColorProvider();
  ASSERT_NE(nullptr, provider);
  EXPECT_EQ(provider, GetLightNormalColorProvider());
}

// Verifies that the initializer is called for each newly created color
// provider.
TEST_F(ColorProviderManagerTest, SetInitializer) {
  ColorProviderManager::GetForTesting().AppendColorProviderInitializer(
      base::BindRepeating(
          [](ColorProvider* provider, const ColorProviderManager::Key&) {
            provider->AddMixer()[kColorTest0] = {SK_ColorBLUE};
          }));

  ColorProvider* provider = GetLightNormalColorProvider();
  ASSERT_NE(nullptr, provider);
  EXPECT_EQ(SK_ColorBLUE, provider->GetColor(kColorTest0));
}

// Verifies resetting the manager clears the provider. This is useful to keep
// unit tests isolated from each other.
TEST_F(ColorProviderManagerTest, Reset) {
  ColorProviderManager::GetForTesting().AppendColorProviderInitializer(
      base::BindRepeating(
          [](ColorProvider* provider, const ColorProviderManager::Key&) {
            provider->AddMixer()[kColorTest0] = {SK_ColorBLUE};
          }));
  ColorProvider* provider = GetLightNormalColorProvider();
  ASSERT_NE(nullptr, provider);
  EXPECT_EQ(SK_ColorBLUE, provider->GetColor(kColorTest0));
  ColorProviderManager::ResetForTesting();
  EXPECT_EQ(gfx::kPlaceholderColor,
            GetLightNormalColorProvider()->GetColor(kColorTest0));
}

TEST_F(ColorProviderManagerTest, LookupWithDeletedMember) {
  ColorProviderManager& manager = ColorProviderManager::GetForTesting();
  ColorProviderManager::Key key;

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
  ColorProviderManager::Key keys[2];

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

TEST_F(ColorProviderManagerTest, CacheLimits) {
  // Count each time colors are generated.
  int counter = 0;
  auto initializer = base::BindRepeating(
      [](int* inc, ColorProvider* provider, const ColorProviderManager::Key&) {
        provider->AddMixer()[kColorTest0] = {SK_ColorBLUE};
        (*inc)++;
      },
      &counter);

  // Only keep 4 color providers.
  ColorProviderManager& manager = ColorProviderManager::GetForTesting(4U);
  manager.AppendColorProviderInitializer(initializer);

  // We need 5 keys to test this.
  ColorProviderManager::Key keys[5] = {
      UserColorKey(SK_ColorGRAY), UserColorKey(SK_ColorWHITE),
      UserColorKey(SK_ColorRED), UserColorKey(SK_ColorBLUE),
      UserColorKey(SK_ColorMAGENTA)};

  for (const ColorProviderManager::Key& key : keys) {
    manager.GetColorProviderFor(key);
  }
  // 5 requests for different keys yields 5 runs of the initializer.
  EXPECT_EQ(5, counter);

  counter = 0;
  // Magenta is the most recent so it should not result in an evaluation.
  manager.GetColorProviderFor(keys[4]);
  EXPECT_EQ(0, counter);

  // Gray should have been evicted so it causes an evaluation.
  manager.GetColorProviderFor(keys[0]);
  EXPECT_EQ(1, counter);

  counter = 0;
  // The most recently used keys are grey, magenta, blue and red. Magenta should
  // not result in an evaluation.
  manager.GetColorProviderFor(keys[4]);
  EXPECT_EQ(0, counter);
}

}  // namespace ui
