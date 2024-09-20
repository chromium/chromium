// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_controller_impl.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"
#include "ui/touch_selection/touch_selection_metrics.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;

namespace {
// Should match kSelectionHandleBarMinHeight in touch_selection_controller.
const int kBarMinHeight = 5;

// Should match kSelectionHandleBarBottomAllowance in
// touch_selection_controller.
const int kBarBottomAllowance = 3;

// For selection bounds |b1| and |b2| in a paragraph of text, returns -1 if |b1|
// is physically before |b2|, +1 if |b2| is before |b1|, and 0 if they are at
// the same location.
int CompareTextSelectionBounds(const gfx::SelectionBound& b1,
                               const gfx::SelectionBound& b2) {
  if (b1.edge_start().y() < b2.edge_start().y() ||
      b1.edge_start().x() < b2.edge_start().x()) {
    return -1;
  }
  if (b1 == b2)
    return 0;
  return 1;
}

}  // namespace

namespace views {

class TouchSelectionControllerImplTest : public ViewsTestBase {
 public:
  TouchSelectionControllerImplTest() = default;

  TouchSelectionControllerImplTest(const TouchSelectionControllerImplTest&) =
      delete;
  TouchSelectionControllerImplTest& operator=(
      const TouchSelectionControllerImplTest&) = delete;

  ~TouchSelectionControllerImplTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    test_cursor_client_ =
        std::make_unique<aura::test::TestCursorClient>(GetContext());
  }

  void TearDown() override {
    textfield_ = nullptr;
    test_cursor_client_.reset();

    auto close_widget = [](std::unique_ptr<Widget>& widget) {
      if (widget && !widget->IsClosed()) {
        widget->Close();
      }
    };
    close_widget(textfield_widget_);
    close_widget(widget_);

    ViewsTestBase::TearDown();
  }

  void CreateTextfield() {
    textfield_widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    textfield_widget_->Init(std::move(params));
    // Focusable views must have an accessible name in order to pass the
    // accessibility paint checks. The name can be literal text, placeholder
    // text or an associated label.
    textfield_ = textfield_widget_->SetContentsView(std::make_unique<View>())
                     ->AddChildView(Builder<Textfield>()
                                        .SetPlaceholderText(u"Foo")
                                        .SetID(1)
                                        .SetBoundsRect(gfx::Rect(0, 0, 200, 21))
                                        .Build());

    textfield_widget_->Show();
    textfield_->RequestFocus();
  }

