// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ozone/evdev/tablet_event_converter_evdev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_converter_test_util.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/test/scoped_event_test_tick_clock.h"
#include "ui/events/types/event_type.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

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

constexpr char kWacomIntuos5SPenLogDescription[] =
    R"(class=ui::TabletEventConverterEvdev id=1
 x_abs_min=0
 x_abs_range=31497
 y_abs_min=0
 y_abs_range=19686
 tilt_x_min=0
 tilt_x_range=128
 tilt_y_min=0
 tilt_y_range=128
 pressure_max=2047
base class=ui::EventConverterEvdev id=1
 path="/dev/input/test-device"
member class=ui::InputDevice id=1
 input_device_type=ui::InputDeviceType::INPUT_DEVICE_USB
 name="Wacom Intuos5 touch S Pen"
 phys=""
 enabled=0
 suspected_keyboard_imposter=0
 suspected_mouse_imposter=0
 sys_path=""
 vendor_id=056A
 product_id=0026
 version=0107
)";

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

const ui::DeviceAbsoluteAxis WacomOnePenTabletMediumCTC6110WLAbsAxes[] = {
    {ABS_X, {0, 0, 21600, 0, 0, 100}},
    {ABS_Y, {0, 0, 13500, 0, 0, 100}},
    {ABS_PRESSURE, {0, 0, 4095, 0, 0, 0}},
    {ABS_TILT_X, {0, -9000, 9000, 0, 0, 5730}},
    {ABS_TILT_Y, {0, -9000, 9000, 0, 0, 5730}},
    {ABS_MISC, {0, 0, 1, 0, 0, 0}},
};

const ui::DeviceCapabilities WacomOnePenTabletMediumCTC6110WL = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-2/"
    "1-2:1.0/0003:0531:0102.0003/input/input22/event14",
    /* name */ "Wacom Co.,Ltd. Wacom One pen tablet medium",
    /* phys */ "usb-0000:00:14.0-2/input0",
    /* uniq */ "2KHS1G1000107",
    /* bustype */ "0003",
    /* vendor */ "0531",
    /* product */ "0102",
    /* version */ "0110",
    /* prop */ "1",
    /* ev */ "1b",
    /* key */ "c03 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "1000d000003",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    WacomOnePenTabletMediumCTC6110WLAbsAxes,
    std::size(WacomOnePenTabletMediumCTC6110WLAbsAxes),
    /* kbd_function_row_physmap */ "",
    /* kbd_top_row_layout */ "",
};

const ui::DeviceAbsoluteAxis WacomIntuousProMPenPTH660AbsAxes[] = {
    {ABS_X, {0, 0, 44800, 4, 0, 200}},
    {ABS_Y, {0, 0, 29600, 4, 0, 200}},
    {ABS_Z, {0, -900, 899, 0, 0, 287}},
    {ABS_WHEEL, {0, 0, 2047, 0, 0, 0}},
    {ABS_PRESSURE, {0, 0, 8191, 0, 0, 0}},
    {ABS_DISTANCE, {0, 0, 63, 0, 0, 0}},
    {ABS_TILT_X, {0, -64, 63, 0, 0, 57}},
    {ABS_TILT_Y, {0, -64, 63, 0, 0, 57}},
    {ABS_MISC, {0, -2147483648, 2147483647, 0, 0, 0}},
};

const ui::DeviceCapabilities WacomIntuousProMPenPTH660 = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-1/"
    "1-1:1.0/0003:056A:0357.0009/input/input41/event13",
    /* name */ "Wacom Intuos Pro M Pen",
    /* phys */ "usb-0000:00:14.0-1/input0",
    /* uniq */ "2DV00A1004065",
    /* bustype */ "0003",
    /* vendor */ "056a",
    /* product */ "0357",
    /* version */ "0110",
    /* prop */ "1",
    /* ev */ "1b",
    /* key */ "1edf 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "1000f000107",
    /* msc */ "1",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    WacomIntuousProMPenPTH660AbsAxes,
    std::size(WacomIntuousProMPenPTH660AbsAxes),
    /* kbd_function_row_physmap */ "",
    /* kbd_top_row_layout */ "",
};

const ui::DeviceAbsoluteAxis XPPenStarG640AbsAxes[] = {
    {ABS_X, {0, 0, 32767, 0, 0, 205}},
    {ABS_Y, {0, 0, 32767, 0, 0, 328}},
    {ABS_PRESSURE, {0, 0, 8191, 0, 0, 0}},
    {ABS_TILT_X, {0, -127, 127, 0, 0, 0}},
    {ABS_TILT_Y, {0, -127, 127, 0, 0, 0}},
};

