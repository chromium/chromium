// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>
#include <wayland-server.h>
#include <memory>

#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
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
using ::testing::SaveArg;

namespace ui {

class WaylandKeyboardTest : public WaylandTest {
 public:
  WaylandKeyboardTest() {}

  void SetUp() override {
    WaylandTest::SetUp();

    wl_seat_send_capabilities(server_.seat()->resource(),
                              WL_SEAT_CAPABILITY_KEYBOARD);

    Sync();

    keyboard_ = server_.seat()->keyboard();
    ASSERT_TRUE(keyboard_);

#if BUILDFLAG(USE_XKBCOMMON)
    // Set up XKB bits and set the keymap to the client.
    xkb_context_.reset(xkb_context_new(XKB_CONTEXT_NO_FLAGS));
    xkb_keymap_.reset(xkb_keymap_new_from_names(
        xkb_context_.get(), nullptr /*names*/, XKB_KEYMAP_COMPILE_NO_FLAGS));
    xkb_state_.reset(xkb_state_new(xkb_keymap_.get()));

    std::unique_ptr<char, base::FreeDeleter> keymap_string(
        xkb_keymap_get_as_string(xkb_keymap_.get(), XKB_KEYMAP_FORMAT_TEXT_V1));
    DCHECK(keymap_string.get());
    size_t keymap_size = strlen(keymap_string.get()) + 1;

    base::UnsafeSharedMemoryRegion shared_keymap_region =
        base::UnsafeSharedMemoryRegion::Create(keymap_size);
    base::WritableSharedMemoryMapping shared_keymap =
        shared_keymap_region.Map();
    base::subtle::PlatformSharedMemoryRegion platform_shared_keymap =
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(shared_keymap_region));
    DCHECK(shared_keymap.IsValid());

    memcpy(shared_keymap.memory(), keymap_string.get(), keymap_size);
    wl_keyboard_send_keymap(
        keyboard_->resource(), WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
        platform_shared_keymap.GetPlatformHandle().fd, keymap_size);
#endif
  }

 protected:
  wl::TestKeyboard* keyboard_;

 private:
#if BUILDFLAG(USE_XKBCOMMON)
  // The Xkb state used for the keyboard.
  std::unique_ptr<xkb_context, ui::XkbContextDeleter> xkb_context_;
  std::unique_ptr<xkb_keymap, ui::XkbKeymapDeleter> xkb_keymap_;
  std::unique_ptr<xkb_state, ui::XkbStateDeleter> xkb_state_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WaylandKeyboardTest);
};

ACTION_P(CloneEvent, ptr) {
  *ptr = Event::Clone(*arg0);
}

