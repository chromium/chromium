// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/display/manager/update_display_configuration_task.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#include "ui/display/types/display_constants.h"

namespace display::test {

namespace {

// Non-zero generic connector IDs.
constexpr uint64_t kEdpConnectorId = 71u;
constexpr uint64_t kSecondConnectorId = kEdpConnectorId + 10u;

class TestSoftwareMirroringController
    : public DisplayConfigurator::SoftwareMirroringController {
 public:
  TestSoftwareMirroringController() : is_enabled_(false) {}

  TestSoftwareMirroringController(const TestSoftwareMirroringController&) =
      delete;
  TestSoftwareMirroringController& operator=(
      const TestSoftwareMirroringController&) = delete;

  ~TestSoftwareMirroringController() override = default;

  // DisplayConfigurator::SoftwareMirroringController:
  void SetSoftwareMirroring(bool enabled) override { is_enabled_ = enabled; }
  bool SoftwareMirroringEnabled() const override { return is_enabled_; }
  bool IsSoftwareMirroringEnforced() const override { return false; }

 private:
  bool is_enabled_;
};

class TestDisplayLayoutManager : public DisplayLayoutManager {
 public:
  TestDisplayLayoutManager()
      : should_mirror_(true),
        display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
        power_state_(chromeos::DISPLAY_POWER_ALL_ON) {}

  TestDisplayLayoutManager(const TestDisplayLayoutManager&) = delete;
  TestDisplayLayoutManager& operator=(const TestDisplayLayoutManager&) = delete;

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
      const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays,
      MultipleDisplayState new_display_state,
      chromeos::DisplayPowerState new_power_state,
      const base::flat_set<int64_t>& new_vrr_state,
      std::vector<DisplayConfigureRequest>* requests) const override {
    gfx::Point origin;
    for (DisplaySnapshot* display : displays) {
      const DisplayMode* mode = display->native_mode();
      if (new_display_state == MULTIPLE_DISPLAY_STATE_MULTI_MIRROR)
        mode = should_mirror_ ? FindMirrorMode(displays) : nullptr;

      if (!mode)
        return false;

      const DisplayMode* request_mode =
          new_power_state == chromeos::DISPLAY_POWER_ALL_ON ? mode : nullptr;
      bool request_vrr_state = new_vrr_state.contains(display->display_id()) &&
                               display->IsVrrCapable();
      requests->push_back(DisplayConfigureRequest(display, request_mode, origin,
                                                  request_vrr_state));

      if (new_display_state != MULTIPLE_DISPLAY_STATE_MULTI_MIRROR)
        origin.Offset(0, mode->size().height());
    }

    return true;
  }

  DisplayConfigurator::DisplayStateList GetDisplayStates() const override {
    NOTREACHED_IN_MIGRATION();
    return DisplayConfigurator::DisplayStateList();
  }

  bool IsMirroring() const override {
    return display_state_ == MULTIPLE_DISPLAY_STATE_MULTI_MIRROR;
  }

 private:
  const DisplayMode* FindMirrorMode(
      const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays)
      const {
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
};

class UpdateDisplayConfigurationTaskTest : public testing::Test {
 public:
  UpdateDisplayConfigurationTaskTest()
      : delegate_(&log_),
        small_mode_({1366, 768}, false, 60.0f),
        big_mode_({2560, 1600}, false, 60.0f, 40.0f) {
    displays_[0] =
        FakeDisplaySnapshot::Builder()
            .SetId(123)
            .SetNativeMode(small_mode_.Clone())
            .SetCurrentMode(small_mode_.Clone())
            .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
            .SetBaseConnectorId(kEdpConnectorId)
            .SetVariableRefreshRateState(VariableRefreshRateState::kVrrDisabled)
            .Build();

    displays_[1] = FakeDisplaySnapshot::Builder()
                       .SetId(456)
                       .SetNativeMode(big_mode_.Clone())
                       .SetCurrentMode(big_mode_.Clone())
                       .SetType(DISPLAY_CONNECTION_TYPE_DISPLAYPORT)
                       .AddMode(small_mode_.Clone())
                       .SetBaseConnectorId(kSecondConnectorId)
                       .SetVariableRefreshRateState(
                           VariableRefreshRateState::kVrrNotCapable)
                       .Build();
  }