  void CreateWidget() {
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(params));
    widget_->Show();
  }

 protected:
  static bool IsCursorHandleVisibleFor(TouchSelectionController* controller) {
    TouchSelectionControllerImpl* impl =
        static_cast<TouchSelectionControllerImpl*>(controller);
    return impl->IsCursorHandleVisible();
  }

  gfx::Rect GetCursorRect(const gfx::SelectionModel& sel) {
    return TextfieldTestApi(textfield_)
        .GetRenderText()
        ->GetCursorBounds(sel, true);
  }

  gfx::Point GetCursorPosition(const gfx::SelectionModel& sel) {
    return GetCursorRect(sel).origin();
  }

  TouchSelectionControllerImpl* GetSelectionController() {
    return static_cast<TouchSelectionControllerImpl*>(
        TextfieldTestApi(textfield_).touch_selection_controller());
  }

  void StartTouchEditing() {
    TextfieldTestApi(textfield_).CreateTouchSelectionControllerAndNotifyIt();
  }

  void EndTouchEditing() {
    TextfieldTestApi(textfield_).ResetTouchSelectionController();
  }

  void SimulateSelectionHandleDrag(gfx::Vector2d v, int selection_handle) {
    TouchSelectionControllerImpl* controller = GetSelectionController();
    views::View* handle = nullptr;
    if (selection_handle == 1)
      handle = controller->GetHandle1View();
    else
      handle = controller->GetHandle2View();

    gfx::Point grip_location =
        gfx::Point(handle->size().width() / 2, handle->size().height() / 2);
    base::TimeTicks time_stamp = base::TimeTicks();
    {
      ui::GestureEventDetails details(ui::EventType::kGestureScrollBegin);
      ui::GestureEvent scroll_begin(grip_location.x(), grip_location.y(), 0,
                                    time_stamp, details);
      handle->OnGestureEvent(&scroll_begin);
    }
    test_cursor_client_->DisableMouseEvents();
    {
      ui::GestureEventDetails details(ui::EventType::kGestureScrollUpdate);
      gfx::Point update_location = grip_location + v;
      ui::GestureEvent scroll_update(update_location.x(), update_location.y(),
                                     0, time_stamp, details);
      handle->OnGestureEvent(&scroll_update);
    }
    {
      ui::GestureEventDetails details(ui::EventType::kGestureScrollEnd);
      ui::GestureEvent scroll_end(grip_location.x(), grip_location.y(), 0,
                                  time_stamp, details);
      handle->OnGestureEvent(&scroll_end);
    }
    test_cursor_client_->EnableMouseEvents();
  }

  gfx::NativeView GetCursorHandleNativeView() {
    return GetSelectionController()->GetCursorHandleNativeView();
  }

  gfx::SelectionBound::Type GetSelectionHandle1Type() {
    return GetSelectionController()->GetSelectionHandle1Type();
  }

  gfx::Rect GetSelectionHandle1Bounds() {
    return GetSelectionController()->GetSelectionHandle1Bounds();
  }

  gfx::Rect GetSelectionHandle2Bounds() {
    return GetSelectionController()->GetSelectionHandle2Bounds();
  }

  gfx::Rect GetCursorHandleBounds() {
    return GetSelectionController()->GetCursorHandleBounds();
  }

  gfx::Rect GetExpectedHandleBounds(const gfx::SelectionBound& bound) {
    return GetSelectionController()->GetExpectedHandleBounds(bound);
  }

  bool IsSelectionHandle1Visible() {
    return GetSelectionController()->IsSelectionHandle1Visible();
  }

  bool IsSelectionHandle2Visible() {
    return GetSelectionController()->IsSelectionHandle2Visible();
  }

  bool IsCursorHandleVisible() {
    return GetSelectionController()->IsCursorHandleVisible();
  }

  bool IsQuickMenuVisible() {
    TouchSelectionControllerImpl* controller = GetSelectionController();
    if (controller && controller->quick_menu_requested_) {
      controller->ShowQuickMenuImmediatelyForTesting();
    }
    return ui::TouchSelectionMenuRunner::GetInstance() &&
           ui::TouchSelectionMenuRunner::GetInstance()->IsRunning();
  }

  bool IsMagnifierVisible() {
    return GetSelectionController()->touch_selection_magnifier_ != nullptr;
  }

  gfx::RenderText* GetRenderText() {
    return TextfieldTestApi(textfield_).GetRenderText();
  }

  gfx::Point GetCursorHandleDragPoint() {
    gfx::Rect rect = GetCursorHandleBounds();
    const gfx::SelectionModel& sel = textfield_->GetSelectionModel();
    int cursor_height = GetCursorRect(sel).height();
    gfx::Point point = rect.CenterPoint();
    point.Offset(0, cursor_height);
    return point;
  }

  // If textfield has selection, this verifies that the selection handles
  // are visible, at the correct positions (at the end points of selection), and
  // (if |check_direction| is set to true), that they have the correct
  // directionality.
  // |cursor_at_selection_handle_1| is used to decide whether selection
  // handle 1's position is matched against the start of selection or the end.
  void VerifyHandlePositions(bool cursor_at_selection_handle_1,
                             bool check_direction,
                             const base::Location& from_here) {
    gfx::SelectionBound anchor, focus;
    textfield_->GetSelectionEndPoints(&anchor, &focus);
    std::string from_str = from_here.ToString();
    if (textfield_->HasSelection()) {
      EXPECT_TRUE(IsSelectionHandle1Visible()) << from_str;
      EXPECT_TRUE(IsSelectionHandle2Visible()) << from_str;
      EXPECT_FALSE(IsCursorHandleVisible());
      gfx::Rect sh1_bounds = GetSelectionHandle1Bounds();
      gfx::Rect sh2_bounds = GetSelectionHandle2Bounds();
      if (cursor_at_selection_handle_1) {
        EXPECT_EQ(sh1_bounds, GetExpectedHandleBounds(focus)) << from_str;
        EXPECT_EQ(sh2_bounds, GetExpectedHandleBounds(anchor)) << from_str;
      } else {
        EXPECT_EQ(sh1_bounds, GetExpectedHandleBounds(anchor)) << from_str;
        EXPECT_EQ(sh2_bounds, GetExpectedHandleBounds(focus)) << from_str;
      }
    } else {
      EXPECT_FALSE(IsSelectionHandle1Visible()) << from_str;
      EXPECT_FALSE(IsSelectionHandle2Visible()) << from_str;
      EXPECT_TRUE(IsCursorHandleVisible());
      gfx::Rect cursor_bounds = GetCursorHandleBounds();
      DCHECK(anchor == focus);
      EXPECT_EQ(cursor_bounds, GetExpectedHandleBounds(anchor)) << from_str;
    }
    if (check_direction) {
      if (CompareTextSelectionBounds(anchor, focus) < 0) {
        EXPECT_EQ(gfx::SelectionBound::LEFT, anchor.type()) << from_str;
        EXPECT_EQ(gfx::SelectionBound::RIGHT, focus.type()) << from_str;
      } else if (CompareTextSelectionBounds(anchor, focus) > 0) {
        EXPECT_EQ(gfx::SelectionBound::LEFT, focus.type()) << from_str;
        EXPECT_EQ(gfx::SelectionBound::RIGHT, anchor.type()) << from_str;
      } else {
        EXPECT_EQ(gfx::SelectionBound::CENTER, focus.type()) << from_str;
        EXPECT_EQ(gfx::SelectionBound::CENTER, anchor.type()) << from_str;
      }
    }
  }

  // Sets up a textfield with a long text string such that it doesn't all fit
  // into the textfield. Then selects the text - the first handle is expected
  // to be invisible. |selection_start| is the position of the first handle.
  void SetupSelectionInvisibleHandle(uint32_t selection_start) {
    // Create a textfield with lots of text in it.
    CreateTextfield();
    std::string some_text("some text");
    std::string textfield_text;
    for (int i = 0; i < 10; ++i)
      textfield_text += some_text;
    textfield_->SetText(ASCIIToUTF16(textfield_text));

    // Tap the textfield to invoke selection.
    ui::GestureEventDetails details(ui::EventType::kGestureTap);
    details.set_tap_count(1);
    ui::GestureEvent tap(0, 0, 0, base::TimeTicks(), details);
    textfield_->OnGestureEvent(&tap);

    // Select some text such that one handle is hidden.
    textfield_->SetSelectedRange(gfx::Range(
        selection_start, static_cast<uint32_t>(textfield_text.length())));

    // Check that one selection handle is hidden.
    EXPECT_FALSE(IsSelectionHandle1Visible());
    EXPECT_TRUE(IsSelectionHandle2Visible());
    EXPECT_EQ(gfx::Range(selection_start,
                         static_cast<uint32_t>(textfield_text.length())),
              textfield_->GetSelectedRange());
  }

  void CloseHandle1Widget() {
    GetSelectionController()->GetHandle1View()->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }

  std::unique_ptr<Widget> textfield_widget_;
  std::unique_ptr<Widget> widget_;

  raw_ptr<Textfield> textfield_ = nullptr;
  std::unique_ptr<aura::test::TestCursorClient> test_cursor_client_;
};

// Tests that the selection handles are placed appropriately when selection in
// a Textfield changes.
TEST_F(TouchSelectionControllerImplTest, SelectionInTextfieldTest) {
  CreateTextfield();
  textfield_->SetText(u"some text");
  // Tap the textfield to invoke touch selection.
  ui::GestureEventDetails details(ui::EventType::kGestureTap);
  details.set_tap_count(1);
  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(), details);
  textfield_->OnGestureEvent(&tap);

  // Test selecting a range.
  textfield_->SetSelectedRange(gfx::Range(3, 7));
  VerifyHandlePositions(false, true, FROM_HERE);

  // Test selecting everything.
  textfield_->SelectAll(false);
  VerifyHandlePositions(false, true, FROM_HERE);

  // Test with no selection.
  textfield_->ClearSelection();
  VerifyHandlePositions(false, true, FROM_HERE);

  // Test with lost focus.
  textfield_widget_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(GetSelectionController());

  // Test with focus re-gained.
  textfield_widget_->GetFocusManager()->SetFocusedView(textfield_);
  EXPECT_FALSE(GetSelectionController());
  textfield_->OnGestureEvent(&tap);
  VerifyHandlePositions(false, true, FROM_HERE);
}

