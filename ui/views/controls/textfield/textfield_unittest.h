// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_UNITTEST_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_UNITTEST_H_

#include "ui/views/controls/textfield/textfield.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/events/event_constants.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/test/views_test_base.h"

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

namespace views {

class TextfieldTestApi;

namespace test {

class MockInputMethod;
class TestTextfield;

class TextfieldTest : public ViewsTestBase, public TextfieldController {
 public:
  TextfieldTest();
  ~TextfieldTest() override;

  // ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

  ui::ClipboardBuffer GetAndResetCopiedToClipboard();
  std::u16string GetClipboardText(ui::ClipboardBuffer type);
  void SetClipboardText(ui::ClipboardBuffer type, const std::u16string& text);

  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override;
  void OnBeforeUserAction(Textfield* sender) override;
  void OnAfterUserAction(Textfield* sender) override;
  void OnAfterCutOrCopy(ui::ClipboardBuffer clipboard_type) override;

  void InitTextfield(int count = 1);
  ui::MenuModel* GetContextMenuModel();

  bool TestingNativeMac() const;
  bool TestingNativeCrOs() const;

  template <typename T>
  T* PrepareTextfields(int count,
                       std::unique_ptr<T> textfield_owned,
                       gfx::Rect bounds) {
    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->SetBounds(bounds);

    View* container = widget_->SetContentsView(std::make_unique<View>());
    T* textfield = container->AddChildView(std::move(textfield_owned));

    PrepareTextfieldsInternal(count, textfield, container, bounds);

    return textfield;
  }

 protected:
  void PrepareTextfieldsInternal(int count,
                                 Textfield* textfield,
                                 View* view,
                                 gfx::Rect bounds);

  void SendKeyPress(ui::KeyboardCode key_code, int flags);
  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool alt,
                    bool shift,
                    bool control_or_command,
                    bool caps_lock);
  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool shift,
                    bool control_or_command);
  void SendKeyEvent(ui::KeyboardCode key_code);
  void SendKeyEvent(char16_t ch);
  void SendKeyEvent(char16_t ch, int flags);
  void SendKeyEvent(char16_t ch, int flags, bool from_vk);
  void DispatchMockInputMethodKeyEvent();

  // Sends a platform-specific move (and select) to the logical start of line.
  // Eg. this should move (and select) to the right end of line for RTL text.
  virtual void SendHomeEvent(bool shift);

  // Sends a platform-specific move (and select) to the logical end of line.
  virtual void SendEndEvent(bool shift);

  // Sends {delete, move, select} word {forward, backward}.
  void SendWordEvent(ui::KeyboardCode key, bool shift);

  // Sends Shift+Delete if supported, otherwise Cmd+X again.
  void SendAlternateCut();

  // Sends Ctrl+Insert if supported, otherwise Cmd+C again.
  void SendAlternateCopy();

  // Sends Shift+Insert if supported, otherwise Cmd+V again.
  void SendAlternatePaste();

  View* GetFocusedView();
  int GetCursorPositionX(int cursor_pos);
  int GetCursorYForTesting();

  // Get the current cursor bounds.
  gfx::Rect GetCursorBounds();

  // Get the cursor bounds of |sel|.
  gfx::Rect GetCursorBounds(const gfx::SelectionModel& sel);

  gfx::Rect GetDisplayRect();
  gfx::Rect GetCursorViewRect();

  // Mouse click on the point whose x-axis is |bound|'s x plus |x_offset| and
  // y-axis is in the middle of |bound|'s vertical range.
  void MouseClick(const gfx::Rect bound, int x_offset);

  // This is to avoid double/triple click.
  void NonClientMouseClick();

  void VerifyTextfieldContextMenuContents(bool textfield_has_selection,
                                          bool can_undo,
                                          ui::MenuModel* menu);
  void PressMouseButton(ui::EventFlags mouse_button_flags);
  void ReleaseMouseButton(ui::EventFlags mouse_button_flags);
  void PressLeftMouseButton();
  void ReleaseLeftMouseButton();
  void ClickLeftMouseButton();
  void ClickRightMouseButton();
  void DragMouseTo(const gfx::Point& where);

  // Textfield does not listen to OnMouseMoved, so this function does not send
  // an event when it updates the cursor position.
  void MoveMouseTo(const gfx::Point& where);
  void TapAtCursor(ui::EventPointerType pointer_type);

  // Returns the test api for the element that is being tested. Virtual
  // because the unit tests for `Textarea` use the same test fixture and various
  // fixture methods depend on the test api of the test element.
  virtual TextfieldTestApi GetTextfieldTestApi();

  // Returns the mock input method set for the test widget created by the
  // fixture.
  MockInputMethod* input_method();
  TextfieldModel* model();

  // We need widget to populate wrapper class.
  std::unique_ptr<Widget> widget_;

  raw_ptr<TestTextfield> textfield_ = nullptr;

  // The string from Controller::ContentsChanged callback.
  std::u16string last_contents_;

  // Indicates how many times OnBeforeUserAction() is called.
  int on_before_user_action_ = 0;

  // Indicates how many times OnAfterUserAction() is called.
  int on_after_user_action_ = 0;

  // Position of the mouse for synthetic mouse events.
  gfx::Point mouse_position_;

  ui::ClipboardBuffer copied_to_clipboard_ = ui::ClipboardBuffer::kMaxValue;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  raw_ptr<View> event_target_ = nullptr;
};

}  // namespace test

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_UNITTEST_H_
