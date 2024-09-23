// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ozone/evdev/stylus_button_event_converter_evdev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_converter_test_util.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/test/scoped_event_test_tick_clock.h"

namespace {

const char kTestDevicePath[] = "/dev/input/test-device";

constexpr char kDellActivePenButtonLogDescription[] =
    R"(class=ui::StylusButtonEventConverterEvdev id=1
base class=ui::EventConverterEvdev id=1
 path="/dev/input/test-device"
member class=ui::InputDevice id=1
 input_device_type=ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH
 name="Dell Active Pen PN579X"
 phys=""
 enabled=0
 suspected_keyboard_imposter=0
 suspected_mouse_imposter=0
 sys_path=""
 vendor_id=413C
 product_id=81D5
 version=0F08
)";

}  // namespace

namespace ui {

class MockStylusButtonEventConverterEvdev
    : public StylusButtonEventConverterEvdev {
 public:
  MockStylusButtonEventConverterEvdev(base::ScopedFD fd,
                                      base::FilePath path,
                                      const EventDeviceInfo& devinfo,
                                      DeviceEventDispatcherEvdev* dispatcher);

  MockStylusButtonEventConverterEvdev(
      const MockStylusButtonEventConverterEvdev&) = delete;
  MockStylusButtonEventConverterEvdev& operator=(
      const MockStylusButtonEventConverterEvdev&) = delete;

  ~MockStylusButtonEventConverterEvdev() override {}

  void ConfigureReadMock(struct input_event* queue,
                         long read_this_many,
                         long queue_index);

  // Actually dispatch the event reader code.
  void ReadNow() {
    OnFileCanReadWithoutBlocking(read_pipe_);
    base::RunLoop().RunUntilIdle();
  }

 private:
  int read_pipe_;
  int write_pipe_;

  std::vector<std::unique_ptr<Event>> dispatched_events_;
};

MockStylusButtonEventConverterEvdev::MockStylusButtonEventConverterEvdev(
    base::ScopedFD fd,
    base::FilePath path,
    const EventDeviceInfo& devinfo,
    DeviceEventDispatcherEvdev* dispatcher)
    : StylusButtonEventConverterEvdev(std::move(fd),
                                      path,
                                      1,
                                      devinfo,
                                      dispatcher) {
  int fds[2];

  if (pipe(fds))
    PLOG(FATAL) << "failed pipe";

  EXPECT_TRUE(base::SetNonBlocking(fds[0]) || base::SetNonBlocking(fds[1]))
      << "failed to set non-blocking: " << strerror(errno);

  read_pipe_ = fds[0];
  write_pipe_ = fds[1];
}

void MockStylusButtonEventConverterEvdev::ConfigureReadMock(
    struct input_event* queue,
    long read_this_many,
    long queue_index) {
  int nwrite = HANDLE_EINTR(write(write_pipe_, queue + queue_index,
                                  sizeof(struct input_event) * read_this_many));
  DPCHECK(nwrite ==
          static_cast<int>(sizeof(struct input_event) * read_this_many))
      << "write() failed";
}

}  // namespace ui

// Test fixture.
class StylusButtonEventConverterEvdevTest : public testing::Test {
 public:
  StylusButtonEventConverterEvdevTest() {}

  StylusButtonEventConverterEvdevTest(
      const StylusButtonEventConverterEvdevTest&) = delete;
  StylusButtonEventConverterEvdevTest& operator=(
      const StylusButtonEventConverterEvdevTest&) = delete;

  // Overridden from testing::Test:
  void SetUp() override {
    device_manager_ = ui::CreateDeviceManagerForTest();
    keyboard_layout_engine_ = std::make_unique<ui::StubKeyboardLayoutEngine>();
    event_factory_ = ui::CreateEventFactoryEvdevForTest(
        nullptr, device_manager_.get(), keyboard_layout_engine_.get(),
        base::BindRepeating(
            &StylusButtonEventConverterEvdevTest::DispatchEventForTest,
            base::Unretained(this)));
    dispatcher_ =
        ui::CreateDeviceEventDispatcherEvdevForTest(event_factory_.get());
  }

