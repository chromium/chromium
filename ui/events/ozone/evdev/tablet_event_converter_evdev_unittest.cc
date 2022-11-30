// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/tablet_event_converter_evdev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
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

const ui::DeviceAbsoluteAxis kWacomIntuos5SPenAbsAxes[] = {
    {ABS_X, {0, 0, 31496, 4, 0, 200}},
    {ABS_Y, {0, 0, 19685, 4, 0, 200}},
    {ABS_Z, {0, -900, 899, 0, 0, 0}},
    {ABS_RZ, {0, -900, 899, 0, 0, 0}},
    {ABS_THROTTLE, {0, -1023, 1023, 0, 0, 0}},
    {ABS_WHEEL, {0, 0, 1023, 0, 0, 0}},
    {ABS_PRESSURE, {0, 0, 2047, 0, 0, 0}},
    {ABS_DISTANCE, {0, 0, 63, 0, 0, 0}},
    {ABS_TILT_X, {0, 0, 127, 0, 0, 0}},
    {ABS_TILT_Y, {0, 0, 127, 0, 0, 0}},
    {ABS_MISC, {0, 0, 0, 0, 0, 0}},
};

const ui::DeviceCapabilities kWacomIntuos5SPen = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:14.0/usb1/"
    "1-1/1-1:1.0/input/input19/event5",
    /* name */ "Wacom Intuos5 touch S Pen",
    /* phys */ "",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "056a",
    /* product */ "0026",
    /* version */ "0107",
    /* prop */ "1",
    /* ev */ "1f",
    /* key */ "1cdf 1f007f 0 0 0 0",
    /* rel */ "100",
    /* abs */ "1000f000167",
    /* msc */ "1",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kWacomIntuos5SPenAbsAxes,
    std::size(kWacomIntuos5SPenAbsAxes),
};

const ui::DeviceAbsoluteAxis EpsonBrightLink1430AbsAxes[] = {
    {ABS_X, {0, 0, 32767, 0, 0, 200}},
    {ABS_Y, {0, 0, 32767, 0, 0, 200}},
    {ABS_Z, {0, 0, 32767, 0, 0, 200}},
    {ABS_RX,{0, 0, 32767, 0, 0, 200}},
    {ABS_PRESSURE, {0, 0, 32767, 0, 0, 0}},
};

const ui::DeviceCapabilities EpsonBrightLink1430 = {
    /* path */
    "/sys/devices/ff580000.usb/usb3/3-1/"
    "3-1.1/3-1.1.3/3-1.1.3:1.1/0003:04B8:061B.0006/"
    "input/input12/event6",
    /* name */ "EPSON EPSON EPSON 1430",
    /* phys */ "USB-ff580000.USB-1.1.3/input1",
    /* uniq */ "2.04",
    /* bustype */ "0003",
    /* vendor */ "04b8",
    /* product */ "061b",
    /* version */ "0200",
    /* prop */ "0",
    /* ev */ "1b",
    /* key */ "c07 30000 0 0 0 0",
    /* rel */ "0",
    /* abs */ "100000f",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    EpsonBrightLink1430AbsAxes,
    std::size(EpsonBrightLink1430AbsAxes),
};

}  // namespace

namespace ui {

class MockTabletEventConverterEvdev : public TabletEventConverterEvdev {
 public:
  MockTabletEventConverterEvdev(base::ScopedFD fd,
                                base::FilePath path,
                                CursorDelegateEvdev* cursor,
                                const EventDeviceInfo& devinfo,
                                DeviceEventDispatcherEvdev* dispatcher);

  MockTabletEventConverterEvdev(const MockTabletEventConverterEvdev&) = delete;
  MockTabletEventConverterEvdev& operator=(
      const MockTabletEventConverterEvdev&) = delete;

  ~MockTabletEventConverterEvdev() override = default;

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

class MockTabletCursorEvdev : public CursorDelegateEvdev {
 public:
  MockTabletCursorEvdev() { cursor_confined_bounds_ = gfx::Rect(1024, 768); }

  MockTabletCursorEvdev(const MockTabletCursorEvdev&) = delete;
  MockTabletCursorEvdev& operator=(const MockTabletCursorEvdev&) = delete;

  ~MockTabletCursorEvdev() override = default;

  // CursorDelegateEvdev:
  void MoveCursorTo(gfx::AcceleratedWidget widget,
                    const gfx::PointF& location) override {
    NOTREACHED();
  }
  void MoveCursorTo(const gfx::PointF& location) override {
    cursor_location_ = location;
  }
  void MoveCursor(const gfx::Vector2dF& delta) override { NOTREACHED(); }
  bool IsCursorVisible() override { return true; }
  gfx::PointF GetLocation() override { return cursor_location_; }
  gfx::Rect GetCursorConfinedBounds() override {
    return cursor_confined_bounds_;
  }
  void InitializeOnEvdev() override {}

