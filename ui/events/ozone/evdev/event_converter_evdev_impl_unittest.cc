// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_converter_evdev_impl.h"

#include <linux/input.h>

#include <memory>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_converter_test_util.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/testing/fake_cursor_delegate_evdev.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/test/scoped_event_test_tick_clock.h"

namespace ui {

const char kTestDevicePath[] = "/dev/input/test-device";

class MockEventConverterEvdevImpl : public EventConverterEvdevImpl {
 public:
  MockEventConverterEvdevImpl(base::ScopedFD fd,
                              const ui::EventDeviceInfo& info,
                              CursorDelegateEvdev* cursor,
                              DeviceEventDispatcherEvdev* dispatcher)
      : EventConverterEvdevImpl(std::move(fd),
                                base::FilePath(kTestDevicePath),
                                1,
                                info,
                                cursor,
                                dispatcher) {
    SetEnabled(true);
  }

  MockEventConverterEvdevImpl(const MockEventConverterEvdevImpl&) = delete;
  MockEventConverterEvdevImpl& operator=(const MockEventConverterEvdevImpl&) =
      delete;

  ~MockEventConverterEvdevImpl() override { SetEnabled(false); }

  // EventConverterEvdevImpl:
  bool HasKeyboard() const override { return true; }
  bool HasTouchpad() const override { return true; }
};

}  // namespace ui

// Test fixture.
class EventConverterEvdevImplTest : public testing::Test {
 public:
  EventConverterEvdevImplTest() {}

  EventConverterEvdevImplTest(const EventConverterEvdevImplTest&) = delete;
  EventConverterEvdevImplTest& operator=(const EventConverterEvdevImplTest&) =
      delete;

  // Overridden from testing::Test:
  void SetUp() override { SetUpDevice(ui::EventDeviceInfo()); }

  void TearDown() override {
    device_.reset();
    cursor_.reset();
    events_out_.reset();
    test_clock_.reset();
  }

  void SetUpDevice(const ui::EventDeviceInfo& info) {
    // Set up pipe to satisfy message pump (unused).
    int evdev_io[2];
    if (pipe(evdev_io))
      PLOG(FATAL) << "failed pipe";
    base::ScopedFD events_in(evdev_io[0]);
    events_out_.reset(evdev_io[1]);

    cursor_ = std::make_unique<ui::FakeCursorDelegateEvdev>();

    keyboard_layout_engine_ = std::make_unique<ui::StubKeyboardLayoutEngine>();
    device_manager_ = ui::CreateDeviceManagerForTest();
    event_factory_ = ui::CreateEventFactoryEvdevForTest(
        cursor_.get(), device_manager_.get(), keyboard_layout_engine_.get(),
        base::BindRepeating(&EventConverterEvdevImplTest::DispatchEventForTest,
                            base::Unretained(this)));
    dispatcher_ =
        ui::CreateDeviceEventDispatcherEvdevForTest(event_factory_.get());
    device_ = std::make_unique<ui::MockEventConverterEvdevImpl>(
        std::move(events_in), info, cursor_.get(), dispatcher_.get());
    test_clock_ = std::make_unique<ui::test::ScopedEventTestTickClock>();
  }

  ui::FakeCursorDelegateEvdev* cursor() { return cursor_.get(); }
  ui::MockEventConverterEvdevImpl* device() { return device_.get(); }

  unsigned size() { return dispatched_events_.size(); }
  ui::KeyEvent* dispatched_event(unsigned index) {
    DCHECK_GT(dispatched_events_.size(), index);
    ui::Event* ev = dispatched_events_[index].get();
    DCHECK(ev->IsKeyEvent());
    return ev->AsKeyEvent();
  }
  ui::MouseEvent* dispatched_mouse_event(unsigned index) {
    DCHECK_GT(dispatched_events_.size(), index);
    ui::Event* ev = dispatched_events_[index].get();
    DCHECK(ev->IsMouseEvent());
    return ev->AsMouseEvent();
  }

