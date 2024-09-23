// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>
#include <wayland-server.h>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "base/memory/free_deleter.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ui/events/keycodes/scoped_xkb.h"  // nogncheck
#endif

using ::testing::_;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Values;

namespace ui {

class WaylandKeyboardTest : public WaylandTestSimple {
 public:
  void SetUp() override {
    WaylandTestSimple::SetUp();

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      wl_seat_send_capabilities(server->seat()->resource(),
                                WL_SEAT_CAPABILITY_KEYBOARD);
    });
    ASSERT_TRUE(connection_->seat()->keyboard());

    EXPECT_EQ(1u,
              DeviceDataManager::GetInstance()->GetKeyboardDevices().size());

    MaybeSetUpXkb();
  }

 protected:
  // Enters the keyboard into the surface.
  void SendEnter(int32_t rate, int32_t delay) {
    PostToServerAndWait(
        [rate, delay, surface_id = window_->root_surface()->get_surface_id()](
            wl::TestWaylandServerThread* server) {
          auto* const keyboard = server->seat()->keyboard()->resource();
          auto* const surface =
              server->GetObject<wl::MockSurface>(surface_id)->resource();

          wl::ScopedWlArray empty({});
          wl_keyboard_send_enter(keyboard, server->GetNextSerial(), surface,
                                 empty.get());

          wl_keyboard_send_repeat_info(keyboard, rate, delay);
        });
  }

  void SendEnter() {
    // Set "no repeat" so that events would be "squashed" with the modifiers,
    // otherwise the behaviour is not determined and we cannot set stable
    // expectations.
    SendEnter(0, 0);
  }
};

ACTION_P(CloneEvent, ptr) {
  *ptr = arg0->Clone();
}

ACTION_P(AppendEvent, ptr) {
  ptr->emplace_back(arg0->Clone());
}

ACTION_P(AppendEventAndQuitLoop, ptr, event_count, closure) {
  ptr->emplace_back(arg0->Clone());
  if (ptr->size() == event_count)
    closure.Run();
}

TEST_F(WaylandKeyboardTest, Keypress) {
  SendEnter();

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 30 /* a */,
                         WL_KEYBOARD_KEY_STATE_PRESSED);

    wl_keyboard_send_leave(keyboard, server->GetNextSerial(), surface);
  });

  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsKeyEvent());

  auto* key_event = event->AsKeyEvent();
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(EventType::kKeyPressed, key_event->type());

  // The window no longer has keyboard focus, and the below should not result in
  // receiving more events.  The expectation set in the beginning of the test
  // should not over-saturate.
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();

    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 30 /* a */,
                         WL_KEYBOARD_KEY_STATE_PRESSED);
  });
}

TEST_F(WaylandKeyboardTest, ControlShiftModifiers) {
  SendEnter();

  std::vector<std::unique_ptr<Event>> events;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(3)
      .WillRepeatedly(AppendEvent(&events));

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();

    // Pressing control.
    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 29 /* Control */,
                         WL_KEYBOARD_KEY_STATE_PRESSED);

    wl_keyboard_send_modifiers(keyboard, server->GetNextSerial(),
                               4 /* mods_depressed*/, 0 /* mods_latched */,
                               0 /* mods_locked */, 0 /* group */);

    // Pressing shift (with control key still held down).
    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 42 /* Shift */,
                         WL_KEYBOARD_KEY_STATE_PRESSED);

    wl_keyboard_send_modifiers(keyboard, server->GetNextSerial(),
                               5 /* mods_depressed*/, 0 /* mods_latched */,
                               0 /* mods_locked */, 0 /* group */);

    // Sending a reguard keypress, eg 'a'.
    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 30 /* a */,
                         WL_KEYBOARD_KEY_STATE_PRESSED);
  });

  auto& last_event = events[events.size() - 1];
  ASSERT_TRUE(last_event);
  ASSERT_TRUE(last_event->IsKeyEvent());

  auto* key_event = last_event->AsKeyEvent();

  EXPECT_EQ(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, key_event->flags());
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(EventType::kKeyPressed, key_event->type());
}

