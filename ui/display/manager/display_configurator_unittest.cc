// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/display/manager/display_configurator.h"

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_features.h"
#include "ui/display/manager/configure_displays_task.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/public/ozone_switches.h"

namespace display::test {

namespace {

constexpr int64_t kDisplayIds[3] = {123, 456, 789};

// Non-zero generic connector IDs.
constexpr uint64_t kEdpConnectorId = 71u;
constexpr uint64_t kSecondConnectorId = kEdpConnectorId + 10u;
constexpr uint64_t kThirdConnectorId = kEdpConnectorId + 20u;

std::unique_ptr<DisplayMode> MakeDisplayMode(int width,
                                             int height,
                                             bool is_interlaced,
                                             float refresh_rate) {
  return std::make_unique<DisplayMode>(gfx::Size{width, height}, is_interlaced,
                                       refresh_rate);
}

enum CallbackResult {
  CALLBACK_FAILURE,
  CALLBACK_SUCCESS,
  CALLBACK_NOT_CALLED,
};

// Expected immediate configurations should be done without any delays.
constexpr base::TimeDelta kNoDelay = base::Milliseconds(0);

// The expected configuration delay when resuming from suspend while in 2+
// display mode.
constexpr base::TimeDelta kLongDelay = base::Milliseconds(
    DisplayConfigurator::kResumeConfigureMultiDisplayDelayMs);

class TestObserver : public DisplayConfigurator::Observer {
 public:
  explicit TestObserver(DisplayConfigurator* configurator)
      : configurator_(configurator) {
    Reset();
    configurator_->AddObserver(this);
  }

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override { configurator_->RemoveObserver(this); }

  int num_changes() const { return num_changes_; }
  int num_failures() const { return num_failures_; }
  int num_power_state_changes() const { return num_power_state_changes_; }
  const DisplayConfigurator::DisplayStateList& latest_outputs() const {
    return latest_outputs_;
  }
  MultipleDisplayState latest_failed_state() const {
    return latest_failed_state_;
  }
  chromeos::DisplayPowerState latest_power_state() const {
    return latest_power_state_;
  }

  void Reset() {
    num_changes_ = 0;
    num_failures_ = 0;
    num_power_state_changes_ = 0;
    latest_outputs_.clear();
    latest_failed_state_ = MULTIPLE_DISPLAY_STATE_INVALID;
    latest_power_state_ = chromeos::DISPLAY_POWER_ALL_OFF;
  }

  // DisplayConfigurator::Observer overrides:
  void OnDisplayConfigurationChanged(
      const DisplayConfigurator::DisplayStateList& outputs) override {
    num_changes_++;
    latest_outputs_ = outputs;
  }

  void OnDisplayConfigurationChangeFailed(
      const DisplayConfigurator::DisplayStateList& outputs,
      MultipleDisplayState failed_new_state) override {
    num_failures_++;
    latest_failed_state_ = failed_new_state;
  }

  void OnPowerStateChanged(chromeos::DisplayPowerState power_state) override {
    num_power_state_changes_++;
    latest_power_state_ = power_state;
  }

 private:
  raw_ptr<DisplayConfigurator> configurator_;  // Not owned.

  // Number of times that OnDisplayMode*() has been called.
  int num_changes_;
  int num_failures_;
  // Number of times that OnPowerStateChanged() has been called.
  int num_power_state_changes_;

  // Parameters most recently passed to OnDisplayMode*().
  DisplayConfigurator::DisplayStateList latest_outputs_;
  MultipleDisplayState latest_failed_state_;
  // Value most recently passed to OnPowerStateChanged().
  chromeos::DisplayPowerState latest_power_state_;
};

class TestStateController : public DisplayConfigurator::StateController {
 public:
  TestStateController() : state_(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED) {}

  TestStateController(const TestStateController&) = delete;
  TestStateController& operator=(const TestStateController&) = delete;

  ~TestStateController() override = default;

  void set_state(MultipleDisplayState state) { state_ = state; }

  // DisplayConfigurator::StateController overrides:
  MultipleDisplayState GetStateForDisplayIds(
      const DisplayConfigurator::DisplayStateList& outputs) override {
    return state_;
  }
  bool GetSelectedModeForDisplayId(
      int64_t display_id,
      ManagedDisplayMode* out_mode) const override {
    return false;
  }

 private:
  MultipleDisplayState state_;
};

class TestMirroringController
    : public DisplayConfigurator::SoftwareMirroringController {
 public:
  TestMirroringController() : software_mirroring_enabled_(false) {}

  TestMirroringController(const TestMirroringController&) = delete;
  TestMirroringController& operator=(const TestMirroringController&) = delete;

  ~TestMirroringController() override = default;

  void SetSoftwareMirroring(bool enabled) override {
    software_mirroring_enabled_ = enabled;
  }

  bool SoftwareMirroringEnabled() const override {
    return software_mirroring_enabled_;
  }

  bool IsSoftwareMirroringEnforced() const override { return false; }

 private:
  bool software_mirroring_enabled_;
};

// Abstracts waiting for the display configuration to be completed and getting
// the time it took to complete.
class ConfigurationWaiter {
 public:
  explicit ConfigurationWaiter(DisplayConfigurator::TestApi* test_api)
      : test_api_(test_api), callback_result_(CALLBACK_NOT_CALLED) {}

  ConfigurationWaiter(const ConfigurationWaiter&) = delete;
  ConfigurationWaiter& operator=(const ConfigurationWaiter&) = delete;

  ~ConfigurationWaiter() = default;

  DisplayConfigurator::ConfigurationCallback on_configuration_callback() {
    return base::BindOnce(&ConfigurationWaiter::OnConfigured,
                          base::Unretained(this));
  }

  CallbackResult callback_result() const { return callback_result_; }

  void Reset() { callback_result_ = CALLBACK_NOT_CALLED; }

  // Simulates waiting for the next configuration. If an async task is pending,
  // runs it and returns base::TimeDelta(). Otherwise, triggers the
  // configuration timer and returns its delay. If the timer wasn't running,
  // returns base::TimeDelta::Max().
  [[nodiscard]] base::TimeDelta Wait() {
    base::RunLoop().RunUntilIdle();
    if (callback_result_ != CALLBACK_NOT_CALLED)
      return base::TimeDelta();

    const base::TimeDelta delay = test_api_->GetConfigureDelay();
    if (!test_api_->TriggerConfigureTimeout())
      return base::TimeDelta::Max();

    return delay;
  }

 private:
  void OnConfigured(bool status) {
    CHECK_EQ(callback_result_, CALLBACK_NOT_CALLED);
    callback_result_ = status ? CALLBACK_SUCCESS : CALLBACK_FAILURE;
  }

  raw_ptr<DisplayConfigurator::TestApi> test_api_;  // Not owned.

  // The status of the display configuration.
  CallbackResult callback_result_;
};

class DisplayConfiguratorTest : public testing::Test {
 public:
  DisplayConfiguratorTest() = default;

  DisplayConfiguratorTest(const DisplayConfiguratorTest&) = delete;
  DisplayConfiguratorTest& operator=(const DisplayConfiguratorTest&) = delete;

  ~DisplayConfiguratorTest() override = default;

  void SetUp() override {
    log_ = std::make_unique<ActionLogger>();

    // TODO(crbug.com/1161556): |kEnableHardwareMirrorMode| is disabled by
    // default. We enable it here to maintain test coverage for hardware mirror
    // mode until it is permanently removed.
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableHardwareMirrorMode);

    native_display_delegate_ = new TestNativeDisplayDelegate(log_.get());
    // Force configuring displays to simulate on-device configurator behavior.
    configurator_.SetConfigureDisplays(true);
    configurator_.SetDelegateForTesting(
        std::unique_ptr<NativeDisplayDelegate>(native_display_delegate_));

    configurator_.set_state_controller(&state_controller_);
    configurator_.set_mirroring_controller(&mirroring_controller_);

    outputs_[0] =
        FakeDisplaySnapshot::Builder()
            .SetId(kDisplayIds[0])
            .SetNativeMode(small_mode_.Clone())
            .SetCurrentMode(small_mode_.Clone())
            .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
            .SetBaseConnectorId(kEdpConnectorId)
            .SetIsAspectPreservingScaling(true)
            .SetVariableRefreshRateState(VariableRefreshRateState::kVrrDisabled)
            .Build();

    outputs_[1] = FakeDisplaySnapshot::Builder()
                      .SetId(kDisplayIds[1])
                      .SetNativeMode(big_mode_.Clone())
                      .SetCurrentMode(big_mode_.Clone())
                      .AddMode(small_mode_.Clone())
                      .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                      .SetBaseConnectorId(kSecondConnectorId)
                      .SetIsAspectPreservingScaling(true)
                      .SetVariableRefreshRateState(
                          VariableRefreshRateState::kVrrNotCapable)
                      .Build();

    outputs_[2] = FakeDisplaySnapshot::Builder()
                      .SetId(kDisplayIds[2])
                      .SetNativeMode(small_mode_.Clone())
                      .SetCurrentMode(small_mode_.Clone())
                      .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                      .SetBaseConnectorId(kThirdConnectorId)
                      .SetIsAspectPreservingScaling(true)
                      .SetVariableRefreshRateState(
                          VariableRefreshRateState::kVrrNotCapable)
                      .Build();

    UpdateOutputs(2, false);
  }

  void OnDisplayControlUpdated(bool success) {
    display_control_result_ = success ? CALLBACK_SUCCESS : CALLBACK_FAILURE;
  }

  // Predefined modes that can be used by outputs.
  const DisplayMode small_mode_ = DisplayMode({1366, 768}, false, 60.0f);
  const DisplayMode big_mode_ = DisplayMode({2560, 1600}, false, 60.0f);