  void ClearDispatchedEvents() {
    dispatched_events_.clear();
  }

  void DestroyDevice() { device_.reset(); }

  ui::InputController* GetInputController() {
    return event_factory_->input_controller();
  }

  void SetTestNowSeconds(int64_t seconds) {
    test_clock_->SetNowSeconds(seconds);
  }

 private:
  void DispatchEventForTest(ui::Event* event) {
    std::unique_ptr<ui::Event> cloned_event = event->Clone();
    dispatched_events_.push_back(std::move(cloned_event));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  std::unique_ptr<ui::KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<ui::FakeCursorDelegateEvdev> cursor_;
  std::unique_ptr<ui::DeviceManager> device_manager_;
  std::unique_ptr<ui::EventFactoryEvdev> event_factory_;
  std::unique_ptr<ui::DeviceEventDispatcherEvdev> dispatcher_;
  std::unique_ptr<ui::MockEventConverterEvdevImpl> device_;
  std::unique_ptr<ui::test::ScopedEventTestTickClock> test_clock_;

  std::vector<std::unique_ptr<ui::Event>> dispatched_events_;

  base::ScopedFD events_out_;
};

// Test fixture which defers device set up, tests need to call SetUpDevice().
class DeferDeviceSetUpEventConverterEvdevImplTest
    : public EventConverterEvdevImplTest {
 public:
  DeferDeviceSetUpEventConverterEvdevImplTest() = default;

  DeferDeviceSetUpEventConverterEvdevImplTest(
      const DeferDeviceSetUpEventConverterEvdevImplTest&) = delete;
  DeferDeviceSetUpEventConverterEvdevImplTest& operator=(
      const DeferDeviceSetUpEventConverterEvdevImplTest&) = delete;

  // Overridden from EventConverterEvdevImplTest:
  void SetUp() override {}
};

TEST_F(EventConverterEvdevImplTest, KeyPress) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(2u, size());

  ui::KeyEvent* event;

  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0x7002au, event->scan_code());
  EXPECT_EQ(0, event->flags());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0x7002au, event->scan_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(EventConverterEvdevImplTest, KeyRepeat) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 2},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 2},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7002a},
      {{0, 0}, EV_KEY, KEY_BACKSPACE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(2u, size());

  ui::KeyEvent* event;

  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0x7002au, event->scan_code());
  EXPECT_EQ(0, event->flags());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_BACK, event->key_code());
  EXPECT_EQ(0x7002au, event->scan_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(EventConverterEvdevImplTest, NoEvents) {
  ui::MockEventConverterEvdevImpl* dev = device();
  dev->ProcessEvents(NULL, 0);
  EXPECT_EQ(0u, size());
}

TEST_F(EventConverterEvdevImplTest, KeyWithModifier) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTSHIFT, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70004},
      {{0, 0}, EV_KEY, KEY_A, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70004},
      {{0, 0}, EV_KEY, KEY_A, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTSHIFT, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(4u, size());

  ui::KeyEvent* event;

  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_SHIFT, event->key_code());
  EXPECT_EQ(0x700e1u, event->scan_code());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, event->flags());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
  EXPECT_EQ(0x70004u, event->scan_code());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, event->flags());

  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
  EXPECT_EQ(0x70004u, event->scan_code());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, event->flags());

  event = dispatched_event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_SHIFT, event->key_code());
  EXPECT_EQ(0x700e1u, event->scan_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(EventConverterEvdevImplTest, KeyWithDuplicateModifier) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTCTRL, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e5},
      {{0, 0}, EV_KEY, KEY_RIGHTCTRL, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7001d},
      {{0, 0}, EV_KEY, KEY_Z, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x7001d},
      {{0, 0}, EV_KEY, KEY_Z, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e1},
      {{0, 0}, EV_KEY, KEY_LEFTCTRL, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x700e5},
      {{0, 0}, EV_KEY, KEY_RIGHTCTRL, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(6u, size());

  ui::KeyEvent* event;

  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(0x700e1u, event->scan_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(0x700e5u, event->scan_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_Z, event->key_code());
  EXPECT_EQ(0x7001du, event->scan_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dispatched_event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_Z, event->key_code());
  EXPECT_EQ(0x7001du, event->scan_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dispatched_event(4);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(0x700e1u, event->scan_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, event->flags());

  event = dispatched_event(5);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_CONTROL, event->key_code());
  EXPECT_EQ(0x700e5u, event->scan_code());
  EXPECT_EQ(0, event->flags());
}

TEST_F(EventConverterEvdevImplTest, KeyWithLock) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_MSC, MSC_SCAN, 0x70039},
      {{0, 0}, EV_KEY, KEY_CAPSLOCK, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_MSC, MSC_SCAN, 0x70039},
      {{0, 0}, EV_KEY, KEY_CAPSLOCK, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(2u, size());

  ui::KeyEvent* event;

  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_CAPITAL, event->key_code());
  EXPECT_EQ(0x70039u, event->scan_code());
  EXPECT_EQ(ui::EF_MOD3_DOWN, event->flags());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_CAPITAL, event->key_code());
  EXPECT_EQ(0x70039u, event->scan_code());
  EXPECT_EQ(ui::EF_NONE, event->flags());
}

