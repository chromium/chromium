// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_factory_evdev.h"

#include <map>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_converter_test_util.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/input_controller_evdev.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev_metrics.h"
#include "ui/events/ozone/evdev/input_device_opener.h"
#include "ui/events/ozone/features.h"

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::HasSubstr;

namespace ui {

namespace {

enum DeviceForm : uint32_t {
  KEYBOARD = 1 << 0,
  MOUSE = 1 << 1,
  POINTING_STICK = 1 << 2,
  TOUCHPAD = 1 << 3,
  HAPTIC_TOUCHPAD = 1 << 4,
  TOUCHSCREEN = 1 << 5,
  PEN = 1 << 6,
  GAMEPAD = 1 << 7,
  CAPS_LOCK_LED = 1 << 8,
  STYLUS_SWITCH = 1 << 9,
};

// Fragment of DescribeForLog() information for evdev converters that should
// appear once per device.
constexpr char kDescriptionLogInputDeviceHeader[] = "class=ui::InputDevice";

constexpr char kKeyboardImposterIsTrue[] = "suspected_keyboard_imposter=1";
constexpr char kKeyboardImposterIsFalse[] = "suspected_keyboard_imposter=0";
constexpr char kMouseImposterIsTrue[] = "suspected_mouse_imposter=1";
constexpr char kMouseImposterIsFalse[] = "suspected_mouse_imposter=0";

// Splits multi-line block of text into array of strings.
std::vector<std::string> SplitLines(const std::string& param) {
  return base::SplitString(param, "\n", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

class FakeEventConverterEvdev : public EventConverterEvdev {
 public:
  explicit FakeEventConverterEvdev(int fd,
                                   base::FilePath path,
                                   int id,
                                   InputDeviceType type,
                                   const std::string& name,
                                   const std::string& phys,
                                   uint16_t vendor_id,
                                   uint16_t product_id,
                                   uint16_t version,
                                   uint32_t device_form)
      : EventConverterEvdev(fd,
                            path,
                            id,
                            type,
                            name,
                            phys,
                            vendor_id,
                            product_id,
                            version),
        device_form_(device_form) {}

  bool HasKeyboard() const override {
    return device_form_ & DeviceForm::KEYBOARD;
  }
  bool HasMouse() const override { return device_form_ & DeviceForm::MOUSE; }
  bool HasPointingStick() const override {
    return device_form_ & DeviceForm::POINTING_STICK;
  }
  bool HasTouchpad() const override {
    return device_form_ & DeviceForm::TOUCHPAD;
  }
  bool HasHapticTouchpad() const override {
    return device_form_ & DeviceForm::HAPTIC_TOUCHPAD;
  }
  bool HasTouchscreen() const override {
    return device_form_ & DeviceForm::TOUCHSCREEN;
  }
  bool HasPen() const override { return device_form_ & DeviceForm::PEN; }
  bool HasGamepad() const override {
    return device_form_ & DeviceForm::GAMEPAD;
  }
  bool HasCapsLockLed() const override {
    return device_form_ & DeviceForm::CAPS_LOCK_LED;
  }
  bool HasStylusSwitch() const override {
    return device_form_ & DeviceForm::STYLUS_SWITCH;
  }

  void OnFileCanReadWithoutBlocking(int fd) override {}
  void SetKeyFilter(bool enable_filter,
                    std::vector<DomCode> allowed_keys) override {}

 private:
  uint32_t device_form_;
};

class StubDeviceEventDispatcherEvdev : public DeviceEventDispatcherEvdev {
 public:
  explicit StubDeviceEventDispatcherEvdev(
      base::RepeatingCallback<void(const std::vector<KeyboardDevice>& devices)>
          keyboard_callback,
      base::RepeatingCallback<void(const std::vector<InputDevice>& devices)>
          mouse_callback)
      : keyboard_callback_(keyboard_callback),
        mouse_callback_(mouse_callback) {}
  ~StubDeviceEventDispatcherEvdev() override = default;

  void DispatchKeyEvent(const KeyEventParams& params) override {}
  void DispatchMouseMoveEvent(const MouseMoveEventParams& params) override {}
  void DispatchMouseButtonEvent(const MouseButtonEventParams& params) override {
  }
  void DispatchMouseWheelEvent(const MouseWheelEventParams& params) override {}
  void DispatchPinchEvent(const PinchEventParams& params) override {}
  void DispatchScrollEvent(const ScrollEventParams& params) override {}
  void DispatchTouchEvent(const TouchEventParams& params) override {}
  void DispatchMicrophoneMuteSwitchValueChanged(bool muted) override {}

  void DispatchKeyboardDevicesUpdated(
      const std::vector<KeyboardDevice>& devices,
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {
    keyboard_callback_.Run(devices);
  }
  void DispatchTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices) override {}
  void DispatchMouseDevicesUpdated(const std::vector<InputDevice>& devices,
                                   bool has_mouse) override {
    mouse_callback_.Run(devices);
  }
  void DispatchPointingStickDevicesUpdated(
      const std::vector<InputDevice>& devices) override {}
  void DispatchTouchpadDevicesUpdated(
      const std::vector<TouchpadDevice>& devices,
      bool has_haptic_touchpad) override {}
  void DispatchGraphicsTabletDevicesUpdated(
      const std::vector<InputDevice>& devices) override {}
  void DispatchUncategorizedDevicesUpdated(
      const std::vector<InputDevice>& devices) override {}
  void DispatchDeviceListsComplete() override {}
  void DispatchStylusStateChanged(StylusState stylus_state) override {}
  void DispatchAnyKeysPressedUpdated(bool any) override {}

  void DispatchGamepadEvent(const GamepadEvent& event) override {}

  void DispatchGamepadDevicesUpdated(
      const std::vector<GamepadDevice>& devices,
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}

 private:
  base::RepeatingCallback<void(const std::vector<KeyboardDevice>& devices)>
      keyboard_callback_;
  base::RepeatingCallback<void(const std::vector<InputDevice>& devices)>
      mouse_callback_;
};

class FakeInputDeviceOpenerEvdev : public InputDeviceOpener {
 public:
  explicit FakeInputDeviceOpenerEvdev(
      std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters)
      : converters_(std::move(converters)) {}

  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters_;

  std::unique_ptr<EventConverterEvdev> OpenInputDevice(
      const OpenInputDeviceParams& params) override {
    std::unique_ptr<FakeEventConverterEvdev> converter =
        std::move(converters_[0]);
    converters_.erase(converters_.begin());
    return std::move(converter);
  }
};

class InputDeviceFactoryEvdevTest : public testing::Test {
 public:
  InputDeviceFactoryEvdevTest() = default;

  // ::testing::Test:
  void SetUp() override {
    dispatcher_ = std::make_unique<StubDeviceEventDispatcherEvdev>(
        base::BindRepeating(
            &InputDeviceFactoryEvdevTest::OnKeyboardDevicesRetrieved,
            base::Unretained(this)),
        base::BindRepeating(
            &InputDeviceFactoryEvdevTest::OnMouseDevicesRetrieved,
            base::Unretained(this)));
  }

 protected:
  // Synchronously invoke DescribeForLog on an input device factory.
  void RunDescribeForLog(InputDeviceFactoryEvdev* input_device_factory) {
    input_device_factory->DescribeForLog(
        base::BindOnce(&InputDeviceFactoryEvdevTest::OnDescribeForLogComplete,
                       base::Unretained(this)));
    // Start the loop, the describe callback will exit.
    fetch_run_loop_.Run();
  }

  void OnDescribeForLogComplete(const std::string& response) {
    log_response_ = response;
    fetch_run_loop_.Quit();
  }

  const std::string& GetLogResponse() const { return log_response_; }

  void OnKeyboardDevicesRetrieved(const std::vector<KeyboardDevice>& devices) {
    keyboards_ = devices;
  }

  void OnMouseDevicesRetrieved(const std::vector<InputDevice>& devices) {
    mice_ = devices;
  }

  int GetReadFdForDevice(int device_id) {
    if (!pipe_fds_.contains(device_id)) {
      base::ScopedFD read_fd, write_fd;
      CHECK(base::CreatePipe(&read_fd, &write_fd));
      pipe_fds_.emplace(
          device_id, std::make_pair(std::move(read_fd), std::move(write_fd)));
    }
    return pipe_fds_[device_id].first.get();
  }

  std::vector<KeyboardDevice> keyboards_;
  std::vector<InputDevice> mice_;
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ui::DeviceEventDispatcherEvdev> dispatcher_;
  InputControllerEvdev input_controller_{nullptr, nullptr, nullptr};
  base::HistogramTester histogram_tester_;

  // Maps device IDs to their corresponding pipe file descriptors.
  std::map<int, std::pair<base::ScopedFD, base::ScopedFD>> pipe_fds_;

 private:
  // Stores results from the log source passed into
  // FetchDescribeForLogCallback().
  std::string log_response_;
  // Used for waiting on fetch results.
  base::RunLoop fetch_run_loop_;
};

TEST_F(InputDeviceFactoryEvdevTest,
       AttachSingularKeyboardFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_USB, "name", "phys_path", 1, 1, 1,
          DeviceForm::KEYBOARD);
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest, AttachSingularMouse) {
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "name", "phys_path", 1, 1, 1,
          DeviceForm::MOUSE);

  converters.push_back(std::move(mouse_converter));
  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(0));
  EXPECT_EQ(mice_.size(), std::size_t(1));
  EXPECT_FALSE(mice_.front().suspected_mouse_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardDifferentPhysFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name",
          "phys_path/mouse", 1, 1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_USB, "keyboard_name",
          "phys_path/keyboard", 2, 2, 2, DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardSameUSBTopologyFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name",
          "usb-0000:00:14.0-9/input0", 1, 1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_USB, "keyboard_name",
          "usb-0000:00:14.0-9/input1", 2, 2, 2, DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_TRUE(keyboards_.front().suspected_keyboard_imposter);
  EXPECT_FALSE(keyboards_.front().suspected_mouse_imposter);
}

TEST_F(
    InputDeviceFactoryEvdevTest,
    AttachMouseAndInternalKeyboardSameUSBTopologyFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name",
          "usb-0000:00:14.0-9/input0", 1, 1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard_name",
          "usb-0000:00:14.0-9/input1", 2, 2, 2, DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_keyboard_imposter);
  EXPECT_FALSE(keyboards_.front().suspected_mouse_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardSameUSBTopologyFakeMouseHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeMouseHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name",
          "usb-0000:00:14.0-9/input0", 1, 1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_USB, "keyboard_name",
          "usb-0000:00:14.0-9/input1", 2, 2, 2, DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(mice_.size(), std::size_t(1));
  EXPECT_TRUE(mice_.front().suspected_mouse_imposter);
  EXPECT_FALSE(mice_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardDifferentUSBTopologyFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name",
          "usb-0000:00:9.0-1/input0", 1, 1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_USB, "keyboard_name",
          "usb-0000:00:14.0-9/input0", 2, 2, 2, DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardDifferentUSBTopologyFakeMouseHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeMouseHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name",
          "usb-0000:00:9.0-1/input0", 1, 1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_USB, "keyboard_name",
          "usb-0000:00:14.0-9/input0", 2, 2, 2, DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(mice_.size(), std::size_t(1));
  EXPECT_FALSE(mice_.front().suspected_mouse_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardSamePhysFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name", "phys_path", 1,
          1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_USB, "keyboard_name", "phys_path", 2, 2,
          2, DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_TRUE(keyboards_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachSingularKeyboardAndMouseFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> keyboard_and_mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_USB, "name", "phys_path", 1, 1, 1,
          DeviceForm::MOUSE | DeviceForm::KEYBOARD);

  converters.push_back(std::move(keyboard_and_mouse_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_TRUE(keyboards_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachSingularKeyboardAndMouseFakeMouseHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeMouseHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> keyboard_and_mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_USB, "name", "phys_path", 1, 1, 1,
          DeviceForm::MOUSE | DeviceForm::KEYBOARD);

  converters.push_back(std::move(keyboard_and_mouse_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(mice_.size(), std::size_t(1));
  EXPECT_TRUE(mice_.front().suspected_mouse_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachSingularKeyboardFakeKeyboardHeuristicDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_USB, "name", "phys_path", 1, 1, 1,
          DeviceForm::KEYBOARD);

  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardDifferentPhysFakeKeyboardHeuristicDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name",
          "phys_path/mouse", 1, 1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_USB, "keyboard_name",
          "phys_path/keyboard", 2, 2, 2, DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardSameUSBTopologyFakeKeyboardHeuristicDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name",
          "usb-0000:00:14.0-9/input0", 1, 1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_USB, "keyboard_name",
          "usb-0000:00:14.0-9/input1", 2, 2, 2, DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardSamePhysFakeKeyboardHeuristicDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name", "phys_path", 1,
          1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_USB, "keyboard_name", "phys_path", 2, 2,
          2, DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachSingularKeyboardAndMouseFakeKeyboardHeuristicDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> keyboard_and_mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_USB, "name", "phys_path", 1, 1, 1,
          DeviceForm::MOUSE | DeviceForm::KEYBOARD);

  converters.push_back(std::move(keyboard_and_mouse_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachSingularKeyboardAndMouseFakeMouseHeuristicDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeMouseHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> keyboard_and_mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_USB, "name", "phys_path", 1, 1, 1,
          DeviceForm::MOUSE | DeviceForm::KEYBOARD);

  converters.push_back(std::move(keyboard_and_mouse_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(mice_.size(), std::size_t(1));
  EXPECT_FALSE(mice_.front().suspected_mouse_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachAndRemoveDeviceFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;
  base::FilePath mouse_path("mouse_path");

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), mouse_path, 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name", "phys_path", 1,
          1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_USB, "keyboard_name", "phys_path", 2, 2,
          2, DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_TRUE(keyboards_.front().suspected_keyboard_imposter);

  input_device_factory_->RemoveInputDevice(mouse_path);
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_keyboard_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachInternalKeyboardTriggersMetricLogging) {
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "name", "phys_path", 1, 1, 1,
          DeviceForm::KEYBOARD);
  converters.push_back(std::move(keyboard_converter));
  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  histogram_tester_.ExpectUniqueSample(kKeyboardAttachmentTypeHistogramName,
                                       AttachmentType::kInternal, 1);
  histogram_tester_.ExpectUniqueSample(kInternalAttachmentFormHistogramName,
                                       AttachmentForm::kKeyboard, 1);
}

TEST_F(InputDeviceFactoryEvdevTest, AttachUSBKeyboardTriggersMetricLogging) {
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_USB, "name", "phys_path", 1, 1, 1,
          DeviceForm::KEYBOARD);
  converters.push_back(std::move(keyboard_converter));
  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  histogram_tester_.ExpectUniqueSample(kKeyboardAttachmentTypeHistogramName,
                                       AttachmentType::kUsb, 1);
  histogram_tester_.ExpectUniqueSample(kUsbAttachmentFormHistogramName,
                                       AttachmentForm::kKeyboard, 1);
}

TEST_F(InputDeviceFactoryEvdevTest, AttachBluetoothMouseTriggersMetricLogging) {
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_BLUETOOTH, "name", "phys_path", 1, 1, 1,
          DeviceForm::MOUSE);
  converters.push_back(std::move(mouse_converter));
  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  histogram_tester_.ExpectUniqueSample(kMouseAttachmentTypeHistogramName,
                                       AttachmentType::kBluetooth, 1);
  histogram_tester_.ExpectUniqueSample(kBluetoothAttachmentFormHistogramName,
                                       AttachmentForm::kMouse, 1);
}

// Following tests exercise the factory's DescribeForLog() facility, where
// we need to mock a variety of devices.

TEST_F(InputDeviceFactoryEvdevTest, DescribeForLogSingularMouse) {
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "name", "phys_path", 1, 1, 1,
          DeviceForm::MOUSE);

  converters.push_back(std::move(mouse_converter));
  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  RunDescribeForLog(input_device_factory_.get());

  std::string response = GetLogResponse();

  EXPECT_NE(response, "");

  auto lines = SplitLines(response);

  EXPECT_THAT(lines,
              Contains(HasSubstr(kDescriptionLogInputDeviceHeader)).Times(1));
  EXPECT_THAT(lines, Contains(HasSubstr(kMouseImposterIsFalse)));
}

TEST_F(InputDeviceFactoryEvdevTest, DescribeForLogAttachInternalKeyboard) {
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "name", "phys_path", 1, 1, 1,
          DeviceForm::KEYBOARD);
  converters.push_back(std::move(keyboard_converter));
  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  RunDescribeForLog(input_device_factory_.get());

  std::string response = GetLogResponse();

  EXPECT_NE(response, "");

  auto lines = SplitLines(response);

  EXPECT_THAT(lines,
              Contains(HasSubstr(kDescriptionLogInputDeviceHeader)).Times(1));
  EXPECT_THAT(lines, Contains(HasSubstr(kKeyboardImposterIsFalse)));
}

TEST_F(
    InputDeviceFactoryEvdevTest,
    DescribeForLogAttachMouseAndKeyboardSamePhysFakeKeyboardHeuristicDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name", "phys_path", 1,
          1, 1, DeviceForm::MOUSE);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(2), base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_USB, "keyboard_name", "phys_path", 2, 2,
          2, DeviceForm::KEYBOARD);
  std::unique_ptr<FakeEventConverterEvdev> touchpad_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(3), base::FilePath("touchpad_path"), 3,
          InputDeviceType::INPUT_DEVICE_USB, "touchpad_name", "phys_path", 3, 3,
          3, DeviceForm::TOUCHPAD);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));
  converters.push_back(std::move(touchpad_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(3, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  RunDescribeForLog(input_device_factory_.get());

  std::string response = GetLogResponse();

  EXPECT_NE(response, "");

  auto lines = SplitLines(response);

  EXPECT_THAT(lines,
              Contains(HasSubstr(kDescriptionLogInputDeviceHeader)).Times(3));
  EXPECT_THAT(lines, Contains(HasSubstr(kKeyboardImposterIsFalse)).Times(3));
}

TEST_F(InputDeviceFactoryEvdevTest, DescribeForLogOneDeviceMouseAndKeyboard) {
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_USB, "name", "phys_path", 1, 1, 1,
          DeviceForm::MOUSE | DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  RunDescribeForLog(input_device_factory_.get());

  std::string response = GetLogResponse();

  EXPECT_NE(response, "");

  auto lines = SplitLines(response);

  EXPECT_THAT(lines,
              Contains(HasSubstr(kDescriptionLogInputDeviceHeader)).Times(1));
  EXPECT_THAT(lines, Contains(HasSubstr(kKeyboardImposterIsTrue)));
  EXPECT_THAT(lines, Contains(HasSubstr(kMouseImposterIsTrue)));
}

TEST_F(InputDeviceFactoryEvdevTest,
       ImposterCheckerStateCanDisableKeyboardCheckAfterDeviceIsAdded) {
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "name", "phys_path", 1, 1, 1,
          DeviceForm::MOUSE | DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->DisableKeyboardImposterCheck();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  RunDescribeForLog(input_device_factory_.get());

  std::string response = GetLogResponse();

  EXPECT_NE(response, "");

  auto lines = SplitLines(response);

  EXPECT_THAT(lines,
              Contains(HasSubstr(kDescriptionLogInputDeviceHeader)).Times(1));
  EXPECT_THAT(lines, Contains(HasSubstr(kKeyboardImposterIsFalse)));
  EXPECT_THAT(lines, Contains(HasSubstr(kMouseImposterIsTrue)));
}

TEST_F(InputDeviceFactoryEvdevTest,
       ImposterCheckerStateCannotOverrideFakeKeyboardHeuristicFeatureFlag) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;

  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          GetReadFdForDevice(1), base::FilePath("path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "name", "phys_path", 1, 1, 1,
          DeviceForm::MOUSE | DeviceForm::KEYBOARD);

  converters.push_back(std::move(mouse_converter));
  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)),
          &input_controller_);
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->DisableKeyboardImposterCheck();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  RunDescribeForLog(input_device_factory_.get());

  std::string response = GetLogResponse();

  EXPECT_NE(response, "");

  auto lines = SplitLines(response);

  EXPECT_THAT(lines,
              Contains(HasSubstr(kDescriptionLogInputDeviceHeader)).Times(1));
  EXPECT_THAT(lines, Contains(HasSubstr(kKeyboardImposterIsFalse)));
  EXPECT_THAT(lines, Contains(HasSubstr(kMouseImposterIsTrue)));
}

}  // namespace ui
