// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/update_display_configuration_task.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/fake/fake_display_snapshot.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/test_native_display_delegate.h"

namespace display {
namespace test {

namespace {

class TestSoftwareMirroringController
    : public DisplayConfigurator::SoftwareMirroringController {
 public:
  TestSoftwareMirroringController() : is_enabled_(false) {}
  ~TestSoftwareMirroringController() override {}

  // DisplayConfigurator::SoftwareMirroringController:
  void SetSoftwareMirroring(bool enabled) override { is_enabled_ = enabled; }
  bool SoftwareMirroringEnabled() const override { return is_enabled_; }
  bool IsSoftwareMirroringEnforced() const override { return false; }

 private:
  bool is_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestSoftwareMirroringController);
};

class TestDisplayLayoutManager : public DisplayLayoutManager {
 public:
  TestDisplayLayoutManager()
      : should_mirror_(true),
        display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
        power_state_(chromeos::DISPLAY_POWER_ALL_ON) {}
  ~TestDisplayLayoutManager() override {}

  void set_should_mirror(bool should_mirror) { should_mirror_ = should_mirror; }

  void set_display_state(MultipleDisplayState state) { display_state_ = state; }

  void set_power_state(chromeos::DisplayPowerState state) {
    power_state_ = state;
  }

  void set_software_mirroring_controller(
      std::unique_ptr<DisplayConfigurator::SoftwareMirroringController>
          software_mirroring_controller) {
    software_mirroring_controller_ = std::move(software_mirroring_controller);
  }

  // DisplayConfigurator::DisplayLayoutManager:
  DisplayConfigurator::SoftwareMirroringController*
  GetSoftwareMirroringController() const override {
    return software_mirroring_controller_.get();
  }

  DisplayConfigurator::StateController* GetStateController() const override {
    return nullptr;
  }

  MultipleDisplayState GetDisplayState() const override {
    return display_state_;
  }

  chromeos::DisplayPowerState GetPowerState() const override {
    return power_state_;
  }

  bool GetDisplayLayout(
      const std::vector<DisplaySnapshot*>& displays,
      MultipleDisplayState new_display_state,
      chromeos::DisplayPowerState new_power_state,
      std::vector<DisplayConfigureRequest>* requests) const override {
    gfx::Point origin;
    for (DisplaySnapshot* display : displays) {
      const DisplayMode* mode = display->native_mode();
      if (new_display_state == MULTIPLE_DISPLAY_STATE_MULTI_MIRROR)
        mode = should_mirror_ ? FindMirrorMode(displays) : nullptr;

      if (!mode)
        return false;

      if (new_power_state == chromeos::DISPLAY_POWER_ALL_ON) {
        requests->push_back(DisplayConfigureRequest(display, mode, origin));
      } else {
        requests->push_back(DisplayConfigureRequest(display, nullptr, origin));
      }

      if (new_display_state != MULTIPLE_DISPLAY_STATE_MULTI_MIRROR)
        origin.Offset(0, mode->size().height());
    }

    return true;
  }

  DisplayConfigurator::DisplayStateList GetDisplayStates() const override {
    NOTREACHED();
    return DisplayConfigurator::DisplayStateList();
  }

  bool IsMirroring() const override {
    return display_state_ == MULTIPLE_DISPLAY_STATE_MULTI_MIRROR;
  }

 private:
  const DisplayMode* FindMirrorMode(
      const std::vector<DisplaySnapshot*>& displays) const {
    const DisplayMode* mode = displays[0]->native_mode();
    for (DisplaySnapshot* display : displays) {
      if (mode->size().GetArea() > display->native_mode()->size().GetArea())
        mode = display->native_mode();
    }

    return mode;
  }

  bool should_mirror_;

  MultipleDisplayState display_state_;

  chromeos::DisplayPowerState power_state_;

  std::unique_ptr<DisplayConfigurator::SoftwareMirroringController>
      software_mirroring_controller_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayLayoutManager);
};

class UpdateDisplayConfigurationTaskTest : public testing::Test {
 public:
  UpdateDisplayConfigurationTaskTest()
      : delegate_(&log_),
        small_mode_(gfx::Size(1366, 768), false, 60.0f),
        big_mode_(gfx::Size(2560, 1600), false, 60.0f),
        configured_(false),
        configuration_status_(false),
        display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
        power_state_(chromeos::DISPLAY_POWER_ALL_ON) {
    displays_[0] = FakeDisplaySnapshot::Builder()
                       .SetId(123)
                       .SetNativeMode(small_mode_.Clone())
                       .SetCurrentMode(small_mode_.Clone())
                       .Build();

    displays_[1] = FakeDisplaySnapshot::Builder()
                       .SetId(456)
                       .SetNativeMode(big_mode_.Clone())
                       .SetCurrentMode(big_mode_.Clone())
                       .AddMode(small_mode_.Clone())
                       .Build();
  }
  ~UpdateDisplayConfigurationTaskTest() override {}

  void UpdateDisplays(size_t count) {
    std::vector<DisplaySnapshot*> displays;
    for (size_t i = 0; i < count; ++i)
      displays.push_back(displays_[i].get());

    delegate_.set_outputs(displays);
  }

