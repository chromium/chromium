// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ozone/evdev/gamepad_event_converter_evdev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/event.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_converter_test_util.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/gamepad/gamepad_event.h"
#include "ui/events/ozone/gamepad/gamepad_observer.h"
#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/test/scoped_event_test_tick_clock.h"

namespace {

const char kTestDevicePath[] = "/dev/input/test-device";

constexpr char kXboxGamepadLogDescription[] =
    R"(class=ui::GamepadEventConverterEvdev id=1
 supports_rumble=1
base class=ui::EventConverterEvdev id=1
 path="/dev/input/test-device"
member class=ui::InputDevice id=1
 input_device_type=ui::InputDeviceType::INPUT_DEVICE_USB
 name="Microsoft X-Box 360 pad"
 phys=""
 enabled=0
 suspected_keyboard_imposter=0
 suspected_mouse_imposter=0
 sys_path=""
 vendor_id=045E
 product_id=028E
 version=0114
)";

class TestGamepadObserver : public ui::GamepadObserver {
 public:
  TestGamepadObserver() {
    ui::GamepadProviderOzone::GetInstance()->AddGamepadObserver(this);
  }

  ~TestGamepadObserver() override {
    ui::GamepadProviderOzone::GetInstance()->RemoveGamepadObserver(this);
  }
  void OnGamepadEvent(const ui::GamepadEvent& event) override {
    events.push_back(event);
  }

  std::vector<ui::GamepadEvent> events;
};

}  // namespace

namespace ui {

class TestGamepadEventConverterEvdev : public ui::GamepadEventConverterEvdev {
 public:
  TestGamepadEventConverterEvdev(base::ScopedFD fd,
                                 base::FilePath path,
                                 int id,
                                 const EventDeviceInfo& info,
                                 DeviceEventDispatcherEvdev* dispatcher)
      : GamepadEventConverterEvdev(std::move(fd), path, id, info, dispatcher) {}

  int UploadFfEffect(const base::ScopedFD& fd,
                     struct ff_effect* effect) override {
    uploaded_ff_effects_.push_back(*effect);
    return kEffectId;
  }

  ssize_t WriteEvent(const base::ScopedFD& fd,
                     const struct input_event& event) override {
    written_input_events_.push_back(event);
    return 0;
  }

  int kEffectId = 0;
  std::vector<ff_effect> uploaded_ff_effects_;
  std::vector<input_event> written_input_events_;
};

class GamepadEventConverterEvdevTest : public testing::Test {
 public:
  GamepadEventConverterEvdevTest() {}

  GamepadEventConverterEvdevTest(const GamepadEventConverterEvdevTest&) =
      delete;
  GamepadEventConverterEvdevTest& operator=(
      const GamepadEventConverterEvdevTest&) = delete;

  // Overriden from testing::Test:
  void SetUp() override {
    device_manager_ = ui::CreateDeviceManagerForTest();
    keyboard_layout_engine_ = std::make_unique<ui::StubKeyboardLayoutEngine>();
    event_factory_ = ui::CreateEventFactoryEvdevForTest(
        nullptr, device_manager_.get(), keyboard_layout_engine_.get(),
        base::BindRepeating(
            &GamepadEventConverterEvdevTest::DispatchEventForTest,
            base::Unretained(this)));
    dispatcher_ =
        ui::CreateDeviceEventDispatcherEvdevForTest(event_factory_.get());
  }

  std::unique_ptr<ui::TestGamepadEventConverterEvdev> CreateDevice(
      const ui::DeviceCapabilities& caps) {
    int evdev_io[2];
    if (pipe(evdev_io))
      PLOG(FATAL) << "failed pipe";
    base::ScopedFD events_in(evdev_io[0]);
    base::ScopedFD events_out(evdev_io[1]);

    ui::EventDeviceInfo devinfo;
    CapabilitiesToDeviceInfo(caps, &devinfo);
    return std::make_unique<ui::TestGamepadEventConverterEvdev>(
        std::move(events_in), base::FilePath(kTestDevicePath), 1, devinfo,
        dispatcher_.get());
  }

