// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/editable_combobox/editable_combobox.h"

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/editable_combobox/editable_combobox_listener.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace views {
namespace {

using base::ASCIIToUTF16;
using views::test::WaitForMenuClosureAnimation;

class DummyListener : public EditableComboboxListener {
 public:
  DummyListener() = default;
  ~DummyListener() override = default;
  void OnContentChanged(EditableCombobox* editable_combobox) override {
    change_count_++;
  }

  int change_count() const { return change_count_; }

 private:
  int change_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DummyListener);
};

// No-op test double of a ContextMenuController
class TestContextMenuController : public ContextMenuController {
 public:
  TestContextMenuController() = default;
  ~TestContextMenuController() override = default;

  // ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {
    opened_menu_ = true;
  }

  bool opened_menu() const { return opened_menu_; }

 private:
  bool opened_menu_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestContextMenuController);
};

class EditableComboboxTest : public ViewsTestBase {
 public:
  EditableComboboxTest() { views::test::DisableMenuClosureAnimations(); }

  void TearDown() override;

  // Initializes the combobox with the given number of items.
  void InitEditableCombobox(int item_count = 8,
                            bool filter_on_edit = false,
                            bool show_on_empty = true);

  // Initializes the combobox with the given items.
  void InitEditableCombobox(
      const std::vector<base::string16>& items,
      bool filter_on_edit,
      bool show_on_empty = true,
      EditableCombobox::Type type = EditableCombobox::Type::kRegular);

  // Initializes the widget where the combobox and the dummy control live.
  void InitWidget();

 protected:
  void ClickArrow();
  void ClickMenuItem(int index);
  void ClickTextfield();
  void DragMouseTo(const gfx::Point& location);
  bool IsMenuOpen();
  void PerformMouseEvent(Widget* widget,
                         const gfx::Point& point,
                         ui::EventType type);
  void PerformClick(Widget* widget, const gfx::Point& point);
  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool alt = false,
                    bool shift = false,
                    bool ctrl_cmd = false);

  // The widget where the control will appear.
  Widget* widget_ = nullptr;

  // |combobox_| and |dummy_focusable_view_| are allocated in
  // |InitEditableCombobox| and then owned by |widget_|.
  EditableCombobox* combobox_ = nullptr;
  View* dummy_focusable_view_ = nullptr;

  // We make |combobox_| a child of another View to test different removal
  // scenarios.
  View* parent_of_combobox_ = nullptr;

  // Listener for our EditableCombobox.
  std::unique_ptr<DummyListener> listener_;

  std::unique_ptr<ui::test::EventGenerator> event_generator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EditableComboboxTest);
};

void EditableComboboxTest::TearDown() {
  if (IsMenuOpen()) {
    combobox_->GetMenuRunnerForTest()->Cancel();
    WaitForMenuClosureAnimation();
  }
  if (widget_)
    widget_->Close();
  ViewsTestBase::TearDown();
}

// Initializes the combobox with the given number of items.
void EditableComboboxTest::InitEditableCombobox(const int item_count,
                                                const bool filter_on_edit,
                                                const bool show_on_empty) {
  std::vector<base::string16> items;
  for (int i = 0; i < item_count; ++i)
    items.push_back(ASCIIToUTF16(base::StringPrintf("item[%i]", i)));
  InitEditableCombobox(items, filter_on_edit, show_on_empty);
}

// Initializes the combobox with the given items.
void EditableComboboxTest::InitEditableCombobox(
    const std::vector<base::string16>& items,
    const bool filter_on_edit,
    const bool show_on_empty,
    const EditableCombobox::Type type) {
  parent_of_combobox_ = new View();
  parent_of_combobox_->SetID(1);
  combobox_ =
      new EditableCombobox(std::make_unique<ui::SimpleComboboxModel>(items),
                           filter_on_edit, show_on_empty, type);
  listener_ = std::make_unique<DummyListener>();
  combobox_->set_listener(listener_.get());
  combobox_->SetID(2);
  dummy_focusable_view_ = new View();
  dummy_focusable_view_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  dummy_focusable_view_->SetID(3);

  InitWidget();
}