 private:
  gfx::PointF cursor_location_;
  gfx::Rect cursor_confined_bounds_;
};

MockTabletEventConverterEvdev::MockTabletEventConverterEvdev(
    base::ScopedFD fd,
    base::FilePath path,
    CursorDelegateEvdev* cursor,
    const EventDeviceInfo& devinfo,
    DeviceEventDispatcherEvdev* dispatcher)
    : TabletEventConverterEvdev(std::move(fd),
                                path,
                                1,
                                cursor,
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

void MockTabletEventConverterEvdev::ConfigureReadMock(struct input_event* queue,
                                                      long read_this_many,
                                                      long queue_index) {
  int nwrite = HANDLE_EINTR(write(write_pipe_, queue + queue_index,
                                  sizeof(struct input_event) * read_this_many));
  DCHECK(nwrite ==
         static_cast<int>(sizeof(struct input_event) * read_this_many))
      << "write() failed, errno: " << errno;
}

}  // namespace ui

// Test fixture.
class TabletEventConverterEvdevTest : public testing::Test {
 public:
  TabletEventConverterEvdevTest() = default;

  TabletEventConverterEvdevTest(const TabletEventConverterEvdevTest&) = delete;
  TabletEventConverterEvdevTest& operator=(
      const TabletEventConverterEvdevTest&) = delete;

  // Overridden from testing::Test:
  void SetUp() override {
    cursor_ = std::make_unique<ui::MockTabletCursorEvdev>();
    device_manager_ = ui::CreateDeviceManagerForTest();
    keyboard_layout_engine_ = std::make_unique<ui::StubKeyboardLayoutEngine>();
    event_factory_ = ui::CreateEventFactoryEvdevForTest(
        cursor_.get(), device_manager_.get(), keyboard_layout_engine_.get(),
        base::BindRepeating(
            &TabletEventConverterEvdevTest::DispatchEventForTest,
            base::Unretained(this)));
    dispatcher_ =
        ui::CreateDeviceEventDispatcherEvdevForTest(event_factory_.get());

    test_clock_ = std::make_unique<ui::test::ScopedEventTestTickClock>();
  }

  void TearDown() override {
    cursor_.reset();
  }

  ui::MockTabletEventConverterEvdev* CreateDevice(
      const ui::DeviceCapabilities& caps) {
    // Set up pipe to satisfy message pump (unused).
    int evdev_io[2];
    if (pipe(evdev_io))
      PLOG(FATAL) << "failed pipe";
    base::ScopedFD events_in(evdev_io[0]);
    events_out_.reset(evdev_io[1]);

    ui::EventDeviceInfo devinfo;
    CapabilitiesToDeviceInfo(caps, &devinfo);
    return new ui::MockTabletEventConverterEvdev(
        std::move(events_in), base::FilePath(kTestDevicePath), cursor_.get(),
        devinfo, dispatcher_.get());
  }

  ui::CursorDelegateEvdev* cursor() { return cursor_.get(); }

  unsigned size() { return dispatched_events_.size(); }
  ui::MouseEvent* dispatched_event(unsigned index) {
    DCHECK_GT(dispatched_events_.size(), index);
    ui::Event* ev = dispatched_events_[index].get();
    DCHECK(ev->IsMouseEvent());
    return ev->AsMouseEvent();
  }

  void DispatchEventForTest(ui::Event* event) {
    std::unique_ptr<ui::Event> cloned_event = event->Clone();
    dispatched_events_.push_back(std::move(cloned_event));
  }

 private:
  std::unique_ptr<ui::MockTabletCursorEvdev> cursor_;
  std::unique_ptr<ui::DeviceManager> device_manager_;
  std::unique_ptr<ui::KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<ui::EventFactoryEvdev> event_factory_;
  std::unique_ptr<ui::DeviceEventDispatcherEvdev> dispatcher_;
  std::unique_ptr<ui::test::ScopedEventTestTickClock> test_clock_;

  std::vector<std::unique_ptr<ui::Event>> dispatched_events_;

