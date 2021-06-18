// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_manager.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
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
       ColorProviderManager::ContrastMode::kNormal});
}

}  // namespace

// Verifies that color providers endure for each call to GetColorProviderFor().
TEST_F(ColorProviderManagerTest, Persistence) {
  ColorProvider* provider = GetLightNormalColorProvider();
  ASSERT_NE(nullptr, provider);
  EXPECT_EQ(gfx::kPlaceholderColor, provider->GetColor(kColorTest0));
  provider->AddMixer().AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  EXPECT_EQ(SK_ColorGREEN,
            GetLightNormalColorProvider()->GetColor(kColorTest0));
}

// Verifies that the initializer is called for each newly created color
// provider.
TEST_F(ColorProviderManagerTest, SetInitializer) {
  ColorProviderManager::GetForTesting().AppendColorProviderInitializer(
      base::BindRepeating([](ColorProvider* provider,
                             ColorProviderManager::ColorMode,
                             ColorProviderManager::ContrastMode) {
        provider->AddMixer().AddSet(
            {kColorSetTest0, {{kColorTest0, SK_ColorBLUE}}});
      }));

  ColorProvider* provider = GetLightNormalColorProvider();
  ASSERT_NE(nullptr, provider);
  EXPECT_EQ(SK_ColorBLUE, provider->GetColor(kColorTest0));
}

// Verifies resetting the manager clears the provider. This is useful to keep
// unit tests isolated from each other.
TEST_F(ColorProviderManagerTest, Reset) {
  ColorProvider* provider = GetLightNormalColorProvider();
  ASSERT_NE(nullptr, provider);
  provider->AddMixer().AddSet({kColorSetTest0, {{kColorTest0, SK_ColorGREEN}}});
  ColorProviderManager::ResetForTesting();
  EXPECT_EQ(gfx::kPlaceholderColor,
            GetLightNormalColorProvider()->GetColor(kColorTest0));
}

}  // namespace ui