// Initializes the widget where the combobox and the dummy control live.
void EditableComboboxTest::InitWidget() {
  widget_ = new Widget();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(0, 0, 1000, 1000);
  parent_of_combobox_->SetBoundsRect(gfx::Rect(0, 0, 500, 40));
  combobox_->SetBoundsRect(gfx::Rect(0, 0, 500, 40));

  widget_->Init(std::move(params));
  View* container = new View();
  widget_->SetContentsView(container);
  container->AddChildView(parent_of_combobox_);
  parent_of_combobox_->AddChildView(combobox_);
  container->AddChildView(dummy_focusable_view_);
  widget_->Show();

#if defined(OS_MACOSX)
  // The event loop needs to be flushed here, otherwise in various tests:
  // 1. The actual showing of the native window backing the widget gets delayed
  //    until a spin of the event loop.
  // 2. The combobox menu object is triggered, and it starts listening for the
  //    "window did become key" notification as a sign that it lost focus and
  //    should close.
  // 3. The event loop is spun, and the actual showing of the native window
  //    triggers the close of the menu opened from within the window.
  base::RunLoop().RunUntilIdle();
#endif

  event_generator_ =
      std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_));
  event_generator_->set_target(ui::test::EventGenerator::Target::WINDOW);
}

void EditableComboboxTest::ClickArrow() {
  const gfx::Point arrow_button(combobox_->x() + combobox_->width() - 1,
                                combobox_->y() + 1);
  PerformClick(widget_, arrow_button);
}

void EditableComboboxTest::ClickMenuItem(const int index) {
  DCHECK(combobox_->GetMenuRunnerForTest());
  const gfx::Point middle_of_item(
      combobox_->x() + combobox_->width() / 2,
      combobox_->y() + combobox_->height() / 2 + combobox_->height() * index);
  // For the menu, we send the click event to the child widget where the menu is
  // shown. That child widget is the MenuHost object created inside
  // EditableCombobox's MenuRunner to host the menu items.
  std::set<Widget*> child_widgets;
  Widget::GetAllOwnedWidgets(widget_->GetNativeView(), &child_widgets);
  ASSERT_EQ(1UL, child_widgets.size());
  PerformClick(*child_widgets.begin(), middle_of_item);
}

void EditableComboboxTest::ClickTextfield() {
  const gfx::Point textfield(combobox_->x() + 1, combobox_->y() + 1);
  PerformClick(widget_, textfield);
}

void EditableComboboxTest::DragMouseTo(const gfx::Point& location) {
  ui::MouseEvent drag(ui::ET_MOUSE_DRAGGED, location, location,
                      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  combobox_->GetTextfieldForTest()->OnMouseDragged(drag);
}

bool EditableComboboxTest::IsMenuOpen() {
  return combobox_ && combobox_->GetMenuRunnerForTest() &&
         combobox_->GetMenuRunnerForTest()->IsRunning();
}

void EditableComboboxTest::PerformMouseEvent(Widget* widget,
                                             const gfx::Point& point,
                                             const ui::EventType type) {
  ui::MouseEvent event = ui::MouseEvent(
      type, point, point, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_NUM_LOCK_ON, ui::EF_LEFT_MOUSE_BUTTON);
  widget->OnMouseEvent(&event);
}

void EditableComboboxTest::PerformClick(Widget* widget,
                                        const gfx::Point& point) {
  PerformMouseEvent(widget, point, ui::ET_MOUSE_PRESSED);
  PerformMouseEvent(widget, point, ui::ET_MOUSE_RELEASED);
}

void EditableComboboxTest::SendKeyEvent(ui::KeyboardCode key_code,
                                        const bool alt,
                                        const bool shift,
                                        const bool ctrl_cmd) {
#if defined(OS_MACOSX)
  bool command = ctrl_cmd;
  bool control = false;
#else
  bool command = false;
  bool control = ctrl_cmd;
#endif

  int flags = (shift ? ui::EF_SHIFT_DOWN : 0) |
              (control ? ui::EF_CONTROL_DOWN : 0) |
              (alt ? ui::EF_ALT_DOWN : 0) | (command ? ui::EF_COMMAND_DOWN : 0);

  event_generator_->PressKey(key_code, flags);
}

TEST_F(EditableComboboxTest, FocusOnTextfieldDoesntOpenMenu) {
  InitEditableCombobox();
  EXPECT_FALSE(IsMenuOpen());
  combobox_->GetTextfieldForTest()->RequestFocus();
  EXPECT_FALSE(IsMenuOpen());
}

TEST_F(EditableComboboxTest, ArrowDownOpensMenu) {
  InitEditableCombobox();
  EXPECT_FALSE(IsMenuOpen());
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
}

TEST_F(EditableComboboxTest, TabMovesToOtherViewAndClosesMenu) {
  InitEditableCombobox();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());
  EXPECT_TRUE(combobox_->GetTextfieldForTest()->HasFocus());
  SendKeyEvent(ui::VKEY_TAB);
  EXPECT_FALSE(combobox_->GetTextfieldForTest()->HasFocus());
  EXPECT_TRUE(dummy_focusable_view_->HasFocus());
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
}