  void ResponseCallback(
      bool success,
      const std::vector<DisplaySnapshot*>& displays,
      const std::vector<DisplaySnapshot*>& unassociated_displays,
      MultipleDisplayState new_display_state,
      chromeos::DisplayPowerState new_power_state) {
    configured_ = true;
    configuration_status_ = success;
    display_states_ = displays;
    display_state_ = new_display_state;
    power_state_ = new_power_state;

    if (success) {
      layout_manager_.set_display_state(display_state_);
      layout_manager_.set_power_state(power_state_);
    }
  }

 protected:
  ActionLogger log_;
  TestNativeDisplayDelegate delegate_;
  TestDisplayLayoutManager layout_manager_;

  const DisplayMode small_mode_;
  const DisplayMode big_mode_;

  std::unique_ptr<DisplaySnapshot> displays_[2];

  bool configured_;
  bool configuration_status_;
  std::vector<DisplaySnapshot*> display_states_;
  MultipleDisplayState display_state_;
  chromeos::DisplayPowerState power_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateDisplayConfigurationTaskTest);
};

}  // namespace

TEST_F(UpdateDisplayConfigurationTaskTest, HeadlessConfiguration) {
  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_HEADLESS,
        chromeos::DISPLAY_POWER_ALL_ON, 0, false,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_HEADLESS, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, SingleConfiguration) {
  UpdateDisplays(1);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_SINGLE,
        chromeos::DISPLAY_POWER_ALL_ON, 0, false,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(
      JoinActions(
          GetCrtcAction(*displays_[0], &small_mode_, gfx::Point()).c_str(),
          nullptr),
      log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, ExtendedConfiguration) {
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
        chromeos::DISPLAY_POWER_ALL_ON, 0, false,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(
      JoinActions(
          GetCrtcAction(*displays_[0], &small_mode_, gfx::Point()).c_str(),
          GetCrtcAction(*displays_[1], &big_mode_,
                        gfx::Point(0, small_mode_.size().height()))
              .c_str(),
          nullptr),
      log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, MirrorConfiguration) {
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_MIRROR,
        chromeos::DISPLAY_POWER_ALL_ON, 0, false,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(
      JoinActions(
          GetCrtcAction(*displays_[0], &small_mode_, gfx::Point()).c_str(),
          GetCrtcAction(*displays_[1], &small_mode_, gfx::Point()).c_str(),
          nullptr),
      log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, FailMirrorConfiguration) {
  layout_manager_.set_should_mirror(false);
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_MIRROR,
        chromeos::DISPLAY_POWER_ALL_ON, 0, false,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_FALSE(configuration_status_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, FailExtendedConfiguration) {
  delegate_.set_max_configurable_pixels(1);
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
        chromeos::DISPLAY_POWER_ALL_ON, 0, false,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_FALSE(configuration_status_);
  EXPECT_EQ(
      JoinActions(
          GetCrtcAction(*displays_[0], &small_mode_, gfx::Point()).c_str(),
          GetCrtcAction(*displays_[1], &big_mode_,
                        gfx::Point(0, small_mode_.size().height()))
              .c_str(),
          GetCrtcAction(*displays_[1], &small_mode_,
                        gfx::Point(0, small_mode_.size().height()))
              .c_str(),
          nullptr),
      log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, SingleChangePowerConfiguration) {
  UpdateDisplays(1);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_SINGLE,
        chromeos::DISPLAY_POWER_ALL_ON, 0, false,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(
      JoinActions(
          GetCrtcAction(*displays_[0], &small_mode_, gfx::Point()).c_str(),
          nullptr),
      log_.GetActionsAndClear());

  // Turn power off
  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_SINGLE,
        chromeos::DISPLAY_POWER_ALL_OFF, 0, false,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, power_state_);
  EXPECT_EQ(
      JoinActions(
          GetCrtcAction(*displays_[0], nullptr, gfx::Point()).c_str(), nullptr),
      log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, NoopSoftwareMirrorConfiguration) {
  layout_manager_.set_should_mirror(false);
  layout_manager_.set_software_mirroring_controller(
      std::make_unique<TestSoftwareMirroringController>());
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
        chromeos::DISPLAY_POWER_ALL_ON, 0, false,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  log_.GetActionsAndClear();

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_MIRROR,
        chromeos::DISPLAY_POWER_ALL_ON, 0, false,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED, display_state_);
  EXPECT_TRUE(layout_manager_.GetSoftwareMirroringController()
                  ->SoftwareMirroringEnabled());
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest,
       ForceConfigurationWhileGoingToSoftwareMirror) {
  layout_manager_.set_should_mirror(false);
  layout_manager_.set_software_mirroring_controller(
      std::make_unique<TestSoftwareMirroringController>());
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
        chromeos::DISPLAY_POWER_ALL_ON, 0, false,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  log_.GetActionsAndClear();

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_MIRROR,
        chromeos::DISPLAY_POWER_ALL_ON, 0, true /* force_configure */,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED, display_state_);
  EXPECT_TRUE(layout_manager_.GetSoftwareMirroringController()
                  ->SoftwareMirroringEnabled());
  EXPECT_EQ(
      JoinActions(
          GetCrtcAction(*displays_[0], &small_mode_, gfx::Point()).c_str(),
          GetCrtcAction(*displays_[1], &big_mode_,
                        gfx::Point(0, small_mode_.size().height()))
              .c_str(),
          nullptr),
      log_.GetActionsAndClear());
}

}  // namespace test
}  // namespace display