// Tests that the selection handles are placed appropriately in bidi text.
TEST_F(TouchSelectionControllerImplTest, SelectionInBidiTextfieldTest) {
  CreateTextfield();
  textfield_->SetText(u"abc\x05d0\x05d1\x05d2");
  // Tap the textfield to invoke touch selection.
  ui::GestureEventDetails details(ui::EventType::kGestureTap);
  details.set_tap_count(1);
  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(), details);
  textfield_->OnGestureEvent(&tap);

  // Test cursor at run boundary and with empty selection.
  textfield_->SelectSelectionModel(
      gfx::SelectionModel(3, gfx::CURSOR_BACKWARD));
  VerifyHandlePositions(false, true, FROM_HERE);

  // Test selection range inside one run and starts or ends at run boundary.
  textfield_->SetSelectedRange(gfx::Range(2, 3));
  VerifyHandlePositions(false, true, FROM_HERE);

  textfield_->SetSelectedRange(gfx::Range(3, 2));
  VerifyHandlePositions(false, true, FROM_HERE);

  // TODO(mfomitchev): crbug.com/429705
  // The correct behavior for handles in mixed ltr/rtl text line is not known,
  // so passing false for |check_direction| in some of these tests.
  textfield_->SetSelectedRange(gfx::Range(3, 4));
  VerifyHandlePositions(false, false, FROM_HERE);

  textfield_->SetSelectedRange(gfx::Range(4, 3));
  VerifyHandlePositions(false, false, FROM_HERE);

  textfield_->SetSelectedRange(gfx::Range(3, 6));
  VerifyHandlePositions(false, false, FROM_HERE);

  textfield_->SetSelectedRange(gfx::Range(6, 3));
  VerifyHandlePositions(false, false, FROM_HERE);

  // Test selection range accross runs.
  textfield_->SetSelectedRange(gfx::Range(0, 6));
  VerifyHandlePositions(false, true, FROM_HERE);

  textfield_->SetSelectedRange(gfx::Range(6, 0));
  VerifyHandlePositions(false, true, FROM_HERE);

  textfield_->SetSelectedRange(gfx::Range(1, 4));
  VerifyHandlePositions(false, true, FROM_HERE);

  textfield_->SetSelectedRange(gfx::Range(4, 1));
  VerifyHandlePositions(false, true, FROM_HERE);
}

// Tests if the selection update callbacks are called appropriately when
// selection handles are moved.
TEST_F(TouchSelectionControllerImplTest, SelectionUpdateCallbackTest) {
  CreateTextfield();
  textfield_->SetText(u"textfield with selected text");
  // Tap the textfield to invoke touch selection.
  ui::GestureEventDetails details(ui::EventType::kGestureTap);
  details.set_tap_count(1);
  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(), details);
  textfield_->OnGestureEvent(&tap);
  textfield_->SetSelectedRange(gfx::Range(3, 7));

  gfx::Point textfield_origin;
  textfield_->ConvertPointToScreen(&textfield_origin);

  EXPECT_EQ("tfie", UTF16ToUTF8(textfield_->GetSelectedText()));
  VerifyHandlePositions(false, true, FROM_HERE);

  // Drag selection handle 2 to right by 3 chars.
  const gfx::FontList& font_list = textfield_->GetFontList();
  int x = gfx::Canvas::GetStringWidth(u"ld ", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 2);
  EXPECT_EQ("tfield ", UTF16ToUTF8(textfield_->GetSelectedText()));
  VerifyHandlePositions(false, true, FROM_HERE);

  // Drag selection handle 1 to the left by a large amount (selection should
  // just stick to the beginning of the textfield).
  SimulateSelectionHandleDrag(gfx::Vector2d(-50, 0), 1);
  EXPECT_EQ("textfield ", UTF16ToUTF8(textfield_->GetSelectedText()));
  VerifyHandlePositions(true, true, FROM_HERE);

  // Drag selection handle 1 across selection handle 2.
  x = gfx::Canvas::GetStringWidth(u"textfield with ", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 1);
  EXPECT_EQ("with ", UTF16ToUTF8(textfield_->GetSelectedText()));
  VerifyHandlePositions(true, true, FROM_HERE);

  // Drag selection handle 2 across selection handle 1.
  x = gfx::Canvas::GetStringWidth(u"with selected ", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 2);
  EXPECT_EQ("selected ", UTF16ToUTF8(textfield_->GetSelectedText()));
  VerifyHandlePositions(false, true, FROM_HERE);
}