TEST_F(EditableComboboxTest,
       ClickOutsideEditableComboboxWithoutLosingFocusClosesMenu) {
  InitEditableCombobox();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());
  EXPECT_TRUE(combobox_->GetTextfieldForTest()->HasFocus());

  const gfx::Point outside_point(combobox_->x() + combobox_->width() + 1,
                                 combobox_->y() + 1);
  PerformClick(widget_, outside_point);

  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_TRUE(combobox_->GetTextfieldForTest()->HasFocus());
}

TEST_F(EditableComboboxTest, ClickTextfieldDoesntCloseMenu) {
  InitEditableCombobox();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());

  MenuRunner* menu_runner1 = combobox_->GetMenuRunnerForTest();
  ClickTextfield();
  MenuRunner* menu_runner2 = combobox_->GetMenuRunnerForTest();
  EXPECT_TRUE(IsMenuOpen());

  // Making sure the menu didn't close and reopen (causing a flicker).
  EXPECT_EQ(menu_runner1, menu_runner2);
}

TEST_F(EditableComboboxTest, RemovingControlWhileMenuOpenClosesMenu) {
  InitEditableCombobox();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());
  parent_of_combobox_->RemoveChildView(combobox_);
  EXPECT_EQ(nullptr, combobox_->GetMenuRunnerForTest());
}

TEST_F(EditableComboboxTest, RemovingParentOfControlWhileMenuOpenClosesMenu) {
  InitEditableCombobox();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());
  widget_->GetContentsView()->RemoveChildView(parent_of_combobox_);
  EXPECT_EQ(nullptr, combobox_->GetMenuRunnerForTest());
}

TEST_F(EditableComboboxTest, LeftOrRightKeysMoveInTextfield) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_E);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_RIGHT);
  SendKeyEvent(ui::VKEY_D);
  EXPECT_EQ(ASCIIToUTF16("abcde"), combobox_->GetText());
}

#if defined(OS_WIN)
// Flaky on Windows. https://crbug.com/965601
#define MAYBE_UpOrDownKeysMoveInMenu DISABLED_UpOrDownKeysMoveInMenu
#else
#define MAYBE_UpOrDownKeysMoveInMenu UpOrDownKeysMoveInMenu
#endif
TEST_F(EditableComboboxTest, MAYBE_UpOrDownKeysMoveInMenu) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_UP);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_EQ(ASCIIToUTF16("item[1]"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, EndOrHomeMovesToBeginningOrEndOfText) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_HOME);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_END);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(ASCIIToUTF16("xabcy"), combobox_->GetText());
}

#if defined(OS_MACOSX)

TEST_F(EditableComboboxTest, AltLeftOrRightMovesToNextWords) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  combobox_->SetText(ASCIIToUTF16("foo bar foobar"));
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(ASCIIToUTF16("foo xbary foobar"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, CtrlLeftOrRightMovesToBeginningOrEndOfText) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(ASCIIToUTF16("xabcy"), combobox_->GetText());
}

#else

TEST_F(EditableComboboxTest, AltLeftOrRightDoesNothing) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(ASCIIToUTF16("abcyx"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, CtrlLeftOrRightMovesToNextWords) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  combobox_->SetText(ASCIIToUTF16("foo bar foobar"));
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_Y);
#if defined(OS_WIN)
  // Matches Windows-specific logic in
  // RenderTextHarfBuzz::AdjacentWordSelectionModel.
  EXPECT_EQ(ASCIIToUTF16("foo xbar yfoobar"), combobox_->GetText());
#else
  EXPECT_EQ(ASCIIToUTF16("foo xbary foobar"), combobox_->GetText());
#endif
}

#endif

