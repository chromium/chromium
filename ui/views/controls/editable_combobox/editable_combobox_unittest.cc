// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/editable_combobox/editable_combobox.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
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
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox_util.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/events/ozone/layout/keyboard_layout_engine_test_utils.h"
#endif

namespace views {
namespace {

using base::ASCIIToUTF16;
using views::test::WaitForMenuClosureAnimation;

// No-op test double of a ContextMenuController
class TestContextMenuController : public ContextMenuController {
 public:
  TestContextMenuController() = default;

  TestContextMenuController(const TestContextMenuController&) = delete;
  TestContextMenuController& operator=(const TestContextMenuController&) =
      delete;

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
};

}  // namespace

class EditableComboboxTest : public ViewsTestBase {
 public:
  static constexpr gfx::Rect kWidgetBounds = gfx::Rect(0, 0, 1000, 1000);
  static constexpr gfx::Rect kComboboxBounds = gfx::Rect(0, 0, 500, 40);

  EditableComboboxTest() { views::test::DisableMenuClosureAnimations(); }

  EditableComboboxTest(const EditableComboboxTest&) = delete;
  EditableComboboxTest& operator=(const EditableComboboxTest&) = delete;

  void SetUp() override;
  void TearDown() override;

  // Initializes the combobox with the given number of items.
  void InitEditableCombobox(int item_count = 8,
                            bool filter_on_edit = false,
                            bool show_on_empty = true);

  // Initializes the combobox with the given items.
  void InitEditableCombobox(const std::vector<std::u16string>& items,
                            bool filter_on_edit,
                            bool show_on_empty = true);

  void InitEditableCombobox(
      const std::vector<ui::SimpleComboboxModel::Item>& items,
      bool filter_on_edit,
      bool show_on_empty = true);

  // Initializes the widget where the combobox and the dummy control live.
  void InitWidget();

  static size_t GetItemCount(const EditableCombobox* combobox);

 protected:
  enum class IconSource { kMenuModel, kComboboxModel };

  size_t GetItemCount() const;
  std::u16string GetItemAt(size_t index) const;
  ui::ImageModel GetIconAt(size_t index, IconSource source) const;
  void ClickArrow();
  void ClickMenuItem(int index);
  void ClickTextfield();
  void FocusTextfield();
  bool IsTextfieldFocused() const;
  std::u16string GetSelectedText() const;
  void SetContextMenuController(ContextMenuController* controller);
  void DragMouseTo(const gfx::Point& location);
  MenuRunner* GetMenuRunner();
  bool IsMenuOpen();
  Button* GetArrowButton();
  void PerformMouseEvent(Widget* widget,
                         const gfx::Point& point,
                         ui::EventType type);
  void PerformClick(Widget* widget, const gfx::Point& point);
  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool alt = false,
                    bool shift = false,
                    bool ctrl_cmd = false);

  int change_count() const { return change_count_; }
  void OnContentChanged() { ++change_count_; }

  // The widget where the control will appear.
  std::unique_ptr<Widget> widget_;

  // |combobox_| and |dummy_focusable_view_| are allocated in
  // |InitEditableCombobox| and then owned by |widget_|.
  raw_ptr<EditableCombobox> combobox_ = nullptr;
  raw_ptr<View> dummy_focusable_view_ = nullptr;

  // We make |combobox_| a child of another View to test different removal
  // scenarios.
  raw_ptr<View> parent_of_combobox_ = nullptr;