 private:
  void DispatchEventForTest(ui::Event* event) {}

  std::unique_ptr<ui::TestGamepadEventConverterEvdev> gamepad_evdev_;
  std::unique_ptr<ui::DeviceManager> device_manager_;
  std::unique_ptr<ui::KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<ui::EventFactoryEvdev> event_factory_;
  std::unique_ptr<ui::DeviceEventDispatcherEvdev> dispatcher_;
};

struct ExpectedEvent {
  GamepadEventType type;
  uint16_t code;
  double value;
};

struct ExpectedVibrationEvent {
  uint16_t code;
  double value;
};

struct ExpectedVibrationEffect {
  uint16_t duration;
  double strong_magnitude;
  double weak_magnitude;
};

TEST_F(GamepadEventConverterEvdevTest, XboxGamepadEvents) {
  TestGamepadObserver observer;
  std::unique_ptr<ui::TestGamepadEventConverterEvdev> dev =
      CreateDevice(kXboxGamepad);

  struct input_event mock_kernel_queue[] = {
      {{1493076826, 766851}, EV_ABS, 0, 19105},
      {{1493076826, 766851}, EV_SYN, SYN_REPORT},
      {{1493076826, 774849}, EV_ABS, 0, 17931},
      {{1493076826, 774849}, EV_SYN, SYN_REPORT},
      {{1493076826, 782849}, EV_ABS, 0, 17398},
      {{1493076826, 782849}, EV_SYN, SYN_REPORT},
      {{1493076829, 670856}, EV_MSC, 4, 90007},
      {{1493076829, 670856}, EV_KEY, 310, 1},
      {{1493076829, 670856}, EV_SYN, SYN_REPORT},
      {{1493076829, 870828}, EV_MSC, 4, 90007},
      {{1493076829, 870828}, EV_KEY, 310, 0},
      {{1493076829, 870828}, EV_SYN, SYN_REPORT},
      {{1493076830, 974859}, EV_MSC, 4, 90005},
      {{1493076830, 974859}, EV_KEY, 308, 1},
      {{1493076830, 974859}, EV_SYN, SYN_REPORT},
      {{1493076831, 158857}, EV_MSC, 4, 90005},
      {{1493076831, 158857}, EV_KEY, 308, 0},
      {{1493076831, 158857}, EV_SYN, SYN_REPORT},
      {{1493076832, 62859}, EV_MSC, 4, 90002},
      {{1493076832, 62859}, EV_KEY, 305, 1},
      {{1493076832, 62859}, EV_SYN, SYN_REPORT},
      {{1493076832, 206859}, EV_MSC, 4, 90002},
      {{1493076832, 206859}, EV_KEY, 305, 0},
      {{1493076832, 206859}, EV_SYN, SYN_REPORT},
      {{1493076832, 406860}, EV_MSC, 4, 90003},
      {{1493076832, 406860}, EV_KEY, 306, 1},
      {{1493076832, 406860}, EV_SYN, SYN_REPORT},
      {{1493076832, 526871}, EV_MSC, 4, 90003},
      {{1493076832, 526871}, EV_KEY, 306, 0},
      {{1493076832, 526871}, EV_SYN, SYN_REPORT},
      {{1493076832, 750860}, EV_MSC, 4, 90004},
      {{1493076832, 750860}, EV_KEY, 307, 1},
      {{1493076832, 750860}, EV_SYN, SYN_REPORT}};

  // Advance test tick clock so the above events are strictly in the past.
  ui::test::ScopedEventTestTickClock clock;
  clock.SetNowSeconds(1493076833);

  struct ExpectedEvent expected_events[] = {
      {GamepadEventType::AXIS, 0, 19105}, {GamepadEventType::FRAME, 0, 0},
      {GamepadEventType::AXIS, 0, 17931}, {GamepadEventType::FRAME, 0, 0},
      {GamepadEventType::AXIS, 0, 17398}, {GamepadEventType::FRAME, 0, 0},
      {GamepadEventType::BUTTON, 310, 1}, {GamepadEventType::FRAME, 0, 0},
      {GamepadEventType::BUTTON, 310, 0}, {GamepadEventType::FRAME, 0, 0},
      {GamepadEventType::BUTTON, 308, 1}, {GamepadEventType::FRAME, 0, 0},
      {GamepadEventType::BUTTON, 308, 0}, {GamepadEventType::FRAME, 0, 0},
      {GamepadEventType::BUTTON, 305, 1}, {GamepadEventType::FRAME, 0, 0},
      {GamepadEventType::BUTTON, 305, 0}, {GamepadEventType::FRAME, 0, 0},
      {GamepadEventType::BUTTON, 306, 1}, {GamepadEventType::FRAME, 0, 0},
      {GamepadEventType::BUTTON, 306, 0}, {GamepadEventType::FRAME, 0, 0},
      {GamepadEventType::BUTTON, 307, 1}, {GamepadEventType::FRAME, 0, 0},
  };

  for (unsigned i = 0; i < std::size(mock_kernel_queue); ++i) {
    dev->ProcessEvent(mock_kernel_queue[i]);
  }

  for (unsigned i = 0; i < observer.events.size(); ++i) {
    EXPECT_EQ(expected_events[i].type, observer.events[i].type());
    EXPECT_EQ(expected_events[i].code, observer.events[i].code());
    EXPECT_FLOAT_EQ(expected_events[i].value, observer.events[i].value());
  }
}

TEST_F(GamepadEventConverterEvdevTest, XboxGamepadVibrationEvents) {
  TestGamepadObserver observer;
  std::unique_ptr<ui::TestGamepadEventConverterEvdev> dev =
      CreateDevice(kXboxGamepad);

  struct ExpectedVibrationEvent expected_events[] = {
      {static_cast<uint16_t>(dev->kEffectId), 1},
      {static_cast<uint16_t>(dev->kEffectId), 0}};
  struct ExpectedVibrationEffect expected_effect = {10000, 0x8080, 0x8080};

  dev->PlayVibrationEffect(0x80, 10000);
  EXPECT_EQ(1UL, dev->written_input_events_.size());

  input_event received_vibration_event = dev->written_input_events_[0];
  EXPECT_EQ(expected_events[0].code, received_vibration_event.code);
  EXPECT_EQ(expected_events[0].value, received_vibration_event.value);

  ff_effect received_effect = dev->uploaded_ff_effects_[0];
  EXPECT_EQ(expected_effect.duration, received_effect.replay.length);
  EXPECT_EQ(expected_effect.strong_magnitude,
            received_effect.u.rumble.strong_magnitude);
  EXPECT_EQ(expected_effect.weak_magnitude,
            received_effect.u.rumble.weak_magnitude);

  dev->StopVibration();
  EXPECT_EQ(2UL, dev->written_input_events_.size());
  input_event received_cancel_event = dev->written_input_events_[1];
  EXPECT_EQ(expected_events[1].code, received_cancel_event.code);
  EXPECT_EQ(expected_events[1].value, received_cancel_event.value);
}

TEST_F(GamepadEventConverterEvdevTest, XboxGamepadHasKeys) {
  TestGamepadObserver observer;
  std::unique_ptr<ui::TestGamepadEventConverterEvdev> dev =
      CreateDevice(kXboxGamepad);

  const std::vector<uint64_t> key_bits = dev->GetGamepadKeyBits();

  // BTN_A should be supported.
  EXPECT_TRUE(EvdevBitUint64IsSet(key_bits.data(), 305));
}

TEST_F(GamepadEventConverterEvdevTest, DescribeStateForLog) {
  std::unique_ptr<ui::TestGamepadEventConverterEvdev> dev =
      CreateDevice(kXboxGamepad);

  std::stringstream output;
  dev->DescribeForLog(output);

  EXPECT_EQ(output.str(), kXboxGamepadLogDescription);
}

}  // namespace ui