const ui::DeviceCapabilities XPPenStarG640 = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-4/1-4:1.1/0003:28BD:0914.0005/"
    "input/input28/event15",
    /* name */ "UGTABLET 6 inch PenTablet",
    /* phys */ "usb-0000:00:14.0-4/input1",
    /* uniq */ "000000",
    /* bustype */ "0003",
    /* vendor */ "28bd",
    /* product */ "0914",
    /* version */ "0100",
    /* prop */ "1",
    /* ev */ "1b",
    /* key */ "c03 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "d000003",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    XPPenStarG640AbsAxes,
    std::size(XPPenStarG640AbsAxes),
    /* kbd_function_row_physmap */ "",
    /* kbd_top_row_layout */ "",
};

const ui::DeviceAbsoluteAxis WacomOneByWacomSPenCTC472AbsAxes[] = {
    {ABS_X, {0, 0, 15200, 4, 0, 100}},
    {ABS_Y, {0, 0, 9500, 4, 0, 100}},
    {ABS_PRESSURE, {0, 0, 2047, 0, 0, 0}},
    {ABS_DISTANCE, {0, 0, 63, 1, 0, 0}},
};

const ui::DeviceCapabilities WacomOneByWacomSPenCTC472 = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-4/1-4:1.0/0003:056A:037A.0002/"
    "input/input16/event9",
    /* name */ "Wacom One by Wacom S Pen",
    /* phys */ "usb-0000:00:14.0-4/input0",
    /* uniq */ "9LE00L1003996",
    /* bustype */ "0003",
    /* vendor */ "056a",
    /* product */ "037a",
    /* version */ "0110",
    /* prop */ "1",
    /* ev */ "b",
    /* key */ "1c03 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "3000003",
    /* msc */ "0",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    WacomOneByWacomSPenCTC472AbsAxes,
    std::size(WacomOneByWacomSPenCTC472AbsAxes),
    /* kbd_function_row_physmap */ "",
    /* kbd_top_row_layout */ "",
};
using PointerType = ui::EventPointerType;

struct ExpectedEvent {
  ui::EventPointerType pointer_type;
  ui::EventType type;
  ui::EventFlags button_flags;
};

std::string LogSubst(std::string description,
                     std::string key,
                     std::string replacement) {
  EXPECT_TRUE(RE2::Replace(&description, "\n(\\s*" + key + ")=[^\n]+\n",
                           "\n\\1=" + replacement + "\n"));
  return description;
}

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
    NOTREACHED_IN_MIGRATION();
  }
  void MoveCursorTo(const gfx::PointF& location) override {
    cursor_location_ = location;
  }
  void MoveCursor(const gfx::Vector2dF& delta) override {
    NOTREACHED_IN_MIGRATION();
  }
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
  DPCHECK(nwrite ==
          static_cast<int>(sizeof(struct input_event) * read_this_many))
      << "write() failed";
}

}  // namespace ui

// Test fixture.
class TabletEventConverterEvdevTest : public testing::Test {
 public:
  TabletEventConverterEvdevTest()
      : cursor_(std::make_unique<ui::MockTabletCursorEvdev>()),
        device_manager_(ui::CreateDeviceManagerForTest()),
        keyboard_layout_engine_(
            std::make_unique<ui::StubKeyboardLayoutEngine>()),
        event_factory_(ui::CreateEventFactoryEvdevForTest(
            cursor_.get(),
            device_manager_.get(),
            keyboard_layout_engine_.get(),
            base::BindRepeating(
                &TabletEventConverterEvdevTest::DispatchEventForTest,
                base::Unretained(this)))),
        dispatcher_(
            ui::CreateDeviceEventDispatcherEvdevForTest(event_factory_.get())),
        test_clock_(std::make_unique<ui::test::ScopedEventTestTickClock>()) {}

  TabletEventConverterEvdevTest(const TabletEventConverterEvdevTest&) = delete;
  TabletEventConverterEvdevTest& operator=(
      const TabletEventConverterEvdevTest&) = delete;

  std::unique_ptr<ui::MockTabletEventConverterEvdev> CreateDevice(
      const ui::EventDeviceInfo& devinfo) {
    // Set up pipe to satisfy message pump (unused).
    int evdev_io[2];
    if (pipe(evdev_io))
      PLOG(FATAL) << "failed pipe";
    base::ScopedFD events_in(evdev_io[0]);
    events_out_.reset(evdev_io[1]);

    return std::make_unique<ui::MockTabletEventConverterEvdev>(
        std::move(events_in), base::FilePath(kTestDevicePath), cursor_.get(),
        devinfo, dispatcher_.get());
  }