  int change_count_ = 0;

  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

void EditableComboboxTest::SetUp() {
  ViewsTestBase::SetUp();

#if BUILDFLAG(IS_OZONE)
  // Setting up the keyboard layout engine depends on the implementation and may
  // be asynchronous.  We ensure that it is ready to use so that tests could
  // handle key events properly.
  ui::WaitUntilLayoutEngineIsReadyForTest();
#endif
}

void EditableComboboxTest::TearDown() {
  if (IsMenuOpen()) {
    GetMenuRunner()->Cancel();
    WaitForMenuClosureAnimation();
  }
  if (widget_) {
    combobox_ = nullptr;
    dummy_focusable_view_ = nullptr;
    parent_of_combobox_ = nullptr;
    widget_->Close();
  }
  ViewsTestBase::TearDown();
}

// Initializes the combobox with the given number of items.
void EditableComboboxTest::InitEditableCombobox(const int item_count,
                                                const bool filter_on_edit,
                                                const bool show_on_empty) {
  std::vector<ui::SimpleComboboxModel::Item> items;
  for (int i = 0; i < item_count; ++i)
    items.emplace_back(ASCIIToUTF16(base::StringPrintf("item[%i]", i)));
  InitEditableCombobox(items, filter_on_edit, show_on_empty);
}

void EditableComboboxTest::InitEditableCombobox(
    const std::vector<std::u16string>& strings,
    bool filter_on_edit,
    bool show_on_empty) {
  std::vector<ui::SimpleComboboxModel::Item> items;
  for (const auto& item_str : strings)
    items.emplace_back(item_str);
  InitEditableCombobox(items, filter_on_edit, show_on_empty);
}

// Initializes the combobox with the given items.
void EditableComboboxTest::InitEditableCombobox(
    const std::vector<ui::SimpleComboboxModel::Item>& items,
    const bool filter_on_edit,
    const bool show_on_empty) {
  InitWidget();

  View* container = widget_->SetContentsView(std::make_unique<View>());
  parent_of_combobox_ = container->AddChildView(std::make_unique<View>());
  parent_of_combobox_->SetBoundsRect(kComboboxBounds);

  combobox_ =
      parent_of_combobox_->AddChildView(std::make_unique<EditableCombobox>(
          std::make_unique<ui::SimpleComboboxModel>(items), filter_on_edit,
          show_on_empty));
  combobox_->SetCallback(base::BindRepeating(
      &EditableComboboxTest::OnContentChanged, base::Unretained(this)));
  combobox_->GetViewAccessibility().SetName(u"abc");
  combobox_->SetBoundsRect(kComboboxBounds);

  dummy_focusable_view_ = container->AddChildView(std::make_unique<View>());
  dummy_focusable_view_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
}

// Initializes the widget where the combobox and the dummy control live.
void EditableComboboxTest::InitWidget() {
  widget_ = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = kWidgetBounds;
  widget_->Init(std::move(params));
  widget_->Show();

#if BUILDFLAG(IS_MAC)
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
      std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_.get()));
  event_generator_->set_target(ui::test::EventGenerator::Target::WINDOW);
}

// static
size_t EditableComboboxTest::GetItemCount(const EditableCombobox* combobox) {
  return combobox->GetMenuModelForTesting()->GetItemCount();
}

size_t EditableComboboxTest::GetItemCount() const {
  return GetItemCount(combobox_);
}

std::u16string EditableComboboxTest::GetItemAt(size_t index) const {
  return combobox_->GetItemTextForTesting(index);
}

ui::ImageModel EditableComboboxTest::GetIconAt(size_t index,
                                               IconSource icon_source) const {
  switch (icon_source) {
    case IconSource::kMenuModel:
      return combobox_->GetMenuModelForTesting()->GetIconAt(index);
    case IconSource::kComboboxModel:
      return combobox_->GetMenuModelForTesting()->GetIconAt(index);
  }
}

void EditableComboboxTest::ClickArrow() {
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(combobox_->GetArrowButtonForTesting());
  test_api.NotifyClick(e);
}

void EditableComboboxTest::FocusTextfield() {
  combobox_->textfield_->RequestFocus();
}

bool EditableComboboxTest::IsTextfieldFocused() const {
  return combobox_->textfield_->HasFocus();
}

std::u16string EditableComboboxTest::GetSelectedText() const {
  return combobox_->textfield_->GetSelectedText();
}

void EditableComboboxTest::SetContextMenuController(
    ContextMenuController* controller) {
  combobox_->textfield_->set_context_menu_controller(controller);
}

void EditableComboboxTest::ClickTextfield() {
  const gfx::Point textfield(combobox_->x() + 1, combobox_->y() + 1);
  PerformClick(widget_.get(), textfield);
}