TEST_F(TouchSelectionControllerImplTest, SelectionUpdateInBidiCallbackTest) {
  CreateTextfield();
  textfield_->SetText(
      u"abc\x05e1\x05e2\x05e3"
      u"def");
  // Tap the textfield to invoke touch selection.
  ui::GestureEventDetails details(ui::EventType::kGestureTap);
  details.set_tap_count(1);
  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(), details);
  textfield_->OnGestureEvent(&tap);

  // Select [c] from left to right.
  textfield_->SetSelectedRange(gfx::Range(2, 3));
  EXPECT_EQ(u"c", textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Drag selection handle 2 to right by 1 char.
  const gfx::FontList& font_list = textfield_->GetFontList();
  int x = gfx::Canvas::GetStringWidth(u"\x05e3", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 2);
  EXPECT_EQ(u"c\x05e1\x05e2", textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Drag selection handle 1 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(u"b", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 1);
  EXPECT_EQ(u"bc\x05e1\x05e2", textfield_->GetSelectedText());
  VerifyHandlePositions(true, true, FROM_HERE);

  // Select [c] from right to left.
  textfield_->SetSelectedRange(gfx::Range(3, 2));
  EXPECT_EQ(u"c", textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Drag selection handle 1 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(u"\x05e3", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 1);
  EXPECT_EQ(u"c\x05e1\x05e2", textfield_->GetSelectedText());
  VerifyHandlePositions(true, true, FROM_HERE);

  // Drag selection handle 2 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(u"b", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 2);
  EXPECT_EQ(u"bc\x05e1\x05e2", textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Select [\x5e1] from right to left.
  textfield_->SetSelectedRange(gfx::Range(3, 4));
  EXPECT_EQ(u"\x05e1", textfield_->GetSelectedText());
  // TODO(mfomitchev): crbug.com/429705
  // The correct behavior for handles in mixed ltr/rtl text line is not known,
  // so passing false for |check_direction| in some of these tests.
  VerifyHandlePositions(false, false, FROM_HERE);

  /* TODO(xji): for bidi text "abcDEF" whose display is "abcFEDhij", when click
     right of 'D' and select [D] then move the left selection handle to left
     by one character, it should select [ED], instead it selects [F].
     Reason: click right of 'D' and left of 'h' return the same x-axis position,
     pass this position to FindCursorPosition() returns index of 'h'. which
     means the selection start changed from 3 to 6.
     Need further investigation on whether this is a bug in Pango and how to
     work around it.
  // Drag selection handle 2 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(u"\x05e2", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 2);
  EXPECT_EQ(u"\x05e1\x05e2", textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);
  */

  // Drag selection handle 1 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(u"d", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 1);
  EXPECT_EQ(
      u"\x05e2\x05e3"
      u"d",
      textfield_->GetSelectedText());
  VerifyHandlePositions(true, true, FROM_HERE);

  // Select [\x5e1] from left to right.
  textfield_->SetSelectedRange(gfx::Range(4, 3));
  EXPECT_EQ(u"\x05e1", textfield_->GetSelectedText());
  VerifyHandlePositions(false, false, FROM_HERE);

  /* TODO(xji): see detail of above commented out test case.
  // Drag selection handle 1 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(u"\x05e2", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 1);
  EXPECT_EQ(u"\x05e1\x05e2", textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(true);
  */

  // Drag selection handle 2 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(u"d", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 2);
  EXPECT_EQ(
      u"\x05e2\x05e3"
      u"d",
      textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Select [\x05r3] from right to left.
  textfield_->SetSelectedRange(gfx::Range(5, 6));
  EXPECT_EQ(u"\x05e3", textfield_->GetSelectedText());
  VerifyHandlePositions(false, false, FROM_HERE);

  // Drag selection handle 2 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(u"c", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 2);
  EXPECT_EQ(u"c\x05e1\x05e2", textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Drag selection handle 1 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(u"\x05e2", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 1);
  EXPECT_EQ(u"c\x05e1", textfield_->GetSelectedText());
  VerifyHandlePositions(true, true, FROM_HERE);

  // Select [\x05r3] from left to right.
  textfield_->SetSelectedRange(gfx::Range(6, 5));
  EXPECT_EQ(u"\x05e3", textfield_->GetSelectedText());
  VerifyHandlePositions(false, false, FROM_HERE);

  // Drag selection handle 1 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(u"c", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 1);
  EXPECT_EQ(u"c\x05e1\x05e2", textfield_->GetSelectedText());
  VerifyHandlePositions(true, true, FROM_HERE);

  // Drag selection handle 2 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(u"\x05e2", font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 2);
  EXPECT_EQ(u"c\x05e1", textfield_->GetSelectedText());
  VerifyHandlePositions(false, false, FROM_HERE);
}

TEST_F(TouchSelectionControllerImplTest,
       HiddenSelectionHandleRetainsCursorPosition) {
  static const uint32_t selection_start = 10u;
  SetupSelectionInvisibleHandle(selection_start);
  // Drag the visible handle around and make sure the selection end point of the
  // invisible handle does not change.
  size_t visible_handle_position = textfield_->GetSelectedRange().end();
  for (int i = 0; i < 10; ++i) {
    static const int drag_diff = -10;
    SimulateSelectionHandleDrag(gfx::Vector2d(drag_diff, 0), 2);
    // Make sure that the visible handle is being dragged.
    EXPECT_NE(visible_handle_position, textfield_->GetSelectedRange().end());
    visible_handle_position = textfield_->GetSelectedRange().end();
    EXPECT_EQ(10u, textfield_->GetSelectedRange().start());
  }
}

// Tests that we can handle the hidden handle getting exposed as a result of a
// drag and that it maintains the correct orientation when exposed.
TEST_F(TouchSelectionControllerImplTest, HiddenSelectionHandleExposed) {
  static const uint32_t selection_start = 0u;
  SetupSelectionInvisibleHandle(selection_start);

  // Drag the handle until the selection shrinks such that the other handle
  // becomes visible.
  while (!IsSelectionHandle1Visible()) {
    static const int drag_diff = -10;
    SimulateSelectionHandleDrag(gfx::Vector2d(drag_diff, 0), 2);
  }

  // Confirm that the exposed handle maintains the LEFT orientation
  // (and does not reset to gfx::SelectionBound::Type::CENTER).
  EXPECT_EQ(gfx::SelectionBound::Type::LEFT, GetSelectionHandle1Type());
}

TEST_F(TouchSelectionControllerImplTest,
       DoubleTapInTextfieldWithCursorHandleShouldSelectText) {
  CreateTextfield();
  textfield_->SetText(u"some text");
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Tap the textfield to invoke touch selection.
  generator.GestureTapAt(gfx::Point(10, 10));

  // Cursor handle should be visible.
  EXPECT_FALSE(textfield_->HasSelection());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Double tap on the cursor handle position. We want to check that the cursor
  // handle is not eating the event and that the event is falling through to the
  // textfield.
  gfx::Point cursor_pos = GetCursorHandleBounds().origin();
  cursor_pos.Offset(GetCursorHandleBounds().width() / 2, 0);
  generator.GestureTapAt(cursor_pos);
  generator.GestureTapAt(cursor_pos);
  EXPECT_TRUE(textfield_->HasSelection());
}

// Touch selection menu is not supported on Cast.
#if BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(TouchSelectionControllerImplTest,
       MenuAppearsAfterDraggingSelectionHandles) {
  CreateTextfield();
  textfield_->SetText(u"some text in a textfield");
  textfield_->SetSelectedRange(gfx::Range(2, 15));
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());
  EXPECT_FALSE(IsQuickMenuVisible());

  // Tap on the selected text to make selection handles appear, menu should also
  // appear.
  generator.GestureTapAt(gfx::Point(30, 15));
  EXPECT_TRUE(IsQuickMenuVisible());

  // Drag the selection handles, menu should appear after each drag ends.
  SimulateSelectionHandleDrag(gfx::Vector2d(3, 0), 1);
  EXPECT_TRUE(IsQuickMenuVisible());

  SimulateSelectionHandleDrag(gfx::Vector2d(-5, 0), 2);
  EXPECT_TRUE(IsQuickMenuVisible());

  // Lose focus, menu should disappear.
  textfield_widget_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(IsQuickMenuVisible());
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(TouchSelectionControllerImplTest,
       MagnifierShownWhenDraggingCursorHandle) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  CreateTextfield();
  textfield_->SetText(u"some text in a textfield");
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Tap the textfield to make the cursor handle appear.
  generator.GestureTapAt(gfx::Point(10, 10));
  EXPECT_TRUE(IsCursorHandleVisible());
  EXPECT_FALSE(IsMagnifierVisible());

  // Drag the cursor handle. Magnifier should be shown while dragging the
  // handle, then hidden once dragging ends.
  const gfx::Point drag_start = GetCursorHandleBounds().CenterPoint();
  const gfx::Point drag_end = drag_start + gfx::Vector2d(50, 0);
  generator.GestureScrollSequenceWithCallback(
      drag_start, drag_end, /*duration=*/base::Milliseconds(50),
      /*steps=*/5,
      base::BindLambdaForTesting(
          [&](ui::EventType event_type, const gfx::Vector2dF& offset) {
            if (event_type == ui::EventType::kGestureScrollUpdate) {
              EXPECT_TRUE(IsMagnifierVisible());
            }
          }));
  EXPECT_FALSE(IsMagnifierVisible());
}

TEST_F(TouchSelectionControllerImplTest,
       MagnifierShownWhenDraggingSelectionHandles) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  CreateTextfield();
  textfield_->SetText(u"some text in a textfield");
  textfield_->SetSelectedRange(gfx::Range(2, 15));
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Tap on the selected text to make selection handles appear.
  generator.GestureTapAt(gfx::Point(30, 15));
  EXPECT_TRUE(IsSelectionHandle1Visible());
  EXPECT_FALSE(IsMagnifierVisible());

  // Drag selection handles. Magnifier should be shown while dragging handles,
  // then hidden once dragging ends.
  gfx::Point drag_start = GetSelectionHandle1Bounds().CenterPoint();
  gfx::Point drag_end = drag_start + gfx::Vector2d(50, 0);
  generator.GestureScrollSequenceWithCallback(
      drag_start, drag_end, /*duration=*/base::Milliseconds(50),
      /*steps=*/5,
      base::BindLambdaForTesting(
          [&](ui::EventType event_type, const gfx::Vector2dF& offset) {
            if (event_type == ui::EventType::kGestureScrollUpdate) {
              EXPECT_TRUE(IsMagnifierVisible());
            }
          }));
  EXPECT_FALSE(IsMagnifierVisible());

  drag_start = GetSelectionHandle2Bounds().CenterPoint();
  drag_end = drag_start + gfx::Vector2d(-60, 0);
  generator.GestureScrollSequenceWithCallback(
      drag_start, drag_end, /*duration=*/base::Milliseconds(50),
      /*steps=*/5,
      base::BindLambdaForTesting(
          [&](ui::EventType event_type, const gfx::Vector2dF& offset) {
            if (event_type == ui::EventType::kGestureScrollUpdate) {
              EXPECT_TRUE(IsMagnifierVisible());
            }
          }));
  EXPECT_FALSE(IsMagnifierVisible());
}

// Tests that the magnifier is shown when directly dragging the cursor in the
// textfield, i.e. when performing a scroll gesture on the textfield rather than
// on the touch handles.
TEST_F(TouchSelectionControllerImplTest, MagnifierShownWhenDraggingCursor) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  CreateTextfield();
  textfield_->SetText(u"some text in a textfield");
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Scroll in a horizontal direction over the textfield to move the cursor.
  // Magnifier should be shown during the dragging movement, then hidden once
  // dragging ends.
  const gfx::Point drag_start =
      GetCursorPosition(gfx::SelectionModel(6, gfx::CURSOR_FORWARD));
  const gfx::Point drag_end = drag_start + gfx::Vector2d(80, 0);
  generator.GestureScrollSequenceWithCallback(
      drag_start, drag_end, /*duration=*/base::Milliseconds(50),
      /*steps=*/5,
      base::BindLambdaForTesting(
          [&](ui::EventType event_type, const gfx::Vector2dF& offset) {
            if (event_type == ui::EventType::kGestureScrollUpdate) {
              EXPECT_TRUE(IsMagnifierVisible());
            }
          }));
  EXPECT_FALSE(IsMagnifierVisible());
}

// Tests that touch handles are correctly shown when directly dragging the
// cursor in the textfield.
TEST_F(TouchSelectionControllerImplTest, DraggingCursorShowsHandle) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  CreateTextfield();
  textfield_->SetText(u"some text in a textfield");
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Scroll in a horizontal direction over the textfield to move the cursor.
  // Touch handles should be hidden during the dragging movement, then the
  // cursor handle should be shown once dragging ends.
  const gfx::Point drag_start =
      GetCursorPosition(gfx::SelectionModel(6, gfx::CURSOR_FORWARD));
  const gfx::Point drag_end = drag_start + gfx::Vector2d(80, 0);
  generator.GestureScrollSequenceWithCallback(
      drag_start, drag_end, /*duration=*/base::Milliseconds(50),
      /*steps=*/5,
      base::BindLambdaForTesting(
          [&](ui::EventType event_type, const gfx::Vector2dF& offset) {
            if (event_type == ui::EventType::kGestureScrollUpdate) {
              EXPECT_FALSE(IsCursorHandleVisible());
              EXPECT_FALSE(IsSelectionHandle1Visible());
              EXPECT_FALSE(IsSelectionHandle2Visible());
            }
          }));
  EXPECT_TRUE(IsCursorHandleVisible());
  EXPECT_FALSE(IsSelectionHandle1Visible());
  EXPECT_FALSE(IsSelectionHandle2Visible());
}