  std::unique_ptr<ui::MockTabletEventConverterEvdev> CreateDevice(
      const ui::DeviceCapabilities& caps) {
    ui::EventDeviceInfo devinfo;
    CapabilitiesToDeviceInfo(caps, &devinfo);
    return CreateDevice(devinfo);
  }

  ui::CursorDelegateEvdev* cursor() { return cursor_.get(); }

  unsigned size() { return dispatched_events_.size(); }
  ui::MouseEvent* dispatched_event(unsigned index) {
    DCHECK_GT(dispatched_events_.size(), index);
    ui::Event* ev = dispatched_events_[index].get();
    DCHECK(ev->IsMouseEvent());
    return ev->AsMouseEvent();
  }

  ui::KeyEvent* dispatched_key_event(unsigned index) {
    DCHECK_GT(dispatched_events_.size(), index);
    ui::Event* ev = dispatched_events_[index].get();
    DCHECK(ev->IsKeyEvent());
    return ev->AsKeyEvent();
  }

  void CheckEvents(struct ExpectedEvent expected_events[],
                   unsigned num_events) {
    ASSERT_EQ(num_events, size());
    for (unsigned i = 0; i < num_events; ++i) {
      ui::MouseEvent* event = dispatched_event(i);
      EXPECT_EQ(event->pointer_details().pointer_type,
                expected_events[i].pointer_type)
          << " at event " << i;
      EXPECT_EQ(event->type(), expected_events[i].type) << " at event " << i;
      EXPECT_EQ(event->flags() & ui::EF_MOUSE_BUTTON,
                expected_events[i].button_flags)
          << " at event " << i;
    }
  }

  void DispatchEventForTest(ui::Event* event) {
    std::unique_ptr<ui::Event> cloned_event = event->Clone();
    dispatched_events_.push_back(std::move(cloned_event));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};

  const std::unique_ptr<ui::MockTabletCursorEvdev> cursor_;
  const std::unique_ptr<ui::DeviceManager> device_manager_;
  const std::unique_ptr<ui::KeyboardLayoutEngine> keyboard_layout_engine_;
  const std::unique_ptr<ui::EventFactoryEvdev> event_factory_;
  const std::unique_ptr<ui::DeviceEventDispatcherEvdev> dispatcher_;
  const std::unique_ptr<ui::test::ScopedEventTestTickClock> test_clock_;

  std::vector<std::unique_ptr<ui::Event>> dispatched_events_;

  base::ScopedFD events_out_;
};

#define EPSILON 20

// Uses real data captured from Wacom Intuos 5 Pen
TEST_F(TabletEventConverterEvdevTest, MoveTopLeft) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(kWacomIntuos5SPen);

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
  EXPECT_EQ(ui::EventType::kMouseMoved, event->type());

  EXPECT_LT(cursor()->GetLocation().x(), EPSILON);
  EXPECT_LT(cursor()->GetLocation().y(), EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveTopRight) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(kWacomIntuos5SPen);

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
  EXPECT_EQ(ui::EventType::kMouseMoved, event->type());

  EXPECT_GT(cursor()->GetLocation().x(),
            cursor()->GetCursorConfinedBounds().width() - EPSILON);
  EXPECT_LT(cursor()->GetLocation().y(), EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveBottomLeft) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(kWacomIntuos5SPen);

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
  EXPECT_EQ(ui::EventType::kMouseMoved, event->type());

  EXPECT_LT(cursor()->GetLocation().x(), EPSILON);
  EXPECT_GT(cursor()->GetLocation().y(),
            cursor()->GetCursorConfinedBounds().height() - EPSILON);
}

TEST_F(TabletEventConverterEvdevTest, MoveBottomRight) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(kWacomIntuos5SPen);

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
  EXPECT_EQ(ui::EventType::kMouseMoved, event->type());

  EXPECT_GT(cursor()->GetLocation().x(),
            cursor()->GetCursorConfinedBounds().height() - EPSILON);
  EXPECT_GT(cursor()->GetLocation().y(),
            cursor()->GetCursorConfinedBounds().height() - EPSILON);
}

TEST_F(TabletEventConverterEvdevTest,
       ShouldDisableMouseWarpingToOtherDisplays) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(kWacomIntuos5SPen);

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
  EXPECT_EQ(ui::EventType::kMouseMoved, event->type());

  EXPECT_EQ(event->flags(), ui::EF_NOT_SUITABLE_FOR_MOUSE_WARPING);
}

