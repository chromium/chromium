// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/microphone_mute_switch_event_converter_evdev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/scoped_file.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/microphone_mute_switch_monitor.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_converter_test_util.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

namespace {

const char kTestDevicePath[] = "/dev/input/test-device";

constexpr char kPuffMicrophoneMuteSwitchDescription[] =
    R"(class=ui::MicrophoneMuteSwitchEventConverterEvdev id=1
base class=ui::EventConverterEvdev id=1
 path="/dev/input/test-device"
member class=ui::InputDevice id=1
 input_device_type=ui::InputDeviceType::INPUT_DEVICE_INTERNAL
 name="mic_mute_switch"
 phys=""
 enabled=0
 suspected_keyboard_imposter=0
 suspected_mouse_imposter=0
 sys_path=""
 vendor_id=0001
 product_id=0001
 version=0100
)";

class TestMicrophoneMuteObserver
    : public ui::MicrophoneMuteSwitchMonitor::Observer {
 public:
  TestMicrophoneMuteObserver() {
    ui::MicrophoneMuteSwitchMonitor::Get()->AddObserver(this);
  }
  TestMicrophoneMuteObserver(const TestMicrophoneMuteObserver&) = delete;
  TestMicrophoneMuteObserver& operator=(const TestMicrophoneMuteObserver&) =
      delete;
  ~TestMicrophoneMuteObserver() override {
    ui::MicrophoneMuteSwitchMonitor::Get()->RemoveObserver(this);
  }

  void OnMicrophoneMuteSwitchValueChanged(bool muted) override {
    observed_values_.push_back(muted);
  }

  std::vector<bool> GetAndResetObservedValues() {
    std::vector<bool> result;
    result.swap(observed_values_);
    return result;
  }

 private:
  std::vector<bool> observed_values_;
};

}  // namespace

class MicrophoneMuteSwitchEventConverterEvdevTest : public testing::Test {
 public:
  MicrophoneMuteSwitchEventConverterEvdevTest() = default;
  ~MicrophoneMuteSwitchEventConverterEvdevTest() override = default;

  // Overridden from testing::Test:
  void SetUp() override {
    device_manager_ = ui::CreateDeviceManagerForTest();
    keyboard_layout_engine_ = std::make_unique<ui::StubKeyboardLayoutEngine>();
    event_factory_ = ui::CreateEventFactoryEvdevForTest(
        nullptr, device_manager_.get(), keyboard_layout_engine_.get(),
        base::BindRepeating([](ui::Event* event) {
          ADD_FAILURE() << "Unexpected event dispatch " << event->ToString();
        }));
    dispatcher_ =
        ui::CreateDeviceEventDispatcherEvdevForTest(event_factory_.get());
  }
  void TearDown() override {
    // Reset any switch value that might have been changed during the test.
    ui::MicrophoneMuteSwitchMonitor::Get()->SetMicrophoneMuteSwitchValue(false);
  }

  std::unique_ptr<ui::MicrophoneMuteSwitchEventConverterEvdev> CreateDevice(
      const ui::DeviceCapabilities& caps) {
    int evdev_io[2];
    if (pipe(evdev_io))
      PLOG(FATAL) << "failed pipe";
    base::ScopedFD events_in(evdev_io[0]);
    events_out_.reset(evdev_io[1]);

    ui::EventDeviceInfo devinfo;
    CapabilitiesToDeviceInfo(caps, &devinfo);
    // The internal device type is derived from sysfs subsystem symlink on DUT.
    // Unittest doesn't run GetInputDeviceTypeFromPath. Set it here manually.
    devinfo.SetDeviceType(ui::InputDeviceType::INPUT_DEVICE_INTERNAL);
    return std::make_unique<ui::MicrophoneMuteSwitchEventConverterEvdev>(
        std::move(events_in), base::FilePath(kTestDevicePath), 1, devinfo,
        dispatcher_.get());
  }

 private:
  std::unique_ptr<ui::DeviceManager> device_manager_;
  std::unique_ptr<ui::KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<ui::EventFactoryEvdev> event_factory_;
  std::unique_ptr<ui::DeviceEventDispatcherEvdev> dispatcher_;

  std::vector<std::unique_ptr<ui::Event>> dispatched_events_;

  base::ScopedFD events_out_;
};

TEST_F(MicrophoneMuteSwitchEventConverterEvdevTest, MuteChangeEvents) {
  TestMicrophoneMuteObserver test_observer;

  std::unique_ptr<ui::MicrophoneMuteSwitchEventConverterEvdev> dev =
      CreateDevice(ui::kPuffMicrophoneMuteSwitch);

  EXPECT_FALSE(
      ui::MicrophoneMuteSwitchMonitor::Get()->microphone_mute_switch_on());

  struct input_event mock_mute_kernel_queue[] = {
      {{0, 0}, EV_SW, SW_MUTE_DEVICE, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  for (auto& event : mock_mute_kernel_queue)
    dev->ProcessEvent(event);

  EXPECT_TRUE(
      ui::MicrophoneMuteSwitchMonitor::Get()->microphone_mute_switch_on());
  EXPECT_EQ(std::vector<bool>{true}, test_observer.GetAndResetObservedValues());

  struct input_event mock_unmute_kernel_queue[] = {
      {{0, 0}, EV_SW, SW_MUTE_DEVICE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  for (auto& event : mock_unmute_kernel_queue)
    dev->ProcessEvent(event);

  EXPECT_EQ(std::vector<bool>{false},
            test_observer.GetAndResetObservedValues());
}

TEST_F(MicrophoneMuteSwitchEventConverterEvdevTest, DescribeStateForLog) {
  std::unique_ptr<ui::MicrophoneMuteSwitchEventConverterEvdev> dev =
      CreateDevice(ui::kPuffMicrophoneMuteSwitch);

  std::stringstream output;
  dev->DescribeForLog(output);

  EXPECT_EQ(output.str(), kPuffMicrophoneMuteSwitchDescription);
}