TEST_F(TouchSelectionControllerImplTest, TapOnHandleTogglesMenu) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  CreateTextfield();
  textfield_->SetText(u"some text in a textfield");
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Tap the textfield to invoke touch selection. Cursor handle should be
  // shown, but not the quick menu.
  generator.GestureTapAt(gfx::Point(10, 10));
  EXPECT_TRUE(IsCursorHandleVisible());
  EXPECT_FALSE(IsQuickMenuVisible());

  // Tap the touch handle, the quick menu should appear.
  gfx::Point handle_pos = GetCursorHandleBounds().CenterPoint();
  generator.GestureTapAt(handle_pos);
  EXPECT_TRUE(IsCursorHandleVisible());
  EXPECT_TRUE(IsQuickMenuVisible());

  // Tap the touch handle again, the quick menu should disappear.
  generator.GestureTapAt(handle_pos);
  EXPECT_TRUE(IsCursorHandleVisible());
  EXPECT_FALSE(IsQuickMenuVisible());

  // Tap the touch handle again, the quick menu should appear.
  generator.GestureTapAt(handle_pos);
  EXPECT_TRUE(IsCursorHandleVisible());
  EXPECT_TRUE(IsQuickMenuVisible());

  // Tap a different spot in the textfield, handle should remain visible but the
  // quick menu should disappear.
  generator.GestureTapAt(gfx::Point(60, 10));
  EXPECT_TRUE(IsCursorHandleVisible());
  EXPECT_FALSE(IsQuickMenuVisible());
}