TEST_F(EventConverterEvdevImplTest, MouseButton) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_LEFT, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_LEFT, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(2u, size());

  ui::MouseEvent* event;

  event = dispatched_mouse_event(0);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(true, event->IsLeftMouseButton());

  event = dispatched_mouse_event(1);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(true, event->IsLeftMouseButton());
}

// Test that BTN_BACK and BTN_SIDE are treated as the same button.
TEST_F(EventConverterEvdevImplTest, MouseBackButton) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_SIDE, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_BACK, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_SIDE, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_BACK, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(2u, size());

  ui::MouseEvent* event = nullptr;

  event = dispatched_mouse_event(0);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(ui::EF_BACK_MOUSE_BUTTON, event->changed_button_flags());

  event = dispatched_mouse_event(1);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(ui::EF_BACK_MOUSE_BUTTON, event->changed_button_flags());
}

// Test that BTN_FORWARD and BTN_EXTRA are treated as the same button.
TEST_F(EventConverterEvdevImplTest, MouseForwardButton) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_FORWARD, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_EXTRA, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_EXTRA, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_FORWARD, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(2u, size());

  ui::MouseEvent* event = nullptr;

  event = dispatched_mouse_event(0);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(ui::EF_FORWARD_MOUSE_BUTTON, event->changed_button_flags());

  event = dispatched_mouse_event(1);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(ui::EF_FORWARD_MOUSE_BUTTON, event->changed_button_flags());
}

TEST_F(EventConverterEvdevImplTest, MouseMove) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_REL, REL_X, 4},
      {{0, 0}, EV_REL, REL_Y, 2},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(1u, size());

  ui::MouseEvent* event;

  event = dispatched_mouse_event(0);
  EXPECT_EQ(ui::ET_MOUSE_MOVED, event->type());
  EXPECT_EQ(cursor()->GetLocation(), gfx::PointF(4, 2));
}

TEST_F(EventConverterEvdevImplTest, UnmappedKeyPress) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, BTN_TOUCH, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_KEY, BTN_TOUCH, 0},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(0u, size());
}

TEST_F(EventConverterEvdevImplTest, ShouldReleaseKeysOnUnplug) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, KEY_A, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(1u, size());

  DestroyDevice();
  EXPECT_EQ(2u, size());

  ui::KeyEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
}

TEST_F(EventConverterEvdevImplTest, ShouldReleaseKeysOnSynDropped) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, KEY_A, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},

      {{0, 0}, EV_SYN, SYN_DROPPED, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(2u, size());

  ui::KeyEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
}