void EditableComboboxTest::DragMouseTo(const gfx::Point& location) {
  ui::MouseEvent drag(ui::EventType::kMouseDragged, location, location,
                      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  combobox_->textfield_->OnMouseDragged(drag);
}

MenuRunner* EditableComboboxTest::GetMenuRunner() {
  return combobox_->menu_runner_.get();
}

bool EditableComboboxTest::IsMenuOpen() {
  return combobox_ && GetMenuRunner() && GetMenuRunner()->IsRunning();
}

Button* EditableComboboxTest::GetArrowButton() {
  return combobox_->GetArrowButtonForTesting();
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
  PerformMouseEvent(widget, point, ui::EventType::kMousePressed);
  PerformMouseEvent(widget, point, ui::EventType::kMouseReleased);
}

void EditableComboboxTest::SendKeyEvent(ui::KeyboardCode key_code,
                                        const bool alt,
                                        const bool shift,
                                        const bool ctrl_cmd) {
#if BUILDFLAG(IS_MAC)
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
  FocusTextfield();
  EXPECT_FALSE(IsMenuOpen());
}

TEST_F(EditableComboboxTest, ArrowDownOpensMenu) {
  InitEditableCombobox();
  EXPECT_FALSE(IsMenuOpen());
  FocusTextfield();
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
}

TEST_F(EditableComboboxTest, TabMovesToOtherViewAndClosesMenu) {
  InitEditableCombobox();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());
  EXPECT_TRUE(IsTextfieldFocused());
  SendKeyEvent(ui::VKEY_TAB);
  EXPECT_FALSE(IsTextfieldFocused());
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
}

TEST_F(EditableComboboxTest,
       ClickOutsideEditableComboboxWithoutLosingFocusClosesMenu) {
  InitEditableCombobox();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());
  EXPECT_TRUE(IsTextfieldFocused());

  const gfx::Point outside_point(combobox_->x() + combobox_->width() + 1,
                                 combobox_->y() + 1);
  PerformClick(widget_.get(), outside_point);

  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_TRUE(IsTextfieldFocused());
}

TEST_F(EditableComboboxTest, ClickTextfieldDoesntCloseMenu) {
  InitEditableCombobox();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());

  MenuRunner* menu_runner1 = GetMenuRunner();
  ClickTextfield();
  MenuRunner* menu_runner2 = GetMenuRunner();
  EXPECT_TRUE(IsMenuOpen());

  // Making sure the menu didn't close and reopen (causing a flicker).
  EXPECT_EQ(menu_runner1, menu_runner2);
}

TEST_F(EditableComboboxTest, RemovingControlWhileMenuOpenClosesMenu) {
  InitEditableCombobox();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());
  auto combobox = parent_of_combobox_->RemoveChildViewT(combobox_);
  EXPECT_EQ(nullptr, GetMenuRunner());
  combobox_ = nullptr;
}

TEST_F(EditableComboboxTest, RemovingParentOfControlWhileMenuOpenClosesMenu) {
  InitEditableCombobox();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());
  auto parent =
      widget_->GetContentsView()->RemoveChildViewT(parent_of_combobox_);
  EXPECT_EQ(nullptr, GetMenuRunner());
  combobox_ = nullptr;
  parent_of_combobox_ = nullptr;
}

TEST_F(EditableComboboxTest, LeftOrRightKeysMoveInTextfield) {
  InitEditableCombobox();
  FocusTextfield();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_E);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_RIGHT);
  SendKeyEvent(ui::VKEY_D);
  EXPECT_EQ(u"abcde", combobox_->GetText());
}

#if BUILDFLAG(IS_WIN)
// Flaky on Windows. https://crbug.com/965601
#define MAYBE_UpOrDownKeysMoveInMenu DISABLED_UpOrDownKeysMoveInMenu
#else
#define MAYBE_UpOrDownKeysMoveInMenu UpOrDownKeysMoveInMenu
#endif
TEST_F(EditableComboboxTest, MAYBE_UpOrDownKeysMoveInMenu) {
  InitEditableCombobox();
  FocusTextfield();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_UP);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_EQ(u"item[1]", combobox_->GetText());
}

TEST_F(EditableComboboxTest, EndOrHomeMovesToBeginningOrEndOfText) {
  InitEditableCombobox();
  FocusTextfield();

  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_HOME);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_END);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(u"xabcy", combobox_->GetText());
}

#if BUILDFLAG(IS_MAC)

TEST_F(EditableComboboxTest, AltLeftOrRightMovesToNextWords) {
  InitEditableCombobox();
  FocusTextfield();

  combobox_->SetText(u"foo bar foobar");
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(u"foo xbary foobar", combobox_->GetText());
}