#if BUILDFLAG(USE_XKBCOMMON)
TEST_F(WaylandKeyboardTest, CapsLockModifier) {
  SendEnter();

  std::vector<std::unique_ptr<Event>> events;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .Times(3)
      .WillRepeatedly(AppendEvent(&events));

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();

    // Pressing capslock (led ON).
    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 58 /* Capslock */,
                         WL_KEYBOARD_KEY_STATE_PRESSED);

    wl_keyboard_send_modifiers(keyboard, server->GetNextSerial(),
                               2 /* mods_depressed*/, 0 /* mods_latched */,
                               2 /* mods_locked */, 0 /* group */);

    // Releasing capslock (led ON).
    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 58 /* Capslock */,
                         WL_KEYBOARD_KEY_STATE_RELEASED);

    wl_keyboard_send_modifiers(keyboard, server->GetNextSerial(),
                               0 /* mods_depressed*/, 0 /* mods_latched */,
                               2 /* mods_locked */, 0 /* group */);

    // Sending a reguard keypress, eg 'a'.
    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 30 /* a */,
                         WL_KEYBOARD_KEY_STATE_PRESSED);
  });

  auto& last_event = events[events.size() - 1];
  ASSERT_TRUE(last_event);
  ASSERT_TRUE(last_event->IsKeyEvent());

  auto* key_event = last_event->AsKeyEvent();

  EXPECT_EQ(ui::EF_CAPS_LOCK_ON, key_event->flags());
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(EventType::kKeyPressed, key_event->type());
}
#endif

TEST_F(WaylandKeyboardTest, EventAutoRepeat) {
  constexpr int32_t rate = 5;    // num key events per second.
  constexpr int32_t delay = 60;  // in milliseconds.

  SendEnter(rate, delay);

  // Press the key and wait for two automatically repeated events to come.
  base::RunLoop run_loop;
  std::vector<std::unique_ptr<Event>> events;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .WillRepeatedly(
          AppendEventAndQuitLoop(&events, 3u, run_loop.QuitClosure()));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();

    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 30 /* a */,
                         WL_KEYBOARD_KEY_STATE_PRESSED);
  });
  run_loop.Run();
  Mock::VerifyAndClearExpectations(&delegate_);

  // Release the key.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();

    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 30 /* a */,
                         WL_KEYBOARD_KEY_STATE_RELEASED);
  });

  ASSERT_EQ(events.size(), 3u);

  const auto press_timestamp = events[0]->time_stamp();

  auto check_repeat_event = [](const Event& event) {
    EXPECT_EQ(EventType::kKeyPressed, event.type());
    EXPECT_TRUE(event.flags() & EF_IS_REPEAT);
    EXPECT_EQ(KeyboardCode::VKEY_A, event.AsKeyEvent()->key_code());
  };

  // The first key repeat event happens after |delay| milliseconds.
  const auto& first_repeat_event = *events[1];
  check_repeat_event(first_repeat_event);
  const auto first_repeat_delay =
      first_repeat_event.time_stamp() - press_timestamp;
  EXPECT_EQ(first_repeat_delay.InMilliseconds(), delay);

  // The next key event happens after 1/|rate| seconds.
  const auto& second_repeat_event = *events[2];
  check_repeat_event(second_repeat_event);
  const auto second_repeat_delay =
      second_repeat_event.time_stamp() - press_timestamp - first_repeat_delay;
  EXPECT_EQ(second_repeat_delay.InMilliseconds(), 1000 / rate);
}

TEST_F(WaylandKeyboardTest, NoEventAutoRepeatOnLeave) {
  constexpr int32_t rate = 5;    // num key events per second.
  constexpr int32_t delay = 60;  // in milliseconds.

  SendEnter(rate, delay);

  // Press the key and wait for one automatically repeated event to come.
  base::RunLoop run_loop;
  std::vector<std::unique_ptr<Event>> events;
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .WillRepeatedly(
          AppendEventAndQuitLoop(&events, 2u, run_loop.QuitClosure()));

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();

    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 30 /* a */,
                         WL_KEYBOARD_KEY_STATE_PRESSED);
  });
  run_loop.Run();
  Mock::VerifyAndClearExpectations(&delegate_);

  // Then leave.
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    wl_keyboard_send_leave(keyboard, server->GetNextSerial(), surface);
  });

  // Check the first key repeating event.
  const auto& first_repeat_event = *events[1];
  EXPECT_EQ(EventType::kKeyPressed, first_repeat_event.type());
  EXPECT_TRUE(first_repeat_event.flags() & EF_IS_REPEAT);
  EXPECT_EQ(KeyboardCode::VKEY_A, first_repeat_event.AsKeyEvent()->key_code());

  // The window no longer has keyboard focus, and the below should not result in
  // receiving more events.
  EXPECT_CALL(delegate_, DispatchEvent(NotNull())).Times(0);

  task_environment_.FastForwardBy(base::Milliseconds(1000));
  Mock::VerifyAndClearExpectations(&delegate_);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const keyboard = server->seat()->keyboard()->resource();

    wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                         server->GetNextTime(), 30 /* a */,
                         WL_KEYBOARD_KEY_STATE_RELEASED);
  });
}