 protected:
  // Returns the output at the specified |index| as it currently exists within
  // |native_display_delegate_|.
  const DisplaySnapshot* GetOutput(size_t index) const {
    return native_display_delegate_->GetOutputs()[index];
  }

  // Sets the test-owned output at the specified |index| without sending updates
  // to |native_display_delegate_|. Must be followed by UpdateOutputs to effect
  // changes.
  void SetOutput(size_t index, std::unique_ptr<DisplaySnapshot> output) {
    outputs_[index] = std::move(output);
  }

  // Configures |native_display_delegate_| to return the first |num_outputs|
  // entries from |outputs_|. If |send_events| is true, also sends screen-change
  // and output-change events to |configurator_| and triggers the configure
  // timeout if one was scheduled.
  void UpdateOutputs(size_t num_outputs, bool send_events) {
    ASSERT_LE(num_outputs, std::size(outputs_));
    std::vector<std::unique_ptr<DisplaySnapshot>> outputs;
    for (size_t i = 0; i < num_outputs; ++i) {
      outputs.push_back(outputs_[i]->Clone());
    }
    native_display_delegate_->SetOutputs(std::move(outputs));

    if (send_events) {
      configurator_.OnConfigurationChanged();
      EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
    }
  }

  void Init(bool panel_fitting_enabled) {
    configurator_.Init(nullptr, panel_fitting_enabled);
  }

  enum class DisplayConfig { kOff, kMirror, kStack };

  // |modes| are expected display modes for |outputs_| at respective positions.
  template <typename... Modes>
  void InitWithOutputs(Modes... modes) {
    UpdateOutputs(sizeof...(modes), false);
    EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
    Init(false);

    EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
    configurator_.ForceInitialConfigure();
    std::string actions = GetCrtcActions(DisplayConfig::kStack, modes...);
    EXPECT_EQ(actions.empty()
                  ? kInit
                  : JoinActions(
                        kInit, kTestModesetStr,
                        GetCrtcActions(DisplayConfig::kStack, modes...).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(DisplayConfig::kStack, modes...).c_str(),
                        kModesetOutcomeSuccess, nullptr),
              log_->GetActionsAndClear());
  }

  template <typename... Modes>
  std::string GetCrtcActions(Modes... modes) {
    return GetCrtcActions(DisplayConfig::kStack, modes...);
  }

  template <typename... Modes>
  std::string GetCrtcActions(DisplayConfig config, Modes... modes) {
    return JoinCrtcActions<0>(config, gfx::Point(), modes...);
  }

  CallbackResult PopDisplayControlResult() {
    CallbackResult result = display_control_result_;
    display_control_result_ = CALLBACK_NOT_CALLED;
    return result;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestStateController state_controller_;
  TestMirroringController mirroring_controller_;
  DisplayConfigurator configurator_;
  TestObserver observer_{&configurator_};
  std::unique_ptr<ActionLogger> log_;
  raw_ptr<TestNativeDisplayDelegate> native_display_delegate_;  // not owned
  DisplayConfigurator::TestApi test_api_{&configurator_};
  ConfigurationWaiter config_waiter_{&test_api_};
  base::test::ScopedFeatureList scoped_feature_list_;

  CallbackResult display_control_result_ = CALLBACK_NOT_CALLED;

 private:
  template <size_t I>
  std::string JoinCrtcActions(DisplayConfig, const gfx::Point&) {
    return {};
  }

  template <size_t I, typename... Modes>
  std::string JoinCrtcActions(DisplayConfig config,
                              gfx::Point origin,
                              const DisplayMode* mode,
                              Modes... modes) {
    static_assert(I < kNumOutputs, "More expected modes than outputs");

    std::string action =
        GetCrtcAction({outputs_[I]->display_id(), origin,
                       config == DisplayConfig::kOff ? nullptr : mode});

    if (mode && config != DisplayConfig::kMirror)
      origin += {0, mode->size().height() + DisplayConfigurator::kVerticalGap};

    std::string rest = JoinCrtcActions<I + 1>(config, origin, modes...);
    return rest.empty() ? action
                        : JoinActions(action.c_str(), rest.c_str(), nullptr);
  }

  static constexpr size_t kNumOutputs = 3;
  // These snapshots are owned by the test. They are cloned whenever updates are
  // sent to |native_display_delegate_|.
  std::unique_ptr<DisplaySnapshot> outputs_[kNumOutputs];
};

}  // namespace

TEST_F(DisplayConfiguratorTest, FindDisplayModeMatchingSize) {
  std::unique_ptr<DisplaySnapshot> output =
      FakeDisplaySnapshot::Builder()
          .SetId(kDisplayIds[0])
          .AddMode(MakeDisplayMode(1920, 1200, false, 60.0))
          .SetNativeMode(MakeDisplayMode(1920, 1200, false, 50.0))
          // Different rates.
          .AddMode(MakeDisplayMode(1920, 1080, false, 30.0))
          .AddMode(MakeDisplayMode(1920, 1080, false, 50.0))
          .AddMode(MakeDisplayMode(1920, 1080, false, 40.0))
          .AddMode(MakeDisplayMode(1920, 1080, false, 0.0))
          // Interlaced vs non-interlaced.
          .AddMode(MakeDisplayMode(1280, 720, true, 60.0))
          .AddMode(MakeDisplayMode(1280, 720, false, 40.0))
          // Interlaced only.
          .AddMode(MakeDisplayMode(1024, 768, true, 0.0))
          .AddMode(MakeDisplayMode(1024, 768, true, 40.0))
          .AddMode(MakeDisplayMode(1024, 768, true, 60.0))
          // Mixed.
          .AddMode(MakeDisplayMode(1024, 600, true, 60.0))
          .AddMode(MakeDisplayMode(1024, 600, false, 40.0))
          .AddMode(MakeDisplayMode(1024, 600, false, 50.0))
          // Just one interlaced mode.
          .AddMode(MakeDisplayMode(640, 480, true, 60.0))
          // Refresh rate not available.
          .AddMode(MakeDisplayMode(320, 200, false, 0.0))
          .Build();

  const std::vector<std::unique_ptr<const DisplayMode>>& modes =
      output->modes();

  // Should pick native over highest refresh rate.
  EXPECT_EQ(modes[1].get(), DisplayConfigurator::FindDisplayModeMatchingSize(
                                *output, gfx::Size(1920, 1200)));

  // Should pick highest refresh rate.
  EXPECT_EQ(modes[3].get(), DisplayConfigurator::FindDisplayModeMatchingSize(
                                *output, gfx::Size(1920, 1080)));

  // Should pick non-interlaced mode.
  EXPECT_EQ(modes[7].get(), DisplayConfigurator::FindDisplayModeMatchingSize(
                                *output, gfx::Size(1280, 720)));

  // Interlaced only. Should pick one with the highest refresh rate in
  // interlaced mode.
  EXPECT_EQ(modes[10].get(), DisplayConfigurator::FindDisplayModeMatchingSize(
                                 *output, gfx::Size(1024, 768)));

  // Mixed: Should pick one with the highest refresh rate in
  // interlaced mode.
  EXPECT_EQ(modes[13].get(), DisplayConfigurator::FindDisplayModeMatchingSize(
                                 *output, gfx::Size(1024, 600)));

  // Just one interlaced mode.
  EXPECT_EQ(modes[14].get(), DisplayConfigurator::FindDisplayModeMatchingSize(
                                 *output, gfx::Size(640, 480)));

  // Refresh rate not available.
  EXPECT_EQ(modes[15].get(), DisplayConfigurator::FindDisplayModeMatchingSize(
                                 *output, gfx::Size(320, 200)));

  // No mode found.
  EXPECT_EQ(nullptr, DisplayConfigurator::FindDisplayModeMatchingSize(
                         *output, gfx::Size(1440, 900)));
}

TEST_F(DisplayConfiguratorTest, ConnectSecondOutput) {
  InitWithOutputs(&small_mode_);

  // Connect a second output and check that the configurator enters
  // extended mode.
  observer_.Reset();
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  UpdateOutputs(2, true);

  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  observer_.Reset();
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Disconnect the second output.
  observer_.Reset();
  UpdateOutputs(1, true);
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Get rid of shared modes to force software mirroring.
  SetOutput(1, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[1])
                   .SetNativeMode(big_mode_.Clone())
                   .SetCurrentMode(big_mode_.Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                   .SetIsAspectPreservingScaling(true)
                   .Build());

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  UpdateOutputs(2, true);
  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());

  observer_.Reset();
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());

  EXPECT_EQ(1, observer_.num_changes());

  // Setting MULTIPLE_DISPLAY_STATE_DUAL_MIRROR should try to reconfigure.
  observer_.Reset();
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Set back to software mirror mode.
  observer_.Reset();
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Disconnect the second output.
  observer_.Reset();
  UpdateOutputs(1, true);
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());
}