TEST_F(TouchSelectionControllerImplTest, TapOnCursorTogglesMenu) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  CreateTextfield();
  textfield_->SetText(u"some text in a textfield");
  textfield_->SetSelectedRange(gfx::Range(7, 7));
  const gfx::Point cursor_position =
      GetCursorRect(textfield_->GetSelectionModel()).CenterPoint();
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Tap the cursor. Cursor handle and quick menu should appear.
  generator.GestureTapAt(cursor_position);
  EXPECT_TRUE(IsCursorHandleVisible());
  EXPECT_TRUE(IsQuickMenuVisible());

  // Tap the cursor, the quick menu should disappear. We advance the clock
  // before tapping again to avoid the tap being treated as a double tap.
  generator.AdvanceClock(base::Milliseconds(1000));
  generator.GestureTapAt(cursor_position);
  EXPECT_TRUE(IsCursorHandleVisible());
  EXPECT_FALSE(IsQuickMenuVisible());

  // Tap the cursor, the quick menu should appear.
  generator.AdvanceClock(base::Milliseconds(1000));
  generator.GestureTapAt(cursor_position);
  EXPECT_TRUE(IsCursorHandleVisible());
  EXPECT_TRUE(IsQuickMenuVisible());

  // Tap a different spot in the textfield to move the cursor. The handle should
  // remain visible but the quick menu should disappear.
  generator.AdvanceClock(base::Milliseconds(1000));
  generator.GestureTapAt(gfx::Point(100, 10));
  EXPECT_TRUE(IsCursorHandleVisible());
  EXPECT_FALSE(IsQuickMenuVisible());
}

// Tests that the quick menu is hidden when moving the cursor with a dragging
// gesture on the textfield.
TEST_F(TouchSelectionControllerImplTest, MenuHiddenWhenDraggingCursor) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  CreateTextfield();
  textfield_->SetText(u"some text in a textfield");
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Tap the textfield to invoke touch selection, then tap the touch handle to
  // show the quick menu.
  generator.GestureTapAt(gfx::Point(10, 10));
  generator.GestureTapAt(GetCursorHandleBounds().CenterPoint());
  EXPECT_TRUE(IsQuickMenuVisible());

  // Scroll in a horizontal direction over the textfield to move the cursor.
  // Menu should be hidden during the dragging movement.
  const gfx::Point drag_start =
      GetCursorPosition(gfx::SelectionModel(6, gfx::CURSOR_FORWARD));
  const gfx::Point drag_end = drag_start + gfx::Vector2d(80, 0);
  generator.GestureScrollSequenceWithCallback(
      drag_start, drag_end, /*duration=*/base::Milliseconds(50),
      /*steps=*/5,
      base::BindLambdaForTesting(
          [&](ui::EventType event_type, const gfx::Vector2dF& offset) {
            if (event_type == ui::EventType::kGestureScrollUpdate) {
              EXPECT_FALSE(IsQuickMenuVisible());
            }
          }));
}

TEST_F(TouchSelectionControllerImplTest, SelectCommands) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  CreateTextfield();
  textfield_->SetText(u"some text");
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Tap the textfield to invoke touch selection.
  generator.GestureTapAt(gfx::Point(10, 10));

  // Select all and select word options should be enabled after initial tap.
  ui::TouchSelectionMenuClient* menu_client = GetSelectionController();
  ASSERT_TRUE(menu_client);
  EXPECT_TRUE(menu_client->IsCommandIdEnabled(ui::TouchEditable::kSelectAll));
  EXPECT_TRUE(menu_client->IsCommandIdEnabled(ui::TouchEditable::kSelectWord));

  // Select word at current position. Select word command should now be disabled
  // since there is already a selection.
  menu_client->ExecuteCommand(ui::TouchEditable::kSelectWord,
                              ui::EF_FROM_TOUCH);
  EXPECT_EQ("some", UTF16ToUTF8(textfield_->GetSelectedText()));
  menu_client = GetSelectionController();
  ASSERT_TRUE(menu_client);
  EXPECT_TRUE(menu_client->IsCommandIdEnabled(ui::TouchEditable::kSelectAll));
  EXPECT_FALSE(menu_client->IsCommandIdEnabled(ui::TouchEditable::kSelectWord));

  // Select all text. Select all and select word commands should now be
  // disabled.
  menu_client->ExecuteCommand(ui::TouchEditable::kSelectAll, ui::EF_FROM_TOUCH);
  EXPECT_EQ("some text", UTF16ToUTF8(textfield_->GetSelectedText()));
  menu_client = GetSelectionController();
  ASSERT_TRUE(menu_client);
  EXPECT_FALSE(menu_client->IsCommandIdEnabled(ui::TouchEditable::kSelectAll));
  EXPECT_FALSE(menu_client->IsCommandIdEnabled(ui::TouchEditable::kSelectWord));
}

TEST_F(TouchSelectionControllerImplTest, CursorHandleDraggingMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  CreateTextfield();
  textfield_->SetText(u"some text in a textfield");
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Tap the textfield to make the cursor handle appear.
  generator.GestureTapAt(gfx::Point(10, 10));
  EXPECT_TRUE(IsCursorHandleVisible());

  // Drag the cursor handle.
  const gfx::Point drag_start = GetCursorHandleBounds().CenterPoint();
  const gfx::Point drag_end = drag_start + gfx::Vector2d(50, 0);
  generator.GestureScrollSequence(drag_start, drag_end,
                                  /*duration=*/base::Milliseconds(50),
                                  /*steps=*/5);
  histogram_tester.ExpectBucketCount(
      ui::kTouchSelectionDragTypeHistogramName,
      ui::TouchSelectionDragType::kCursorHandleDrag, 1);
}

