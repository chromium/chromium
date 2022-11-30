// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/haptic_touchpad_handler.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"

namespace ui {

class TestHapticTouchpadHandler : public HapticTouchpadHandler {
 public:
  explicit TestHapticTouchpadHandler(int fd) : HapticTouchpadHandler(fd) {}

  ~TestHapticTouchpadHandler() override = default;

  void PlayFfEffect(int effect_id) override {
    played_effect_ids_.push_back(effect_id);
  }

  int UploadFfEffect(uint16_t hid_usage, uint8_t intensity) override {
    return ++num_effects_uploaded_;
  }

  bool TakeControlOfClickEffects() override { return true; }

  void DestroyFfEffect(int effect_id) override {}

  int num_effects_uploaded_ = 0;
  std::vector<int> played_effect_ids_;
};

class HapticTouchpadHandlerTest : public testing::Test {
 public:
  HapticTouchpadHandlerTest() = default;

  HapticTouchpadHandlerTest(const HapticTouchpadHandlerTest&) = delete;
  HapticTouchpadHandlerTest& operator=(const HapticTouchpadHandlerTest&) =
      delete;

  std::unique_ptr<ui::TestHapticTouchpadHandler> CreateHandler(
      const ui::DeviceCapabilities& caps) {
    ui::EventDeviceInfo devinfo;
    CapabilitiesToDeviceInfo(caps, &devinfo);
    return std::make_unique<TestHapticTouchpadHandler>(1234);
  }
};

TEST_F(HapticTouchpadHandlerTest, BasicUiEffects) {
  std::unique_ptr<ui::TestHapticTouchpadHandler> handler =
      CreateHandler(kRedrixTouchpad);
  handler->Initialize();

  EXPECT_TRUE(handler->IsValid());

  // Three effects requested from the UI.  The first and last are the same, so
  // should use the same effect ID.
  handler->PlayEffect(HapticTouchpadEffect::kSnap,
                      HapticTouchpadEffectStrength::kLow);
  handler->PlayEffect(HapticTouchpadEffect::kSnap,
                      HapticTouchpadEffectStrength::kMedium);
  handler->PlayEffect(HapticTouchpadEffect::kSnap,
                      HapticTouchpadEffectStrength::kLow);

  EXPECT_EQ(3UL, handler->played_effect_ids_.size());
  EXPECT_EQ(handler->played_effect_ids_[0], handler->played_effect_ids_[2]);
  EXPECT_NE(handler->played_effect_ids_[0], handler->played_effect_ids_[1]);
}

TEST_F(HapticTouchpadHandlerTest, BasicButtonClickEffects) {
  std::unique_ptr<ui::TestHapticTouchpadHandler> handler =
      CreateHandler(kRedrixTouchpad);
  handler->Initialize();

  // Two full button clicks. The press effects should be the same as each other,
  // but different from the release effects.
  handler->OnGestureClick(true);
  handler->OnGestureClick(false);
  handler->OnGestureClick(true);
  handler->OnGestureClick(false);

  EXPECT_EQ(4UL, handler->played_effect_ids_.size());
  EXPECT_EQ(handler->played_effect_ids_[0], handler->played_effect_ids_[2]);
  EXPECT_EQ(handler->played_effect_ids_[1], handler->played_effect_ids_[3]);
  EXPECT_NE(handler->played_effect_ids_[0], handler->played_effect_ids_[1]);
}

TEST_F(HapticTouchpadHandlerTest, SetReleaseEffectWhenButtonNotPressed) {
  std::unique_ptr<ui::TestHapticTouchpadHandler> handler =
      CreateHandler(kRedrixTouchpad);
  handler->Initialize();

  // Set the effect for next button release when the button is _not_ currently
  // pressed: No effect, use the default release effect.
  handler->OnGestureClick(true);
  handler->OnGestureClick(false);
  handler->SetEffectForNextButtonRelease(HapticTouchpadEffect::kKnock,
                                         HapticTouchpadEffectStrength::kHigh);
  handler->OnGestureClick(true);
  handler->OnGestureClick(false);

  EXPECT_EQ(4UL, handler->played_effect_ids_.size());
  EXPECT_EQ(handler->played_effect_ids_[1], handler->played_effect_ids_[3]);
}

TEST_F(HapticTouchpadHandlerTest, SetReleaseEffectWhenButtonIsPressed) {
  std::unique_ptr<ui::TestHapticTouchpadHandler> handler =
      CreateHandler(kRedrixTouchpad);
  handler->Initialize();

  // Set the effect for next button release when the button is currently
  // pressed: Use new release effect once, then revert to default.
  handler->OnGestureClick(true);
  handler->OnGestureClick(false);
  handler->OnGestureClick(true);
  handler->SetEffectForNextButtonRelease(HapticTouchpadEffect::kKnock,
                                         HapticTouchpadEffectStrength::kHigh);
  handler->OnGestureClick(false);
  handler->OnGestureClick(true);
  handler->OnGestureClick(false);

  EXPECT_EQ(6UL, handler->played_effect_ids_.size());
  EXPECT_NE(handler->played_effect_ids_[1], handler->played_effect_ids_[3]);
  EXPECT_EQ(handler->played_effect_ids_[1], handler->played_effect_ids_[5]);
}

TEST_F(HapticTouchpadHandlerTest, SetStrengthForClickEffects) {
  std::unique_ptr<ui::TestHapticTouchpadHandler> handler =
      CreateHandler(kRedrixTouchpad);
  handler->Initialize();

  // Set the strength for click effects. The effect ID should change for press
  // and release.
  handler->OnGestureClick(true);
  handler->OnGestureClick(false);
  handler->SetClickStrength(HapticTouchpadEffectStrength::kHigh);
  handler->OnGestureClick(true);
  handler->OnGestureClick(false);
  handler->OnGestureClick(true);
  handler->OnGestureClick(false);

  EXPECT_EQ(6UL, handler->played_effect_ids_.size());
  EXPECT_NE(handler->played_effect_ids_[0], handler->played_effect_ids_[2]);
  EXPECT_NE(handler->played_effect_ids_[1], handler->played_effect_ids_[3]);
  EXPECT_EQ(handler->played_effect_ids_[2], handler->played_effect_ids_[4]);
  EXPECT_EQ(handler->played_effect_ids_[3], handler->played_effect_ids_[5]);
}

}  // namespace ui