TEST_F(DisplayConfiguratorTest, SetDisplayPower) {
  InitWithOutputs(&small_mode_);

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  observer_.Reset();
  UpdateOutputs(2, true);
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turning off the internal display should switch the external display to
  // its native mode.
  observer_.Reset();
  config_waiter_.Reset();
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(
      JoinActions(kTestModesetStr, GetCrtcActions(nullptr, &big_mode_).c_str(),
                  kModesetOutcomeSuccess, kCommitModesetStr,
                  GetCrtcActions(nullptr, &big_mode_).c_str(),
                  kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, configurator_.display_state());
  EXPECT_EQ(1, observer_.num_changes());

  // When all displays are turned off, the framebuffer should switch back
  // to the mirrored size.
  observer_.Reset();
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(
      JoinActions(kTestModesetStr,
                  GetCrtcActions(DisplayConfig::kOff, nullptr, nullptr).c_str(),
                  kModesetOutcomeSuccess, kCommitModesetStr,
                  GetCrtcActions(DisplayConfig::kOff, nullptr, nullptr).c_str(),
                  kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR, configurator_.display_state());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turn all displays on and check that mirroring is still used.
  observer_.Reset();
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR, configurator_.display_state());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Get rid of shared modes to force software mirroring.
  SetOutput(1, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[1])
                   .SetNativeMode(big_mode_.Clone())
                   .SetCurrentMode(big_mode_.Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                   .SetIsAspectPreservingScaling(true)
                   .Build());

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  observer_.Reset();
  UpdateOutputs(2, true);

  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turning off the internal display should switch the external display to
  // its native mode.
  observer_.Reset();
  config_waiter_.Reset();
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(
      JoinActions(kTestModesetStr, GetCrtcActions(nullptr, &big_mode_).c_str(),
                  kModesetOutcomeSuccess, kCommitModesetStr,
                  GetCrtcActions(nullptr, &big_mode_).c_str(),
                  kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, configurator_.display_state());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // When all displays are turned off, the framebuffer should switch back
  // to the extended + software mirroring.
  observer_.Reset();
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kOff, &small_mode_, &big_mode_).c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kOff, &small_mode_, &big_mode_).c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turn all displays on and check that mirroring is still used.
  observer_.Reset();
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());
}

TEST_F(DisplayConfiguratorTest, SuspendAndResume) {
  InitWithOutputs(&small_mode_);

  // Set the initial power state to on.
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());

  // No preparation is needed before suspending when the display is already
  // on.  The configurator should still reprobe on resume in case a display
  // was connected while suspended.
  config_waiter_.Reset();
  configurator_.SuspendDisplays(config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(nullptr).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(nullptr).c_str(), kModesetOutcomeSuccess,
                        nullptr),
            log_->GetActionsAndClear());

  // No resume delay in single display mode.
  config_waiter_.Reset();
  configurator_.ResumeDisplays();
  // The timer should not be running.
  EXPECT_EQ(base::TimeDelta::Max(), config_waiter_.Wait());
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());

  // Now turn the display off before suspending and check that the
  // configurator turns it back on and syncs with the server.
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(nullptr).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(nullptr).c_str(), kModesetOutcomeSuccess,
                        nullptr),
            log_->GetActionsAndClear());

  config_waiter_.Reset();
  configurator_.SuspendDisplays(config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  config_waiter_.Reset();
  configurator_.ResumeDisplays();
  // The timer should not be running.
  EXPECT_EQ(base::TimeDelta::Max(), config_waiter_.Wait());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  UpdateOutputs(2, true);
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());

  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR, configurator_.display_state());
  EXPECT_EQ(
      JoinActions(kTestModesetStr,
                  GetCrtcActions(DisplayConfig::kOff, nullptr, nullptr).c_str(),
                  kModesetOutcomeSuccess, kCommitModesetStr,
                  GetCrtcActions(DisplayConfig::kOff, nullptr, nullptr).c_str(),
                  kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());

  // No delay in suspend.
  config_waiter_.Reset();
  configurator_.SuspendDisplays(config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF,
            configurator_.current_power_state());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR, configurator_.display_state());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // If a display is disconnected while suspended, the configurator should
  // pick up the change and only turn on the internal display. The should be
  // a longer configuration delay when we set the displays back to on.
  UpdateOutputs(1, false);
  config_waiter_.Reset();
  configurator_.ResumeDisplays();
  // Since we were in dual display mirror mode before suspend, the timer should
  // be running with kMinLongDelayMs.
  EXPECT_EQ(kLongDelay, test_api_.GetConfigureDelay());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(CALLBACK_NOT_CALLED, config_waiter_.callback_result());
  EXPECT_EQ(kLongDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, Headless) {
  InitWithOutputs();

  // Not much should happen when the display power state is changed while
  // no displays are connected.
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // Connect an external display and check that it's configured correctly.
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(big_mode_.Clone())
                   .SetCurrentMode(big_mode_.Clone())
                   .AddMode(small_mode_.Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetIsAspectPreservingScaling(true)
                   .Build());

  UpdateOutputs(1, true);
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(&big_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&big_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());

  UpdateOutputs(0, true);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, StartWithTwoOutputs) {
  UpdateOutputs(2, false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  Init(false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  configurator_.ForceInitialConfigure();
  EXPECT_EQ(
      JoinActions(
          kInit, kTestModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, InvalidMultipleDisplayStates) {
  UpdateOutputs(0, false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  Init(false);
  configurator_.ForceInitialConfigure();
  observer_.Reset();
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_HEADLESS);
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(0, observer_.num_failures());
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_SINGLE);
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(3, observer_.num_failures());

  UpdateOutputs(1, true);
  observer_.Reset();
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_HEADLESS);
  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_EQ(1, observer_.num_failures());
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_SINGLE);
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(1, observer_.num_failures());
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(3, observer_.num_failures());

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  UpdateOutputs(2, true);
  observer_.Reset();
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_HEADLESS);
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_SINGLE);
  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_EQ(2, observer_.num_failures());
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  EXPECT_EQ(2, observer_.num_changes());
  EXPECT_EQ(2, observer_.num_failures());
}

TEST_F(DisplayConfiguratorTest, GetMultipleDisplayStateForHWMirroredDisplays) {
  UpdateOutputs(2, false);
  Init(false);
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  configurator_.ForceInitialConfigure();
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR, configurator_.display_state());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
}

TEST_F(DisplayConfiguratorTest, GetMultipleDisplayStateForSWMirroredDisplays) {
  // Disable hardware mirroring.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      features::kEnableHardwareMirrorMode);
  UpdateOutputs(2, false);
  Init(false);
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  configurator_.ForceInitialConfigure();
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
  EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
}

TEST_F(DisplayConfiguratorTest, UpdateCachedOutputsEvenAfterFailure) {
  InitWithOutputs(&small_mode_);

  const DisplayConfigurator::DisplayStateList& cached =
      configurator_.cached_displays();
  ASSERT_EQ(static_cast<size_t>(1), cached.size());
  EXPECT_EQ(GetOutput(0)->current_mode(), cached[0]->current_mode());

  // After connecting a second output, check that it shows up in
  // |cached_displays_| even if an invalid state is requested.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(2, true);
  ASSERT_EQ(static_cast<size_t>(2), cached.size());
  EXPECT_EQ(GetOutput(0)->current_mode(), cached[0]->current_mode());
  EXPECT_EQ(GetOutput(1)->current_mode(), cached[1]->current_mode());
}

TEST_F(DisplayConfiguratorTest, VerifyInternalPanelIsAtTheTopOfTheList) {
  InitWithOutputs(&small_mode_);

  // Initialize with 3 displays where the internal panel is not at the top of
  // the display list.
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(1L)
                   .SetType(DISPLAY_CONNECTION_TYPE_DISPLAYPORT)
                   .SetNativeMode(big_mode_.Clone())
                   .Build());
  SetOutput(1, FakeDisplaySnapshot::Builder()
                   .SetId(2L)
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetNativeMode(small_mode_.Clone())
                   .Build());
  SetOutput(2, FakeDisplaySnapshot::Builder()
                   .SetId(3L)
                   .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                   .SetNativeMode(big_mode_.Clone())
                   .Build());

  native_display_delegate_->set_max_configurable_pixels(
      big_mode_.size().GetArea());
  UpdateOutputs(3, true);

  // We expect the internal display to be at the top of DisplayConfigurator's
  // |cached_displays_| list post configuration. The rest of the display should
  // be in the original order from DRM.
  const DisplayConfigurator::DisplayStateList& cached =
      configurator_.cached_displays();
  ASSERT_EQ(cached.size(), 3U);

  EXPECT_EQ(cached[0]->display_id(), 2L);
  EXPECT_EQ(cached[0]->type(), DISPLAY_CONNECTION_TYPE_INTERNAL);

  EXPECT_EQ(cached[1]->display_id(), 1L);
  EXPECT_EQ(cached[1]->type(), DISPLAY_CONNECTION_TYPE_DISPLAYPORT);

  EXPECT_EQ(cached[2]->display_id(), 3L);
  EXPECT_EQ(cached[2]->type(), DISPLAY_CONNECTION_TYPE_HDMI);
}