TEST_F(EditableComboboxTest, CtrlLeftOrRightMovesToBeginningOrEndOfText) {
  InitEditableCombobox();
  FocusTextfield();

  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(u"xabcy", combobox_->GetText());
}

#else

TEST_F(EditableComboboxTest, AltLeftOrRightDoesNothing) {
  InitEditableCombobox();
  FocusTextfield();

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
  EXPECT_EQ(u"abcyx", combobox_->GetText());
}

TEST_F(EditableComboboxTest, CtrlLeftOrRightMovesToNextWords) {
  InitEditableCombobox();
  FocusTextfield();

  combobox_->SetText(u"foo bar foobar");
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_Y);
#if BUILDFLAG(IS_WIN)
  // Matches Windows-specific logic in
  // RenderTextHarfBuzz::AdjacentWordSelectionModel.
  EXPECT_EQ(u"foo xbar yfoobar", combobox_->GetText());
#else
  EXPECT_EQ(u"foo xbary foobar", combobox_->GetText());
#endif
}

#endif

TEST_F(EditableComboboxTest, ShiftLeftOrRightSelectsCharInTextfield) {
  InitEditableCombobox();
  FocusTextfield();

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
  EXPECT_EQ(u"ayx", combobox_->GetText());
}

TEST_F(EditableComboboxTest, EnterClosesMenuWhileSelectingHighlightedMenuItem) {
  InitEditableCombobox();
  FocusTextfield();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(u"item[0]", combobox_->GetText());
}

#if BUILDFLAG(IS_WIN)
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
  FocusTextfield();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
  SendKeyEvent(ui::VKEY_F4);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(u"item[0]", combobox_->GetText());
}

TEST_F(EditableComboboxTest, EscClosesMenuWithoutSelectingHighlightedMenuItem) {
  InitEditableCombobox();
  FocusTextfield();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
  SendKeyEvent(ui::VKEY_ESCAPE);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(u"a", combobox_->GetText());
}

TEST_F(EditableComboboxTest, TypingInTextfieldUnhighlightsMenuItem) {
  InitEditableCombobox();
  FocusTextfield();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_RETURN);
  EXPECT_EQ(u"abc", combobox_->GetText());
}

// This is different from the regular read-only Combobox, where SPACE
// opens/closes the menu.
TEST_F(EditableComboboxTest, SpaceIsReflectedInTextfield) {
  InitEditableCombobox();
  FocusTextfield();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_SPACE);
  SendKeyEvent(ui::VKEY_SPACE);
  SendKeyEvent(ui::VKEY_B);
  EXPECT_EQ(u"a  b", combobox_->GetText());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// Flaky on Windows and Linux. https://crbug.com/965601
#define MAYBE_MenuCanAdaptToContentChange DISABLED_MenuCanAdaptToContentChange
#else
#define MAYBE_MenuCanAdaptToContentChange MenuCanAdaptToContentChange
#endif
// We test that the menu can adapt to content change by using an
// EditableCombobox with |filter_on_edit| set to true, which will change the
// menu's content as the user types.
TEST_F(EditableComboboxTest, MAYBE_MenuCanAdaptToContentChange) {
  std::vector<std::u16string> items = {u"abc", u"abd", u"bac", u"bad"};
  InitEditableCombobox(items, /*filter_on_edit=*/true);
  ClickArrow();
  ASSERT_TRUE(IsMenuOpen());

  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_EQ(u"abc", combobox_->GetText());

  SendKeyEvent(ui::VKEY_BACK);
  SendKeyEvent(ui::VKEY_BACK);
  SendKeyEvent(ui::VKEY_BACK);
  MenuRunner* menu_runner1 = GetMenuRunner();
  SendKeyEvent(ui::VKEY_B);
  MenuRunner* menu_runner2 = GetMenuRunner();
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_EQ(u"bac", combobox_->GetText());

  // Even though the items shown change, the menu runner shouldn't have been
  // reset, otherwise there could be a flicker when the menu closes and reopens.
  EXPECT_EQ(menu_runner1, menu_runner2);
}

#if BUILDFLAG(IS_LINUX)
// Flaky on Linux. https://crbug.com/1204584
#define MAYBE_RefocusingReopensMenuBasedOnLatestContent \
    DISABLED_RefocusingReopensMenuBasedOnLatestContent
