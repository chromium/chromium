// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/touchui/touch_selection_controller_impl.h"
#include "ui/views/views_touch_selection_controller_factory.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using base::WideToUTF16;

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
  TouchSelectionControllerImplTest()
      : views_tsc_factory_(new ViewsTouchEditingControllerFactory) {
    ui::TouchEditingControllerFactory::SetInstance(views_tsc_factory_.get());
  }

  ~TouchSelectionControllerImplTest() override {
    ui::TouchEditingControllerFactory::SetInstance(nullptr);
  }

  void SetUp() override {
    ViewsTestBase::SetUp();
    test_cursor_client_ =
        std::make_unique<aura::test::TestCursorClient>(GetContext());
  }

  void TearDown() override {
    test_cursor_client_.reset();
    if (textfield_widget_ && !textfield_widget_->IsClosed())
      textfield_widget_->Close();
    if (widget_ && !widget_->IsClosed())
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  void CreateTextfield() {
    textfield_ = new Textfield();
    textfield_widget_ = new Widget;
    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    textfield_widget_->Init(std::move(params));
    View* container = new View();
    textfield_widget_->SetContentsView(container);
    container->AddChildView(textfield_);

    textfield_->SetBoundsRect(gfx::Rect(0, 0, 200, 21));
    textfield_->SetID(1);
    textfield_widget_->Show();

    textfield_->RequestFocus();
    textfield_test_api_ = std::make_unique<TextfieldTestApi>(textfield_);
  }

  void CreateWidget() {
    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(params));
    widget_->Show();
}

 protected:
  static bool IsCursorHandleVisibleFor(
      ui::TouchEditingControllerDeprecated* controller) {
    TouchSelectionControllerImpl* impl =
        static_cast<TouchSelectionControllerImpl*>(controller);
    return impl->IsCursorHandleVisible();
  }

  gfx::Rect GetCursorRect(const gfx::SelectionModel& sel) {
    return textfield_test_api_->GetRenderText()->GetCursorBounds(sel, true);
  }

  gfx::Point GetCursorPosition(const gfx::SelectionModel& sel) {
    return GetCursorRect(sel).origin();
  }

  TouchSelectionControllerImpl* GetSelectionController() {
    return static_cast<TouchSelectionControllerImpl*>(
        textfield_test_api_->touch_selection_controller());
  }

  void StartTouchEditing() {
    textfield_test_api_->CreateTouchSelectionControllerAndNotifyIt();
  }

  void EndTouchEditing() {
    textfield_test_api_->ResetTouchSelectionController();
  }

  void SimulateSelectionHandleDrag(gfx::Vector2d v, int selection_handle) {
    TouchSelectionControllerImpl* controller = GetSelectionController();
    views::WidgetDelegateView* handle = nullptr;
    if (selection_handle == 1)
      handle = controller->GetHandle1View();
    else
      handle = controller->GetHandle2View();

    gfx::Point grip_location = gfx::Point(handle->size().width() / 2,
                                          handle->size().height() / 2);
    base::TimeTicks time_stamp = base::TimeTicks();
    {
      ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_BEGIN);
      ui::GestureEvent scroll_begin(
          grip_location.x(), grip_location.y(), 0, time_stamp, details);
      handle->OnGestureEvent(&scroll_begin);
    }
    test_cursor_client_->DisableMouseEvents();
    {
      ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_UPDATE);
      gfx::Point update_location = grip_location + v;
      ui::GestureEvent scroll_update(
          update_location.x(), update_location.y(), 0, time_stamp, details);
      handle->OnGestureEvent(&scroll_update);
    }
    {
      ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_END);
      ui::GestureEvent scroll_end(
          grip_location.x(), grip_location.y(), 0, time_stamp, details);
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

  gfx::RenderText* GetRenderText() {
    return textfield_test_api_->GetRenderText();
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
    if (check_direction)  {
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
    ui::GestureEventDetails details(ui::ET_GESTURE_TAP);
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

  Widget* textfield_widget_ = nullptr;
  Widget* widget_ = nullptr;

  Textfield* textfield_ = nullptr;
  std::unique_ptr<TextfieldTestApi> textfield_test_api_;
  std::unique_ptr<ViewsTouchEditingControllerFactory> views_tsc_factory_;
  std::unique_ptr<aura::test::TestCursorClient> test_cursor_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerImplTest);
};

