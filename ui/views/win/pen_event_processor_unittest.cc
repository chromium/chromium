// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/pen_event_processor.h"

#include "base/test/task_environment.h"
#include "base/win/scoped_winrt_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/sequential_id_generator.h"

namespace views {

class PenProcessorTest : public ::testing::Test {
 public:
  PenProcessorTest() = default;
  ~PenProcessorTest() override = default;

  // testing::Test overrides.
  void SetUp() override;

 private:
  base::win::ScopedWinrtInitializer scoped_winrt_initializer_;
  base::test::TaskEnvironment task_environment_;
};

void PenProcessorTest::SetUp() {
  ASSERT_TRUE(scoped_winrt_initializer_.Succeeded());
}

TEST_F(PenProcessorTest, TypicalCaseDMDisabled) {
  views::PenIdHandler::ScopedPenIdStaticsForTesting scoper(nullptr);
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ false);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERENTER, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseEntered, event->AsMouseEvent()->type());

  pen_info.pointerInfo.pointerFlags =
      POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;

  event = processor.GenerateEvent(WM_POINTERDOWN, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMousePressed, event->AsMouseEvent()->type());
  EXPECT_EQ(1, event->AsMouseEvent()->GetClickCount());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            event->AsMouseEvent()->changed_button_flags());

  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_NONE;
  event = processor.GenerateEvent(WM_POINTERUPDATE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseDragged, event->AsMouseEvent()->type());

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_INCONTACT;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;
  event = processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseReleased, event->AsMouseEvent()->type());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            event->AsMouseEvent()->changed_button_flags());

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_NONE;
  event = processor.GenerateEvent(WM_POINTERUPDATE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseMoved, event->AsMouseEvent()->type());

  event = processor.GenerateEvent(WM_POINTERLEAVE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseExited, event->AsMouseEvent()->type());
}

TEST_F(PenProcessorTest, TypicalCaseDMEnabled) {
  views::PenIdHandler::ScopedPenIdStaticsForTesting scoper(nullptr);
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  // Set up the modifier state that shift is down so we can test
  // modifiers are propagated for mouse and touch events.
  BYTE restore_key_state[256];
  GetKeyboardState(restore_key_state);
  BYTE shift_key_state[256];
  memset(shift_key_state, 0, sizeof(shift_key_state));
  // Mask high order bit on indicating it is down.
  // See MSDN GetKeyState().
  shift_key_state[VK_SHIFT] |= 0x80;
  SetKeyboardState(shift_key_state);

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERENTER, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseEntered, event->AsMouseEvent()->type());
  EXPECT_TRUE(event->flags() & ui::EF_SHIFT_DOWN);

  pen_info.pointerInfo.pointerFlags =
      POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;

  event = processor.GenerateEvent(WM_POINTERDOWN, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchPressed, event->AsTouchEvent()->type());
  EXPECT_TRUE(event->flags() & ui::EF_SHIFT_DOWN);

  // Restore the keyboard state back to what it was in the beginning.
  SetKeyboardState(restore_key_state);

  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_NONE;
  event = processor.GenerateEvent(WM_POINTERUPDATE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchMoved, event->AsTouchEvent()->type());

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_NONE;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;
  event = processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchReleased, event->AsTouchEvent()->type());

  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_NONE;
  event = processor.GenerateEvent(WM_POINTERUPDATE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseMoved, event->type());

  event = processor.GenerateEvent(WM_POINTERLEAVE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseExited, event->AsMouseEvent()->type());
}

TEST_F(PenProcessorTest, UnpairedPointerDownTouchDMEnabled) {
  views::PenIdHandler::ScopedPenIdStaticsForTesting scoper(nullptr);
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  pen_info.pointerInfo.pointerFlags =
      POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  EXPECT_EQ(nullptr, event.get());
}

TEST_F(PenProcessorTest, UnpairedPointerDownMouseDMEnabled) {
  views::PenIdHandler::ScopedPenIdStaticsForTesting scoper(nullptr);
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  EXPECT_EQ(nullptr, event.get());
}

TEST_F(PenProcessorTest, TouchFlagDMEnabled) {
  views::PenIdHandler::ScopedPenIdStaticsForTesting scoper(nullptr);
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  pen_info.pointerInfo.pointerFlags =
      POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERDOWN, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchPressed, event->AsTouchEvent()->type());
  EXPECT_TRUE(event->flags() & ui::EF_LEFT_MOUSE_BUTTON);

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_UP;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

  event = processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchReleased, event->AsTouchEvent()->type());
  EXPECT_FALSE(event->flags() & ui::EF_LEFT_MOUSE_BUTTON);
}

TEST_F(PenProcessorTest, MouseFlagDMEnabled) {
  views::PenIdHandler::ScopedPenIdStaticsForTesting scoper(nullptr);
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERDOWN, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMousePressed, event->AsMouseEvent()->type());
  EXPECT_TRUE(event->flags() & ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            event->AsMouseEvent()->changed_button_flags());

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_NONE;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

  event = processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseReleased, event->AsMouseEvent()->type());
  EXPECT_TRUE(event->flags() & ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            event->AsMouseEvent()->changed_button_flags());
}

TEST_F(PenProcessorTest, PenEraserFlagDMEnabled) {
  views::PenIdHandler::ScopedPenIdStaticsForTesting scoper(nullptr);
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  pen_info.pointerInfo.pointerFlags =
      POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;
  pen_info.penFlags = PEN_FLAG_ERASER;

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERDOWN, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchPressed, event->AsTouchEvent()->type());
  EXPECT_EQ(ui::EventPointerType::kEraser,
            event->AsTouchEvent()->pointer_details().pointer_type);

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_UP;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

  event = processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchReleased, event->AsTouchEvent()->type());
  EXPECT_EQ(ui::EventPointerType::kEraser,
            event->AsTouchEvent()->pointer_details().pointer_type);
}

TEST_F(PenProcessorTest, MultiPenDMEnabled) {
  views::PenIdHandler::ScopedPenIdStaticsForTesting scoper(nullptr);
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  std::array<POINTER_PEN_INFO, 3> pen_info;
  for (auto& i : pen_info) {
    memset(&i, 0, sizeof(POINTER_PEN_INFO));
  }

  gfx::Point point(100, 100);

  for (size_t i = 0; i < pen_info.size(); i++) {
    pen_info[i].pointerInfo.pointerFlags =
        POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
    pen_info[i].pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;

    size_t pointer_id = i;
    std::unique_ptr<ui::Event> event =
        processor.GenerateEvent(WM_POINTERDOWN, pointer_id, pen_info[i], point);
    ASSERT_TRUE(event);
    ASSERT_TRUE(event->IsTouchEvent());
    EXPECT_EQ(ui::EventType::kTouchPressed, event->AsTouchEvent()->type());
  }

  for (size_t i = 0; i < pen_info.size(); i++) {
    pen_info[i].pointerInfo.pointerFlags = POINTER_FLAG_UP;
    pen_info[i].pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

    size_t pointer_id = i;
    std::unique_ptr<ui::Event> event =
        processor.GenerateEvent(WM_POINTERUP, pointer_id, pen_info[i], point);
    ASSERT_TRUE(event);
    ASSERT_TRUE(event->IsTouchEvent());
    EXPECT_EQ(ui::EventType::kTouchReleased, event->AsTouchEvent()->type());
  }
}

}  // namespace views