TEST_F(DisplayConfiguratorTest, DoNotConfigureWithSuspendedDisplays) {
  InitWithOutputs(&small_mode_);

  // The DisplayConfigurator may occasionally receive OnConfigurationChanged()
  // after the displays have been suspended.  This event should be ignored since
  // the DisplayConfigurator will force a probe and reconfiguration of displays
  // at resume time.
  config_waiter_.Reset();
  configurator_.SuspendDisplays(config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(nullptr).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(nullptr).c_str(), kModesetOutcomeSuccess,
                        nullptr),
            log_->GetActionsAndClear());

  // The configuration timer should not be started when the displays
  // are suspended.
  configurator_.OnConfigurationChanged();
  EXPECT_FALSE(test_api_.TriggerConfigureTimeout());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // Calls to SetDisplayPower should do nothing if the power state doesn't
  // change.
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());

  UpdateOutputs(2, false);
  configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());

  // The DisplayConfigurator should do nothing at resume time if there is no
  // state change.
  config_waiter_.Reset();
  UpdateOutputs(1, false);
  configurator_.ResumeDisplays();
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // If a configuration task is pending when the displays are suspended, that
  // task should not run either and the timer should be stopped. The displays
  // should be turned off by suspend.
  configurator_.OnConfigurationChanged();
  config_waiter_.Reset();
  configurator_.SuspendDisplays(config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(nullptr).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(nullptr).c_str(), kModesetOutcomeSuccess,
                        nullptr),
            log_->GetActionsAndClear());
  EXPECT_FALSE(test_api_.TriggerConfigureTimeout());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  config_waiter_.Reset();
  configurator_.ResumeDisplays();
  // The timer should not be running.
  EXPECT_EQ(base::TimeDelta::Max(), config_waiter_.Wait());
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, HandleConfigureCrtcFailure) {
  InitWithOutputs(&small_mode_);

  std::vector<std::unique_ptr<const DisplayMode>> modes;
  // The first mode is the mode we are requesting DisplayConfigurator to choose.
  // The test will be setup so that this mode will fail and it will have to
  // choose the next best option.
  modes.push_back(MakeDisplayMode(2560, 1600, false, 60.0));
  modes.push_back(MakeDisplayMode(1024, 768, false, 60.0));
  modes.push_back(MakeDisplayMode(1280, 720, false, 60.0));
  modes.push_back(MakeDisplayMode(1920, 1080, false, 60.0));
  modes.push_back(MakeDisplayMode(1920, 1080, false, 40.0));

  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .AddMode(modes[2]->Clone())
                   .AddMode(modes[3]->Clone())
                   .AddMode(modes[4]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());

  // Since Chrome restricts the internal display to its native mode it should
  // not attempt other available modes. The likelihood of an internal display
  // failing to pass a modeset test is low, but we cover this case here.
  native_display_delegate_->set_max_configurable_pixels(
      modes[2]->size().GetArea());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(1, true);

  EXPECT_EQ(JoinActions(
                // Initial attempt fails.
                kTestModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               GetOutput(0)->native_mode()})
                    .c_str(),
                kModesetOutcomeFailure,
                // Initiate retry logic, which fails since it cannot downgrade
                // the internal display.
                kTestModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               GetOutput(0)->native_mode()})
                    .c_str(),
                kModesetOutcomeFailure, nullptr),
            log_->GetActionsAndClear());

  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .AddMode(modes[2]->Clone())
                   .AddMode(modes[3]->Clone())
                   .AddMode(modes[4]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_DISPLAYPORT)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());

  // This test simply fails in MULTIPLE_DISPLAY_STATE_SINGLE mode for an
  // external display (assuming the internal display is disabled; e.g. the lid
  // is closed).
  UpdateOutputs(1, true);

  EXPECT_EQ(JoinActions(
                // Initial attempt fails.
                kTestModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[0].get()})
                    .c_str(),
                kModesetOutcomeFailure,
                // Initiate retry logic.
                kTestModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[0].get()})
                    .c_str(),
                kModesetOutcomeFailure,
                // Retry attempts trying all available modes.
                kTestModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[3].get()})
                    .c_str(),
                kModesetOutcomeFailure, kTestModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[4].get()})
                    .c_str(),
                kModesetOutcomeFailure,
                // Test-modeset passes for this mode.
                kTestModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[2].get()})
                    .c_str(),
                kModesetOutcomeSuccess, kCommitModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[2].get()})
                    .c_str(),
                kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());

  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .AddMode(modes[2]->Clone())
                   .AddMode(modes[3]->Clone())
                   .AddMode(modes[4]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());

  SetOutput(1, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[1])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .AddMode(modes[2]->Clone())
                   .AddMode(modes[3]->Clone())
                   .AddMode(modes[4]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                   .SetBaseConnectorId(kSecondConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());

  // This test should attempt to configure a mirror mode that will not succeed
  // and should end up in extended mode.
  native_display_delegate_->set_max_configurable_pixels(
      modes[1]->size().GetArea());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  UpdateOutputs(2, true);

  EXPECT_EQ(
      JoinActions(
          // Initial attempt fails. Initiate retry logic.
          kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                         GetOutput(0)->native_mode()})
              .c_str(),
          GetCrtcAction(
              {GetOutput(1)->display_id(), gfx::Point(0, 0), modes[0].get()})
              .c_str(),
          kModesetOutcomeFailure,
          // We first test-modeset the internal display with all other displays
          // disabled, which will fail.
          kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                         GetOutput(0)->native_mode()})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          kModesetOutcomeFailure,
          // Since internal displays are restricted to their preferred mode,
          // there are no other modes to try. Disable the internal display so we
          // can attempt to modeset displays that are connected to other
          // connectors. Next, the external display will cycle through all its
          // available modes before failing completely.
          kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction(
              {GetOutput(1)->display_id(), gfx::Point(0, 0), modes[0].get()})
              .c_str(),
          kModesetOutcomeFailure, kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction(
              {GetOutput(1)->display_id(), gfx::Point(0, 0), modes[3].get()})
              .c_str(),
          kModesetOutcomeFailure, kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction(
              {GetOutput(1)->display_id(), gfx::Point(0, 0), modes[4].get()})
              .c_str(),
          kModesetOutcomeFailure, kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction(
              {GetOutput(1)->display_id(), gfx::Point(0, 0), modes[2].get()})
              .c_str(),
          kModesetOutcomeFailure,
          // This configuration still passes intermediate test-modeset.
          kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction(
              {GetOutput(1)->display_id(), gfx::Point(0, 0), modes[1].get()})
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction(
              {GetOutput(1)->display_id(), gfx::Point(0, 0), modes[1].get()})
              .c_str(),
          kModesetOutcomeSuccess,
          // Since mirror mode configuration failed it should now attempt to
          // configure in extended mode. However, initial attempt fails.
          // Initiate retry logic.
          kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                         GetOutput(0)->native_mode()})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, modes[0]->size().height() +
                                           DisplayConfigurator::kVerticalGap),
                         modes[0].get()})
              .c_str(),
          kModesetOutcomeFailure,
          // We first test-modeset the internal display with all other displays
          // disabled, which will fail.
          kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                         GetOutput(0)->native_mode()})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, modes[0]->size().height() +
                                           DisplayConfigurator::kVerticalGap),
                         nullptr})
              .c_str(),
          kModesetOutcomeFailure,
          // The configuration fails completely but still attempts to modeset
          // the external display.
          kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, modes[0]->size().height() +
                                           DisplayConfigurator::kVerticalGap),
                         modes[0].get()})
              .c_str(),
          kModesetOutcomeFailure, kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, modes[0]->size().height() +
                                           DisplayConfigurator::kVerticalGap),
                         modes[3].get()})
              .c_str(),
          kModesetOutcomeFailure, kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, modes[0]->size().height() +
                                           DisplayConfigurator::kVerticalGap),
                         modes[4].get()})
              .c_str(),
          kModesetOutcomeFailure, kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, modes[0]->size().height() +
                                           DisplayConfigurator::kVerticalGap),
                         modes[2].get()})
              .c_str(),
          kModesetOutcomeFailure,
          // This configuration passes test-modeset.
          kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, modes[0]->size().height() +
                                           DisplayConfigurator::kVerticalGap),
                         modes[1].get()})
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, modes[0]->size().height() +
                                           DisplayConfigurator::kVerticalGap),
                         modes[1].get()})
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
}

// Tests that power state requests are saved after failed configuration attempts
// so they can be reused later: http://crosbug.com/p/31571
TEST_F(DisplayConfiguratorTest, SaveDisplayPowerStateOnConfigFailure) {
  // Start out with two displays in extended mode.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  Init(false);
  configurator_.ForceInitialConfigure();
  log_->GetActionsAndClear();
  observer_.Reset();

  // Turn off the internal display, simulating docked mode.
  config_waiter_.Reset();
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(0, observer_.num_failures());
  log_->GetActionsAndClear();

  // Make all subsequent configuration requests fail and try to turn the
  // internal display back on.
  config_waiter_.Reset();
  native_display_delegate_->set_max_configurable_pixels(1);
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_FAILURE, config_waiter_.callback_result());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(1, observer_.num_failures());
  log_->GetActionsAndClear();

  // Simulate the external display getting disconnected and check that the
  // internal display is turned on (i.e. DISPLAY_POWER_ALL_ON is used) rather
  // than the earlier DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON state.
  native_display_delegate_->set_max_configurable_pixels(0);
  UpdateOutputs(1, true);
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
}