  UpdateDisplayConfigurationTaskTest(
      const UpdateDisplayConfigurationTaskTest&) = delete;
  UpdateDisplayConfigurationTaskTest& operator=(
      const UpdateDisplayConfigurationTaskTest&) = delete;

  ~UpdateDisplayConfigurationTaskTest() override = default;

  void UpdateDisplays(size_t count) {
    std::vector<std::unique_ptr<DisplaySnapshot>> displays;
    for (size_t i = 0; i < count; ++i)
      displays.push_back(displays_[i]->Clone());

    delegate_.SetOutputs(std::move(displays));
  }

  void ResponseCallback(
      bool success,
      const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>& displays,
      const std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>>&
          unassociated_displays,
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

  bool configured_ = false;
  bool configuration_status_ = false;
  std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>> display_states_;
  MultipleDisplayState display_state_ = MULTIPLE_DISPLAY_STATE_INVALID;
  chromeos::DisplayPowerState power_state_ = chromeos::DISPLAY_POWER_ALL_ON;
};

}  // namespace

TEST_F(UpdateDisplayConfigurationTaskTest, HeadlessConfiguration) {
  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_HEADLESS,
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
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
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, ExtendedConfiguration) {
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(
      JoinActions(kTestModesetStr,
                  GetCrtcAction(
                      {displays_[0]->display_id(), gfx::Point(), &small_mode_})
                      .c_str(),
                  GetCrtcAction({displays_[1]->display_id(),
                                 gfx::Point(0, small_mode_.size().height()),
                                 &big_mode_})
                      .c_str(),
                  kModesetOutcomeSuccess, kCommitModesetStr,
                  GetCrtcAction(
                      {displays_[0]->display_id(), gfx::Point(), &small_mode_})
                      .c_str(),
                  GetCrtcAction({displays_[1]->display_id(),
                                 gfx::Point(0, small_mode_.size().height()),
                                 &big_mode_})
                      .c_str(),
                  kModesetOutcomeSuccess, nullptr),
      log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, MirrorConfiguration) {
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_MIRROR,
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        GetCrtcAction({displays_[1]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, FailMirrorConfiguration) {
  layout_manager_.set_should_mirror(false);
  UpdateDisplays(2);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_MIRROR,
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
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
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_FALSE(configuration_status_);
  EXPECT_EQ(
      JoinActions(
          // All displays will fail to modeset together. Initiate retry logic.
          kTestModesetStr,
          GetCrtcAction(
              {displays_[0]->display_id(), gfx::Point(), &small_mode_})
              .c_str(),
          GetCrtcAction({displays_[1]->display_id(),
                         gfx::Point(0, small_mode_.size().height()),
                         &big_mode_})
              .c_str(),
          kModesetOutcomeFailure,
          // We first attempt to modeset the internal display with all
          // other displays disabled, which will fail.
          kTestModesetStr,
          GetCrtcAction(
              {displays_[0]->display_id(), gfx::Point(), &small_mode_})
              .c_str(),
          GetCrtcAction({displays_[1]->display_id(),
                         gfx::Point(0, small_mode_.size().height()), nullptr})
              .c_str(),
          kModesetOutcomeFailure,
          // Since internal displays are restricted to their preferred mode,
          // there are no other modes to try. Disable the internal display when
          // we attempt to modeset displays that are connected to other
          // connectors. Regardless of what happens next, the configuration will
          // still fail completely. External display fail modeset, downgrade
          // once, and then fail completely.
          kTestModesetStr,
          GetCrtcAction({displays_[0]->display_id(), gfx::Point(), nullptr})
              .c_str(),
          GetCrtcAction({displays_[1]->display_id(),
                         gfx::Point(0, small_mode_.size().height()),
                         &big_mode_})
              .c_str(),
          kModesetOutcomeFailure, kTestModesetStr,
          GetCrtcAction({displays_[0]->display_id(), gfx::Point(), nullptr})
              .c_str(),
          GetCrtcAction({displays_[1]->display_id(),
                         gfx::Point(0, small_mode_.size().height()),
                         &small_mode_})
              .c_str(),
          kModesetOutcomeFailure, nullptr),
      log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, SingleChangePowerConfiguration) {
  UpdateDisplays(1);

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_SINGLE,
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                       &small_mode_})
                            .c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_.GetActionsAndClear());

  // Turn power off
  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_SINGLE,
        chromeos::DISPLAY_POWER_ALL_OFF,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, power_state_);
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcAction({displays_[0]->display_id(), gfx::Point(), nullptr})
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcAction({displays_[0]->display_id(), gfx::Point(), nullptr})
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
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
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  log_.GetActionsAndClear();

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_MIRROR,
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
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
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  log_.GetActionsAndClear();

  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_MIRROR,
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/true, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED, display_state_);
  EXPECT_TRUE(layout_manager_.GetSoftwareMirroringController()
                  ->SoftwareMirroringEnabled());
  EXPECT_EQ(
      JoinActions(kTestModesetStr,
                  GetCrtcAction(
                      {displays_[0]->display_id(), gfx::Point(), &small_mode_})
                      .c_str(),
                  GetCrtcAction({displays_[1]->display_id(),
                                 gfx::Point(0, small_mode_.size().height()),
                                 &big_mode_})
                      .c_str(),
                  kModesetOutcomeSuccess, kCommitModesetStr,
                  GetCrtcAction(
                      {displays_[0]->display_id(), gfx::Point(), &small_mode_})
                      .c_str(),
                  GetCrtcAction({displays_[1]->display_id(),
                                 gfx::Point(0, small_mode_.size().height()),
                                 &big_mode_})
                      .c_str(),
                  kModesetOutcomeSuccess, nullptr),
      log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, VrrConfiguration) {
  UpdateDisplays(2);

  // Initial configuration.
  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  configured_ = false;
  configuration_status_ = false;
  log_.GetActionsAndClear();

  // VRR configuration.
  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerOnlyIfSingleInternalDisplay,
        /*new_vrr_state=*/
        {displays_[0]->display_id(), displays_[1]->display_id()},
        /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(
      JoinActions(kTestModesetStr,
                  GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                 &small_mode_, /*enable_vrr=*/true})
                      .c_str(),
                  GetCrtcAction({displays_[1]->display_id(),
                                 gfx::Point(0, small_mode_.size().height()),
                                 &big_mode_, /*enable_vrr=*/false})
                      .c_str(),
                  kModesetOutcomeSuccess, kCommitModesetStr,
                  GetCrtcAction({displays_[0]->display_id(), gfx::Point(),
                                 &small_mode_, /*enable_vrr=*/true})
                      .c_str(),
                  GetCrtcAction({displays_[1]->display_id(),
                                 gfx::Point(0, small_mode_.size().height()),
                                 &big_mode_, /*enable_vrr=*/false})
                      .c_str(),
                  kModesetOutcomeSuccess, nullptr),
      log_.GetActionsAndClear());
}

TEST_F(UpdateDisplayConfigurationTaskTest, NoopConfiguration) {
  UpdateDisplays(2);

  // Initial configuration.
  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerNoFlags,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  configured_ = false;
  configuration_status_ = false;
  log_.GetActionsAndClear();

  // Noop configuration.
  {
    UpdateDisplayConfigurationTask task(
        &delegate_, &layout_manager_, MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
        chromeos::DISPLAY_POWER_ALL_ON,
        DisplayConfigurator::kSetDisplayPowerOnlyIfSingleInternalDisplay,
        /*new_vrr_state=*/{}, /*refresh_rate_overrides=*/{},
        /*force_configure=*/false, kConfigurationTypeFull,
        base::BindOnce(&UpdateDisplayConfigurationTaskTest::ResponseCallback,
                       base::Unretained(this)));
    task.Run();
  }

  EXPECT_TRUE(configured_);
  EXPECT_TRUE(configuration_status_);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED, display_state_);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, power_state_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
}

}  // namespace display::test