TEST_F(TabletEventConverterEvdevTest, Tap) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(kWacomIntuos5SPen);

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
  EXPECT_EQ(ui::EventType::kMouseMoved, event->type());
  EXPECT_EQ(ui::EventPointerType::kPen, event->pointer_details().pointer_type);
  EXPECT_FLOAT_EQ(5.625f, event->pointer_details().tilt_x);
  EXPECT_FLOAT_EQ(0.f, event->pointer_details().tilt_y);
  event = dispatched_event(1);
  EXPECT_EQ(ui::EventType::kMousePressed, event->type());
  EXPECT_EQ(ui::EventPointerType::kPen, event->pointer_details().pointer_type);
  EXPECT_FLOAT_EQ((float)992 / 2047, event->pointer_details().force);
  EXPECT_EQ(true, event->IsLeftMouseButton());
  event = dispatched_event(2);
  EXPECT_EQ(ui::EventPointerType::kPen, event->pointer_details().pointer_type);
  EXPECT_EQ(ui::EventType::kMouseReleased, event->type());
  EXPECT_FLOAT_EQ(0.0f, event->pointer_details().force);
  EXPECT_EQ(true, event->IsLeftMouseButton());
}

TEST_F(TabletEventConverterEvdevTest, StylusButtonPress) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(kWacomIntuos5SPen);

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
  EXPECT_EQ(ui::EventType::kMouseMoved, event->type());
  event = dispatched_event(1);
  EXPECT_EQ(ui::EventType::kMousePressed, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());
  event = dispatched_event(2);
  EXPECT_EQ(ui::EventType::kMouseReleased, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(TabletEventConverterEvdevTest, TabletButtonPress) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({ash::features::kInputDeviceSettingsSplit,
                                 ash::features::kPeripheralCustomization},
                                {});

  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(kWacomIntuos5SPen);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_0, 1},
      {{0, 0}, EV_KEY, BTN_0, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(2u, size());

  ui::KeyEvent* event = dispatched_key_event(0);
  EXPECT_EQ(ui::EventType::kKeyPressed, event->type());
  EXPECT_EQ(ui::VKEY_BUTTON_0, event->key_code());
  event = dispatched_key_event(1);
  EXPECT_EQ(ui::EventType::kKeyReleased, event->type());
  EXPECT_EQ(ui::VKEY_BUTTON_0, event->key_code());
}
#endif

// Should only get an event if BTN_TOOL received
TEST_F(TabletEventConverterEvdevTest, CheckStylusFiltering) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(kWacomIntuos5SPen);

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
      CreateDevice(EpsonBrightLink1430);

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
  EXPECT_EQ(ui::EventType::kMouseMoved, event->type());

  event = dispatched_event(1);
  EXPECT_EQ(ui::EventType::kMousePressed, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());

  event = dispatched_event(2);
  EXPECT_EQ(ui::EventType::kMouseReleased, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());
}