  base::ScopedFD events_out_;
};

#define EPSILON 20

// Uses real data captured from Wacom Intuos 5 Pen
TEST_F(TabletEventConverterEvdevTest, MoveTopLeft) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_X, 477},
      {{0, 0}, EV_ABS, ABS_TILT_X, 66},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 62},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  ASSERT_EQ(1u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_LT(cursor()->GetLocation().x(), EPSILON);
  EXPECT_LT(cursor()->GetLocation().y(), EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveTopRight) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_X, 31496},
      {{0, 0}, EV_ABS, ABS_Y, 109},
      {{0, 0}, EV_ABS, ABS_TILT_X, 66},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 61},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  ASSERT_EQ(1u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_GT(cursor()->GetLocation().x(),
            cursor()->GetCursorConfinedBounds().width() - EPSILON);
  EXPECT_LT(cursor()->GetLocation().y(), EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveBottomLeft) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_Y, 19685},
      {{0, 0}, EV_ABS, ABS_TILT_X, 64},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 61},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  ASSERT_EQ(1u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_LT(cursor()->GetLocation().x(), EPSILON);
  EXPECT_GT(cursor()->GetLocation().y(),
            cursor()->GetCursorConfinedBounds().height() - EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveBottomRight) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_X, 31496},
      {{0, 0}, EV_ABS, ABS_Y, 19685},
      {{0, 0}, EV_ABS, ABS_TILT_X, 67},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 63},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  ASSERT_EQ(1u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_GT(cursor()->GetLocation().x(),
            cursor()->GetCursorConfinedBounds().height() - EPSILON);
  EXPECT_GT(cursor()->GetLocation().y(),
            cursor()->GetCursorConfinedBounds().height() - EPSILON);
}

TEST_F(TabletEventConverterEvdevTest,
       ShouldDisableMouseWarpingToOtherDisplays) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(kWacomIntuos5SPen));

  // Move to bottom right, even though the end position doesn't matter for this
  // test, as long as it is a move.
  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_X, 31496},
      {{0, 0}, EV_ABS, ABS_Y, 19685},
      {{0, 0}, EV_ABS, ABS_TILT_X, 67},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 63},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  ASSERT_EQ(1u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  EXPECT_EQ(event->flags(), ui::EF_NOT_SUITABLE_FOR_MOUSE_WARPING);
}

TEST_F(TabletEventConverterEvdevTest, Tap) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 15456},
      {{0, 0}, EV_ABS, ABS_Y, 8605},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 49},
      {{0, 0}, EV_ABS, ABS_TILT_X, 68},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 64},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 15725},
      {{0, 0}, EV_ABS, ABS_Y, 8755},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 29},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 992},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 15922},
      {{0, 0}, EV_ABS, ABS_Y, 8701},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 32},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(3u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());
  EXPECT_EQ(ui::EventPointerType::kPen, event->pointer_details().pointer_type);
  EXPECT_FLOAT_EQ(5.625f, event->pointer_details().tilt_x);
  EXPECT_FLOAT_EQ(0.f, event->pointer_details().tilt_y);
  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(ui::EventPointerType::kPen, event->pointer_details().pointer_type);
  EXPECT_FLOAT_EQ((float)992 / 2047, event->pointer_details().force);
  EXPECT_EQ(true, event->IsLeftMouseButton());
  event = dispatched_event(2);
  EXPECT_EQ(ui::EventPointerType::kPen, event->pointer_details().pointer_type);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_FLOAT_EQ(0.0f, event->pointer_details().force);
  EXPECT_EQ(true, event->IsLeftMouseButton());
}

TEST_F(TabletEventConverterEvdevTest, StylusButtonPress) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_X, 18372},
      {{0, 0}, EV_ABS, ABS_Y, 9880},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 61},
      {{0, 0}, EV_ABS, ABS_TILT_X, 60},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 63},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 18294},
      {{0, 0}, EV_ABS, ABS_Y, 9723},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 20},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 1015},
      {{0, 0}, EV_KEY, BTN_STYLUS2, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 18516},
      {{0, 0}, EV_ABS, ABS_Y, 9723},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 23},
      {{0, 0}, EV_KEY, BTN_STYLUS2, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(3u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());
  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());
  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());
}

// Should only get an event if BTN_TOOL received
TEST_F(TabletEventConverterEvdevTest, CheckStylusFiltering) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(kWacomIntuos5SPen));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(0u, size());
}

// for digitizer pen with only one side button
TEST_F(TabletEventConverterEvdevTest, DigitizerPenOneSideButtonPress) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      base::WrapUnique(CreateDevice(EpsonBrightLink1430));

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_ABS, ABS_X, 18372},
      {{0, 0}, EV_ABS, ABS_Y, 9880},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 61},
      {{0, 0}, EV_ABS, ABS_TILT_X, 60},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 63},
      {{0, 0}, EV_ABS, ABS_MISC, 1050626},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 18294},
      {{0, 0}, EV_ABS, ABS_Y, 9723},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 20},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 1015},
      {{0, 0}, EV_KEY, BTN_STYLUS, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 18516},
      {{0, 0}, EV_ABS, ABS_Y, 9723},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 23},
      {{0, 0}, EV_KEY, BTN_STYLUS, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 897618290},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(3u, size());

  ui::MouseEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());

  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());
}