TEST_F(EditableComboboxTest, ShiftLeftOrRightSelectsCharInTextfield) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/false, /*shift=*/true,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/false, /*shift=*/true,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(ASCIIToUTF16("ayx"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, EnterClosesMenuWhileSelectingHighlightedMenuItem) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(ASCIIToUTF16("item[0]"), combobox_->GetText());
}

#if defined(OS_WIN)
// Flaky on Windows. https://crbug.com/965601
#define MAYBE_F4ClosesMenuWhileSelectingHighlightedMenuItem \
  DISABLED_F4ClosesMenuWhileSelectingHighlightedMenuItem
#else
#define MAYBE_F4ClosesMenuWhileSelectingHighlightedMenuItem \
  F4ClosesMenuWhileSelectingHighlightedMenuItem
#endif
TEST_F(EditableComboboxTest,
       MAYBE_F4ClosesMenuWhileSelectingHighlightedMenuItem) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
  SendKeyEvent(ui::VKEY_F4);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(ASCIIToUTF16("item[0]"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, EscClosesMenuWithoutSelectingHighlightedMenuItem) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
  SendKeyEvent(ui::VKEY_ESCAPE);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(ASCIIToUTF16("a"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, TypingInTextfieldUnhighlightsMenuItem) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_RETURN);
  EXPECT_EQ(ASCIIToUTF16("abc"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, ClickOnMenuItemSelectsItAndClosesMenu) {
  InitEditableCombobox();
  ClickArrow();
  ASSERT_TRUE(IsMenuOpen());

  ClickMenuItem(/*index=*/0);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(ASCIIToUTF16("item[0]"), combobox_->GetText());
}

// This is different from the regular read-only Combobox, where SPACE
// opens/closes the menu.
TEST_F(EditableComboboxTest, SpaceIsReflectedInTextfield) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_SPACE);
  SendKeyEvent(ui::VKEY_SPACE);
  SendKeyEvent(ui::VKEY_B);
  EXPECT_EQ(ASCIIToUTF16("a  b"), combobox_->GetText());
}

#if defined(OS_WIN)
// Flaky on Windows. https://crbug.com/965601
#define MAYBE_MenuCanAdaptToContentChange DISABLED_MenuCanAdaptToContentChange
#else
#define MAYBE_MenuCanAdaptToContentChange MenuCanAdaptToContentChange
#endif
// We test that the menu can adapt to content change by using an
// EditableCombobox with |filter_on_edit| set to true, which will change the
// menu's content as the user types.
TEST_F(EditableComboboxTest, MAYBE_MenuCanAdaptToContentChange) {
  std::vector<base::string16> items = {ASCIIToUTF16("abc"), ASCIIToUTF16("abd"),
                                       ASCIIToUTF16("bac"),
                                       ASCIIToUTF16("bad")};
  InitEditableCombobox(items, /*filter_on_edit=*/true);
  ClickArrow();
  ASSERT_TRUE(IsMenuOpen());

  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_EQ(ASCIIToUTF16("abc"), combobox_->GetText());

  SendKeyEvent(ui::VKEY_BACK);
  SendKeyEvent(ui::VKEY_BACK);
  SendKeyEvent(ui::VKEY_BACK);
  MenuRunner* menu_runner1 = combobox_->GetMenuRunnerForTest();
  SendKeyEvent(ui::VKEY_B);
  MenuRunner* menu_runner2 = combobox_->GetMenuRunnerForTest();
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_EQ(ASCIIToUTF16("bac"), combobox_->GetText());

  // Even though the items shown change, the menu runner shouldn't have been
  // reset, otherwise there could be a flicker when the menu closes and reopens.
  EXPECT_EQ(menu_runner1, menu_runner2);
}

TEST_F(EditableComboboxTest, RefocusingReopensMenuBasedOnLatestContent) {
  std::vector<base::string16> items = {ASCIIToUTF16("abc"), ASCIIToUTF16("abd"),
                                       ASCIIToUTF16("bac"), ASCIIToUTF16("bad"),
                                       ASCIIToUTF16("bac2")};
  InitEditableCombobox(items, /*filter_on_edit=*/true);
  combobox_->GetTextfieldForTest()->RequestFocus();

  SendKeyEvent(ui::VKEY_B);
  ASSERT_EQ(3, combobox_->GetItemCountForTest());

  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(ASCIIToUTF16("bac"), combobox_->GetText());

  // Blur then focus to make the menu reopen. It should only show completions of
  // "bac", the selected item, instead of showing completions of "b", what we
  // had typed.
  dummy_focusable_view_->RequestFocus();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
}