TEST_P(WaylandKeyboardTest, Keypress) {
  struct wl_array empty;
  wl_array_init(&empty);
  wl_keyboard_send_enter(keyboard_->resource(), 1, surface_->resource(),
                         &empty);
  wl_array_release(&empty);

  wl_keyboard_send_key(keyboard_->resource(), 2, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsKeyEvent());

  auto* key_event = event->AsKeyEvent();
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(ET_KEY_PRESSED, key_event->type());

  wl_keyboard_send_leave(keyboard_->resource(), 3, surface_->resource());

  Sync();

  wl_keyboard_send_key(keyboard_->resource(), 3, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
}

TEST_P(WaylandKeyboardTest, AltModifierKeypress) {
  struct wl_array empty;
  wl_array_init(&empty);
  wl_keyboard_send_enter(keyboard_->resource(), 1, surface_->resource(),
                         &empty);
  wl_array_release(&empty);

  // Alt
  wl_keyboard_send_key(keyboard_->resource(), 2, 0, 56 /* left Alt */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsKeyEvent());

  auto* key_event = event->AsKeyEvent();

  EXPECT_EQ(ui::EF_ALT_DOWN, key_event->flags());
  EXPECT_EQ(ui::VKEY_MENU, key_event->key_code());
  EXPECT_EQ(ET_KEY_PRESSED, key_event->type());
}

TEST_P(WaylandKeyboardTest, ControlModifierKeypress) {
  struct wl_array empty;
  wl_array_init(&empty);
  wl_keyboard_send_enter(keyboard_->resource(), 1, surface_->resource(),
                         &empty);
  wl_array_release(&empty);

  // Control
  wl_keyboard_send_key(keyboard_->resource(), 2, 0, 29 /* left Control */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsKeyEvent());

  auto* key_event = event->AsKeyEvent();

  EXPECT_EQ(ui::EF_CONTROL_DOWN, key_event->flags());
  EXPECT_EQ(ui::VKEY_CONTROL, key_event->key_code());
  EXPECT_EQ(ET_KEY_PRESSED, key_event->type());
}

TEST_P(WaylandKeyboardTest, ShiftModifierKeypress) {
  struct wl_array empty;
  wl_array_init(&empty);
  wl_keyboard_send_enter(keyboard_->resource(), 1, surface_->resource(),
                         &empty);
  wl_array_release(&empty);

  // Shift
  wl_keyboard_send_key(keyboard_->resource(), 2, 0, 42 /* left Shift */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsKeyEvent());

  auto* key_event = event->AsKeyEvent();

  EXPECT_EQ(ui::EF_SHIFT_DOWN, key_event->flags());
  EXPECT_EQ(ui::VKEY_SHIFT, key_event->key_code());
  EXPECT_EQ(ET_KEY_PRESSED, key_event->type());
}

TEST_P(WaylandKeyboardTest, ControlShiftModifiers) {
  struct wl_array empty;
  wl_array_init(&empty);
  wl_array_init(&empty);
  wl_keyboard_send_enter(keyboard_->resource(), 1, surface_->resource(),
                         &empty);
  wl_array_release(&empty);

  // Pressing control.
  wl_keyboard_send_key(keyboard_->resource(), 2, 0, 29 /* Control */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  Sync();

  wl_keyboard_send_modifiers(keyboard_->resource(), 3, 4 /* mods_depressed*/,
                             0 /* mods_latched */, 0 /* mods_locked */,
                             0 /* group */);
  Sync();

  // Pressing shift (with control key still held down).
  wl_keyboard_send_key(keyboard_->resource(), 4, 0, 42 /* Shift */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event2;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event2));
  Sync();

  wl_keyboard_send_modifiers(keyboard_->resource(), 5, 5 /* mods_depressed*/,
                             0 /* mods_latched */, 0 /* mods_locked */,
                             0 /* group */);
  Sync();

  // Sending a reguard keypress, eg 'a'.
  wl_keyboard_send_key(keyboard_->resource(), 6, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event3;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event3));
  Sync();

  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsKeyEvent());

  auto* key_event3 = event3->AsKeyEvent();

  EXPECT_EQ(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, key_event3->flags());
  EXPECT_EQ(ui::VKEY_A, key_event3->key_code());
  EXPECT_EQ(ET_KEY_PRESSED, key_event3->type());
}

TEST_P(WaylandKeyboardTest, CapsLockKeypress) {
  struct wl_array empty;
  wl_array_init(&empty);
  wl_array_init(&empty);
  wl_keyboard_send_enter(keyboard_->resource(), 1, surface_->resource(),
                         &empty);
  wl_array_release(&empty);

  // Capslock
  wl_keyboard_send_key(keyboard_->resource(), 2, 0, 58 /* Capslock */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsKeyEvent());

  auto* key_event = event->AsKeyEvent();

  EXPECT_EQ(ui::EF_MOD3_DOWN, key_event->flags());
  EXPECT_EQ(ui::VKEY_CAPITAL, key_event->key_code());
  EXPECT_EQ(ET_KEY_PRESSED, key_event->type());

  Sync();

  wl_keyboard_send_key(keyboard_->resource(), 2, 0, 58 /* Capslock */,
                       WL_KEYBOARD_KEY_STATE_RELEASED);

  std::unique_ptr<Event> event2;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event2));

  Sync();
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsKeyEvent());

  auto* key_event2 = event2->AsKeyEvent();

  EXPECT_EQ(0, key_event2->flags());
  EXPECT_EQ(ui::VKEY_CAPITAL, key_event2->key_code());
  EXPECT_EQ(ET_KEY_RELEASED, key_event2->type());
}