#else
#define MAYBE_RefocusingReopensMenuBasedOnLatestContent \
    RefocusingReopensMenuBasedOnLatestContent
#endif
TEST_F(EditableComboboxTest, MAYBE_RefocusingReopensMenuBasedOnLatestContent) {
  std::vector<std::u16string> items = {u"abc", u"abd", u"bac", u"bad", u"bac2"};
  InitEditableCombobox(items, /*filter_on_edit=*/true);
  FocusTextfield();

  SendKeyEvent(ui::VKEY_B);
  ASSERT_EQ(3u, GetItemCount());

  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(u"bac", combobox_->GetText());

  // Blur then focus to make the menu reopen. It should only show completions of
  // "bac", the selected item, instead of showing completions of "b", what we
  // had typed.
  dummy_focusable_view_->RequestFocus();
  ClickArrow();
  EXPECT_TRUE(IsMenuOpen());
  ASSERT_EQ(2u, GetItemCount());
}

TEST_F(EditableComboboxTest, GetItemsWithoutFiltering) {
  std::vector<std::u16string> items = {u"item0", u"item1"};
  InitEditableCombobox(items, /*filter_on_edit=*/false, /*show_on_empty=*/true);

  combobox_->SetText(u"z");
  ASSERT_EQ(2u, GetItemCount());
  ASSERT_EQ(u"item0", GetItemAt(0));
  ASSERT_EQ(u"item1", GetItemAt(1));
}

TEST_F(EditableComboboxTest, FilteringEffectOnGetItems) {
  std::vector<std::u16string> items = {u"abc", u"abd", u"bac", u"bad"};
  InitEditableCombobox(items, /*filter_on_edit=*/true, /*show_on_empty=*/true);

  ASSERT_EQ(4u, GetItemCount());
  ASSERT_EQ(u"abc", GetItemAt(0));
  ASSERT_EQ(u"abd", GetItemAt(1));
  ASSERT_EQ(u"bac", GetItemAt(2));
  ASSERT_EQ(u"bad", GetItemAt(3));

  combobox_->SetText(u"b");
  ASSERT_EQ(2u, GetItemCount());
  ASSERT_EQ(u"bac", GetItemAt(0));
  ASSERT_EQ(u"bad", GetItemAt(1));

  combobox_->SetText(u"bc");
  ASSERT_EQ(0u, GetItemCount());

  combobox_->SetText(std::u16string());
  ASSERT_EQ(4u, GetItemCount());
  ASSERT_EQ(u"abc", GetItemAt(0));
  ASSERT_EQ(u"abd", GetItemAt(1));
  ASSERT_EQ(u"bac", GetItemAt(2));
  ASSERT_EQ(u"bad", GetItemAt(3));
}

TEST_F(EditableComboboxTest, FilteringEffectOnIcons) {
  ui::SimpleComboboxModel::Item item1(
      u"abc", std::u16string(),
      ui::ImageModel::FromImage(gfx::test::CreateImage(16, 16)));

  ui::SimpleComboboxModel::Item item2(
      u"def", std::u16string(),
      ui::ImageModel::FromImage(gfx::test::CreateImage(20, 20)));

  InitEditableCombobox({item1, item2},
                       /*filter_on_edit=*/true,
                       /*show_on_empty=*/true);

  ASSERT_EQ(2u, GetItemCount());
  EXPECT_EQ(16, GetIconAt(0, IconSource::kComboboxModel).Size().width());
  EXPECT_EQ(20, GetIconAt(1, IconSource::kComboboxModel).Size().width());

  combobox_->SetText(u"a");
  ASSERT_EQ(1u, GetItemCount());
  EXPECT_EQ(16, GetIconAt(0, IconSource::kMenuModel).Size().width());

  combobox_->SetText(u"d");
  ASSERT_EQ(1u, GetItemCount());
  EXPECT_EQ(20, GetIconAt(0, IconSource::kMenuModel).Size().width());
}