// This test verifies the following scenario:
//
// 1/ press and hold a modifier key (in this case SHIFT, ALT, CTRL);
// 2/ ensure that no auto-repeat gets triggered;
// 3/ with the modifier key still pressed, press another
//    key (in this case 'a');
// 4/ ensure that they auto-repeat logic gets started and
//    the modifier key is properly handled as part of the
//    event construction.
TEST_F(WaylandKeyboardTest, NoEventAutoRepeatForModifiers) {
  constexpr int32_t rate = 5;    // num key events per second.
  constexpr int32_t delay = 60;  // in milliseconds.

  SendEnter(rate, delay);

  const struct {
    int evdev_key;
    KeyboardCode key_code;
    int mods_depressed;
    int modifier;
  } kIncomeModifiersAndExpectedResults[] = {
      {42 /*shift*/, VKEY_SHIFT, 1, EF_SHIFT_DOWN},
      {29 /*ctrl*/, VKEY_CONTROL, 4, EF_CONTROL_DOWN},
      {56 /*alt*/, VKEY_MENU, 8, EF_ALT_DOWN},
  };

  for (auto values : kIncomeModifiersAndExpectedResults) {
    // Press a modifier key and wait 1s, to ensure no repeated events come.
    base::RunLoop run_loop;
    std::vector<std::unique_ptr<Event>> events;
    EXPECT_CALL(delegate_, DispatchEvent(_))
        .WillRepeatedly(AppendEvent(&events));

    PostToServerAndWait([&values](wl::TestWaylandServerThread* server) {
      auto* const keyboard = server->seat()->keyboard()->resource();

      wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                           server->GetNextTime(), values.evdev_key,
                           WL_KEYBOARD_KEY_STATE_PRESSED);
      wl_keyboard_send_modifiers(keyboard, server->GetNextSerial(),
                                 values.mods_depressed /* mods_depressed*/,
                                 0 /* mods_latched */, 0 /* mods_locked */,
                                 0 /* group */);
    });

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(1000));
    run_loop.Run();
    Mock::VerifyAndClearExpectations(&delegate_);

    // Ensure that only the modifier key down event is processed, ie no auto
    // repeat is triggered.
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0]->AsKeyEvent()->key_code(), values.key_code);

    // With the modifier key still held, press another key ('a' in this case).
    std::vector<std::unique_ptr<Event>> events2;
    base::RunLoop run_loop2;
    EXPECT_CALL(delegate_, DispatchEvent(_))
        .WillRepeatedly(
            AppendEventAndQuitLoop(&events2, 5u, run_loop2.QuitClosure()));
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto* const keyboard = server->seat()->keyboard()->resource();

      wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                           server->GetNextTime(), 30 /* a */,
                           WL_KEYBOARD_KEY_STATE_PRESSED);
    });
    run_loop2.Run();

    // Ensure that 4 consecutive auto-repeat key press events are dispatched,
    // with the proper modifier key.
    auto check_repeat_event = [&values](const Event& event) {
      EXPECT_EQ(EventType::kKeyPressed, event.type());
      EXPECT_TRUE(event.flags() & (EF_IS_REPEAT | values.modifier));
      EXPECT_EQ(KeyboardCode::VKEY_A, event.AsKeyEvent()->key_code());
    };
    for (size_t i = 1; i < events2.size(); i++) {
      const auto& repeat_event = *events2[i];
      check_repeat_event(repeat_event);
    }

    // Release the keys.
    Mock::VerifyAndClearExpectations(&delegate_);

    PostToServerAndWait([&values](wl::TestWaylandServerThread* server) {
      auto* const keyboard = server->seat()->keyboard()->resource();

      wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                           server->GetNextTime(), values.evdev_key,
                           WL_KEYBOARD_KEY_STATE_RELEASED);
      wl_keyboard_send_key(keyboard, server->GetNextSerial(),
                           server->GetNextTime(), 30 /* a */,
                           WL_KEYBOARD_KEY_STATE_RELEASED);
    });
  }
}

}  // namespace ui