// Tests that the SetDisplayPowerState() task posted by HandleResume() doesn't
// use a stale state if a new state is requested before it runs:
// http://crosbug.com/p/32393
TEST_F(DisplayConfiguratorTest, DontRestoreStalePowerStateAfterResume) {
  // Start out with two displays in mirrored mode.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  Init(false);
  configurator_.ForceInitialConfigure();
  log_->GetActionsAndClear();
  observer_.Reset();

  // Turn off the internal display, simulating docked mode.
  configurator_.SetDisplayPower(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      DisplayConfigurator::kSetDisplayPowerNoFlags,
      config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(0, observer_.num_failures());
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, nullptr, &big_mode_).c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, nullptr, &big_mode_).c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());

  // Suspend and resume the system. Resuming should restore the previous power
  // state and force a probe. Suspend should turn off the displays since an
  // external monitor is connected.
  config_waiter_.Reset();
  configurator_.SuspendDisplays(config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(2, observer_.num_changes());
  EXPECT_EQ(
      JoinActions(kTestModesetStr,
                  GetCrtcActions(DisplayConfig::kOff, nullptr, nullptr).c_str(),
                  kModesetOutcomeSuccess, kCommitModesetStr,
                  GetCrtcActions(DisplayConfig::kOff, nullptr, nullptr).c_str(),
                  kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());

  // Before the task runs, exit docked mode.
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(3, observer_.num_changes());
  EXPECT_EQ(0, observer_.num_failures());
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());

  // Check that the display states are not changed after resuming.
  config_waiter_.Reset();
  // Since we are in dual display mode, a configuration task is scheduled after
  // kMinLongDelayMs delay.
  configurator_.ResumeDisplays();
  EXPECT_EQ(kLongDelay, test_api_.GetConfigureDelay());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON,
            configurator_.current_power_state());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
  // Now trigger that delayed configuration.
  EXPECT_EQ(kLongDelay, config_waiter_.Wait());
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kMirror, &small_mode_, &small_mode_)
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, ExternalControl) {
  InitWithOutputs(&small_mode_);
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);

  // Set the initial power state and verify that it is restored when control is
  // taken.
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());

  configurator_.RelinquishControl(
      base::BindOnce(&DisplayConfiguratorTest::OnDisplayControlUpdated,
                     base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopDisplayControlResult());
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(nullptr).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(nullptr).c_str(), kModesetOutcomeSuccess,
                        kRelinquishDisplayControl, nullptr),
            log_->GetActionsAndClear());
  configurator_.TakeControl(
      base::BindOnce(&DisplayConfiguratorTest::OnDisplayControlUpdated,
                     base::Unretained(this)));
  EXPECT_EQ(CALLBACK_SUCCESS, PopDisplayControlResult());
  EXPECT_EQ(
      JoinActions(kTakeDisplayControl, kTestModesetStr,
                  GetCrtcActions(&small_mode_).c_str(), kModesetOutcomeSuccess,
                  kCommitModesetStr, GetCrtcActions(&small_mode_).c_str(),
                  kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest,
       SetDisplayPowerWhilePendingConfigurationTaskRunning) {
  // Start out with two displays in extended mode.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  Init(false);
  configurator_.ForceInitialConfigure();
  log_->GetActionsAndClear();
  observer_.Reset();

  native_display_delegate_->set_run_async(true);

  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(CALLBACK_NOT_CALLED, config_waiter_.callback_result());

  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());

  EXPECT_EQ(CALLBACK_NOT_CALLED, config_waiter_.callback_result());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(0, observer_.num_failures());

  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kOff, &small_mode_, &big_mode_).c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kOff, &small_mode_, &big_mode_).c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());

  config_waiter_.Reset();
  EXPECT_EQ(base::Milliseconds(DisplayConfigurator::kConfigureDelayMs),
            config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_NOT_CALLED, config_waiter_.callback_result());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(2, observer_.num_changes());
  EXPECT_EQ(0, observer_.num_failures());

  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest,
       SetDisplayPowerAfterFailedDisplayConfiguration) {
  // Start out with two displays in extended mode.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  Init(false);
  configurator_.ForceInitialConfigure();
  log_->GetActionsAndClear();
  observer_.Reset();

  // Fail display configuration.
  native_display_delegate_->set_max_configurable_pixels(-1);

  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_FAILURE, config_waiter_.callback_result());
  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_EQ(1, observer_.num_failures());

  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kOff, &small_mode_, &big_mode_).c_str(),
          kModesetOutcomeFailure, kTestModesetStr,
          GetCrtcActions(DisplayConfig::kOff, &small_mode_, &big_mode_).c_str(),
          kModesetOutcomeFailure, kTestModesetStr,
          GetCrtcActions(DisplayConfig::kOff, &small_mode_, &big_mode_).c_str(),
          kModesetOutcomeFailure, nullptr),
      log_->GetActionsAndClear());

  // This configuration should trigger a display configuration since the
  // previous configuration failed.
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());

  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_EQ(2, observer_.num_failures());
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr, GetCrtcActions(&small_mode_, &big_mode_).c_str(),
          kModesetOutcomeFailure,
          // We first attempt to modeset the internal display with all
          // other displays disabled, which will fail.
          kTestModesetStr, GetCrtcActions(&small_mode_).c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, small_mode_.size().height() +
                                           DisplayConfigurator::kVerticalGap),
                         nullptr})
              .c_str(),
          kModesetOutcomeFailure,
          // Since internal displays are restricted to their preferred mode,
          // there are no other modes to try. Disable the internal display while
          // we attempt to modeset displays that are connected to other
          // connectors. Configuration will fail.
          kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, small_mode_.size().height() +
                                           DisplayConfigurator::kVerticalGap),
                         &big_mode_})
              .c_str(),
          kModesetOutcomeFailure, kTestModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0), nullptr})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, small_mode_.size().height() +
                                           DisplayConfigurator::kVerticalGap),
                         &small_mode_})
              .c_str(),
          kModesetOutcomeFailure, nullptr),
      log_->GetActionsAndClear());

  // Allow configuration to succeed.
  native_display_delegate_->set_max_configurable_pixels(0);

  // Validate that a configuration event has the proper power state (displays
  // should be on).
  configurator_.OnConfigurationChanged();
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());

  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(2, observer_.num_failures());

  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, TestWithThreeDisplays) {
  // Start out with two displays in extended mode.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  Init(false);
  configurator_.ForceInitialConfigure();
  log_->GetActionsAndClear();
  observer_.Reset();

  UpdateOutputs(3, true);
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);

  EXPECT_EQ(JoinActions(
                kTestModesetStr,
                GetCrtcActions(&small_mode_, &big_mode_, &small_mode_).c_str(),
                kModesetOutcomeSuccess, kCommitModesetStr,
                GetCrtcActions(&small_mode_, &big_mode_, &small_mode_).c_str(),
                kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());

  // Verify that turning the power off works.
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_OFF,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcActions(DisplayConfig::kOff, &small_mode_,
                                       &big_mode_, &small_mode_)
                            .c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(DisplayConfig::kOff, &small_mode_,
                                       &big_mode_, &small_mode_)
                            .c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());

  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(JoinActions(
                kTestModesetStr,
                GetCrtcActions(&small_mode_, &big_mode_, &small_mode_).c_str(),
                kModesetOutcomeSuccess, kCommitModesetStr,
                GetCrtcActions(&small_mode_, &big_mode_, &small_mode_).c_str(),
                kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());

  // Disconnect the third output.
  observer_.Reset();
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  UpdateOutputs(2, true);
  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
}

// Tests the suspend and resume behavior when in dual or multi display modes.
TEST_F(DisplayConfiguratorTest, SuspendResumeWithMultipleDisplays) {
  InitWithOutputs(&small_mode_);

  // Set the initial power state and verify that it is restored on resume.
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());

  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  observer_.Reset();
  UpdateOutputs(2, true);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
  EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON,
            configurator_.current_power_state());

  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());

  // Suspending displays should result in an immediate configuration without
  // delays, even in dual display mode.
  config_waiter_.Reset();
  configurator_.SuspendDisplays(config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF,
            configurator_.current_power_state());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kOff, &small_mode_, &big_mode_).c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kOff, &small_mode_, &big_mode_).c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());

  // Resuming from suspend with dual displays. Configuration should be done
  // after a long delay. Afterwards, we should still expect to be in a dual
  // display mode.
  config_waiter_.Reset();
  configurator_.ResumeDisplays();
  EXPECT_EQ(kLongDelay, config_waiter_.Wait());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON,
            configurator_.current_power_state());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
  EXPECT_EQ(JoinActions(kTestModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_, &big_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());

  // Suspend displays and disconnect one of them while in suspend.
  config_waiter_.Reset();
  configurator_.SuspendDisplays(config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF,
            configurator_.current_power_state());
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr,
          GetCrtcActions(DisplayConfig::kOff, &small_mode_, &big_mode_).c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr,
          GetCrtcActions(DisplayConfig::kOff, &small_mode_, &big_mode_).c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
  UpdateOutputs(1, false);
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // Now resume, and expect that we'll still have a long delay since we were in
  // dual mode before suspend. The configurator should pick up the change and
  // detect that we are in single display mode now.
  config_waiter_.Reset();
  configurator_.ResumeDisplays();
  EXPECT_EQ(kLongDelay, config_waiter_.Wait());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON,
            configurator_.current_power_state());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_SINGLE, configurator_.display_state());
  EXPECT_EQ(JoinActions(kTestModesetStr, GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        GetCrtcActions(&small_mode_).c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());

  // Verify that the above is the exact same behavior for 3+ displays.
  UpdateOutputs(3, true);
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());

  // Suspend.
  config_waiter_.Reset();
  configurator_.SuspendDisplays(config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF,
            configurator_.current_power_state());

  // Resume and expect the correct delay.
  config_waiter_.Reset();
  configurator_.ResumeDisplays();
  EXPECT_EQ(kLongDelay, config_waiter_.Wait());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON,
            configurator_.current_power_state());
  EXPECT_EQ(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            configurator_.display_state());
}

TEST_F(DisplayConfiguratorTest, PowerStateChange) {
  InitWithOutputs(&small_mode_);

  native_display_delegate_->set_run_async(true);

  // Set the initial power state and verify that it is restored on resume.
  config_waiter_.Reset();
  configurator_.SetDisplayPower(chromeos::DISPLAY_POWER_ALL_ON,
                                DisplayConfigurator::kSetDisplayPowerNoFlags,
                                config_waiter_.on_configuration_callback());

  // SuspendDisplays causes notifying the DISPLAY_POWER_ALL_OFF state to the
  // observer.
  config_waiter_.Reset();
  observer_.Reset();
  configurator_.SuspendDisplays(config_waiter_.on_configuration_callback());
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  EXPECT_EQ(1, observer_.num_power_state_changes());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, observer_.latest_power_state());

  // ResumeDisplays causes notifying the DISPLAY_POWER_ALL_ON state to the
  // observer.
  config_waiter_.Reset();
  configurator_.ResumeDisplays();
  EXPECT_EQ(base::TimeDelta::Max(), config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_NOT_CALLED, config_waiter_.callback_result());
  EXPECT_EQ(2, observer_.num_power_state_changes());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, observer_.latest_power_state());

  // SuspendDisplays and ResumeDisplays before running the configuration task
  // causes notifying only one power state change to the observer.
  config_waiter_.Reset();
  observer_.Reset();
  configurator_.SuspendDisplays(config_waiter_.on_configuration_callback());
  EXPECT_EQ(0, observer_.num_power_state_changes());
  configurator_.ResumeDisplays();
  // Run the task posted by TestNativeDisplayDelegate::GetDisplays() which is
  // called by SuspendDisplays().
  EXPECT_EQ(kNoDelay, config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_SUCCESS, config_waiter_.callback_result());
  config_waiter_.Reset();
  // Run the task posted by OnConfigured().
  EXPECT_EQ(base::Milliseconds(DisplayConfigurator::kConfigureDelayMs),
            config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_NOT_CALLED, config_waiter_.callback_result());
  config_waiter_.Reset();
  // Run the task posted by TestNativeDisplayDelegate::GetDisplays().
  EXPECT_EQ(base::TimeDelta::Max(), config_waiter_.Wait());
  EXPECT_EQ(CALLBACK_NOT_CALLED, config_waiter_.callback_result());
  EXPECT_EQ(1, observer_.num_power_state_changes());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, observer_.latest_power_state());
}

