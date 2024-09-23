// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/selection_controller.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/render_text.h"
#include "ui/views/metrics.h"
#include "ui/views/selection_controller_delegate.h"
#include "ui/views/style/platform_style.h"

namespace views {
namespace {

const gfx::Point CenterLeft(const gfx::Rect& rect) {
  return gfx::Point(rect.x(), rect.CenterPoint().y());
}

const gfx::Point CenterRight(const gfx::Rect& rect) {
  return gfx::Point(rect.right(), rect.CenterPoint().y());
}

class TestSelectionControllerDelegate : public SelectionControllerDelegate {
 public:
  explicit TestSelectionControllerDelegate(gfx::RenderText* render_text)
      : render_text_(render_text) {}

  TestSelectionControllerDelegate(const TestSelectionControllerDelegate&) =
      delete;
  TestSelectionControllerDelegate& operator=(
      const TestSelectionControllerDelegate&) = delete;

  ~TestSelectionControllerDelegate() override = default;

  gfx::RenderText* GetRenderTextForSelectionController() override {
    return render_text_;
  }

  bool IsReadOnly() const override { return true; }
  bool SupportsDrag() const override { return true; }
  bool HasTextBeingDragged() const override { return false; }
  void SetTextBeingDragged(bool value) override {}
  int GetViewHeight() const override {
    return render_text_->GetStringSize().height();
  }
  int GetViewWidth() const override {
    return render_text_->GetStringSize().width();
  }
  int GetDragSelectionDelay() const override { return 0; }
  void OnBeforePointerAction() override {}
  void OnAfterPointerAction(bool text_changed,
                            bool selection_changed) override {}
  bool PasteSelectionClipboard() override { return false; }
  void UpdateSelectionClipboard() override {}

 private:
  raw_ptr<gfx::RenderText> render_text_;
};

class SelectionControllerTest : public ::testing::Test {
 public:
  void SetUp() override {
    render_text_ = gfx::RenderText::CreateRenderText();
    delegate_ =
        std::make_unique<TestSelectionControllerDelegate>(render_text_.get());
    controller_ = std::make_unique<SelectionController>(delegate_.get());
  }

  SelectionControllerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  SelectionControllerTest(const SelectionControllerTest&) = delete;
  SelectionControllerTest& operator=(const SelectionControllerTest&) = delete;

  ~SelectionControllerTest() override = default;

  void SetText(const std::string& text) {
    render_text_->SetText(base::ASCIIToUTF16(text));
  }

  std::string GetSelectedText() {
    return base::UTF16ToASCII(
        render_text_->GetTextFromRange(render_text_->selection()));
  }

  void LeftMouseDown(const gfx::Point& location, bool focused = false) {
    PressMouseButton(location, ui::EF_LEFT_MOUSE_BUTTON, focused);
  }

  void LeftMouseUp() { ReleaseMouseButton(ui::EF_LEFT_MOUSE_BUTTON); }

  void DragMouse(const gfx::Point& location) {
    mouse_location_ = location;
    controller_->OnMouseDragged(
        ui::MouseEvent(ui::EventType::kMouseDragged, location, location,
                       last_event_time_, mouse_flags_, 0));
  }

  void RightMouseDown(const gfx::Point& location, bool focused = false) {
    PressMouseButton(location, ui::EF_RIGHT_MOUSE_BUTTON, focused);
  }

  void RightMouseUp() { ReleaseMouseButton(ui::EF_RIGHT_MOUSE_BUTTON); }

  const gfx::Rect BoundsOfChar(int index) {
    return render_text_->GetSubstringBounds(gfx::Range(index, index + 1))[0];
  }

  gfx::Point TranslatePointX(const gfx::Point& point, int delta) {
    return point + gfx::Vector2d(delta, 0);
  }