// Tests that the selection handles are placed appropriately when selection in
// a Textfield changes.
TEST_F(TouchSelectionControllerImplTest, SelectionInTextfieldTest) {
  CreateTextfield();
  textfield_->SetText(ASCIIToUTF16("some text"));
  // Tap the textfield to invoke touch selection.
  ui::GestureEventDetails details(ui::ET_GESTURE_TAP);
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
  textfield_->SetText(WideToUTF16(L"abc\x05d0\x05d1\x05d2"));
  // Tap the textfield to invoke touch selection.
  ui::GestureEventDetails details(ui::ET_GESTURE_TAP);
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

// Tests if the SelectRect callback is called appropriately when selection
// handles are moved.
TEST_F(TouchSelectionControllerImplTest, SelectRectCallbackTest) {
  CreateTextfield();
  textfield_->SetText(ASCIIToUTF16("textfield with selected text"));
  // Tap the textfield to invoke touch selection.
  ui::GestureEventDetails details(ui::ET_GESTURE_TAP);
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
  int x = gfx::Canvas::GetStringWidth(ASCIIToUTF16("ld "), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 2);
  EXPECT_EQ("tfield ", UTF16ToUTF8(textfield_->GetSelectedText()));
  VerifyHandlePositions(false, true, FROM_HERE);

  // Drag selection handle 1 to the left by a large amount (selection should
  // just stick to the beginning of the textfield).
  SimulateSelectionHandleDrag(gfx::Vector2d(-50, 0), 1);
  EXPECT_EQ("textfield ", UTF16ToUTF8(textfield_->GetSelectedText()));
  VerifyHandlePositions(true, true, FROM_HERE);

  // Drag selection handle 1 across selection handle 2.
  x = gfx::Canvas::GetStringWidth(ASCIIToUTF16("textfield with "), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 1);
  EXPECT_EQ("with ", UTF16ToUTF8(textfield_->GetSelectedText()));
  VerifyHandlePositions(true, true, FROM_HERE);

  // Drag selection handle 2 across selection handle 1.
  x = gfx::Canvas::GetStringWidth(ASCIIToUTF16("with selected "), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 2);
  EXPECT_EQ("selected ", UTF16ToUTF8(textfield_->GetSelectedText()));
  VerifyHandlePositions(false, true, FROM_HERE);
}

TEST_F(TouchSelectionControllerImplTest, SelectRectInBidiCallbackTest) {
  CreateTextfield();
  textfield_->SetText(WideToUTF16(L"abc\x05e1\x05e2\x05e3" L"def"));
  // Tap the textfield to invoke touch selection.
  ui::GestureEventDetails details(ui::ET_GESTURE_TAP);
  details.set_tap_count(1);
  ui::GestureEvent tap(0, 0, 0, base::TimeTicks(), details);
  textfield_->OnGestureEvent(&tap);

  // Select [c] from left to right.
  textfield_->SetSelectedRange(gfx::Range(2, 3));
  EXPECT_EQ(WideToUTF16(L"c"), textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Drag selection handle 2 to right by 1 char.
  const gfx::FontList& font_list = textfield_->GetFontList();
  int x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e3"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"c\x05e1\x05e2"), textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Drag selection handle 1 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"b"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"bc\x05e1\x05e2"), textfield_->GetSelectedText());
  VerifyHandlePositions(true, true, FROM_HERE);

  // Select [c] from right to left.
  textfield_->SetSelectedRange(gfx::Range(3, 2));
  EXPECT_EQ(WideToUTF16(L"c"), textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Drag selection handle 1 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e3"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"c\x05e1\x05e2"), textfield_->GetSelectedText());
  VerifyHandlePositions(true, true, FROM_HERE);

  // Drag selection handle 2 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"b"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"bc\x05e1\x05e2"), textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Select [\x5e1] from right to left.
  textfield_->SetSelectedRange(gfx::Range(3, 4));
  EXPECT_EQ(WideToUTF16(L"\x05e1"), textfield_->GetSelectedText());
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
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e2"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"\x05e1\x05e2"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(false);
  */

  // Drag selection handle 1 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"d"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"\x05e2\x05e3" L"d"), textfield_->GetSelectedText());
  VerifyHandlePositions(true, true, FROM_HERE);

  // Select [\x5e1] from left to right.
  textfield_->SetSelectedRange(gfx::Range(4, 3));
  EXPECT_EQ(WideToUTF16(L"\x05e1"), textfield_->GetSelectedText());
  VerifyHandlePositions(false, false, FROM_HERE);

  /* TODO(xji): see detail of above commented out test case.
  // Drag selection handle 1 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e2"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"\x05e1\x05e2"), textfield_->GetSelectedText());
  VERIFY_HANDLE_POSITIONS(true);
  */

  // Drag selection handle 2 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"d"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"\x05e2\x05e3" L"d"), textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Select [\x05r3] from right to left.
  textfield_->SetSelectedRange(gfx::Range(5, 6));
  EXPECT_EQ(WideToUTF16(L"\x05e3"), textfield_->GetSelectedText());
  VerifyHandlePositions(false, false, FROM_HERE);

  // Drag selection handle 2 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"c"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"c\x05e1\x05e2"), textfield_->GetSelectedText());
  VerifyHandlePositions(false, true, FROM_HERE);

  // Drag selection handle 1 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e2"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"c\x05e1"), textfield_->GetSelectedText());
  VerifyHandlePositions(true, true, FROM_HERE);

  // Select [\x05r3] from left to right.
  textfield_->SetSelectedRange(gfx::Range(6, 5));
  EXPECT_EQ(WideToUTF16(L"\x05e3"), textfield_->GetSelectedText());
  VerifyHandlePositions(false, false, FROM_HERE);

  // Drag selection handle 1 to left by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"c"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(-x, 0), 1);
  EXPECT_EQ(WideToUTF16(L"c\x05e1\x05e2"), textfield_->GetSelectedText());
  VerifyHandlePositions(true, true, FROM_HERE);

  // Drag selection handle 2 to right by 1 char.
  x = gfx::Canvas::GetStringWidth(WideToUTF16(L"\x05e2"), font_list);
  SimulateSelectionHandleDrag(gfx::Vector2d(x, 0), 2);
  EXPECT_EQ(WideToUTF16(L"c\x05e1"), textfield_->GetSelectedText());
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
  textfield_->SetText(ASCIIToUTF16("some text"));
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

// A simple implementation of TouchEditable that allows faking cursor position
// inside its boundaries.
class TestTouchEditable : public ui::TouchEditable {
 public:
  explicit TestTouchEditable(aura::Window* window)
      : window_(window) {
    DCHECK(window);
  }

  void set_bounds(const gfx::Rect& bounds) {
    bounds_ = bounds;
  }

  void set_cursor_rect(const gfx::RectF& cursor_rect) {
    cursor_bound_.SetEdge(cursor_rect.origin(), cursor_rect.bottom_left());
    cursor_bound_.set_type(gfx::SelectionBound::Type::CENTER);
  }

  ~TestTouchEditable() override = default;

 private:
  // Overridden from ui::TouchEditable.
  void SelectRect(const gfx::Point& start, const gfx::Point& end) override {
    NOTREACHED();
  }
  void MoveCaretTo(const gfx::Point& point) override { NOTREACHED(); }
  void GetSelectionEndPoints(gfx::SelectionBound* anchor,
                             gfx::SelectionBound* focus) override {
    *anchor = *focus = cursor_bound_;
  }
  gfx::Rect GetBounds() override { return gfx::Rect(bounds_.size()); }
  gfx::NativeView GetNativeView() const override { return window_; }
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
  bool DrawsHandles() override { return false; }
  void OpenContextMenu(const gfx::Point& anchor) override { NOTREACHED(); }
  void DestroyTouchSelection() override { NOTREACHED(); }

  // Overridden from ui::SimpleMenuModel::Delegate.
  bool IsCommandIdChecked(int command_id) const override {
    NOTREACHED();
    return false;
  }
  bool IsCommandIdEnabled(int command_id) const override {
    NOTREACHED();
    return false;
  }
  void ExecuteCommand(int command_id, int event_flags) override {
    NOTREACHED();
  }

  aura::Window* window_;

  // Boundaries of the client view.
  gfx::Rect bounds_;

  // Cursor position inside the client view.
  gfx::SelectionBound cursor_bound_;

  DISALLOW_COPY_AND_ASSIGN(TestTouchEditable);
};

// Tests if the touch editing handle is shown or hidden properly according to
// the cursor position relative to the client boundaries.
TEST_F(TouchSelectionControllerImplTest,
       VisibilityOfHandleRegardingClientBounds) {
  CreateWidget();

  TestTouchEditable touch_editable(widget_->GetNativeView());
  std::unique_ptr<ui::TouchEditingControllerDeprecated>
      touch_selection_controller(
          ui::TouchEditingControllerDeprecated::Create(&touch_editable));

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
  ui::MouseEvent test_event1(ui::ET_MOUSE_MOVED, test_point, test_point,
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
  ui::MouseEvent test_event2(ui::ET_MOUSE_MOVED, test_point, test_point,
                             ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  EXPECT_EQ(window2, targeter->FindTargetForEvent(root, &test_event2));

  // Move the first window to top and check that the handle is kept above the
  // first window.
  window1->GetRootWindow()->StackChildAtTop(window1);
  ui::MouseEvent test_event3(ui::ET_MOUSE_MOVED, test_point, test_point,
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
  ui::MouseEvent capture_changed(ui::ET_MOUSE_CAPTURE_CHANGED, gfx::Point(5, 5),
                                 gfx::Point(5, 5), base::TimeTicks(), 0, 0);
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

}  // namespace views