TEST_F(DisplayConfiguratorTest, RefreshRateThrottle_SingleDisplay) {
  InitWithOutputs(&small_mode_);
  // Set up display with HRR native mode and eligible throttle candidate mode.
  std::vector<std::unique_ptr<const DisplayMode>> modes;
  modes.push_back(MakeDisplayMode(1366, 768, false, 120.0));
  modes.push_back(MakeDisplayMode(1366, 768, false, 60.0));
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(1, true);
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  log_->GetActionsAndClear();
  observer_.Reset();

  // Set throttle state noop.
  configurator_.SetRefreshRateOverrides({});
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // Set throttle state enabled.
  configurator_.SetRefreshRateOverrides(
      {std::make_pair(GetOutput(0)->display_id(), 60.f)});
  EXPECT_EQ(60.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(JoinActions(kTestModesetStr, kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[1].get(), false})
                            .c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[1].get(), false})
                            .c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  observer_.Reset();

  // Set throttle state disabled.
  configurator_.SetRefreshRateOverrides({});
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(JoinActions(kTestModesetStr, kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[0].get()})
                            .c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[0].get()})
                            .c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, RefreshRateThrottle_AlreadyThrottled) {
  InitWithOutputs(&small_mode_);
  // Set up display with HRR native mode and eligible throttle candidate mode.
  std::vector<std::unique_ptr<const DisplayMode>> modes;
  modes.push_back(MakeDisplayMode(1366, 768, false, 120.0));
  modes.push_back(MakeDisplayMode(1366, 768, false, 60.0));
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(1, true);
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  log_->GetActionsAndClear();
  observer_.Reset();

  // Set throttle state enabled.
  configurator_.SetRefreshRateOverrides(
      {std::make_pair(GetOutput(0)->display_id(), 60.f)});
  EXPECT_EQ(60.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(JoinActions(kTestModesetStr, kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[1].get(), false})
                            .c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[1].get(), false})
                            .c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  observer_.Reset();

  // Set the same throttle state. This should be a no-op.
  configurator_.SetRefreshRateOverrides(
      {std::make_pair(GetOutput(0)->display_id(), 60.f)});
  EXPECT_EQ(60.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, RefreshRateThrottle_OverrideWithNative) {
  InitWithOutputs(&small_mode_);
  // Set up display with HRR native mode and eligible throttle candidate mode.
  std::vector<std::unique_ptr<const DisplayMode>> modes;
  modes.push_back(MakeDisplayMode(1366, 768, false, 120.0));
  modes.push_back(MakeDisplayMode(1366, 768, false, 60.0));
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(1, true);
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  log_->GetActionsAndClear();
  observer_.Reset();

  // Set throttle state enabled.
  configurator_.SetRefreshRateOverrides(
      {std::make_pair(GetOutput(0)->display_id(), 60.f)});
  EXPECT_EQ(60.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(JoinActions(kTestModesetStr, kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[1].get(), false})
                            .c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[1].get(), false})
                            .c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  observer_.Reset();

  // Specifying the native mode's refresh rate as an override.
  configurator_.SetRefreshRateOverrides(
      {std::make_pair(GetOutput(0)->display_id(), 120.f)});
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());

  // No override should be set.
  EXPECT_EQ(JoinActions(kTestModesetStr, kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[0].get(), false})
                            .c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[0].get(), false})
                            .c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  observer_.Reset();
}

TEST_F(DisplayConfiguratorTest, RefreshRateThrottle_WrongDisplayId) {
  InitWithOutputs(&small_mode_);
  // Set up display with HRR native mode and eligible throttle candidate mode.
  std::vector<std::unique_ptr<const DisplayMode>> modes;
  modes.push_back(MakeDisplayMode(1366, 768, false, 120.0));
  modes.push_back(MakeDisplayMode(1366, 768, false, 60.0));
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(1, true);
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  log_->GetActionsAndClear();
  observer_.Reset();

  // Set throttle state for wrong display id.
  configurator_.SetRefreshRateOverrides(
      {std::make_pair(GetOutput(0)->display_id() + 100, 60.f)});
  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, RefreshRateThrottle_MultipleDisplays) {
  InitWithOutputs(&small_mode_, &big_mode_);
  // Set up each display with HRR native mode and eligible throttle candidate
  // mode.
  std::vector<std::unique_ptr<const DisplayMode>> modes;
  modes.push_back(MakeDisplayMode(1366, 768, false, 120.0));
  modes.push_back(MakeDisplayMode(1366, 768, false, 60.0));
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());
  // External display should never be throttled irregardless of its modes.
  SetOutput(1, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[1])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                   .SetBaseConnectorId(kSecondConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  UpdateOutputs(2, true);
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(120.0f, GetOutput(1)->current_mode()->refresh_rate());
  log_->GetActionsAndClear();
  observer_.Reset();

  // Set refresh rate override noop.
  configurator_.SetRefreshRateOverrides({});
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(120.0f, GetOutput(1)->current_mode()->refresh_rate());
  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // Set refresh rate override for internal display.
  configurator_.SetRefreshRateOverrides(
      {std::make_pair(GetOutput(0)->display_id(), 60.f)});
  EXPECT_EQ(60.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(120.0f, GetOutput(1)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  int vertical_offset = GetOutput(0)->native_mode()->size().height() +
                        DisplayConfigurator::kVerticalGap;
  EXPECT_EQ(JoinActions(
                kTestModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[1].get()})
                    .c_str(),
                GetCrtcAction({GetOutput(1)->display_id(),
                               gfx::Point(0, vertical_offset), modes[0].get()})
                    .c_str(),
                kModesetOutcomeSuccess, kCommitModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[1].get()})
                    .c_str(),
                GetCrtcAction({GetOutput(1)->display_id(),
                               gfx::Point(0, vertical_offset), modes[0].get()})
                    .c_str(),
                kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  observer_.Reset();
  observer_.Reset();

  // Remove refresh rate overrides.
  configurator_.SetRefreshRateOverrides({});
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(120.0f, GetOutput(1)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(JoinActions(
                kTestModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[0].get()})
                    .c_str(),
                GetCrtcAction({GetOutput(1)->display_id(),
                               gfx::Point(0, vertical_offset), modes[0].get()})
                    .c_str(),
                kModesetOutcomeSuccess, kCommitModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[0].get()})
                    .c_str(),
                GetCrtcAction({GetOutput(1)->display_id(),
                               gfx::Point(0, vertical_offset), modes[0].get()})
                    .c_str(),
                kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, RefreshRateThrottle_RaceWithDockMode) {
  InitWithOutputs(&small_mode_, &big_mode_);
  // Set up two displays with HRR native mode and eligible throttle candidate
  // mode.
  std::vector<std::unique_ptr<const DisplayMode>> modes;
  modes.push_back(MakeDisplayMode(1366, 768, false, 120.0));
  modes.push_back(MakeDisplayMode(1366, 768, false, 60.0));
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());
  SetOutput(1, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[1])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                   .SetBaseConnectorId(kSecondConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());
  UpdateOutputs(2, true);
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(120.0f, GetOutput(1)->current_mode()->refresh_rate());
  log_->GetActionsAndClear();
  observer_.Reset();

  // Get DisplayConfigureRequests for a configuration that simultaneously
  // attempts to enable refresh rate throttling, and docked mode.
  std::vector<DisplayConfigureRequest> requests;
  test_api_.GetDisplayLayoutManager()->GetDisplayLayout(
      native_display_delegate_->GetOutputs(), MULTIPLE_DISPLAY_STATE_SINGLE,
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      /*new_vrr_enabled_state=*/{}, &requests);

  bool has_internal_request = false;
  for (auto& request: requests) {
    if (request.display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL) {
        has_internal_request = true;
        EXPECT_EQ(request.mode, nullptr);
    }
  }
  EXPECT_TRUE(has_internal_request);
}