TEST_F(EditableComboboxTest, GetItemsWithoutFiltering) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  InitEditableCombobox(items, /*filter_on_edit=*/false, /*show_on_empty=*/true);

  combobox_->SetText(ASCIIToUTF16("z"));
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("item0"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("item1"), combobox_->GetItemForTest(1));
}

TEST_F(EditableComboboxTest, FilteringEffectOnGetItems) {
  std::vector<base::string16> items = {ASCIIToUTF16("abc"), ASCIIToUTF16("abd"),
                                       ASCIIToUTF16("bac"),
                                       ASCIIToUTF16("bad")};
  InitEditableCombobox(items, /*filter_on_edit=*/true, /*show_on_empty=*/true);

  ASSERT_EQ(4, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("abc"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("abd"), combobox_->GetItemForTest(1));
  ASSERT_EQ(ASCIIToUTF16("bac"), combobox_->GetItemForTest(2));
  ASSERT_EQ(ASCIIToUTF16("bad"), combobox_->GetItemForTest(3));

  combobox_->SetText(ASCIIToUTF16("b"));
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("bac"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("bad"), combobox_->GetItemForTest(1));

  combobox_->SetText(ASCIIToUTF16("bc"));
  ASSERT_EQ(0, combobox_->GetItemCountForTest());

  combobox_->SetText(base::string16());
  ASSERT_EQ(4, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("abc"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("abd"), combobox_->GetItemForTest(1));
  ASSERT_EQ(ASCIIToUTF16("bac"), combobox_->GetItemForTest(2));
  ASSERT_EQ(ASCIIToUTF16("bad"), combobox_->GetItemForTest(3));
}

TEST_F(EditableComboboxTest, FilteringWithMismatchedCase) {
  std::vector<base::string16> items = {
      ASCIIToUTF16("AbCd"), ASCIIToUTF16("aBcD"), ASCIIToUTF16("xyz")};
  InitEditableCombobox(items, /*filter_on_edit=*/true, /*show_on_empty=*/true);

  ASSERT_EQ(3, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("AbCd"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("aBcD"), combobox_->GetItemForTest(1));
  ASSERT_EQ(ASCIIToUTF16("xyz"), combobox_->GetItemForTest(2));

  combobox_->SetText(ASCIIToUTF16("abcd"));
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("AbCd"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("aBcD"), combobox_->GetItemForTest(1));

  combobox_->SetText(ASCIIToUTF16("ABCD"));
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("AbCd"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("aBcD"), combobox_->GetItemForTest(1));
}

TEST_F(EditableComboboxTest, DontShowOnEmpty) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  InitEditableCombobox(items, /*filter_on_edit=*/false,
                       /*show_on_empty=*/false);

  ASSERT_EQ(0, combobox_->GetItemCountForTest());
  combobox_->SetText(ASCIIToUTF16("a"));
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("item0"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("item1"), combobox_->GetItemForTest(1));
}

TEST_F(EditableComboboxTest, NoFilteringNotifiesListener) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  InitEditableCombobox(items, /*filter_on_edit=*/false, /*show_on_empty=*/true);

  ASSERT_EQ(0, listener_->change_count());
  combobox_->SetText(ASCIIToUTF16("a"));
  ASSERT_EQ(1, listener_->change_count());
  combobox_->SetText(ASCIIToUTF16("ab"));
  ASSERT_EQ(2, listener_->change_count());
}

TEST_F(EditableComboboxTest, FilteringNotifiesListener) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  InitEditableCombobox(items, /*filter_on_edit=*/true, /*show_on_empty=*/true);

  ASSERT_EQ(0, listener_->change_count());
  combobox_->SetText(ASCIIToUTF16("i"));
  ASSERT_EQ(1, listener_->change_count());
  combobox_->SetText(ASCIIToUTF16("ix"));
  ASSERT_EQ(2, listener_->change_count());
  combobox_->SetText(ASCIIToUTF16("ixy"));
  ASSERT_EQ(3, listener_->change_count());
}