TEST_F(EditableComboboxTest, FilteringWithMismatchedCase) {
  std::vector<std::u16string> items = {u"AbCd", u"aBcD", u"xyz"};
  InitEditableCombobox(items, /*filter_on_edit=*/true, /*show_on_empty=*/true);

  ASSERT_EQ(3u, GetItemCount());
  ASSERT_EQ(u"AbCd", GetItemAt(0));
  ASSERT_EQ(u"aBcD", GetItemAt(1));
  ASSERT_EQ(u"xyz", GetItemAt(2));

  combobox_->SetText(u"abcd");
  ASSERT_EQ(2u, GetItemCount());
  ASSERT_EQ(u"AbCd", GetItemAt(0));
  ASSERT_EQ(u"aBcD", GetItemAt(1));

  combobox_->SetText(u"ABCD");
  ASSERT_EQ(2u, GetItemCount());
  ASSERT_EQ(u"AbCd", GetItemAt(0));
  ASSERT_EQ(u"aBcD", GetItemAt(1));
}

TEST_F(EditableComboboxTest, DontShowOnEmpty) {
  std::vector<std::u16string> items = {u"item0", u"item1"};
  InitEditableCombobox(items, /*filter_on_edit=*/false,
                       /*show_on_empty=*/false);

  ASSERT_EQ(0u, GetItemCount());
  combobox_->SetText(u"a");
  ASSERT_EQ(2u, GetItemCount());
  ASSERT_EQ(u"item0", GetItemAt(0));
  ASSERT_EQ(u"item1", GetItemAt(1));
}

TEST_F(EditableComboboxTest, NoFilteringNotifiesCallback) {
  std::vector<std::u16string> items = {u"item0", u"item1"};
  InitEditableCombobox(items, /*filter_on_edit=*/false, /*show_on_empty=*/true);

  ASSERT_EQ(0, change_count());
  combobox_->SetText(u"a");
  ASSERT_EQ(1, change_count());
  combobox_->SetText(u"ab");
  ASSERT_EQ(2, change_count());
}

TEST_F(EditableComboboxTest, FilteringNotifiesCallback) {
  std::vector<std::u16string> items = {u"item0", u"item1"};
  InitEditableCombobox(items, /*filter_on_edit=*/true, /*show_on_empty=*/true);

  ASSERT_EQ(0, change_count());
  combobox_->SetText(u"i");
  ASSERT_EQ(1, change_count());
  combobox_->SetText(u"ix");
  ASSERT_EQ(2, change_count());
  combobox_->SetText(u"ixy");
  ASSERT_EQ(3, change_count());
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
  std::vector<std::u16string> items = {u"item0", u"item1"};
  InitEditableCombobox(items, /*filter_on_edit=*/false,
                       /*show_on_empty=*/true);
  EXPECT_FALSE(IsMenuOpen());
  TestContextMenuController context_menu_controller;
  SetContextMenuController(&context_menu_controller);
  const gfx::Point textfield_point(combobox_->x() + 1, combobox_->y() + 1);
  ui::MouseEvent click_mouse_event(
      ui::EventType::kMousePressed, textfield_point, textfield_point,
      ui::EventTimeForNow(), ui::EF_RIGHT_MOUSE_BUTTON,
      ui::EF_RIGHT_MOUSE_BUTTON);
  widget_->OnMouseEvent(&click_mouse_event);
  EXPECT_FALSE(IsMenuOpen());
  ui::MouseEvent release_mouse_event(
      ui::EventType::kMouseReleased, textfield_point, textfield_point,
      ui::EventTimeForNow(), ui::EF_RIGHT_MOUSE_BUTTON,
      ui::EF_RIGHT_MOUSE_BUTTON);
  widget_->OnMouseEvent(&release_mouse_event);
  // The context menu should appear, not the combobox dropdown.
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_TRUE(context_menu_controller.opened_menu());
}

TEST_F(EditableComboboxTest, DragToSelectDoesntOpenTheMenu) {
  std::vector<std::u16string> items = {u"item0", u"item1"};
  InitEditableCombobox(items, /*filter_on_edit=*/false,
                       /*show_on_empty=*/true);
  combobox_->SetText(u"abc");
  dummy_focusable_view_->RequestFocus();
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());

  const int kCursorXStart = 0;
  const int kCursorXEnd = combobox_->x() + combobox_->width();
  const int kCursorY = combobox_->y() + 1;
  gfx::Point start_point(kCursorXStart, kCursorY);
  gfx::Point end_point(kCursorXEnd, kCursorY);

  PerformMouseEvent(widget_.get(), start_point, ui::EventType::kMousePressed);
  EXPECT_TRUE(GetSelectedText().empty());

  DragMouseTo(end_point);
  ASSERT_EQ(u"abc", GetSelectedText());
  EXPECT_FALSE(IsMenuOpen());

  PerformMouseEvent(widget_.get(), end_point, ui::EventType::kMouseReleased);
  ASSERT_EQ(u"abc", GetSelectedText());
  EXPECT_FALSE(IsMenuOpen());
}