TEST_F(DisplayConfiguratorTest,
       RefreshRateThrottle_StaysThrottledForSeamlessConfig) {
  InitWithOutputs(&small_mode_);
  // Set up display with HRR native mode, eligible throttle candidate mode, and
  // VRR.
  std::vector<std::unique_ptr<const DisplayMode>> modes;
  modes.push_back(MakeDisplayMode(1366, 768, false, 120.0));
  modes.push_back(MakeDisplayMode(1366, 768, false, 60.0));
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .SetVariableRefreshRateState(
                       VariableRefreshRateState::kVrrDisabled)
                   .Build());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(1, true);
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  log_->GetActionsAndClear();
  observer_.Reset();

  // Set throttle state enabled.
  configurator_.SetRefreshRateOverrides(
      {std::make_pair(GetOutput(0)->display_id(), 60.f)});

  EXPECT_EQ(60.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(JoinActions(kTestModesetStr, kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[1].get()})
                            .c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[1].get()})
                            .c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  observer_.Reset();

  // Set VRR enabled. VRR should be enabled, and downclock mode should be used.
  configurator_.SetVrrEnabled({GetOutput(0)->display_id()});
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_TRUE(GetOutput(0)->IsVrrEnabled());
  EXPECT_EQ(JoinActions(
                kTestModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[1].get(), /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, kCommitModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[1].get(), /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  observer_.Reset();

  // Set throttle state disabled.
  configurator_.SetRefreshRateOverrides({});

  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_TRUE(GetOutput(0)->IsVrrEnabled());
  EXPECT_EQ(JoinActions(
                kTestModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[0].get(), /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, kCommitModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[0].get(), /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  observer_.Reset();
}

TEST_F(DisplayConfiguratorTest,
       RefreshRateThrottle_ResetThrottleForFullConfig) {
  InitWithOutputs(&small_mode_);
  // Set up display with HRR native mode, eligible throttle candidate mode.
  std::vector<std::unique_ptr<const DisplayMode>> modes;
  modes.push_back(MakeDisplayMode(1366, 768, false, 120.0));
  modes.push_back(MakeDisplayMode(1366, 768, false, 60.0));
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .Build());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(1, true);
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  log_->GetActionsAndClear();
  observer_.Reset();

  // Set throttle state enabled.
  configurator_.SetRefreshRateOverrides(
      {std::make_pair(GetOutput(0)->display_id(), 60.f)});

  EXPECT_EQ(60.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(JoinActions(kTestModesetStr, kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[1].get()})
                            .c_str(),
                        kModesetOutcomeSuccess, kCommitModesetStr,
                        kSeamlessModesetStr,
                        GetCrtcAction({GetOutput(0)->display_id(),
                                       gfx::Point(0, 0), modes[1].get()})
                            .c_str(),
                        kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  observer_.Reset();

  // Plug in new display.
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  UpdateOutputs(2, true);

  // Configuration should use preferred mode rather than throttled mode.
  int vertical_offset = GetOutput(0)->native_mode()->size().height() +
                        DisplayConfigurator::kVerticalGap;
  EXPECT_EQ(
      JoinActions(kTestModesetStr,
                  GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                                 modes[0].get()})
                      .c_str(),
                  GetCrtcAction({GetOutput(1)->display_id(),
                                 gfx::Point(0, vertical_offset), &big_mode_})
                      .c_str(),
                  kModesetOutcomeSuccess, kCommitModesetStr,
                  GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                                 modes[0].get()})
                      .c_str(),
                  GetCrtcAction({GetOutput(1)->display_id(),
                                 gfx::Point(0, vertical_offset), &big_mode_})
                      .c_str(),
                  kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, SetVrrEnabled) {
  InitWithOutputs(&small_mode_);
  UpdateOutputs(2, true);
  EXPECT_FALSE(GetOutput(0)->IsVrrEnabled());
  EXPECT_FALSE(GetOutput(1)->IsVrrEnabled());
  log_->GetActionsAndClear();
  observer_.Reset();

  // Set VRR noop.
  configurator_.SetVrrEnabled({});
  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_FALSE(GetOutput(0)->IsVrrEnabled());
  EXPECT_FALSE(GetOutput(1)->IsVrrEnabled());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());

  // Set VRR enabled.
  configurator_.SetVrrEnabled(
      {GetOutput(0)->display_id(), GetOutput(1)->display_id()});
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_TRUE(GetOutput(0)->IsVrrEnabled());
  EXPECT_FALSE(GetOutput(1)->IsVrrEnabled());
  int vertical_offset = GetOutput(0)->native_mode()->size().height() +
                        DisplayConfigurator::kVerticalGap;
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr, kSeamlessModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                         GetOutput(0)->native_mode(), /*enable_vrr=*/true})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, vertical_offset),
                         GetOutput(1)->native_mode(), /*enable_vrr=*/false})
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr, kSeamlessModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                         GetOutput(0)->native_mode(), /*enable_vrr=*/true})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, vertical_offset),
                         GetOutput(1)->native_mode(), /*enable_vrr=*/false})
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
  observer_.Reset();

  // Set VRR disabled.
  configurator_.SetVrrEnabled({});
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_FALSE(GetOutput(0)->IsVrrEnabled());
  EXPECT_FALSE(GetOutput(1)->IsVrrEnabled());
  EXPECT_EQ(
      JoinActions(
          kTestModesetStr, kSeamlessModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                         GetOutput(0)->native_mode(), /*enable_vrr=*/false})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, vertical_offset),
                         GetOutput(1)->native_mode(), /*enable_vrr=*/false})
              .c_str(),
          kModesetOutcomeSuccess, kCommitModesetStr, kSeamlessModesetStr,
          GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                         GetOutput(0)->native_mode(), /*enable_vrr=*/false})
              .c_str(),
          GetCrtcAction({GetOutput(1)->display_id(),
                         gfx::Point(0, vertical_offset),
                         GetOutput(1)->native_mode(), /*enable_vrr=*/false})
              .c_str(),
          kModesetOutcomeSuccess, nullptr),
      log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, SetVrrEnabled_NotCapable) {
  std::unique_ptr<DisplaySnapshot> output = GetOutput(0)->Clone();
  output->set_variable_refresh_rate_state(
      VariableRefreshRateState::kVrrNotCapable);
  SetOutput(0, std::move(output));
  InitWithOutputs(&small_mode_);
  UpdateOutputs(2, true);
  EXPECT_FALSE(GetOutput(0)->IsVrrEnabled());
  EXPECT_FALSE(GetOutput(1)->IsVrrEnabled());
  log_->GetActionsAndClear();
  observer_.Reset();

  configurator_.SetVrrEnabled(
      {GetOutput(0)->display_id(), GetOutput(1)->display_id()});
  EXPECT_EQ(0, observer_.num_changes());
  EXPECT_FALSE(GetOutput(0)->IsVrrEnabled());
  EXPECT_FALSE(GetOutput(1)->IsVrrEnabled());
  EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest, RefreshRateThrottle_VrrEnabled) {
  InitWithOutputs(&small_mode_);
  // Set up display with HRR native mode and eligible throttle candidate mode.
  std::vector<std::unique_ptr<const DisplayMode>> modes;
  modes.push_back(MakeDisplayMode(1366, 768, false, 120.0));
  modes.push_back(MakeDisplayMode(1366, 768, false, 60.0));
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .SetVariableRefreshRateState(
                       VariableRefreshRateState::kVrrDisabled)
                   .Build());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_SINGLE);
  UpdateOutputs(1, true);
  // Enable VRR on internal display.
  configurator_.SetVrrEnabled({GetOutput(0)->display_id()});
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_TRUE(GetOutput(0)->IsVrrEnabled());
  log_->GetActionsAndClear();
  observer_.Reset();

  // Set throttle state enabled.
  configurator_.SetRefreshRateOverrides(
      {std::make_pair(GetOutput(0)->display_id(), 60.f)});

  EXPECT_EQ(60.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  // Throttling should be unaffected by the internal display VRR state and still
  // result in seamless modesets.
  EXPECT_EQ(JoinActions(
                kTestModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[1].get(), /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, kCommitModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[1].get(), /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  observer_.Reset();

  // Set throttle state disabled.
  configurator_.SetRefreshRateOverrides({});

  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  // Unthrottling should be unaffected by the internal display VRR state and
  // still result in seamless modesets.
  EXPECT_EQ(JoinActions(
                kTestModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[0].get(), /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, kCommitModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[0].get(), /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
}

TEST_F(DisplayConfiguratorTest,
       RefreshRateThrottle_VrrEnabledOnExternalDisplay) {
  InitWithOutputs(&small_mode_, &big_mode_);
  // Set up each display with HRR native mode and eligible throttle candidate
  // mode.
  std::vector<std::unique_ptr<const DisplayMode>> modes;
  modes.push_back(MakeDisplayMode(1366, 768, false, 120.0));
  modes.push_back(MakeDisplayMode(1366, 768, false, 60.0));
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetNativeMode(modes[0]->Clone())
                   .SetCurrentMode(modes[0]->Clone())
                   .AddMode(modes[1]->Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetBaseConnectorId(kEdpConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .SetVariableRefreshRateState(
                       VariableRefreshRateState::kVrrNotCapable)
                   .Build());
  SetOutput(1, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[1])
                   .SetNativeMode(big_mode_.Clone())
                   .SetCurrentMode(big_mode_.Clone())
                   .AddMode(small_mode_.Clone())
                   .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                   .SetBaseConnectorId(kSecondConnectorId)
                   .SetIsAspectPreservingScaling(true)
                   .SetVariableRefreshRateState(
                       VariableRefreshRateState::kVrrDisabled)
                   .Build());
  state_controller_.set_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  UpdateOutputs(2, true);
  // Enable VRR when only the external display is VRR-capable.
  configurator_.SetVrrEnabled(
      {GetOutput(0)->display_id(), GetOutput(1)->display_id()});
  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(60.0f, GetOutput(1)->current_mode()->refresh_rate());
  EXPECT_FALSE(GetOutput(0)->IsVrrEnabled());
  EXPECT_TRUE(GetOutput(1)->IsVrrEnabled());
  log_->GetActionsAndClear();
  observer_.Reset();

  // Set throttle state enabled.
  configurator_.SetRefreshRateOverrides(
      {std::make_pair(GetOutput(0)->display_id(), 60.f)});

  EXPECT_EQ(60.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(60.0f, GetOutput(1)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  int vertical_offset = GetOutput(0)->native_mode()->size().height() +
                        DisplayConfigurator::kVerticalGap;
  // Throttling should be unaffected by the external display VRR state and still
  // result in seamless modesets.
  EXPECT_EQ(JoinActions(
                kTestModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[1].get(), /*enable_vrr=*/false})
                    .c_str(),
                GetCrtcAction({GetOutput(1)->display_id(),
                               gfx::Point(0, vertical_offset), &big_mode_,
                               /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, kCommitModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[1].get(), /*enable_vrr=*/false})
                    .c_str(),
                GetCrtcAction({GetOutput(1)->display_id(),
                               gfx::Point(0, vertical_offset), &big_mode_,
                               /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
  observer_.Reset();

  // Set throttle state disabled.
  configurator_.SetRefreshRateOverrides({});

  EXPECT_EQ(120.0f, GetOutput(0)->current_mode()->refresh_rate());
  EXPECT_EQ(60.0f, GetOutput(1)->current_mode()->refresh_rate());
  EXPECT_EQ(1, observer_.num_changes());
  // Unthrottling should be unaffected by the external display VRR state and
  // still result in seamless modesets.
  EXPECT_EQ(JoinActions(
                kTestModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[0].get(), /*enable_vrr=*/false})
                    .c_str(),
                GetCrtcAction({GetOutput(1)->display_id(),
                               gfx::Point(0, vertical_offset), &big_mode_,
                               /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, kCommitModesetStr, kSeamlessModesetStr,
                GetCrtcAction({GetOutput(0)->display_id(), gfx::Point(0, 0),
                               modes[0].get(), /*enable_vrr=*/false})
                    .c_str(),
                GetCrtcAction({GetOutput(1)->display_id(),
                               gfx::Point(0, vertical_offset), &big_mode_,
                               /*enable_vrr=*/true})
                    .c_str(),
                kModesetOutcomeSuccess, nullptr),
            log_->GetActionsAndClear());
}

class DisplayConfiguratorMultiMirroringTest : public DisplayConfiguratorTest {
 public:
  DisplayConfiguratorMultiMirroringTest() = default;

  DisplayConfiguratorMultiMirroringTest(
      const DisplayConfiguratorMultiMirroringTest&) = delete;
  DisplayConfiguratorMultiMirroringTest& operator=(
      const DisplayConfiguratorMultiMirroringTest&) = delete;

  ~DisplayConfiguratorMultiMirroringTest() override = default;

  void SetUp() override { DisplayConfiguratorTest::SetUp(); }

  // Test that setting mirror mode with current outputs, all displays are set to
  // expected mirror mode.
  void TestHardwareMirrorModeExist(
      std::unique_ptr<DisplayMode> expected_mirror_mode) {
    UpdateOutputs(3, true);
    log_->GetActionsAndClear();
    observer_.Reset();
    configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
    EXPECT_EQ(
        JoinActions(kTestModesetStr,
                    GetCrtcActions(
                        DisplayConfig::kMirror, expected_mirror_mode.get(),
                        expected_mirror_mode.get(), expected_mirror_mode.get())
                        .c_str(),
                    kModesetOutcomeSuccess, kCommitModesetStr,
                    GetCrtcActions(
                        DisplayConfig::kMirror, expected_mirror_mode.get(),
                        expected_mirror_mode.get(), expected_mirror_mode.get())
                        .c_str(),
                    kModesetOutcomeSuccess, nullptr),
        log_->GetActionsAndClear());
    EXPECT_FALSE(mirroring_controller_.SoftwareMirroringEnabled());
    EXPECT_EQ(1, observer_.num_changes());
  }

  // Test that setting mirror mode with current outputs, no matching mirror mode
  // is found.
  void TestHardwareMirrorModeNotExist() {
    UpdateOutputs(3, true);
    log_->GetActionsAndClear();
    observer_.Reset();
    configurator_.SetMultipleDisplayState(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
    EXPECT_EQ(kNoActions, log_->GetActionsAndClear());
    EXPECT_TRUE(mirroring_controller_.SoftwareMirroringEnabled());
    EXPECT_EQ(1, observer_.num_changes());
  }
};

TEST_F(DisplayConfiguratorMultiMirroringTest,
       FindMirrorModeWithInternalDisplay) {
  // Initialize with one internal display and two external displays.
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetType(DISPLAY_CONNECTION_TYPE_INTERNAL)
                   .SetNativeMode(MakeDisplayMode(1920, 1600, false, 60.0))
                   .AddMode(MakeDisplayMode(1920, 1600, false, 60.0))
                   .AddMode(MakeDisplayMode(1920, 1200, false, 60.0))
                   .AddMode(MakeDisplayMode(1920, 1080, true, 60.0))
                   .AddMode(MakeDisplayMode(1440, 900, true, 60.0))
                   .Build());
  SetOutput(1,
            FakeDisplaySnapshot::Builder()
                .SetId(kDisplayIds[1])
                .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                .SetNativeMode(MakeDisplayMode(1920, 1200, true, 60.0))
                .AddMode(MakeDisplayMode(1920, 1200, true, 60.0))  // same AR
                .AddMode(MakeDisplayMode(1920, 1080, true, 60.0))
                .AddMode(MakeDisplayMode(1680, 1050, false, 60.0))  // same AR
                .AddMode(MakeDisplayMode(1440, 900, true, 60.0))    // same AR
                .AddMode(MakeDisplayMode(500, 500, false, 60.0))
                .Build());
  SetOutput(2,
            FakeDisplaySnapshot::Builder()
                .SetId(kDisplayIds[2])
                .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                .SetNativeMode(MakeDisplayMode(1920, 1200, false, 60.0))
                .AddMode(MakeDisplayMode(1920, 1200, false, 60.0))  // same AR
                .AddMode(MakeDisplayMode(1920, 1080, true, 60.0))
                .AddMode(MakeDisplayMode(1680, 1050, false, 60.0))  // same AR
                .AddMode(MakeDisplayMode(1440, 900, true, 60.0))    // same AR
                .Build());

  // Find an exactly matching mirror mode while preserving aspect.
  TestHardwareMirrorModeExist(MakeDisplayMode(1440, 900, true, 60.0));

  // Find an exactly matching mirror mode while not preserving aspect.
  SetOutput(2,
            FakeDisplaySnapshot::Builder()
                .SetId(kDisplayIds[2])
                .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                .SetNativeMode(MakeDisplayMode(1920, 1200, false, 60.0))
                .AddMode(MakeDisplayMode(1920, 1200, false, 60.0))  // same AR
                .AddMode(MakeDisplayMode(1920, 1080, true, 60.0))
                .Build());
  TestHardwareMirrorModeExist(MakeDisplayMode(1920, 1080, true, 60.0));

  // Cannot find a matching mirror mode, so enable software mirroring.
  SetOutput(2,
            FakeDisplaySnapshot::Builder()
                .SetId(kDisplayIds[2])
                .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                .SetNativeMode(MakeDisplayMode(1920, 1200, false, 60.0))
                .AddMode(MakeDisplayMode(1920, 1200, false, 60.0))  // same AR
                .AddMode(MakeDisplayMode(500, 500, true, 60.0))
                .Build());
  TestHardwareMirrorModeNotExist();
}

TEST_F(DisplayConfiguratorMultiMirroringTest,
       FindMirrorModeWithoutInternalDisplay) {
  // Initialize with 3 external displays.
  SetOutput(0, FakeDisplaySnapshot::Builder()
                   .SetId(kDisplayIds[0])
                   .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                   .SetNativeMode(MakeDisplayMode(1920, 1200, true, 60.0))
                   .AddMode(MakeDisplayMode(1920, 1200, true, 60.0))  // same AR
                   .AddMode(MakeDisplayMode(1920, 1080, false, 60.0))
                   .AddMode(MakeDisplayMode(1680, 1050, true, 60.0))  // same AR
                   .Build());
  SetOutput(1,
            FakeDisplaySnapshot::Builder()
                .SetId(kDisplayIds[1])
                .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                .SetNativeMode(MakeDisplayMode(1920, 1200, false, 60.0))
                .AddMode(MakeDisplayMode(1920, 1200, false, 60.0))  // same AR
                .AddMode(MakeDisplayMode(1920, 1080, false, 60.0))
                .AddMode(MakeDisplayMode(1680, 1050, true, 60.0))  // same AR
                .Build());
  SetOutput(2,
            FakeDisplaySnapshot::Builder()
                .SetId(kDisplayIds[2])
                .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                .SetNativeMode(MakeDisplayMode(1920, 1200, false, 60.0))
                .AddMode(MakeDisplayMode(1920, 1200, false, 60.0))  // same AR
                .AddMode(MakeDisplayMode(1920, 1080, false, 60.0))
                .AddMode(MakeDisplayMode(1680, 1050, true, 60.0))  // same AR
                .Build());

  // Find an exactly matching mirror mode while preserving aspect.
  TestHardwareMirrorModeExist(MakeDisplayMode(1680, 1050, true, 60.0));

  // Find an exactly matching mirror mode while not preserving aspect.
  SetOutput(2,
            FakeDisplaySnapshot::Builder()
                .SetId(kDisplayIds[2])
                .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                .SetNativeMode(MakeDisplayMode(1920, 1600, false, 60.0))
                .AddMode(MakeDisplayMode(1920, 1600, false, 60.0))  // same AR
                .AddMode(MakeDisplayMode(1920, 1200, false, 60.0))
                .AddMode(MakeDisplayMode(1920, 1080, false, 60.0))
                .Build());
  TestHardwareMirrorModeExist(MakeDisplayMode(1920, 1080, false, 60.0));

  // Cannot find a matching mirror mode, so enable software mirroring.
  SetOutput(2,
            FakeDisplaySnapshot::Builder()
                .SetId(kDisplayIds[2])
                .SetType(DISPLAY_CONNECTION_TYPE_HDMI)
                .SetNativeMode(MakeDisplayMode(1920, 1600, false, 60.0))
                .AddMode(MakeDisplayMode(1920, 1600, false, 60.0))  // same AR
                .AddMode(MakeDisplayMode(1920, 1200, false, 60.0))
                .Build());
  TestHardwareMirrorModeNotExist();
}

}  // namespace display::test