 private:
  void PressMouseButton(const gfx::Point& location, int button, bool focused) {
    DCHECK(!(mouse_flags_ & button));
    mouse_flags_ |= button;
    mouse_location_ = location;
    // Ensure that mouse presses are spaced apart by at least the double-click
    // interval to avoid triggering a double-click.
    last_event_time_ += base::Milliseconds(views::GetDoubleClickInterval() + 1);
    controller_->OnMousePressed(
        ui::MouseEvent(ui::EventType::kMousePressed, location, location,
                       last_event_time_, mouse_flags_, button),
        false,
        focused
            ? SelectionController::InitialFocusStateOnMousePress::kFocused
            : SelectionController::InitialFocusStateOnMousePress::kUnFocused);
  }

  void ReleaseMouseButton(int button) {
    DCHECK(mouse_flags_ & button);
    mouse_flags_ &= ~button;
    controller_->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMouseReleased, mouse_location_, mouse_location_,
        last_event_time_, mouse_flags_, button));
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<gfx::RenderText> render_text_;
  std::unique_ptr<TestSelectionControllerDelegate> delegate_;
  std::unique_ptr<SelectionController> controller_;

  int mouse_flags_ = 0;
  gfx::Point mouse_location_;
  base::TimeTicks last_event_time_;
};

TEST_F(SelectionControllerTest, ClickAndDragToSelect) {
  SetText("abc def");
  EXPECT_EQ("", GetSelectedText());

  LeftMouseDown(CenterLeft(BoundsOfChar(0)));
  DragMouse(CenterRight(BoundsOfChar(0)));
  EXPECT_EQ("a", GetSelectedText());

  DragMouse(CenterRight(BoundsOfChar(2)));
  EXPECT_EQ("abc", GetSelectedText());

  LeftMouseUp();
  EXPECT_EQ("abc", GetSelectedText());

  LeftMouseDown(CenterRight(BoundsOfChar(3)));
  EXPECT_EQ("", GetSelectedText());

  DragMouse(CenterRight(BoundsOfChar(4)));
  EXPECT_EQ("d", GetSelectedText());
}

TEST_F(SelectionControllerTest, RightClickWhenUnfocused) {
  SetText("abc def");

  RightMouseDown(CenterRight(BoundsOfChar(0)));
  if (PlatformStyle::kSelectAllOnRightClickWhenUnfocused)
    EXPECT_EQ("abc def", GetSelectedText());
  else
    EXPECT_EQ("", GetSelectedText());
}

TEST_F(SelectionControllerTest, RightClickSelectsWord) {
  SetText("abc def");
  RightMouseDown(CenterRight(BoundsOfChar(5)), true);
  if (PlatformStyle::kSelectWordOnRightClick)
    EXPECT_EQ("def", GetSelectedText());
  else
    EXPECT_EQ("", GetSelectedText());
}

// Regression test for https://crbug.com/856609
TEST_F(SelectionControllerTest, RightClickPastEndDoesntSelectLastWord) {
  SetText("abc def");

  RightMouseDown(CenterRight(BoundsOfChar(6)), true);
  EXPECT_EQ("", GetSelectedText());
}

// Regression test for https://crbug.com/731252
// This test validates that drags which are:
//   a) Above or below the text, and
//   b) Past one end of the text
// behave properly with regard to RenderText::kDragToEndIfOutsideVerticalBounds.
// When that option is true, drags outside the text that are horizontally
// "towards" the text should select all of it; when that option is false, those
// drags should have no effect.
TEST_F(SelectionControllerTest, DragPastEndUsesProperOrigin) {
  SetText("abc def");

  gfx::Point point = BoundsOfChar(6).top_right() + gfx::Vector2d(100, -10);

  LeftMouseDown(point);
  EXPECT_EQ("", GetSelectedText());

  DragMouse(TranslatePointX(point, -1));
  if (gfx::RenderText::kDragToEndIfOutsideVerticalBounds)
    EXPECT_EQ("abc def", GetSelectedText());
  else
    EXPECT_EQ("", GetSelectedText());

  DragMouse(TranslatePointX(point, 1));
  EXPECT_EQ("", GetSelectedText());
}

}  // namespace
}  // namespace views