TEST_F(TouchSelectionControllerImplTest, SelectionHandleDraggingMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  CreateTextfield();
  textfield_->SetText(u"some text in a textfield");
  textfield_->SetSelectedRange(gfx::Range(2, 15));
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Tap on the selected text to make selection handles appear.
  generator.GestureTapAt(gfx::Point(30, 15));
  EXPECT_TRUE(IsSelectionHandle1Visible());

  // Drag selection handle 1.
  gfx::Point drag_start = GetSelectionHandle1Bounds().CenterPoint();
  gfx::Point drag_end = drag_start + gfx::Vector2d(50, 0);
  generator.GestureScrollSequence(drag_start, drag_end,
                                  /*duration=*/base::Milliseconds(50),
                                  /*steps=*/5);
  histogram_tester.ExpectBucketCount(
      ui::kTouchSelectionDragTypeHistogramName,
      ui::TouchSelectionDragType::kSelectionHandleDrag, 1);

  // Drag selection handle 2.
  drag_start = GetSelectionHandle2Bounds().CenterPoint();
  drag_end = drag_start + gfx::Vector2d(-60, 0);
  generator.GestureScrollSequence(drag_start, drag_end,
                                  /*duration=*/base::Milliseconds(50),
                                  /*steps=*/5);
  histogram_tester.ExpectBucketCount(
      ui::kTouchSelectionDragTypeHistogramName,
      ui::TouchSelectionDragType::kSelectionHandleDrag, 2);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// A simple implementation of TouchEditable that allows faking cursor position
// inside its boundaries.
class TestTouchEditable : public ui::TouchEditable {
 public:
  explicit TestTouchEditable(aura::Window* window) : window_(window) {
    DCHECK(window);
  }

  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds; }

  void set_cursor_rect(const gfx::RectF& cursor_rect) {
    cursor_bound_.SetEdge(cursor_rect.origin(), cursor_rect.bottom_left());
    cursor_bound_.set_type(gfx::SelectionBound::Type::CENTER);
  }

  TestTouchEditable(const TestTouchEditable&) = delete;
  TestTouchEditable& operator=(const TestTouchEditable&) = delete;

  ~TestTouchEditable() override = default;

 private:
  // Overridden from ui::TouchEditable.
  void MoveCaret(const gfx::Point& position) override { NOTREACHED(); }
  void MoveRangeSelectionExtent(const gfx::Point& extent) override {
    NOTREACHED();
  }
  void SelectBetweenCoordinates(const gfx::Point& base,
                                const gfx::Point& extent) override {
    NOTREACHED();
  }
  void GetSelectionEndPoints(gfx::SelectionBound* anchor,
                             gfx::SelectionBound* focus) override {
    *anchor = *focus = cursor_bound_;
  }
  gfx::Rect GetBounds() override { return gfx::Rect(bounds_.size()); }
  gfx::NativeView GetNativeView() const override { return window_; }
  bool IsSelectionDragging() const override { return false; }
  void ConvertPointToScreen(gfx::Point* point) override {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(window_->GetRootWindow());
    if (screen_position_client)
      screen_position_client->ConvertPointToScreen(window_, point);
  }
  void ConvertPointFromScreen(gfx::Point* point) override {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(window_->GetRootWindow());
    if (screen_position_client)
      screen_position_client->ConvertPointFromScreen(window_, point);
  }
  void OpenContextMenu(const gfx::Point& anchor) override { NOTREACHED(); }
  void DestroyTouchSelection() override { NOTREACHED(); }

  // Overridden from ui::SimpleMenuModel::Delegate.
  bool IsCommandIdChecked(int command_id) const override { NOTREACHED(); }
  bool IsCommandIdEnabled(int command_id) const override { NOTREACHED(); }
  void ExecuteCommand(int command_id, int event_flags) override {
    NOTREACHED();
  }

  raw_ptr<aura::Window> window_;

  // Boundaries of the client view.
  gfx::Rect bounds_;

  // Cursor position inside the client view.
  gfx::SelectionBound cursor_bound_;
};

// Tests if the touch editing handle is shown or hidden properly according to
// the cursor position relative to the client boundaries.
TEST_F(TouchSelectionControllerImplTest,
       VisibilityOfHandleRegardingClientBounds) {
  CreateWidget();

  TestTouchEditable touch_editable(widget_->GetNativeView());
  std::unique_ptr<TouchSelectionController> touch_selection_controller =
      std::make_unique<TouchSelectionControllerImpl>(&touch_editable);

  touch_editable.set_bounds(gfx::Rect(0, 0, 100, 20));

  // Put the cursor completely inside the client bounds. Handle should be
  // visible.
  touch_editable.set_cursor_rect(gfx::RectF(2.f, 0.f, 1.f, 20.f));
  touch_selection_controller->SelectionChanged();
  EXPECT_TRUE(IsCursorHandleVisibleFor(touch_selection_controller.get()));

  // Move the cursor up such that |kBarMinHeight| pixels are still in the client
  // bounds. Handle should still be visible.
  touch_editable.set_cursor_rect(
      gfx::RectF(2.f, kBarMinHeight - 20.f, 1.f, 20.f));
  touch_selection_controller->SelectionChanged();
  EXPECT_TRUE(IsCursorHandleVisibleFor(touch_selection_controller.get()));

  // Move the cursor up such that less than |kBarMinHeight| pixels are in the
  // client bounds. Handle should be hidden.
  touch_editable.set_cursor_rect(
      gfx::RectF(2.f, kBarMinHeight - 20.f - 1.f, 1.f, 20.f));
  touch_selection_controller->SelectionChanged();
  EXPECT_FALSE(IsCursorHandleVisibleFor(touch_selection_controller.get()));

  // Move the Cursor down such that |kBarBottomAllowance| pixels are out of the
  // client bounds. Handle should be visible.
  touch_editable.set_cursor_rect(
      gfx::RectF(2.f, kBarBottomAllowance, 1.f, 20.f));
  touch_selection_controller->SelectionChanged();
  EXPECT_TRUE(IsCursorHandleVisibleFor(touch_selection_controller.get()));

  // Move the cursor down such that more than |kBarBottomAllowance| pixels are
  // out of the client bounds. Handle should be hidden.
  touch_editable.set_cursor_rect(
      gfx::RectF(2.f, kBarBottomAllowance + 1.f, 1.f, 20.f));
  touch_selection_controller->SelectionChanged();
  EXPECT_FALSE(IsCursorHandleVisibleFor(touch_selection_controller.get()));

  touch_selection_controller.reset();
}