TEST_F(TabletEventConverterEvdevTest, NoButtonPressedKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomOnePenTabletMediumCTC6110WL);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_X, 10843},
      {{0, 0}, EV_ABS, ABS_Y, 1848},
      {{0, 0}, EV_ABS, ABS_TILT_X, 1500},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_ABS, ABS_X, 10999},
      {{0, 0}, EV_ABS, ABS_Y, 2062},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 129},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 2

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 3
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 4

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, SideEraserAlwaysPressedKernel5) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomOnePenTabletMediumCTC6110WL);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_ABS, ABS_X, 10668},
      {{0, 0}, EV_ABS, ABS_Y, 3327},
      {{0, 0}, EV_ABS, ABS_TILT_X, 2000},
      {{0, 0}, EV_ABS, ABS_TILT_Y, -100},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0045},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_Y, 3389},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 2

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0045},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 3
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 4

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, SideEraserAlwaysPressedKernel6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomOnePenTabletMediumCTC6110WL);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_ABS, ABS_X, 10812},
      {{0, 0}, EV_ABS, ABS_Y, 5781},
      {{0, 0}, EV_ABS, ABS_TILT_X, 1400},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 600},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0045},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_ABS, ABS_PRESSURE, 1},
      {{0, 0}, EV_ABS, ABS_Y, 6381},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 2

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0045},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 3
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 4

      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, SideEraserReleasedWhileTouchingKernel5) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomOnePenTabletMediumCTC6110WL);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_ABS, ABS_X, 10326},
      {{0, 0}, EV_ABS, ABS_Y, 6075},
      {{0, 0}, EV_ABS, ABS_TILT_X, 1900},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 1400},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0045},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_X, 10274},
      {{0, 0}, EV_ABS, ABS_Y, 6397},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 2

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0045},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 3
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 4

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_X, 10297},
      {{0, 0}, EV_ABS, ABS_Y, 6947},
      {{0, 0}, EV_ABS, ABS_TILT_X, 2200},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 1800},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 5
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, SideEraserReleasedWhileTouchingKernel6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomOnePenTabletMediumCTC6110WL);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_ABS, ABS_X, 12465},
      {{0, 0}, EV_ABS, ABS_Y, 7531},
      {{0, 0}, EV_ABS, ABS_TILT_X, 1200},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 700},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0045},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_ABS, ABS_PRESSURE, 1},
      {{0, 0}, EV_ABS, ABS_X, 12804},
      {{0, 0}, EV_ABS, ABS_Y, 8026},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 2

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0045},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 3
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 4

      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_Y, 8255},
      {{0, 0}, EV_ABS, ABS_TILT_X, 1100},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 500},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 5
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest,
       SideEraserPressedWhileTouchingKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomOnePenTabletMediumCTC6110WL);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_X, 10408},
      {{0, 0}, EV_ABS, ABS_Y, 6417},
      {{0, 0}, EV_ABS, ABS_TILT_X, 1800},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 1700},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_ABS, ABS_X, 11131},
      {{0, 0}, EV_ABS, ABS_Y, 6660},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 58},
      {{0, 0}, EV_ABS, ABS_TILT_X, 2600},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 2

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 3
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 4

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_ABS, ABS_X, 11216},
      {{0, 0}, EV_ABS, ABS_TILT_X, 2700},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 2100},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 5

      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, TailEraserKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomIntuousProMPenPTH660);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 21040},
      {{0, 0}, EV_ABS, ABS_Y, 17635},
      {{0, 0}, EV_ABS, ABS_TILT_X, -4},
      {{0, 0}, EV_ABS, ABS_TILT_Y, -7},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 2122},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_ABS, ABS_X, 21888},
      {{0, 0}, EV_ABS, ABS_Y, 17761},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 99},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 21175},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 2
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, Button1AlwaysPressedKernel5) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(XPPenStarG640);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0044},
      {{0, 0}, EV_KEY, BTN_STYLUS, 1},  // Event 0
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_X, 24124},
      {{0, 0}, EV_ABS, ABS_Y, 25239},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 1

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 2
      {{0, 0}, EV_ABS, ABS_X, 24832},
      {{0, 0}, EV_ABS, ABS_Y, 27186},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 3

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 4
      {{0, 0}, EV_ABS, ABS_X, 24544},
      {{0, 0}, EV_ABS, ABS_Y, 27101},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 5

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0044},
      {{0, 0}, EV_KEY, BTN_STYLUS, 0},  // Event 6
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_RIGHT_MOUSE_BUTTON},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, Button1AlwaysPressedKernel6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(XPPenStarG640);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0044},
      {{0, 0}, EV_KEY, BTN_STYLUS, 1},  // Event 0
      {{0, 0}, EV_ABS, ABS_X, 24908},
      {{0, 0}, EV_ABS, ABS_Y, 22555},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 1

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 2
      {{0, 0}, EV_ABS, ABS_PRESSURE, 127},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 3

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 4
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_ABS, ABS_X, 23225},
      {{0, 0}, EV_ABS, ABS_Y, 22283},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 5

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0044},
      {{0, 0}, EV_KEY, BTN_STYLUS, 0},  // Event 6
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_RIGHT_MOUSE_BUTTON},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, Button1ReleasedWhileTouchingKernel5) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(XPPenStarG640);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_STYLUS, 1},  // Event 0
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_X, 22151},
      {{0, 0}, EV_ABS, ABS_Y, 20812},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 1

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 2
      {{0, 0}, EV_ABS, ABS_PRESSURE, 263},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 3

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 4
      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0044},
      {{0, 0}, EV_KEY, BTN_STYLUS, 0},  // Event 5
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_X, 22579},
      {{0, 0}, EV_ABS, ABS_Y, 20625},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 6

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 7
      {{0, 0}, EV_ABS, ABS_PRESSURE, 5259},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 8

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 9
      {{0, 0}, EV_ABS, ABS_X, 22459},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 10

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, Button1ReleasedWhileTouchingKernel6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(XPPenStarG640);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0044},
      {{0, 0}, EV_KEY, BTN_STYLUS, 1},  // Event 0
      {{0, 0}, EV_ABS, ABS_X, 22981},
      {{0, 0}, EV_ABS, ABS_Y, 23302},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 1

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 2
      {{0, 0}, EV_ABS, ABS_PRESSURE, 83},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 3

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 4
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0044},
      {{0, 0}, EV_KEY, BTN_STYLUS, 0},  // Event 5
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_X, 21478},
      {{0, 0}, EV_ABS, ABS_Y, 24548},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 6

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 7
      {{0, 0}, EV_ABS, ABS_PRESSURE, 4434},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 8

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 9
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_ABS, ABS_X, 20626},
      {{0, 0}, EV_ABS, ABS_Y, 25075},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 10

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, Button1PressedWhileTouchingKernel5) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(XPPenStarG640);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_X, 21303},
      {{0, 0}, EV_ABS, ABS_Y, 20815},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_ABS, ABS_PRESSURE, 347},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 2

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0044},
      {{0, 0}, EV_KEY, BTN_STYLUS, 1},  // Event 3
      {{0, 0}, EV_ABS, ABS_X, 21514},
      {{0, 0}, EV_ABS, ABS_Y, 21441},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 2300},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 4

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 5
      {{0, 0}, EV_ABS, ABS_X, 21555},
      {{0, 0}, EV_ABS, ABS_Y, 21225},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 6

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0044},
      {{0, 0}, EV_KEY, BTN_STYLUS, 0},  // Event 7
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_RIGHT_MOUSE_BUTTON},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, Button1PressedWhileTouchingKernel6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(XPPenStarG640);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_ABS, ABS_X, 26776},
      {{0, 0}, EV_ABS, ABS_Y, 25088},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_ABS, ABS_PRESSURE, 10},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 2

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 3
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0044},
      {{0, 0}, EV_KEY, BTN_STYLUS, 1},  // Event 4
      {{0, 0}, EV_ABS, ABS_X, 26164},
      {{0, 0}, EV_ABS, ABS_Y, 24295},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 5

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 6
      {{0, 0}, EV_ABS, ABS_X, 26131},
      {{0, 0}, EV_ABS, ABS_Y, 24289},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 844},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 7

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0042},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 8
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 9

      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0xd0044},
      {{0, 0}, EV_KEY, BTN_STYLUS, 0},  // Event 10
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_RIGHT_MOUSE_BUTTON},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, Button2AlwaysPressedKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomOneByWacomSPenCTC472);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 8430},
      {{0, 0}, EV_ABS, ABS_Y, 3571},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 46},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_KEY, BTN_STYLUS2, 1},  // Event 1
      {{0, 0}, EV_ABS, ABS_X, 8464},
      {{0, 0}, EV_ABS, ABS_Y, 3587},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 2

      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 3
      {{0, 0}, EV_ABS, ABS_X, 8585},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 489},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 4

      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 5
      {{0, 0}, EV_ABS, ABS_X, 8167},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 6

      {{0, 0}, EV_KEY, BTN_STYLUS2, 0},  // Event 7
      {{0, 0}, EV_ABS, ABS_X, 8029},
      {{0, 0}, EV_ABS, ABS_Y, 4274},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 8

      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, Button2ReleasedWhileTouchingKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomOneByWacomSPenCTC472);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 9826},
      {{0, 0}, EV_ABS, ABS_Y, 4743},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 46},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_KEY, BTN_STYLUS2, 1},  // Event 1
      {{0, 0}, EV_ABS, ABS_X, 9835},
      {{0, 0}, EV_ABS, ABS_Y, 4765},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 2

      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 3
      {{0, 0}, EV_ABS, ABS_X, 10144},
      {{0, 0}, EV_ABS, ABS_Y, 5180},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 439},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 4

      {{0, 0}, EV_KEY, BTN_STYLUS2, 0},  // Event 5
      {{0, 0}, EV_ABS, ABS_X, 9240},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 1129},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 6

      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 7
      {{0, 0}, EV_ABS, ABS_Y, 5015},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 8

      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, Button2PressedWhileTouchingKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomOneByWacomSPenCTC472);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 9886},
      {{0, 0}, EV_ABS, ABS_Y, 5154},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 46},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_ABS, ABS_X, 10015},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 300},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 2

      {{0, 0}, EV_KEY, BTN_STYLUS2, 1},  // Event 3
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 4
      {{0, 0}, EV_ABS, ABS_X, 9265},
      {{0, 0}, EV_ABS, ABS_X, 5278},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 5

      {{0, 0}, EV_KEY, BTN_STYLUS2, 0},  // Event 6
      {{0, 0}, EV_ABS, ABS_X, 9351},
      {{0, 0}, EV_ABS, ABS_Y, 5029},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 7

      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_PEN, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseDragged,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseReleased,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kPen, ui::EventType::kMouseMoved, ui::EF_NONE},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest,
       Button1AlwaysPressedWithTailEraserKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomIntuousProMPenPTH660);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 38777},
      {{0, 0}, EV_ABS, ABS_Y, 10444},
      {{0, 0}, EV_ABS, ABS_TILT_X, 7},
      {{0, 0}, EV_ABS, ABS_TILT_Y, -14},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 2122},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_ABS, ABS_Y, 10382},
      {{0, 0}, EV_ABS, ABS_TILT_Y, -7},
      {{0, 0}, EV_KEY, BTN_STYLUS, 1},  // Event 1
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 38629},
      {{0, 0}, EV_ABS, ABS_Y, 10647},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 896},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 2
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 35089},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 3
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_STYLUS, 0},  // Event 4
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_MIDDLE_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_MIDDLE_MOUSE_BUTTON},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest,
       Button1ReleasedWhileTouchingWithTailEraserKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomIntuousProMPenPTH660);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 34762},
      {{0, 0}, EV_ABS, ABS_Y, 11079},
      {{0, 0}, EV_ABS, ABS_TILT_X, -2},
      {{0, 0}, EV_ABS, ABS_TILT_Y, -8},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 2122},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_ABS, ABS_X, 34749},
      {{0, 0}, EV_ABS, ABS_Y, 11135},
      {{0, 0}, EV_KEY, BTN_STYLUS, 1},  // Event 1
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 34807},
      {{0, 0}, EV_ABS, ABS_Y, 11825},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 612},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 2
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_STYLUS, 0},  // Event 3
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 33191},
      {{0, 0}, EV_ABS, ABS_Y, 14021},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 4
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_MIDDLE_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest,
       Button1PressedWhileTouchingWithTailEraserKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomIntuousProMPenPTH660);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 35858},
      {{0, 0}, EV_ABS, ABS_Y, 11342},
      {{0, 0}, EV_ABS, ABS_TILT_X, -3},
      {{0, 0}, EV_ABS, ABS_TILT_Y, -4},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 2122},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_ABS, ABS_X, 35577},
      {{0, 0}, EV_ABS, ABS_Y, 11096},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 558},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_Y, 11140},
      {{0, 0}, EV_KEY, BTN_STYLUS, 1},  // Event 2
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 3
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_STYLUS, 0},  // Event 4
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_MIDDLE_MOUSE_BUTTON},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest,
       Button2AlwaysPressedWithTailEraserKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomIntuousProMPenPTH660);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 37215},
      {{0, 0}, EV_ABS, ABS_Y, 11866},
      {{0, 0}, EV_ABS, ABS_TILT_X, -10},
      {{0, 0}, EV_ABS, ABS_TILT_Y, -18},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 2122},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_ABS, ABS_TILT_X, 1},
      {{0, 0}, EV_ABS, ABS_TILT_Y, -9},
      {{0, 0}, EV_KEY, BTN_STYLUS2, 1},  // Event 1
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 37491},
      {{0, 0}, EV_ABS, ABS_Y, 11497},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 1467},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 13},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 2
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 36109},
      {{0, 0}, EV_ABS, ABS_Y, 13970},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 3
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_STYLUS2, 0},  // Event 4
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_RIGHT_MOUSE_BUTTON},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest,
       Button2ReleasedWhileTouchingWithTailEraserKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomIntuousProMPenPTH660);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 37450},
      {{0, 0}, EV_ABS, ABS_Y, 8565},
      {{0, 0}, EV_ABS, ABS_TILT_X, -9},
      {{0, 0}, EV_ABS, ABS_TILT_Y, -18},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 2122},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_ABS, ABS_X, 37477},
      {{0, 0}, EV_ABS, ABS_TILT_X, -6},
      {{0, 0}, EV_ABS, ABS_TILT_Y, -14},
      {{0, 0}, EV_KEY, BTN_STYLUS2, 1},  // Event 1
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 36055},
      {{0, 0}, EV_ABS, ABS_Y, 9797},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 2730},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 2
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_Y, 10203},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 7888},
      {{0, 0}, EV_KEY, BTN_STYLUS2, 0},  // Event 3
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 4
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest,
       Button2PressedWhileTouchingWithTailEraserKernel5And6) {
  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(WacomIntuousProMPenPTH660);

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_ABS, ABS_X, 38408},
      {{0, 0}, EV_ABS, ABS_Y, 12781},
      {{0, 0}, EV_ABS, ABS_TILT_X, 6},
      {{0, 0}, EV_ABS, ABS_TILT_Y, -6},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 63},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 1},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 2122},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},  // Event 0

      {{0, 0}, EV_ABS, ABS_X, 38348},
      {{0, 0}, EV_ABS, ABS_Y, 11646},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 705},
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},  // Event 1
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_PRESSURE, 6282},
      {{0, 0}, EV_KEY, BTN_STYLUS2, 1},  // Event 2
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_Y, 11627},
      {{0, 0}, EV_ABS, ABS_PRESSURE, 0},
      {{0, 0}, EV_KEY, BTN_TOUCH, 0},  // Event 3
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_STYLUS2, 0},  // Event 4
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_ABS, ABS_X, 0},
      {{0, 0}, EV_ABS, ABS_Y, 0},
      {{0, 0}, EV_ABS, ABS_TILT_X, 0},
      {{0, 0}, EV_ABS, ABS_TILT_Y, 0},
      {{0, 0}, EV_ABS, ABS_DISTANCE, 0},
      {{0, 0}, EV_KEY, BTN_TOOL_RUBBER, 0},
      {{0, 0}, EV_MSC, MSC_SERIAL, 562066246},
      {{0, 0}, EV_ABS, ABS_MISC, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  ExpectedEvent expected_events[] = {
      {PointerType::kEraser, ui::EventType::kMouseMoved, ui::EF_NONE},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMousePressed,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON},
      {PointerType::kEraser, ui::EventType::kMouseReleased,
       ui::EF_RIGHT_MOUSE_BUTTON},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  CheckEvents(expected_events, std::size(expected_events));
}