TEST_F(EventConverterEvdevImplTest, ShouldReleaseKeysOnDisable) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event mock_kernel_queue[] = {
      {{0, 0}, EV_KEY, KEY_A, 1},
      {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(1u, size());

  dev->SetEnabled(false);
  EXPECT_EQ(2u, size());

  ui::KeyEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());

  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
}


// Test that SetAllowedKeys() causes events for non-allowed keys to be dropped.
TEST_F(EventConverterEvdevImplTest, SetAllowedKeys) {
  ui::MockEventConverterEvdevImpl* dev = device();
  struct input_event mock_kernel_queue[] = {
    {{0, 0}, EV_KEY, KEY_A, 1},
    {{0, 0}, EV_SYN, SYN_REPORT, 0},

    {{0, 0}, EV_KEY, KEY_A, 0},
    {{0, 0}, EV_SYN, SYN_REPORT, 0},

    {{0, 0}, EV_KEY, KEY_POWER, 1},
    {{0, 0}, EV_SYN, SYN_REPORT, 0},

    {{0, 0}, EV_KEY, KEY_POWER, 0},
    {{0, 0}, EV_KEY, SYN_REPORT, 0},
  };

  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));

  ASSERT_EQ(4u, size());
  ui::KeyEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_POWER, event->key_code());
  event = dispatched_event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_POWER, event->key_code());

  ClearDispatchedEvents();
  std::vector<ui::DomCode> allowed_keys;
  allowed_keys.push_back(ui::DomCode::POWER);
  dev->SetKeyFilter(true /* enable_filter */, allowed_keys);
  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));

  ASSERT_EQ(2u, size());
  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_POWER, event->key_code());
  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_POWER, event->key_code());

  ClearDispatchedEvents();
  dev->SetKeyFilter(false /* enable_filter */, std::vector<ui::DomCode>());
  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));

  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
  event = dispatched_event(1);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_A, event->key_code());
  event = dispatched_event(2);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());
  EXPECT_EQ(ui::VKEY_POWER, event->key_code());
  event = dispatched_event(3);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());
  EXPECT_EQ(ui::VKEY_POWER, event->key_code());
}