TEST_F(EditableComboboxTest, AccessibleNameAndRole) {
  InitEditableCombobox();

  ui::AXNodeData data;
  combobox_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kComboBoxGrouping);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"abc");
  EXPECT_EQ(combobox_->GetViewAccessibility().GetCachedName(), u"abc");

  data = ui::AXNodeData();
  combobox_->GetViewAccessibility().SetName(u"New name");
  combobox_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"New name");
  EXPECT_EQ(combobox_->GetViewAccessibility().GetCachedName(), u"New name");
}

TEST_F(EditableComboboxTest, AccessibleValue) {
  InitEditableCombobox();
  // kValue should be empty when the combobox is empty.
  ui::AXNodeData data;
  combobox_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kValue), u"");

  FocusTextfield();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());

  data = ui::AXNodeData();
  combobox_->GetViewAccessibility().GetAccessibleNodeData(&data);
  ASSERT_TRUE(data.HasStringAttribute(ax::mojom::StringAttribute::kValue));
  std::u16string val =
      data.GetString16Attribute(ax::mojom::StringAttribute::kValue);
  EXPECT_EQ(u"item[0]", val);
}

TEST_F(EditableComboboxTest, AccessibleArrowDefaultActionVerb) {
  InitEditableCombobox();
  auto* arrow_button = GetArrowButton();
  ui::AXNodeData data;
  arrow_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kOpen);

  arrow_button->SetEnabled(false);
  data = ui::AXNodeData();
  arrow_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(
      data.HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));

  arrow_button->SetEnabled(true);
  data = ui::AXNodeData();
  arrow_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kOpen);
}

using EditableComboboxDefaultTest = ViewsTestBase;

class ConfigurableComboboxModel final : public ui::ComboboxModel {
 public:
  explicit ConfigurableComboboxModel(bool* destroyed = nullptr)
      : destroyed_(destroyed) {
    if (destroyed_)
      *destroyed_ = false;
  }
  ConfigurableComboboxModel(ConfigurableComboboxModel&) = delete;
  ConfigurableComboboxModel& operator=(const ConfigurableComboboxModel&) =
      delete;
  ~ConfigurableComboboxModel() override {
    if (destroyed_)
      *destroyed_ = true;
  }

  // ui::ComboboxModel:
  size_t GetItemCount() const override { return item_count_; }
  std::u16string GetItemAt(size_t index) const override {
    DCHECK_LT(index, item_count_);
    return base::NumberToString16(index);
  }

  void SetItemCount(size_t item_count) { item_count_ = item_count; }

 private:
  const raw_ptr<bool> destroyed_;
  size_t item_count_ = 0;
};

TEST_F(EditableComboboxDefaultTest, Default) {
  auto combobox = std::make_unique<EditableCombobox>();
  EXPECT_EQ(0u, EditableComboboxTest::GetItemCount(combobox.get()));
}

TEST_F(EditableComboboxDefaultTest, SetModel) {
  std::unique_ptr<ConfigurableComboboxModel> model =
      std::make_unique<ConfigurableComboboxModel>();
  model->SetItemCount(42);
  auto combobox = std::make_unique<EditableCombobox>();
  combobox->SetModel(std::move(model));
  EXPECT_EQ(42u, EditableComboboxTest::GetItemCount(combobox.get()));
}

TEST_F(EditableComboboxDefaultTest, SetModelOverwrite) {
  bool destroyed_first = false;
  bool destroyed_second = false;
  {
    auto combobox = std::make_unique<EditableCombobox>();
    combobox->SetModel(
        std::make_unique<ConfigurableComboboxModel>(&destroyed_first));
    ASSERT_FALSE(destroyed_first);
    combobox->SetModel(
        std::make_unique<ConfigurableComboboxModel>(&destroyed_second));
    EXPECT_TRUE(destroyed_first);
    ASSERT_FALSE(destroyed_second);
  }
  EXPECT_TRUE(destroyed_second);
}

}  // namespace views