TEST_F(TouchSelectionControllerImplTest, HandlesStackAboveParent) {
  aura::Window* root = GetContext();
  ui::EventTargeter* targeter =
      root->GetHost()->dispatcher()->GetDefaultEventTargeter();

  // Create the first window containing a Views::Textfield.
  CreateTextfield();
  aura::Window* window1 = textfield_widget_->GetNativeView();

  // Start touch editing, check that the handle is above the first window, and
  // end touch editing.
  StartTouchEditing();
  gfx::Point test_point = GetCursorHandleDragPoint();
  ui::MouseEvent test_event1(ui::EventType::kMouseMoved, test_point, test_point,
                             ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  EXPECT_EQ(GetCursorHandleNativeView(),
            targeter->FindTargetForEvent(root, &test_event1));
  EndTouchEditing();

  // Create the second (empty) window over the first one.
  CreateWidget();
  aura::Window* window2 = widget_->GetNativeView();

  // Start touch editing (in the first window) and check that the handle is not
  // above the second window.
  StartTouchEditing();
  ui::MouseEvent test_event2(ui::EventType::kMouseMoved, test_point, test_point,
                             ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  EXPECT_EQ(window2, targeter->FindTargetForEvent(root, &test_event2));

  // Move the first window to top and check that the handle is kept above the
  // first window.
  window1->GetRootWindow()->StackChildAtTop(window1);
  ui::MouseEvent test_event3(ui::EventType::kMouseMoved, test_point, test_point,
                             ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  EXPECT_EQ(GetCursorHandleNativeView(),
            targeter->FindTargetForEvent(root, &test_event3));
}

TEST_F(TouchSelectionControllerImplTest, MouseEventDeactivatesTouchSelection) {
  CreateTextfield();
  EXPECT_FALSE(GetSelectionController());

  ui::test::EventGenerator generator(
      textfield_widget_->GetNativeView()->GetRootWindow());

  generator.set_current_screen_location(gfx::Point(5, 5));
  RunPendingMessages();

  // Start touch editing; then move mouse over the textfield and ensure it
  // deactivates touch selection.
  StartTouchEditing();
  EXPECT_TRUE(GetSelectionController());
  generator.MoveMouseTo(gfx::Point(5, 10));
  RunPendingMessages();
  EXPECT_FALSE(GetSelectionController());

  generator.MoveMouseTo(gfx::Point(5, 50));
  RunPendingMessages();

  // Start touch editing; then move mouse out of the textfield, but inside the
  // winow and ensure it deactivates touch selection.
  StartTouchEditing();
  EXPECT_TRUE(GetSelectionController());
  generator.MoveMouseTo(gfx::Point(5, 55));
  RunPendingMessages();
  EXPECT_FALSE(GetSelectionController());

  generator.MoveMouseTo(gfx::Point(5, 500));
  RunPendingMessages();

  // Start touch editing; then move mouse out of the textfield and window and
  // ensure it deactivates touch selection.
  StartTouchEditing();
  EXPECT_TRUE(GetSelectionController());
  generator.MoveMouseTo(5, 505);
  RunPendingMessages();
  EXPECT_FALSE(GetSelectionController());
}

TEST_F(TouchSelectionControllerImplTest, MouseCaptureChangedEventIgnored) {
  CreateTextfield();
  EXPECT_FALSE(GetSelectionController());

  ui::test::EventGenerator generator(
      textfield_widget_->GetNativeView()->GetRootWindow());
  RunPendingMessages();

  // Start touch editing; then generate a mouse-capture-changed event and ensure
  // it does not deactivate touch selection.
  StartTouchEditing();
  EXPECT_TRUE(GetSelectionController());
  ui::MouseEvent capture_changed(ui::EventType::kMouseCaptureChanged,
                                 gfx::Point(5, 5), gfx::Point(5, 5),
                                 base::TimeTicks(), 0, 0);
  generator.Dispatch(&capture_changed);
  RunPendingMessages();
  EXPECT_TRUE(GetSelectionController());
}

TEST_F(TouchSelectionControllerImplTest, KeyEventDeactivatesTouchSelection) {
  CreateTextfield();
  EXPECT_FALSE(GetSelectionController());

  ui::test::EventGenerator generator(
      textfield_widget_->GetNativeView()->GetRootWindow());

  RunPendingMessages();

  // Start touch editing; then press a key and ensure it deactivates touch
  // selection.
  StartTouchEditing();
  EXPECT_TRUE(GetSelectionController());
  generator.PressKey(ui::VKEY_A, 0);
  RunPendingMessages();
  EXPECT_FALSE(GetSelectionController());
}

// Tests that the touch selection controller doesn't crash when a handle widget
// is destroyed while touch selection is still active. Regression test for
// https://crbug.com/1448682.
TEST_F(TouchSelectionControllerImplTest,
       DestroyingEditingHandleWidgetDoesNotCrash) {
  CreateTextfield();
  textfield_->SetText(u"some text in a textfield");
  textfield_->SetSelectedRange(gfx::Range(7, 7));
  const gfx::Point cursor_position =
      GetCursorRect(textfield_->GetSelectionModel()).CenterPoint();
  ui::test::EventGenerator generator(
      textfield_->GetWidget()->GetNativeView()->GetRootWindow());

  // Tap the textfield to start touch selection.
  generator.GestureTapAt(cursor_position);
  EXPECT_TRUE(GetSelectionController());

  // Close one of the handle widgets.
  CloseHandle1Widget();
  RunPendingMessages();

  // Try to continue touch selection by tapping at the cursor. This should not
  // crash.
  generator.GestureTapAt(cursor_position);
  generator.GestureTapAt(cursor_position);

  // Check that we can destroy touch selection without crashing.
  textfield_->DestroyTouchSelection();
  RunPendingMessages();
  EXPECT_FALSE(GetSelectionController());
}

}  // namespace views