TEST_F(TabletEventConverterEvdevTest, Basic) {
  ui::EventDeviceInfo devinfo;
  ui::CapabilitiesToDeviceInfo(kWacomIntuos5SPen, &devinfo);

  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(devinfo);

  std::stringstream output;
  dev->DescribeForLog(output);

  EXPECT_EQ(output.str(), kWacomIntuos5SPenLogDescription);
}

// Twiddle each field that can reasonably be changed independently.
TEST_F(TabletEventConverterEvdevTest, LogPressureAbs) {
  ui::EventDeviceInfo devinfo;
  ui::CapabilitiesToDeviceInfo(kWacomIntuos5SPen, &devinfo);
  input_absinfo absinfo = {.minimum = 12, .maximum = 24, .resolution = 55};
  devinfo.SetAbsInfo(ABS_PRESSURE, absinfo);

  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(devinfo);

  std::string log = kWacomIntuos5SPenLogDescription;
  log = LogSubst(log, "pressure_max", "24");

  std::stringstream output;
  dev->DescribeForLog(output);

  EXPECT_EQ(output.str(), log);
}

TEST_F(TabletEventConverterEvdevTest, LogXYAbs) {
  ui::EventDeviceInfo devinfo;
  CapabilitiesToDeviceInfo(kWacomIntuos5SPen, &devinfo);

  input_absinfo absinfo_x = {.minimum = -12, .maximum = 400, .resolution = 123};
  input_absinfo absinfo_y = {
      .minimum = 3000, .maximum = 4000, .resolution = 9000};
  devinfo.SetAbsInfo(ABS_X, absinfo_x);
  devinfo.SetAbsInfo(ABS_Y, absinfo_y);

  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(devinfo);

  std::string log = kWacomIntuos5SPenLogDescription;
  log = LogSubst(log, "x_abs_min", "-12");
  log = LogSubst(log, "x_abs_range", "413");
  log = LogSubst(log, "y_abs_min", "3000");
  log = LogSubst(log, "y_abs_range", "1001");

  std::stringstream output;
  dev->DescribeForLog(output);

  EXPECT_EQ(output.str(), log);
}

TEST_F(TabletEventConverterEvdevTest, LogXYTilt) {
  ui::EventDeviceInfo devinfo;
  CapabilitiesToDeviceInfo(kWacomIntuos5SPen, &devinfo);

  input_absinfo absinfo_tilt_x = {
      .minimum = -120, .maximum = 40, .resolution = 12};
  input_absinfo absinfo_tilt_y = {
      .minimum = 300, .maximum = 400, .resolution = 900};
  devinfo.SetAbsInfo(ABS_TILT_X, absinfo_tilt_x);
  devinfo.SetAbsInfo(ABS_TILT_Y, absinfo_tilt_y);

  std::unique_ptr<ui::MockTabletEventConverterEvdev> dev =
      CreateDevice(devinfo);

  std::string log = kWacomIntuos5SPenLogDescription;
  log = LogSubst(log, "tilt_x_min", "-120");
  log = LogSubst(log, "tilt_x_range", "161");
  log = LogSubst(log, "tilt_y_min", "300");
  log = LogSubst(log, "tilt_y_range", "101");

  std::stringstream output;
  dev->DescribeForLog(output);

  EXPECT_EQ(output.str(), log);
}