#if BUILDFLAG(USE_XKBCOMMON)
TEST_P(WaylandKeyboardTest, CapsLockModifier) {
  struct wl_array empty;
  wl_array_init(&empty);
  wl_array_init(&empty);
  wl_keyboard_send_enter(keyboard_->resource(), 1, surface_->resource(),
                         &empty);
  wl_array_release(&empty);

  // Pressing capslock (led ON).
  wl_keyboard_send_key(keyboard_->resource(), 2, 0, 58 /* Capslock */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  Sync();

  wl_keyboard_send_modifiers(keyboard_->resource(), 3, 2 /* mods_depressed*/,
                             0 /* mods_latched */, 2 /* mods_locked */,
                             0 /* group */);
  Sync();

  // Releasing capslock (led ON).
  wl_keyboard_send_key(keyboard_->resource(), 4, 0, 58 /* Capslock */,
                       WL_KEYBOARD_KEY_STATE_RELEASED);

  std::unique_ptr<Event> event2;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event2));
  Sync();

  wl_keyboard_send_modifiers(keyboard_->resource(), 5, 0 /* mods_depressed*/,
                             0 /* mods_latched */, 2 /* mods_locked */,
                             0 /* group */);
  Sync();

  // Sending a reguard keypress, eg 'a'.
  wl_keyboard_send_key(keyboard_->resource(), 6, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event3;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event3));
  Sync();

  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsKeyEvent());

  auto* key_event3 = event3->AsKeyEvent();

  EXPECT_EQ(ui::EF_CAPS_LOCK_ON, key_event3->flags());
  EXPECT_EQ(ui::VKEY_A, key_event3->key_code());
  EXPECT_EQ(ET_KEY_PRESSED, key_event3->type());
}
#endif

TEST_P(WaylandKeyboardTest, EventAutoRepeat) {
  struct wl_array empty;
  wl_array_init(&empty);

  wl_keyboard_send_enter(keyboard_->resource(), 1, surface_->resource(),
                         &empty);
  wl_array_release(&empty);

  // Auto repeat info in ms.
  uint32_t rate = 75;
  uint32_t delay = 25;

  wl_keyboard_send_repeat_info(keyboard_->resource(), rate, delay);

  wl_keyboard_send_key(keyboard_->resource(), 2, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();

  {
    scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner =
        new base::TestMockTimeTaskRunner();
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(
        mock_time_task_runner.get());

    // Keep it pressed for doubled the rate time, to ensure events get fired.
    mock_time_task_runner->FastForwardBy(
        base::TimeDelta::FromMilliseconds(rate * 2));
  }

  Sync();

  std::unique_ptr<Event> event2;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event2));

  wl_keyboard_send_key(keyboard_->resource(), 3, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_RELEASED);
  Sync();
}

TEST_P(WaylandKeyboardTest, NoEventAutoRepeatOnLeave) {
  struct wl_array empty;
  wl_array_init(&empty);

  wl_keyboard_send_enter(keyboard_->resource(), 1, surface_->resource(),
                         &empty);
  wl_array_release(&empty);

  // Auto repeat info in ms.
  uint32_t rate = 75;
  uint32_t delay = 25;

  wl_keyboard_send_repeat_info(keyboard_->resource(), rate, delay);

  wl_keyboard_send_key(keyboard_->resource(), 2, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();

  {
    scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner =
        new base::TestMockTimeTaskRunner();
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(
        mock_time_task_runner.get());

    // Keep it pressed for doubled the rate time, to ensure events get fired.
    mock_time_task_runner->FastForwardBy(
        base::TimeDelta::FromMilliseconds(rate * 2));
  }

  wl_keyboard_send_leave(keyboard_->resource(), 3, surface_->resource());

  Sync();

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);

  wl_keyboard_send_key(keyboard_->resource(), 4, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_RELEASED);
  Sync();
}

TEST_P(WaylandKeyboardTest, NoEventAutoRepeatBeforeTimeout) {
  struct wl_array empty;
  wl_array_init(&empty);

  wl_keyboard_send_enter(keyboard_->resource(), 1, surface_->resource(),
                         &empty);
  wl_array_release(&empty);

  // Auto repeat info in ms.
  uint32_t rate = 500;
  uint32_t delay = 50;

  wl_keyboard_send_repeat_info(keyboard_->resource(), rate, delay);

  wl_keyboard_send_key(keyboard_->resource(), 2, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();

  {
    scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner =
        new base::TestMockTimeTaskRunner();
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(
        mock_time_task_runner.get());

    // Keep it pressed for a fifth of the rate time, and no auto repeat events
    // should get dispatched.
    mock_time_task_runner->FastForwardBy(
        base::TimeDelta::FromMilliseconds(rate / 5));
  }

  wl_keyboard_send_key(keyboard_->resource(), 4, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_RELEASED);

  std::unique_ptr<Event> event2;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event2));

  Sync();
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsKeyEvent());

  auto* key_event2 = event2->AsKeyEvent();
  EXPECT_EQ(ET_KEY_RELEASED, key_event2->type());
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandKeyboardTest,
                         ::testing::Values(kXdgShellStable));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandKeyboardTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