// Test that if a non-allowed key is pressed when SetAllowedKeys() is called
// that the non-allowed key is released.
TEST_F(EventConverterEvdevImplTest, SetAllowedKeysBlockedKeyPressed) {
  ui::MockEventConverterEvdevImpl* dev = device();

  struct input_event key_press[] = {
    {{0, 0}, EV_KEY, KEY_A, 1},
    {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };
  struct input_event key_release[] = {
    {{0, 0}, EV_KEY, KEY_A, 0},
    {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };

  dev->ProcessEvents(key_press, std::size(key_press));
  ASSERT_EQ(1u, size());
  ui::KeyEvent* event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_PRESSED, event->type());

  // Block all key events. Calling SetAllowKeys() should dispatch a synthetic
  // key release for VKEY_A.
  ClearDispatchedEvents();
  std::vector<ui::DomCode> allowed_keys;
  dev->SetKeyFilter(true /* enable_filter */, allowed_keys);
  ASSERT_EQ(1u, size());
  event = dispatched_event(0);
  EXPECT_EQ(ui::ET_KEY_RELEASED, event->type());

  // The real key release should be dropped, whenever it comes.
  ClearDispatchedEvents();
  dev->ProcessEvents(key_release, std::size(key_release));
  ASSERT_EQ(0u, size());
}

TEST_F(EventConverterEvdevImplTest, ShouldSwapMouseButtonsFromUserPreference) {
  ui::MockEventConverterEvdevImpl* dev = device();

  // Captured from Evoluent VerticalMouse 4.
  const struct input_event mock_kernel_queue[] = {
      {{1510019413, 83905}, EV_MSC, MSC_SCAN, 589825},
      {{1510019413, 83905}, EV_KEY, BTN_LEFT, 1},
      {{1510019413, 83905}, EV_SYN, SYN_REPORT, 0},
      {{1510019413, 171859}, EV_MSC, MSC_SCAN, 589825},
      {{1510019413, 171859}, EV_KEY, BTN_LEFT, 0},
      {{1510019413, 171859}, EV_SYN, SYN_REPORT, 0},

      {{1510019414, 43907}, EV_MSC, MSC_SCAN, 589826},
      {{1510019414, 43907}, EV_KEY, BTN_RIGHT, 1},
      {{1510019414, 43907}, EV_SYN, SYN_REPORT, 0},
      {{1510019414, 171863}, EV_MSC, MSC_SCAN, 589826},
      {{1510019414, 171863}, EV_KEY, BTN_RIGHT, 0},
      {{1510019414, 171863}, EV_SYN, SYN_REPORT, 0},
  };

  SetTestNowSeconds(1510019415);
  ClearDispatchedEvents();
  GetInputController()->SetPrimaryButtonRight(false);
  dev->ProcessEvents(mock_kernel_queue, std::size(mock_kernel_queue));
  EXPECT_EQ(4u, size());

  ui::MouseEvent* event;
  event = dispatched_mouse_event(0);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(true, event->IsLeftMouseButton());

  event = dispatched_mouse_event(1);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(true, event->IsLeftMouseButton());

  event = dispatched_mouse_event(2);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());

  event = dispatched_mouse_event(3);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());

  // Captured from Evoluent VerticalMouse 4.
  const struct input_event mock_kernel_queue2[] = {
      {{1510019415, 747945}, EV_MSC, MSC_SCAN, 589825},
      {{1510019415, 747945}, EV_KEY, BTN_LEFT, 1},
      {{1510019415, 747945}, EV_SYN, SYN_REPORT, 0},
      {{1510019415, 859942}, EV_MSC, MSC_SCAN, 589825},
      {{1510019415, 859942}, EV_KEY, BTN_LEFT, 0},
      {{1510019415, 859942}, EV_SYN, SYN_REPORT, 0},

      {{1510019416, 459916}, EV_MSC, MSC_SCAN, 589826},
      {{1510019416, 459916}, EV_KEY, BTN_RIGHT, 1},
      {{1510019416, 459916}, EV_SYN, SYN_REPORT, 0},
      {{1510019416, 555892}, EV_MSC, MSC_SCAN, 589826},
      {{1510019416, 555892}, EV_KEY, BTN_RIGHT, 0},
      {{1510019416, 555892}, EV_SYN, SYN_REPORT, 0},
  };
  SetTestNowSeconds(1510019417);

  ClearDispatchedEvents();
  GetInputController()->SetPrimaryButtonRight(true);
  dev->ProcessEvents(mock_kernel_queue2, std::size(mock_kernel_queue2));
  EXPECT_EQ(4u, size());

  event = dispatched_mouse_event(0);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());

  event = dispatched_mouse_event(1);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(true, event->IsRightMouseButton());

  event = dispatched_mouse_event(2);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, event->type());
  EXPECT_EQ(true, event->IsLeftMouseButton());

  event = dispatched_mouse_event(3);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, event->type());
  EXPECT_EQ(true, event->IsLeftMouseButton());
}

TEST_F(DeferDeviceSetUpEventConverterEvdevImplTest, KeyboardHasKeys) {
  ui::EventDeviceInfo devinfo;
  CapabilitiesToDeviceInfo(ui::kLogitechKeyboardK120, &devinfo);
  SetUpDevice(devinfo);
  ui::MockEventConverterEvdevImpl* dev = device();

  const std::vector<uint64_t> key_bits = dev->GetKeyboardKeyBits();

  // KEY_A should be supported.
  EXPECT_TRUE(ui::EvdevBitUint64IsSet(key_bits.data(), 30));
  // BTN_A shouldn't be supported.
  EXPECT_FALSE(ui::EvdevBitUint64IsSet(key_bits.data(), 305));
}