  ui::MockStylusButtonEventConverterEvdev* CreateDevice(
      const ui::DeviceCapabilities& caps) {
    int evdev_io[2];
    if (pipe(evdev_io))
      PLOG(FATAL) << "failed pipe";
    base::ScopedFD events_in(evdev_io[0]);
    events_out_.reset(evdev_io[1]);

    ui::EventDeviceInfo devinfo;
    CapabilitiesToDeviceInfo(caps, &devinfo);
    return new ui::MockStylusButtonEventConverterEvdev(
        std::move(events_in), base::FilePath(kTestDevicePath), devinfo,
        dispatcher_.get());
  }

  unsigned size() { return dispatched_events_.size(); }
  ui::KeyEvent* dispatched_event(unsigned index) {
    DCHECK_GT(dispatched_events_.size(), index);
    ui::Event* ev = dispatched_events_[index].get();
    DCHECK(ev->IsKeyEvent());
    return ev->AsKeyEvent();
  }

  void DispatchEventForTest(ui::Event* event) {
    std::unique_ptr<ui::Event> cloned_event = event->Clone();
    dispatched_events_.push_back(std::move(cloned_event));
  }

 private:
  std::unique_ptr<ui::DeviceManager> device_manager_;
  std::unique_ptr<ui::KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<ui::EventFactoryEvdev> event_factory_;
  std::unique_ptr<ui::DeviceEventDispatcherEvdev> dispatcher_;

  std::vector<std::unique_ptr<ui::Event>> dispatched_events_;

  base::ScopedFD events_out_;
};

TEST_F(StylusButtonEventConverterEvdevTest, DellActivePenSingleClick) {
  std::unique_ptr<ui::MockStylusButtonEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(ui::kDellActivePenButton));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e3},
      {{0, 0}, EV_KEY, KEY_LEFTMETA, 1},
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7006f},
      {{0, 0}, EV_KEY, KEY_F20, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7006f},
      {{0, 0}, EV_KEY, KEY_F20, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e3},
      {{0, 0}, EV_KEY, KEY_LEFTMETA, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  for (unsigned i = 0; i < std::size(mock_kernel_queue); ++i) {
    dev->ProcessEvent(mock_kernel_queue[i]);
  }
  EXPECT_EQ(0u, size());
}

TEST_F(StylusButtonEventConverterEvdevTest, DellActivePenDoubleClick) {
  std::unique_ptr<ui::MockStylusButtonEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(ui::kDellActivePenButton));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e3},
      {{0, 0}, EV_KEY, KEY_LEFTMETA, 1},
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7006e},
      {{0, 0}, EV_KEY, KEY_F19, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7006e},
      {{0, 0}, EV_KEY, KEY_F19, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e3},
      {{0, 0}, EV_KEY, KEY_LEFTMETA, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  for (unsigned i = 0; i < std::size(mock_kernel_queue); ++i) {
    dev->ProcessEvent(mock_kernel_queue[i]);
  }
  EXPECT_EQ(2u, size());

  ui::KeyEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::EventType::kKeyPressed, event->type());
  EXPECT_TRUE(event->flags() & ui::EF_IS_STYLUS_BUTTON);

  event = dispatched_event(1);
  EXPECT_EQ(ui::EventType::kKeyReleased, event->type());
  EXPECT_TRUE(event->flags() & ui::EF_IS_STYLUS_BUTTON);
}

TEST_F(StylusButtonEventConverterEvdevTest, DellActivePenLongPress) {
  std::unique_ptr<ui::MockStylusButtonEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(ui::kDellActivePenButton));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e3},
      {{0, 0}, EV_KEY, KEY_LEFTMETA, 1},
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7006d},
      {{0, 0}, EV_KEY, KEY_F18, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7006d},
      {{0, 0}, EV_KEY, KEY_F18, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e3},
      {{0, 0}, EV_KEY, KEY_LEFTMETA, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  for (unsigned i = 0; i < std::size(mock_kernel_queue); ++i) {
    dev->ProcessEvent(mock_kernel_queue[i]);
  }
  EXPECT_EQ(0u, size());
}

TEST_F(StylusButtonEventConverterEvdevTest, DescribeStateForLog) {
  std::unique_ptr<ui::MockStylusButtonEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(ui::kDellActivePenButton));

  std::stringstream output;
  dev->DescribeForLog(output);

  EXPECT_EQ(output.str(), kDellActivePenButtonLogDescription);
}