TEST_F(EditableComboboxTest, PasswordCanBeHiddenAndRevealed) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  InitEditableCombobox(items, /*filter_on_edit=*/false, /*show_on_empty=*/true,
                       EditableCombobox::Type::kPassword);

  ASSERT_EQ(2, combobox_->GetItemCountForTest());
  ASSERT_EQ(base::string16(5, gfx::RenderText::kPasswordReplacementChar),
            combobox_->GetItemForTest(0));
  ASSERT_EQ(base::string16(5, gfx::RenderText::kPasswordReplacementChar),
            combobox_->GetItemForTest(1));

  combobox_->RevealPasswords(/*revealed=*/true);
  ASSERT_EQ(ASCIIToUTF16("item0"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("item1"), combobox_->GetItemForTest(1));

  combobox_->RevealPasswords(/*revealed=*/false);
  ASSERT_EQ(base::string16(5, gfx::RenderText::kPasswordReplacementChar),
            combobox_->GetItemForTest(0));
  ASSERT_EQ(base::string16(5, gfx::RenderText::kPasswordReplacementChar),
            combobox_->GetItemForTest(1));
}

TEST_F(EditableComboboxTest, ArrowButtonOpensAndClosesMenu) {
  InitEditableCombobox();
  dummy_focusable_view_->RequestFocus();
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());

  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());
  ClickArrow();
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
}

TEST_F(EditableComboboxTest, ShowContextMenuOnMouseRelease) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  InitEditableCombobox(items, /*filter_on_edit=*/false,
                       /*show_on_empty=*/true);
  EXPECT_FALSE(IsMenuOpen());
  TestContextMenuController context_menu_controller;
  combobox_->GetTextfieldForTest()->set_context_menu_controller(
      &context_menu_controller);
  const gfx::Point textfield_point(combobox_->x() + 1, combobox_->y() + 1);
  ui::MouseEvent click_mouse_event(ui::ET_MOUSE_PRESSED, textfield_point,
                                   textfield_point, ui::EventTimeForNow(),
                                   ui::EF_RIGHT_MOUSE_BUTTON,
                                   ui::EF_RIGHT_MOUSE_BUTTON);
  widget_->OnMouseEvent(&click_mouse_event);
  EXPECT_FALSE(IsMenuOpen());
  ui::MouseEvent release_mouse_event(ui::ET_MOUSE_RELEASED, textfield_point,
                                     textfield_point, ui::EventTimeForNow(),
                                     ui::EF_RIGHT_MOUSE_BUTTON,
                                     ui::EF_RIGHT_MOUSE_BUTTON);
  widget_->OnMouseEvent(&release_mouse_event);
  // The context menu should appear, not the combobox dropdown.
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_TRUE(context_menu_controller.opened_menu());
}

TEST_F(EditableComboboxTest, DragToSelectDoesntOpenTheMenu) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  InitEditableCombobox(items, /*filter_on_edit=*/false,
                       /*show_on_empty=*/true);
  combobox_->SetText(ASCIIToUTF16("abc"));
  dummy_focusable_view_->RequestFocus();
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());

  const int kCursorXStart = 0;
  const int kCursorXEnd = combobox_->x() + combobox_->width();
  const int kCursorY = combobox_->y() + 1;
  gfx::Point start_point(kCursorXStart, kCursorY);
  gfx::Point end_point(kCursorXEnd, kCursorY);

  PerformMouseEvent(widget_, start_point, ui::ET_MOUSE_PRESSED);
  EXPECT_TRUE(combobox_->GetTextfieldForTest()->GetSelectedText().empty());

  DragMouseTo(end_point);
  ASSERT_EQ(ASCIIToUTF16("abc"),
            combobox_->GetTextfieldForTest()->GetSelectedText());
  EXPECT_FALSE(IsMenuOpen());

  PerformMouseEvent(widget_, end_point, ui::ET_MOUSE_RELEASED);
  ASSERT_EQ(ASCIIToUTF16("abc"),
            combobox_->GetTextfieldForTest()->GetSelectedText());
  EXPECT_FALSE(IsMenuOpen());
}

TEST_F(EditableComboboxTest, NoCrashWithoutWidget) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  auto combobox = std::make_unique<EditableCombobox>(
      std::make_unique<ui::SimpleComboboxModel>(items),
      /*filter_on_edit=*/false,
      /*show_on_empty=*/true, EditableCombobox::Type::kPassword);
  // Showing the dropdown should silently fail.
  combobox->RevealPasswords(true);
}

}  // namespace
}  // namespace views
