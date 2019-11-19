// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/i18n/rtl.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/gfx/render_text.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/textfield/textfield_model.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/base/ime/linux/text_edit_key_bindings_delegate_auralinux.h"
#endif

#if defined(OS_CHROMEOS)
#include "ui/aura/window.h"
#include "ui/wm/core/ime_util_chromeos.h"
#endif

#if defined(OS_MACOSX)
#include "ui/base/cocoa/secure_password_input.h"
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::WideToUTF16;

#define EXPECT_STR_EQ(ascii, utf16) EXPECT_EQ(ASCIIToUTF16(ascii), utf16)

namespace {

const base::char16 kHebrewLetterSamekh = 0x05E1;

class MockInputMethod : public ui::InputMethodBase {
 public:
  MockInputMethod();
  ~MockInputMethod() override;

  // Overridden from InputMethod:
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* key) override;
  void OnTextInputTypeChanged(const ui::TextInputClient* client) override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void CancelComposition(const ui::TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;
  void ShowVirtualKeyboardIfEnabled() override {}

  bool untranslated_ime_message_called() const {
    return untranslated_ime_message_called_;
  }
  bool text_input_type_changed() const { return text_input_type_changed_; }
  bool cancel_composition_called() const { return cancel_composition_called_; }

  // Clears all internal states and result.
  void Clear();

  void SetCompositionTextForNextKey(const ui::CompositionText& composition);
  void SetResultTextForNextKey(const base::string16& result);

 private:
  // Overridden from InputMethodBase.
  void OnWillChangeFocusedClient(ui::TextInputClient* focused_before,
                                 ui::TextInputClient* focused) override;

  // Clears boolean states defined below.
  void ClearStates();

  // Whether a mock composition or result is scheduled for the next key event.
  bool HasComposition();

  // Clears only composition information and result text.
  void ClearComposition();

  // Composition information for the next key event. It'll be cleared
  // automatically after dispatching the next key event.
  ui::CompositionText composition_;

  // Result text for the next key event. It'll be cleared automatically after
  // dispatching the next key event.
  base::string16 result_text_;

  // Record call state of corresponding methods. They will be set to false
  // automatically before dispatching a key event.
  bool untranslated_ime_message_called_ = false;
  bool text_input_type_changed_ = false;
  bool cancel_composition_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockInputMethod);
};

MockInputMethod::MockInputMethod() : ui::InputMethodBase(nullptr) {}

MockInputMethod::~MockInputMethod() = default;

ui::EventDispatchDetails MockInputMethod::DispatchKeyEvent(ui::KeyEvent* key) {
// On Mac, emulate InputMethodMac behavior for character events. Composition
// still needs to be mocked, since it's not possible to generate test events
// which trigger the appropriate NSResponder action messages for composition.
#if defined(OS_MACOSX)
  if (key->is_char())
    return DispatchKeyEventPostIME(key);
#endif

  // Checks whether the key event is from EventGenerator on Windows which will
  // generate key event for WM_CHAR.
  // The MockInputMethod will insert char on WM_KEYDOWN so ignore WM_CHAR here.
  if (key->is_char() && key->HasNativeEvent()) {
    key->SetHandled();
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails dispatch_details;

  bool handled = !IsTextInputTypeNone() && HasComposition();
  ClearStates();
  if (handled) {
    DCHECK(!key->is_char());
    ui::KeyEvent mock_key(ui::ET_KEY_PRESSED,
                          ui::VKEY_PROCESSKEY,
                          key->flags());
    dispatch_details = DispatchKeyEventPostIME(&mock_key);
  } else {
    dispatch_details = DispatchKeyEventPostIME(key);
  }

  if (key->handled() || dispatch_details.dispatcher_destroyed)
    return dispatch_details;

  ui::TextInputClient* client = GetTextInputClient();
  if (client) {
    if (handled) {
      if (result_text_.length())
        client->InsertText(result_text_);
      if (composition_.text.length())
        client->SetCompositionText(composition_);
      else
        client->ClearCompositionText();
    } else if (key->type() == ui::ET_KEY_PRESSED) {
      base::char16 ch = key->GetCharacter();
      if (ch)
        client->InsertChar(*key);
    }
  }

  ClearComposition();

  return dispatch_details;
}

void MockInputMethod::OnTextInputTypeChanged(
    const ui::TextInputClient* client) {
  if (IsTextInputClientFocused(client))
    text_input_type_changed_ = true;
  InputMethodBase::OnTextInputTypeChanged(client);
}

void MockInputMethod::CancelComposition(const ui::TextInputClient* client) {
  if (IsTextInputClientFocused(client)) {
    cancel_composition_called_ = true;
    ClearComposition();
  }
}

bool MockInputMethod::IsCandidatePopupOpen() const {
  return false;
}

void MockInputMethod::OnWillChangeFocusedClient(
    ui::TextInputClient* focused_before,
    ui::TextInputClient* focused) {
  ui::TextInputClient* client = GetTextInputClient();
  if (client && client->HasCompositionText())
    client->ConfirmCompositionText(/* keep_selection */ false);
  ClearComposition();
}

void MockInputMethod::Clear() {
  ClearStates();
  ClearComposition();
}

void MockInputMethod::SetCompositionTextForNextKey(
    const ui::CompositionText& composition) {
  composition_ = composition;
}

void MockInputMethod::SetResultTextForNextKey(const base::string16& result) {
  result_text_ = result;
}

void MockInputMethod::ClearStates() {
  untranslated_ime_message_called_ = false;
  text_input_type_changed_ = false;
  cancel_composition_called_ = false;
}

bool MockInputMethod::HasComposition() {
  return composition_.text.length() || result_text_.length();
}

void MockInputMethod::ClearComposition() {
  composition_ = ui::CompositionText();
  result_text_.clear();
}

// A Textfield wrapper to intercept OnKey[Pressed|Released]() results.
class TestTextfield : public views::Textfield {
 public:
  TestTextfield() = default;

  // ui::TextInputClient overrides:
  void InsertChar(const ui::KeyEvent& e) override {
    views::Textfield::InsertChar(e);
#if defined(OS_MACOSX)
    // On Mac, characters are inserted directly rather than attempting to get a
    // unicode character from the ui::KeyEvent (which isn't always possible).
    key_received_ = true;
#endif
  }

  bool key_handled() const { return key_handled_; }
  bool key_received() const { return key_received_; }
  int event_flags() const { return event_flags_; }

  void clear() {
    key_received_ = key_handled_ = false;
    event_flags_ = 0;
  }

  void OnAccessibilityEvent(ax::mojom::Event event_type) override {
    if (event_type == ax::mojom::Event::kTextSelectionChanged)
      ++accessibility_selection_fired_count_;
  }

  int GetAccessibilitySelectionFiredCount() {
    return accessibility_selection_fired_count_;
  }

 private:
  // views::View override:
  void OnKeyEvent(ui::KeyEvent* event) override {
    key_received_ = true;
    event_flags_ = event->flags();

    // Since Textfield::OnKeyPressed() might destroy |this|, get a weak pointer
    // and verify it isn't null before writing the bool value to key_handled_.
    base::WeakPtr<TestTextfield> textfield(weak_ptr_factory_.GetWeakPtr());
    views::View::OnKeyEvent(event);

    if (!textfield)
      return;

    key_handled_ = event->handled();

    // Currently, Textfield::OnKeyReleased always returns false.
    if (event->type() == ui::ET_KEY_RELEASED)
      EXPECT_FALSE(key_handled_);
  }

  bool key_handled_ = false;
  bool key_received_ = false;
  int event_flags_ = 0;
  int accessibility_selection_fired_count_ = 0;

  base::WeakPtrFactory<TestTextfield> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestTextfield);
};

// Convenience to make constructing a GestureEvent simpler.
class GestureEventForTest : public ui::GestureEvent {
 public:
  GestureEventForTest(int x, int y, ui::GestureEventDetails details)
      : GestureEvent(x, y, 0, base::TimeTicks(), details) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureEventForTest);
};

// This controller will happily destroy the target textfield passed on
// construction when a key event is triggered.
class TextfieldDestroyerController : public views::TextfieldController {
 public:
  explicit TextfieldDestroyerController(views::Textfield* target)
      : target_(target) {
    target_->set_controller(this);
  }

  views::Textfield* target() { return target_.get(); }

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (target_)
      target_->OnBlur();
    target_.reset();
    return false;
  }

 private:
  std::unique_ptr<views::Textfield> target_;
};

// Class that focuses a textfield when it sees a KeyDown event.
class TextfieldFocuser : public views::View {
 public:
  explicit TextfieldFocuser(views::Textfield* textfield)
      : textfield_(textfield) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }

  void set_consume(bool consume) { consume_ = consume; }

  // View:
  bool OnKeyPressed(const ui::KeyEvent& event) override {
    textfield_->RequestFocus();
    return consume_;
  }

 private:
  bool consume_ = true;
  views::Textfield* textfield_;

  DISALLOW_COPY_AND_ASSIGN(TextfieldFocuser);
};

base::string16 GetClipboardText(ui::ClipboardBuffer clipboard_buffer) {
  base::string16 text;
  ui::Clipboard::GetForCurrentThread()->ReadText(clipboard_buffer, &text);
  return text;
}

void SetClipboardText(ui::ClipboardBuffer clipboard_buffer,
                      const std::string& text) {
  ui::ScopedClipboardWriter(clipboard_buffer).WriteText(ASCIIToUTF16(text));
}

}  // namespace

namespace views {

class TextfieldTest : public ViewsTestBase, public TextfieldController {
 public:
  TextfieldTest() {
    input_method_ = new MockInputMethod();
    ui::SetUpInputMethodForTesting(input_method_);
  }

  // ::testing::Test:
  void TearDown() override {
    if (widget_)
      widget_->Close();
    // Clear kill buffer used for "Yank" text editing command so that no state
    // persists between tests.
    TextfieldModel::ClearKillBuffer();
    ViewsTestBase::TearDown();
  }

  void SetUp() override {
    // OS clipboard is a global resource, which causes flakiness when unit tests
    // run in parallel. So, use a per-instance test clipboard.
    ui::TestClipboard::CreateForCurrentThread();
    ViewsTestBase::SetUp();
  }

  ui::ClipboardBuffer GetAndResetCopiedToClipboard() {
    ui::ClipboardBuffer clipboard_buffer = copied_to_clipboard_;
    copied_to_clipboard_ = ui::ClipboardBuffer::kMaxValue;
    return clipboard_buffer;
  }

  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const base::string16& new_contents) override {
    // Paste calls TextfieldController::ContentsChanged() explicitly even if the
    // paste action did not change the content. So |new_contents| may match
    // |last_contents_|. For more info, see http://crbug.com/79002
    last_contents_ = new_contents;
  }

  void OnBeforeUserAction(Textfield* sender) override {
    ++on_before_user_action_;
  }

  void OnAfterUserAction(Textfield* sender) override {
    ++on_after_user_action_;
  }

  void OnAfterCutOrCopy(ui::ClipboardBuffer clipboard_buffer) override {
    copied_to_clipboard_ = clipboard_buffer;
  }

  void InitTextfield() {
    InitTextfields(1);
  }

  void InitTextfields(int count) {
    ASSERT_FALSE(textfield_);
    textfield_ = new TestTextfield();
    textfield_->set_controller(this);
    widget_ = new Widget();

    // The widget type must be an activatable type, and we don't want to worry
    // about the non-client view, which leaves just TYPE_WINDOW_FRAMELESS.
    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);

    params.bounds = gfx::Rect(100, 100, 100, 100);
    widget_->Init(std::move(params));
    input_method_->SetDelegate(
        test::WidgetTest::GetInputMethodDelegateForWidget(widget_));
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(textfield_);
    textfield_->SetBoundsRect(params.bounds);
    textfield_->SetID(1);
    test_api_ = std::make_unique<TextfieldTestApi>(textfield_);

    for (int i = 1; i < count; i++) {
      Textfield* textfield = new Textfield();
      container->AddChildView(textfield);
      textfield->SetID(i + 1);
    }

    model_ = test_api_->model();
    model_->ClearEditHistory();

    // Since the window type is activatable, showing the widget will also
    // activate it. Calling Activate directly is insufficient, since that does
    // not also _focus_ an aura::Window (i.e. using the FocusClient). Both the
    // widget and the textfield must have focus to properly handle input.
    widget_->Show();
    textfield_->RequestFocus();

    event_generator_ =
        std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_));
    event_generator_->set_target(ui::test::EventGenerator::Target::WINDOW);
  }
  ui::MenuModel* GetContextMenuModel() {
    test_api_->UpdateContextMenu();
    return test_api_->context_menu_contents();
  }

  // True if native Mac keystrokes should be used (to avoid ifdef litter).
  bool TestingNativeMac() {
#if defined(OS_MACOSX)
    return true;
#else
    return false;
#endif
  }

  bool TestingNativeCrOs() const {
#if defined(OS_CHROMEOS)
    return true;
#else
    return false;
#endif  // defined(OS_CHROMEOS)
  }

 protected:
  void SendKeyPress(ui::KeyboardCode key_code, int flags) {
    event_generator_->PressKey(key_code, flags);
  }

  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool alt,
                    bool shift,
                    bool control_or_command,
                    bool caps_lock) {
    bool control = control_or_command;
    bool command = false;

    // By default, swap control and command for native events on Mac. This
    // handles most cases.
    if (TestingNativeMac())
      std::swap(control, command);

    int flags = (shift ? ui::EF_SHIFT_DOWN : 0) |
                (control ? ui::EF_CONTROL_DOWN : 0) |
                (alt ? ui::EF_ALT_DOWN : 0) |
                (command ? ui::EF_COMMAND_DOWN : 0) |
                (caps_lock ? ui::EF_CAPS_LOCK_ON : 0);

    SendKeyPress(key_code, flags);
  }

  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool shift,
                    bool control_or_command) {
    SendKeyEvent(key_code, false, shift, control_or_command, false);
  }

  void SendKeyEvent(ui::KeyboardCode key_code) {
    SendKeyEvent(key_code, false, false);
  }

  void SendKeyEvent(base::char16 ch) { SendKeyEvent(ch, ui::EF_NONE, false); }

  void SendKeyEvent(base::char16 ch, int flags) {
    SendKeyEvent(ch, flags, false);
  }

  void SendKeyEvent(base::char16 ch, int flags, bool from_vk) {
    if (ch < 0x80) {
      ui::KeyboardCode code =
          ch == ' ' ? ui::VKEY_SPACE :
          static_cast<ui::KeyboardCode>(ui::VKEY_A + ch - 'a');
      SendKeyPress(code, flags);
    } else {
      // For unicode characters, assume they come from IME rather than the
      // keyboard. So they are dispatched directly to the input method. But on
      // Mac, key events don't pass through InputMethod. Hence they are
      // dispatched regularly.
      ui::KeyEvent event(ch, ui::VKEY_UNKNOWN, ui::DomCode::NONE, flags);
      if (from_vk) {
        ui::Event::Properties properties;
        properties[ui::kPropertyFromVK] =
            std::vector<uint8_t>(ui::kPropertyFromVKSize);
        event.SetProperties(properties);
      }
#if defined(OS_MACOSX)
      event_generator_->Dispatch(&event);
#else
      input_method_->DispatchKeyEvent(&event);
#endif
    }
  }

  // Send a key to trigger MockInputMethod::DispatchKeyEvent(). Note the
  // specific VKEY isn't used (MockInputMethod will mock a ui::VKEY_PROCESSKEY
  // whenever it has a test composition). However, on Mac, it can't be a letter
  // (e.g. VKEY_A) since all native character events on Mac are unicode events
  // and don't have a meaningful ui::KeyEvent that would trigger
  // DispatchKeyEvent(). It also can't be VKEY_ENTER, since those key events may
  // need to be suppressed when interacting with real system IME.
  void DispatchMockInputMethodKeyEvent() { SendKeyEvent(ui::VKEY_INSERT); }

  // Sends a platform-specific move (and select) to the logical start of line.
  // Eg. this should move (and select) to the right end of line for RTL text.
  void SendHomeEvent(bool shift) {
    if (TestingNativeMac()) {
      // [NSResponder moveToBeginningOfLine:] is the correct way to do this on
      // Mac, but that doesn't have a default key binding. Since
      // views::Textfield doesn't currently support multiple lines, the same
      // effect can be achieved by Cmd+Up which maps to
      // [NSResponder moveToBeginningOfDocument:].
      SendKeyEvent(ui::VKEY_UP, shift /* shift */, true /* command */);
      return;
    }
    SendKeyEvent(ui::VKEY_HOME, shift /* shift */, false /* control */);
  }

  // Sends a platform-specific move (and select) to the logical end of line.
  void SendEndEvent(bool shift) {
    if (TestingNativeMac()) {
      SendKeyEvent(ui::VKEY_DOWN, shift, true);  // Cmd+Down.
      return;
    }
    SendKeyEvent(ui::VKEY_END, shift, false);
  }

  // Sends {delete, move, select} word {forward, backward}.
  void SendWordEvent(ui::KeyboardCode key, bool shift) {
    bool alt = false;
    bool control = true;
    bool caps = false;
    if (TestingNativeMac()) {
      // Use Alt+Left/Right/Backspace on native Mac.
      alt = true;
      control = false;
    }
    SendKeyEvent(key, alt, shift, control, caps);
  }

  // Sends Shift+Delete if supported, otherwise Cmd+X again.
  void SendAlternateCut() {
    if (TestingNativeMac())
      SendKeyEvent(ui::VKEY_X, false, true);
    else
      SendKeyEvent(ui::VKEY_DELETE, true, false);
  }

  // Sends Ctrl+Insert if supported, otherwise Cmd+C again.
  void SendAlternateCopy() {
    if (TestingNativeMac())
      SendKeyEvent(ui::VKEY_C, false, true);
    else
      SendKeyEvent(ui::VKEY_INSERT, false, true);
  }

  // Sends Shift+Insert if supported, otherwise Cmd+V again.
  void SendAlternatePaste() {
    if (TestingNativeMac())
      SendKeyEvent(ui::VKEY_V, false, true);
    else
      SendKeyEvent(ui::VKEY_INSERT, true, false);
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  int GetCursorPositionX(int cursor_pos) {
    return test_api_->GetRenderText()->GetCursorBounds(
        gfx::SelectionModel(cursor_pos, gfx::CURSOR_FORWARD), false).x();
  }

  int GetCursorYForTesting() {
    return test_api_->GetRenderText()->GetLineOffset(0).y() + 1;
  }

  // Get the current cursor bounds.
  gfx::Rect GetCursorBounds() {
    return test_api_->GetRenderText()->GetUpdatedCursorBounds();
  }

  // Get the cursor bounds of |sel|.
  gfx::Rect GetCursorBounds(const gfx::SelectionModel& sel) {
    return test_api_->GetRenderText()->GetCursorBounds(sel, true);
  }

  gfx::Rect GetDisplayRect() {
    return test_api_->GetRenderText()->display_rect();
  }

  // Mouse click on the point whose x-axis is |bound|'s x plus |x_offset| and
  // y-axis is in the middle of |bound|'s vertical range.
  void MouseClick(const gfx::Rect bound, int x_offset) {
    gfx::Point point(bound.x() + x_offset, bound.y() + bound.height() / 2);
    ui::MouseEvent click(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMousePressed(click);
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, point, point,
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMouseReleased(release);
  }

  // This is to avoid double/triple click.
  void NonClientMouseClick() {
    ui::MouseEvent click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_NON_CLIENT,
                         ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMousePressed(click);
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(),
                           ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_NON_CLIENT,
                           ui::EF_LEFT_MOUSE_BUTTON);
    textfield_->OnMouseReleased(release);
  }

  void VerifyTextfieldContextMenuContents(bool textfield_has_selection,
                                          bool can_undo,
                                          ui::MenuModel* menu) {
    const auto& text = textfield_->GetText();
    const bool is_all_selected = !text.empty() &&
        textfield_->GetSelectedRange().length() == text.length();

    int menu_index = 0;

#if defined(OS_MACOSX)
    if (textfield_has_selection) {
      EXPECT_TRUE(menu->IsEnabledAt(menu_index++ /* Look Up "Selection" */));
      EXPECT_TRUE(menu->IsEnabledAt(menu_index++ /* Separator */));
    }
#endif

    if (ui::IsEmojiPanelSupported()) {
      EXPECT_TRUE(menu->IsEnabledAt(menu_index++ /* EMOJI */));
      EXPECT_TRUE(menu->IsEnabledAt(menu_index++ /* Separator */));
    }

    EXPECT_EQ(can_undo, menu->IsEnabledAt(menu_index++ /* UNDO */));
    EXPECT_TRUE(menu->IsEnabledAt(menu_index++ /* Separator */));
    EXPECT_EQ(textfield_has_selection,
              menu->IsEnabledAt(menu_index++ /* CUT */));
    EXPECT_EQ(textfield_has_selection,
              menu->IsEnabledAt(menu_index++ /* COPY */));
    EXPECT_NE(GetClipboardText(ui::ClipboardBuffer::kCopyPaste).empty(),
              menu->IsEnabledAt(menu_index++ /* PASTE */));
    EXPECT_EQ(textfield_has_selection,
              menu->IsEnabledAt(menu_index++ /* DELETE */));
    EXPECT_TRUE(menu->IsEnabledAt(menu_index++ /* Separator */));
    EXPECT_EQ(!is_all_selected,
              menu->IsEnabledAt(menu_index++ /* SELECT ALL */));
  }

  void PressMouseButton(ui::EventFlags mouse_button_flags) {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, mouse_position_, mouse_position_,
                         ui::EventTimeForNow(), mouse_button_flags,
                         mouse_button_flags);
    textfield_->OnMousePressed(press);
  }

  void ReleaseMouseButton(ui::EventFlags mouse_button_flags) {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, mouse_position_,
                           mouse_position_, ui::EventTimeForNow(),
                           mouse_button_flags, mouse_button_flags);
    textfield_->OnMouseReleased(release);
  }

  void PressLeftMouseButton() {
    PressMouseButton(ui::EF_LEFT_MOUSE_BUTTON);
  }

  void ReleaseLeftMouseButton() {
    ReleaseMouseButton(ui::EF_LEFT_MOUSE_BUTTON);
  }

  void ClickLeftMouseButton() {
    PressLeftMouseButton();
    ReleaseLeftMouseButton();
  }

  void ClickRightMouseButton() {
    PressMouseButton(ui::EF_RIGHT_MOUSE_BUTTON);
    ReleaseMouseButton(ui::EF_RIGHT_MOUSE_BUTTON);
  }

  void DragMouseTo(const gfx::Point& where) {
    mouse_position_ = where;
    ui::MouseEvent drag(ui::ET_MOUSE_DRAGGED, where, where,
                        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    textfield_->OnMouseDragged(drag);
  }

  // Textfield does not listen to OnMouseMoved, so this function does not send
  // an event when it updates the cursor position.
  void MoveMouseTo(const gfx::Point& where) { mouse_position_ = where; }

  // We need widget to populate wrapper class.
  Widget* widget_ = nullptr;

  TestTextfield* textfield_ = nullptr;
  std::unique_ptr<TextfieldTestApi> test_api_;
  TextfieldModel* model_ = nullptr;

  // The string from Controller::ContentsChanged callback.
  base::string16 last_contents_;

  // For testing input method related behaviors.
  MockInputMethod* input_method_ = nullptr;

  // Indicates how many times OnBeforeUserAction() is called.
  int on_before_user_action_ = 0;

  // Indicates how many times OnAfterUserAction() is called.
  int on_after_user_action_ = 0;

  // Position of the mouse for synthetic mouse events.
  gfx::Point mouse_position_;
  ui::ClipboardBuffer copied_to_clipboard_ = ui::ClipboardBuffer::kMaxValue;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextfieldTest);
};

TEST_F(TextfieldTest, ModelChangesTest) {
  InitTextfield();

  // TextfieldController::ContentsChanged() shouldn't be called when changing
  // text programmatically.
  last_contents_.clear();
  textfield_->SetText(ASCIIToUTF16("this is"));

  EXPECT_STR_EQ("this is", model_->text());
  EXPECT_STR_EQ("this is", textfield_->GetText());
  EXPECT_TRUE(last_contents_.empty());

  textfield_->AppendText(ASCIIToUTF16(" a test"));
  EXPECT_STR_EQ("this is a test", model_->text());
  EXPECT_STR_EQ("this is a test", textfield_->GetText());
  EXPECT_TRUE(last_contents_.empty());

  EXPECT_EQ(base::string16(), textfield_->GetSelectedText());
  textfield_->SelectAll(false);
  EXPECT_STR_EQ("this is a test", textfield_->GetSelectedText());
  EXPECT_TRUE(last_contents_.empty());
}

TEST_F(TextfieldTest, KeyTest) {
  InitTextfield();
  // Event flags:  key,    alt,   shift, ctrl,  caps-lock.
  SendKeyEvent(ui::VKEY_T, false, true,  false, false);
  SendKeyEvent(ui::VKEY_E, false, false, false, false);
  SendKeyEvent(ui::VKEY_X, false, true,  false, true);
  SendKeyEvent(ui::VKEY_T, false, false, false, true);
  SendKeyEvent(ui::VKEY_1, false, true,  false, false);
  SendKeyEvent(ui::VKEY_1, false, false, false, false);
  SendKeyEvent(ui::VKEY_1, false, true,  false, true);
  SendKeyEvent(ui::VKEY_1, false, false, false, true);

  // On Mac, Caps+Shift remains uppercase.
  if (TestingNativeMac())
    EXPECT_STR_EQ("TeXT!1!1", textfield_->GetText());
  else
    EXPECT_STR_EQ("TexT!1!1", textfield_->GetText());
}

#if defined(OS_LINUX)
// Control key shouldn't generate a printable character on Linux.
TEST_F(TextfieldTest, KeyTestControlModifier) {
  InitTextfield();
  // 0x0448 is for 'CYRILLIC SMALL LETTER SHA'.
  SendKeyEvent(0x0448, 0);
  SendKeyEvent(0x0448, ui::EF_CONTROL_DOWN);
  // 0x044C is for 'CYRILLIC SMALL LETTER FRONT YER'.
  SendKeyEvent(0x044C, 0);
  SendKeyEvent(0x044C, ui::EF_CONTROL_DOWN);
  SendKeyEvent('i', 0);
  SendKeyEvent('i', ui::EF_CONTROL_DOWN);
  SendKeyEvent('m', 0);
  SendKeyEvent('m', ui::EF_CONTROL_DOWN);

  EXPECT_EQ(WideToUTF16(L"\x0448\x044C"
                        L"im"),
            textfield_->GetText());
}
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_KeysWithModifiersTest KeysWithModifiersTest
#else
// TODO(crbug.com/645104): Implement keyboard layout changing for other
//                         platforms.
#define MAYBE_KeysWithModifiersTest DISABLED_KeysWithModifiersTest
#endif

TEST_F(TextfieldTest, MAYBE_KeysWithModifiersTest) {
  // Activate U.S. English keyboard layout. Modifier keys in other layouts may
  // change the text inserted into a texfield and cause this test to fail.
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);

  InitTextfield();
  const int ctrl = ui::EF_CONTROL_DOWN;
  const int alt = ui::EF_ALT_DOWN;
  const int command = ui::EF_COMMAND_DOWN;
  const int altgr = ui::EF_ALTGR_DOWN;
  const int shift = ui::EF_SHIFT_DOWN;

  SendKeyPress(ui::VKEY_T, shift | alt | altgr);
  SendKeyPress(ui::VKEY_H, alt);
  SendKeyPress(ui::VKEY_E, altgr);
  SendKeyPress(ui::VKEY_T, shift);
  SendKeyPress(ui::VKEY_E, shift | altgr);
  SendKeyPress(ui::VKEY_X, 0);
  SendKeyPress(ui::VKEY_T, ctrl);  // This causes transpose on Mac.
  SendKeyPress(ui::VKEY_1, alt);
  SendKeyPress(ui::VKEY_2, command);
  SendKeyPress(ui::VKEY_3, 0);
  SendKeyPress(ui::VKEY_4, 0);
  SendKeyPress(ui::VKEY_OEM_PLUS, ctrl);
  SendKeyPress(ui::VKEY_OEM_PLUS, ctrl | shift);
  SendKeyPress(ui::VKEY_OEM_MINUS, ctrl);
  SendKeyPress(ui::VKEY_OEM_MINUS, ctrl | shift);

  if (TestingNativeCrOs())
    EXPECT_STR_EQ("TeTEx34", textfield_->GetText());
  else if (TestingNativeMac())
    EXPECT_STR_EQ("TheTxE134", textfield_->GetText());
  else
    EXPECT_STR_EQ("TeTEx234", textfield_->GetText());
}

TEST_F(TextfieldTest, ControlAndSelectTest) {
  // Insert a test string in a textfield.
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("one two three"));
  SendHomeEvent(false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);

  EXPECT_STR_EQ("one", textfield_->GetSelectedText());

  // Test word select.
  SendWordEvent(ui::VKEY_RIGHT, true);
#if defined(OS_WIN)  // Windows breaks on word starts and includes spaces.
  EXPECT_STR_EQ("one ", textfield_->GetSelectedText());
  SendWordEvent(ui::VKEY_RIGHT, true);
  EXPECT_STR_EQ("one two ", textfield_->GetSelectedText());
#else  // Non-Windows breaks on word ends and does NOT include spaces.
  EXPECT_STR_EQ("one two", textfield_->GetSelectedText());
#endif
  SendWordEvent(ui::VKEY_RIGHT, true);
  EXPECT_STR_EQ("one two three", textfield_->GetSelectedText());
  SendWordEvent(ui::VKEY_LEFT, true);
  EXPECT_STR_EQ("one two ", textfield_->GetSelectedText());
  SendWordEvent(ui::VKEY_LEFT, true);
  EXPECT_STR_EQ("one ", textfield_->GetSelectedText());

  // Replace the selected text.
  SendKeyEvent(ui::VKEY_Z, true, false);
  SendKeyEvent(ui::VKEY_E, true, false);
  SendKeyEvent(ui::VKEY_R, true, false);
  SendKeyEvent(ui::VKEY_O, true, false);
  SendKeyEvent(ui::VKEY_SPACE, false, false);
  EXPECT_STR_EQ("ZERO two three", textfield_->GetText());

  SendEndEvent(true);
  EXPECT_STR_EQ("two three", textfield_->GetSelectedText());
  SendHomeEvent(true);

// On Mac, the existing selection should be extended.
#if defined(OS_MACOSX)
  EXPECT_STR_EQ("ZERO two three", textfield_->GetSelectedText());
#else
  EXPECT_STR_EQ("ZERO ", textfield_->GetSelectedText());
#endif
}

TEST_F(TextfieldTest, WordSelection) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("12 34567 89"));

  // Place the cursor after "5".
  textfield_->SetEditableSelectionRange(gfx::Range(6));

  // Select word towards right.
  SendWordEvent(ui::VKEY_RIGHT, true);
#if defined(OS_WIN)  // Select word right includes space/punctuation.
  EXPECT_STR_EQ("67 ", textfield_->GetSelectedText());
#else  // Non-Win: select word right does NOT include space/punctuation.
  EXPECT_STR_EQ("67", textfield_->GetSelectedText());
#endif
  SendWordEvent(ui::VKEY_RIGHT, true);
  EXPECT_STR_EQ("67 89", textfield_->GetSelectedText());

  // Select word towards left.
  SendWordEvent(ui::VKEY_LEFT, true);
  EXPECT_STR_EQ("67 ", textfield_->GetSelectedText());
  SendWordEvent(ui::VKEY_LEFT, true);

// On Mac, the selection should reduce to a caret when the selection direction
// changes for a word selection.
#if defined(OS_MACOSX)
  EXPECT_EQ(gfx::Range(6), textfield_->GetSelectedRange());
#else
  EXPECT_STR_EQ("345", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(6, 3), textfield_->GetSelectedRange());
#endif

  SendWordEvent(ui::VKEY_LEFT, true);
#if defined(OS_MACOSX)
  EXPECT_STR_EQ("345", textfield_->GetSelectedText());
#else
  EXPECT_STR_EQ("12 345", textfield_->GetSelectedText());
#endif
  EXPECT_TRUE(textfield_->GetSelectedRange().is_reversed());

  SendWordEvent(ui::VKEY_LEFT, true);
  EXPECT_STR_EQ("12 345", textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, LineSelection) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("12 34567 89"));

  // Place the cursor after "5".
  textfield_->SetEditableSelectionRange(gfx::Range(6));

  // Select line towards right.
  SendEndEvent(true);
  EXPECT_STR_EQ("67 89", textfield_->GetSelectedText());

  // Select line towards left. On Mac, the existing selection should be extended
  // to cover the whole line.
  SendHomeEvent(true);
#if defined(OS_MACOSX)
  EXPECT_EQ(textfield_->GetText(), textfield_->GetSelectedText());
#else
  EXPECT_STR_EQ("12 345", textfield_->GetSelectedText());
#endif
  EXPECT_TRUE(textfield_->GetSelectedRange().is_reversed());

  // Select line towards right.
  SendEndEvent(true);
#if defined(OS_MACOSX)
  EXPECT_EQ(textfield_->GetText(), textfield_->GetSelectedText());
#else
  EXPECT_STR_EQ("67 89", textfield_->GetSelectedText());
#endif
  EXPECT_FALSE(textfield_->GetSelectedRange().is_reversed());
}

TEST_F(TextfieldTest, MoveUpDownAndModifySelection) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("12 34567 89"));
  textfield_->SetEditableSelectionRange(gfx::Range(6));

  // Up/Down keys won't be handled except on Mac where they map to move
  // commands.
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_TRUE(textfield_->key_received());
#if defined(OS_MACOSX)
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_EQ(gfx::Range(0), textfield_->GetSelectedRange());
#else
  EXPECT_FALSE(textfield_->key_handled());
#endif
  textfield_->clear();

  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(textfield_->key_received());
#if defined(OS_MACOSX)
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_EQ(gfx::Range(11), textfield_->GetSelectedRange());
#else
  EXPECT_FALSE(textfield_->key_handled());
#endif
  textfield_->clear();

  textfield_->SetEditableSelectionRange(gfx::Range(6));

  // Shift+[Up/Down] should select the text to the beginning and end of the
  // line, respectively.
  SendKeyEvent(ui::VKEY_UP, true /* shift */, false /* command */);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_EQ(gfx::Range(6, 0), textfield_->GetSelectedRange());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_DOWN, true /* shift */, false /* command */);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_EQ(gfx::Range(6, 11), textfield_->GetSelectedRange());
  textfield_->clear();
}

TEST_F(TextfieldTest, MovePageUpDownAndModifySelection) {
  InitTextfield();

// MOVE_PAGE_[UP/DOWN] and the associated selection commands should only be
// enabled on Mac.
#if defined(OS_MACOSX)
  textfield_->SetText(ASCIIToUTF16("12 34567 89"));
  textfield_->SetEditableSelectionRange(gfx::Range(6));

  EXPECT_TRUE(
      textfield_->IsTextEditCommandEnabled(ui::TextEditCommand::MOVE_PAGE_UP));
  EXPECT_TRUE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::MOVE_PAGE_DOWN));
  EXPECT_TRUE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION));
  EXPECT_TRUE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION));

  test_api_->ExecuteTextEditCommand(ui::TextEditCommand::MOVE_PAGE_UP);
  EXPECT_EQ(gfx::Range(0), textfield_->GetSelectedRange());

  test_api_->ExecuteTextEditCommand(ui::TextEditCommand::MOVE_PAGE_DOWN);
  EXPECT_EQ(gfx::Range(11), textfield_->GetSelectedRange());

  textfield_->SetEditableSelectionRange(gfx::Range(6));
  test_api_->ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION);
  EXPECT_EQ(gfx::Range(6, 0), textfield_->GetSelectedRange());

  test_api_->ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION);
  EXPECT_EQ(gfx::Range(6, 11), textfield_->GetSelectedRange());
#else
  EXPECT_FALSE(
      textfield_->IsTextEditCommandEnabled(ui::TextEditCommand::MOVE_PAGE_UP));
  EXPECT_FALSE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::MOVE_PAGE_DOWN));
  EXPECT_FALSE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION));
  EXPECT_FALSE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION));
#endif
}

TEST_F(TextfieldTest, MoveParagraphForwardBackwardAndModifySelection) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("12 34567 89"));
  textfield_->SetEditableSelectionRange(gfx::Range(6));

  test_api_->ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PARAGRAPH_FORWARD_AND_MODIFY_SELECTION);
  EXPECT_EQ(gfx::Range(6, 11), textfield_->GetSelectedRange());

  test_api_->ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PARAGRAPH_BACKWARD_AND_MODIFY_SELECTION);
// On Mac, the selection should reduce to a caret when the selection direction
// is reversed for MOVE_PARAGRAPH_[FORWARD/BACKWARD]_AND_MODIFY_SELECTION.
#if defined(OS_MACOSX)
  EXPECT_EQ(gfx::Range(6), textfield_->GetSelectedRange());
#else
  EXPECT_EQ(gfx::Range(6, 0), textfield_->GetSelectedRange());
#endif

  test_api_->ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PARAGRAPH_BACKWARD_AND_MODIFY_SELECTION);
  EXPECT_EQ(gfx::Range(6, 0), textfield_->GetSelectedRange());

  test_api_->ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PARAGRAPH_FORWARD_AND_MODIFY_SELECTION);
#if defined(OS_MACOSX)
  EXPECT_EQ(gfx::Range(6), textfield_->GetSelectedRange());
#else
  EXPECT_EQ(gfx::Range(6, 11), textfield_->GetSelectedRange());
#endif
}

TEST_F(TextfieldTest, InsertionDeletionTest) {
  // Insert a test string in a textfield.
  InitTextfield();
  for (size_t i = 0; i < 10; i++)
    SendKeyEvent(static_cast<ui::KeyboardCode>(ui::VKEY_A + i));
  EXPECT_STR_EQ("abcdefghij", textfield_->GetText());

  // Test the delete and backspace keys.
  textfield_->SetSelectedRange(gfx::Range(5));
  for (size_t i = 0; i < 3; i++)
    SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("abfghij", textfield_->GetText());
  for (size_t i = 0; i < 3; i++)
    SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("abij", textfield_->GetText());

  // Select all and replace with "k".
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_K);
  EXPECT_STR_EQ("k", textfield_->GetText());

  // Delete the previous word from cursor.
  bool shift = false;
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  SendEndEvent(shift);
  SendWordEvent(ui::VKEY_BACK, shift);
  EXPECT_STR_EQ("one two three ", textfield_->GetText());

  // Delete to a line break on Linux and ChromeOS, to a word break on Windows
  // and Mac.
  SendWordEvent(ui::VKEY_LEFT, shift);
  shift = true;
  SendWordEvent(ui::VKEY_BACK, shift);
#if defined(OS_LINUX)
  EXPECT_STR_EQ("three ", textfield_->GetText());
#else
  EXPECT_STR_EQ("one three ", textfield_->GetText());
#endif

  // Delete the next word from cursor.
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  shift = false;
  SendHomeEvent(shift);
  SendWordEvent(ui::VKEY_DELETE, shift);
#if defined(OS_WIN)  // Delete word incldes space/punctuation.
  EXPECT_STR_EQ("two three four", textfield_->GetText());
#else  // Non-Windows: delete word does NOT include space/punctuation.
  EXPECT_STR_EQ(" two three four", textfield_->GetText());
#endif
  // Delete to a line break on Linux and ChromeOS, to a word break on Windows
  // and Mac.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  shift = true;
  SendWordEvent(ui::VKEY_DELETE, shift);
#if defined(OS_LINUX)
  EXPECT_STR_EQ(" two", textfield_->GetText());
#elif defined(OS_WIN)
  EXPECT_STR_EQ("two four", textfield_->GetText());
#else
  EXPECT_STR_EQ(" two four", textfield_->GetText());
#endif
}

// Test that deletion operations behave correctly with an active selection.
TEST_F(TextfieldTest, DeletionWithSelection) {
  struct {
    ui::KeyboardCode key;
    bool shift;
  } cases[] = {
      {ui::VKEY_BACK, false},
      {ui::VKEY_BACK, true},
      {ui::VKEY_DELETE, false},
      {ui::VKEY_DELETE, true},
  };

  InitTextfield();
  // [Ctrl] ([Alt] on Mac) + [Delete]/[Backspace] should delete the active
  // selection, regardless of [Shift].
  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    textfield_->SetText(ASCIIToUTF16("one two three"));
    textfield_->SetSelectedRange(gfx::Range(2, 6));
    // Make selection as - on|e tw|o three.
    SendWordEvent(cases[i].key, cases[i].shift);
    // Verify state is on|o three.
    EXPECT_STR_EQ("ono three", textfield_->GetText());
    EXPECT_EQ(gfx::Range(2), textfield_->GetSelectedRange());
  }
}

// Test deletions not covered by other tests with key events.
TEST_F(TextfieldTest, DeletionWithEditCommands) {
  struct {
    ui::TextEditCommand command;
    const char* expected;
  } cases[] = {
      {ui::TextEditCommand::DELETE_TO_BEGINNING_OF_LINE, "two three"},
      {ui::TextEditCommand::DELETE_TO_BEGINNING_OF_PARAGRAPH, "two three"},
      {ui::TextEditCommand::DELETE_TO_END_OF_LINE, "one "},
      {ui::TextEditCommand::DELETE_TO_END_OF_PARAGRAPH, "one "},
  };

  InitTextfield();
  for (size_t i = 0; i < base::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    textfield_->SetText(ASCIIToUTF16("one two three"));
    textfield_->SetSelectedRange(gfx::Range(4));
    test_api_->ExecuteTextEditCommand(cases[i].command);
    EXPECT_STR_EQ(cases[i].expected, textfield_->GetText());
  }
}

TEST_F(TextfieldTest, PasswordTest) {
  InitTextfield();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());
  EXPECT_TRUE(textfield_->GetEnabled());
  EXPECT_TRUE(textfield_->IsFocusable());

  last_contents_.clear();
  textfield_->SetText(ASCIIToUTF16("password"));
  // Ensure GetText() and the callback returns the actual text instead of "*".
  EXPECT_STR_EQ("password", textfield_->GetText());
  EXPECT_TRUE(last_contents_.empty());
  model_->SelectAll(false);
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "foo");

  // Cut and copy should be disabled.
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_->ExecuteCommand(IDS_APP_CUT, 0);
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_->ExecuteCommand(IDS_APP_COPY, 0);
  SendKeyEvent(ui::VKEY_C, false, true);
  SendAlternateCopy();
  EXPECT_STR_EQ("foo", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_STR_EQ("password", textfield_->GetText());
  // [Shift]+[Delete] should just delete without copying text to the clipboard.
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_DELETE, true, false);

  // Paste should work normally.
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_->ExecuteCommand(IDS_APP_PASTE, 0);
  SendKeyEvent(ui::VKEY_V, false, true);
  SendAlternatePaste();
  EXPECT_STR_EQ("foo", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_STR_EQ("foofoofoo", textfield_->GetText());
}

// Check that text insertion works appropriately for password and read-only
// textfields.
TEST_F(TextfieldTest, TextInputType_InsertionTest) {
  InitTextfield();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());

  SendKeyEvent(ui::VKEY_A);
  EXPECT_EQ(-1, textfield_->GetPasswordCharRevealIndex());
  SendKeyEvent(kHebrewLetterSamekh, ui::EF_NONE, true /* from_vk */);
#if !defined(OS_MACOSX)
  // Don't verifies the password character reveal on MacOS, because on MacOS,
  // the text insertion is not done through TextInputClient::InsertChar().
  EXPECT_EQ(1, textfield_->GetPasswordCharRevealIndex());
#endif
  SendKeyEvent(ui::VKEY_B);
  EXPECT_EQ(-1, textfield_->GetPasswordCharRevealIndex());

  EXPECT_EQ(WideToUTF16(L"a\x05E1"
                        L"b"),
            textfield_->GetText());

  textfield_->SetReadOnly(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, textfield_->GetTextInputType());
  SendKeyEvent(ui::VKEY_C);

  // No text should be inserted for read only textfields.
  EXPECT_EQ(WideToUTF16(L"a\x05E1"
                        L"b"),
            textfield_->GetText());
}

TEST_F(TextfieldTest, TextInputType) {
  InitTextfield();

  // Defaults to TEXT
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, textfield_->GetTextInputType());

  // And can be set.
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_URL, textfield_->GetTextInputType());
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());

  // Readonly textfields have type NONE
  textfield_->SetReadOnly(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, textfield_->GetTextInputType());

  textfield_->SetReadOnly(false);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());

  // As do disabled textfields
  textfield_->SetEnabled(false);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, textfield_->GetTextInputType());

  textfield_->SetEnabled(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());
}

TEST_F(TextfieldTest, OnKeyPress) {
  InitTextfield();

  // Character keys are handled by the input method.
  SendKeyEvent(ui::VKEY_A);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  // Arrow keys and home/end are handled by the textfield.
  SendKeyEvent(ui::VKEY_LEFT);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  const bool shift = false;
  SendHomeEvent(shift);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  SendEndEvent(shift);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  // F20 key won't be handled.
  SendKeyEvent(ui::VKEY_F20);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();
}

// Tests that default key bindings are handled even with a delegate installed.
TEST_F(TextfieldTest, OnKeyPressBinding) {
  InitTextfield();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Install a TextEditKeyBindingsDelegateAuraLinux that does nothing.
  class TestDelegate : public ui::TextEditKeyBindingsDelegateAuraLinux {
   public:
    TestDelegate() = default;
    ~TestDelegate() override = default;

    bool MatchEvent(
        const ui::Event& event,
        std::vector<ui::TextEditCommandAuraLinux>* commands) override {
      return false;
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(TestDelegate);
  };

  TestDelegate delegate;
  ui::SetTextEditKeyBindingsDelegate(&delegate);
#endif

  SendKeyEvent(ui::VKEY_A, false, false);
  EXPECT_STR_EQ("a", textfield_->GetText());
  textfield_->clear();

  // Undo/Redo command keys are handled by the textfield.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_TRUE(textfield_->GetText().empty());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_STR_EQ("a", textfield_->GetText());
  textfield_->clear();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  ui::SetTextEditKeyBindingsDelegate(nullptr);
#endif
}

TEST_F(TextfieldTest, CursorMovement) {
  InitTextfield();

  // Test with trailing whitespace.
  textfield_->SetText(ASCIIToUTF16("one two hre "));

  // Send the cursor at the end.
  SendKeyEvent(ui::VKEY_END);

  // Ctrl+Left should move the cursor just before the last word.
  const bool shift = false;
  SendWordEvent(ui::VKEY_LEFT, shift);
  SendKeyEvent(ui::VKEY_T);
  EXPECT_STR_EQ("one two thre ", textfield_->GetText());
  EXPECT_STR_EQ("one two thre ", last_contents_);

#if defined(OS_WIN)  // Move right by word includes space/punctuation.
  // Ctrl+Right should move the cursor to the end of the last word.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  SendKeyEvent(ui::VKEY_E);
  EXPECT_STR_EQ("one two thre e", textfield_->GetText());
  EXPECT_STR_EQ("one two thre e", last_contents_);

  // Ctrl+Right again should not move the cursor, because
  // it is aleady at the end.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("one two thre ", textfield_->GetText());
  EXPECT_STR_EQ("one two thre ", last_contents_);
#else  // Non-Windows: move right by word does NOT include space/punctuation.
  // Ctrl+Right should move the cursor to the end of the last word.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  SendKeyEvent(ui::VKEY_E);
  EXPECT_STR_EQ("one two three ", textfield_->GetText());
  EXPECT_STR_EQ("one two three ", last_contents_);

  // Ctrl+Right again should move the cursor to the end.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("one two three", textfield_->GetText());
  EXPECT_STR_EQ("one two three", last_contents_);
#endif
  // Test with leading whitespace.
  textfield_->SetText(ASCIIToUTF16(" ne two"));

  // Send the cursor at the beginning.
  SendHomeEvent(shift);

  // Ctrl+Right, then Ctrl+Left should move the cursor to the beginning of the
  // first word.
  SendWordEvent(ui::VKEY_RIGHT, shift);
#if defined(OS_WIN)  // Windows breaks on word start, move further to pass "ne".
  SendWordEvent(ui::VKEY_RIGHT, shift);
#endif
  SendWordEvent(ui::VKEY_LEFT, shift);
  SendKeyEvent(ui::VKEY_O);
  EXPECT_STR_EQ(" one two", textfield_->GetText());
  EXPECT_STR_EQ(" one two", last_contents_);

  // Ctrl+Left to move the cursor to the beginning of the first word.
  SendWordEvent(ui::VKEY_LEFT, shift);
  // Ctrl+Left again should move the cursor back to the very beginning.
  SendWordEvent(ui::VKEY_LEFT, shift);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("one two", textfield_->GetText());
  EXPECT_STR_EQ("one two", last_contents_);
}

TEST_F(TextfieldTest, FocusTraversalTest) {
  InitTextfields(3);
  textfield_->RequestFocus();

  EXPECT_EQ(1, GetFocusedView()->GetID());
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(2, GetFocusedView()->GetID());
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(3, GetFocusedView()->GetID());
  // Cycle back to the first textfield.
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(1, GetFocusedView()->GetID());

  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->GetID());
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(2, GetFocusedView()->GetID());
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(1, GetFocusedView()->GetID());
  // Cycle back to the last textfield.
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->GetID());

  // Request focus should still work.
  textfield_->RequestFocus();
  EXPECT_EQ(1, GetFocusedView()->GetID());

  // Test if clicking on textfield view sets the focus.
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->GetID());
  MoveMouseTo(gfx::Point(0, GetCursorYForTesting()));
  ClickLeftMouseButton();
  EXPECT_EQ(1, GetFocusedView()->GetID());

  // Tab/Shift+Tab should also cycle focus, not insert a tab character.
  SendKeyEvent(ui::VKEY_TAB, false, false);
  EXPECT_EQ(2, GetFocusedView()->GetID());
  SendKeyEvent(ui::VKEY_TAB, false, false);
  EXPECT_EQ(3, GetFocusedView()->GetID());
  // Cycle back to the first textfield.
  SendKeyEvent(ui::VKEY_TAB, false, false);
  EXPECT_EQ(1, GetFocusedView()->GetID());

  SendKeyEvent(ui::VKEY_TAB, true, false);
  EXPECT_EQ(3, GetFocusedView()->GetID());
  SendKeyEvent(ui::VKEY_TAB, true, false);
  EXPECT_EQ(2, GetFocusedView()->GetID());
  SendKeyEvent(ui::VKEY_TAB, true, false);
  EXPECT_EQ(1, GetFocusedView()->GetID());
  // Cycle back to the last textfield.
  SendKeyEvent(ui::VKEY_TAB, true, false);
  EXPECT_EQ(3, GetFocusedView()->GetID());
}

TEST_F(TextfieldTest, ContextMenuDisplayTest) {
  InitTextfield();

  EXPECT_TRUE(textfield_->context_menu_controller());
  textfield_->SetText(ASCIIToUTF16("hello world"));
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  textfield_->ClearEditHistory();
  EXPECT_TRUE(GetContextMenuModel());
  VerifyTextfieldContextMenuContents(false, false, GetContextMenuModel());

  textfield_->SelectAll(false);
  VerifyTextfieldContextMenuContents(true, false, GetContextMenuModel());

  SendKeyEvent(ui::VKEY_T);
  VerifyTextfieldContextMenuContents(false, true, GetContextMenuModel());

  textfield_->SelectAll(false);
  VerifyTextfieldContextMenuContents(true, true, GetContextMenuModel());

  // Exercise the "paste enabled?" check in the verifier.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "Test");
  VerifyTextfieldContextMenuContents(true, true, GetContextMenuModel());
}

TEST_F(TextfieldTest, DoubleAndTripleClickTest) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  // Test for double click.
  MoveMouseTo(gfx::Point(0, GetCursorYForTesting()));
  ClickLeftMouseButton();
  EXPECT_TRUE(textfield_->GetSelectedText().empty());
  ClickLeftMouseButton();
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());

  // Test for triple click.
  ClickLeftMouseButton();
  EXPECT_STR_EQ("hello world", textfield_->GetSelectedText());

  // Another click should reset back to double click.
  ClickLeftMouseButton();
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
}

// Tests text selection behavior on a right click.
TEST_F(TextfieldTest, SelectionOnRightClick) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  // Verify right clicking within the selection does not alter the selection.
  textfield_->SetSelectedRange(gfx::Range(1, 5));
  EXPECT_STR_EQ("ello", textfield_->GetSelectedText());
  const int cursor_y = GetCursorYForTesting();
  MoveMouseTo(gfx::Point(GetCursorPositionX(3), cursor_y));
  ClickRightMouseButton();
  EXPECT_STR_EQ("ello", textfield_->GetSelectedText());

  // Verify right clicking outside the selection, selects the word under the
  // cursor on platforms where this is expected.
  MoveMouseTo(gfx::Point(GetCursorPositionX(8), cursor_y));
  const char* expected_right_click_word =
      PlatformStyle::kSelectWordOnRightClick ? "world" : "ello";
  ClickRightMouseButton();
  EXPECT_STR_EQ(expected_right_click_word, textfield_->GetSelectedText());

  // Verify right clicking inside an unfocused textfield selects all the text on
  // platforms where this is expected. Else the older selection is retained.
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(textfield_->HasFocus());
  MoveMouseTo(gfx::Point(GetCursorPositionX(0), cursor_y));
  ClickRightMouseButton();
  EXPECT_TRUE(textfield_->HasFocus());
  const char* expected_right_click_unfocused =
      PlatformStyle::kSelectAllOnRightClickWhenUnfocused
          ? "hello world"
          : expected_right_click_word;
  EXPECT_STR_EQ(expected_right_click_unfocused, textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, DragToSelect) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  const int kStart = GetCursorPositionX(5);
  const int kEnd = 500;
  const int cursor_y = GetCursorYForTesting();
  gfx::Point start_point(kStart, cursor_y);
  gfx::Point end_point(kEnd, cursor_y);

  MoveMouseTo(start_point);
  PressLeftMouseButton();
  EXPECT_TRUE(textfield_->GetSelectedText().empty());

  // Check that dragging left selects the beginning of the string.
  DragMouseTo(gfx::Point(0, cursor_y));
  base::string16 text_left = textfield_->GetSelectedText();
  EXPECT_STR_EQ("hello", text_left);

  // Check that dragging right selects the rest of the string.
  DragMouseTo(end_point);
  base::string16 text_right = textfield_->GetSelectedText();
  EXPECT_STR_EQ(" world", text_right);

  // Check that releasing in the same location does not alter the selection.
  ReleaseLeftMouseButton();
  EXPECT_EQ(text_right, textfield_->GetSelectedText());

  // Check that dragging from beyond the text length works too.
  MoveMouseTo(end_point);
  PressLeftMouseButton();
  DragMouseTo(gfx::Point(0, cursor_y));
  ReleaseLeftMouseButton();
  EXPECT_EQ(textfield_->GetText(), textfield_->GetSelectedText());
}

// Ensures dragging above or below the textfield extends a selection to either
// end, depending on the relative x offsets of the text and mouse cursors.
TEST_F(TextfieldTest, DragUpOrDownSelectsToEnd) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  const base::string16 expected_left = base::ASCIIToUTF16(
      gfx::RenderText::kDragToEndIfOutsideVerticalBounds ? "hello" : "lo");
  const base::string16 expected_right = base::ASCIIToUTF16(
      gfx::RenderText::kDragToEndIfOutsideVerticalBounds ? " world" : " w");
  const int right_x = GetCursorPositionX(7);
  const int left_x = GetCursorPositionX(3);

  // All drags start from here.
  MoveMouseTo(gfx::Point(GetCursorPositionX(5), GetCursorYForTesting()));
  PressLeftMouseButton();

  // Perform one continuous drag, checking the selection at various points.
  DragMouseTo(gfx::Point(left_x, -500));
  EXPECT_EQ(expected_left, textfield_->GetSelectedText());  // NW.
  DragMouseTo(gfx::Point(right_x, -500));
  EXPECT_EQ(expected_right, textfield_->GetSelectedText());  // NE.
  DragMouseTo(gfx::Point(right_x, 500));
  EXPECT_EQ(expected_right, textfield_->GetSelectedText());  // SE.
  DragMouseTo(gfx::Point(left_x, 500));
  EXPECT_EQ(expected_left, textfield_->GetSelectedText());  // SW.
}

#if defined(OS_WIN)
TEST_F(TextfieldTest, DragAndDrop_AcceptDrop) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));

  ui::OSExchangeData data;
  base::string16 string(ASCIIToUTF16("string "));
  data.SetString(string);
  int formats = 0;
  std::set<ui::ClipboardFormatType> format_types;

  // Ensure that disabled textfields do not accept drops.
  textfield_->SetEnabled(false);
  EXPECT_FALSE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(0, formats);
  EXPECT_TRUE(format_types.empty());
  EXPECT_FALSE(textfield_->CanDrop(data));
  textfield_->SetEnabled(true);

  // Ensure that read-only textfields do not accept drops.
  textfield_->SetReadOnly(true);
  EXPECT_FALSE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(0, formats);
  EXPECT_TRUE(format_types.empty());
  EXPECT_FALSE(textfield_->CanDrop(data));
  textfield_->SetReadOnly(false);

  // Ensure that enabled and editable textfields do accept drops.
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(format_types.empty());
  EXPECT_TRUE(textfield_->CanDrop(data));
  gfx::PointF drop_point(GetCursorPositionX(6), 0);
  ui::DropTargetEvent drop(data, drop_point, drop_point,
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE,
            textfield_->OnDragUpdated(drop));
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, textfield_->OnPerformDrop(drop));
  EXPECT_STR_EQ("hello string world", textfield_->GetText());

  // Ensure that textfields do not accept non-OSExchangeData::STRING types.
  ui::OSExchangeData bad_data;
  bad_data.SetFilename(base::FilePath(FILE_PATH_LITERAL("x")));
  ui::ClipboardFormatType fmt = ui::ClipboardFormatType::GetBitmapType();
  bad_data.SetPickledData(fmt, base::Pickle());
  bad_data.SetFileContents(base::FilePath(L"x"), "x");
  bad_data.SetHtml(base::string16(ASCIIToUTF16("x")), GURL("x.org"));
  ui::OSExchangeData::DownloadFileInfo download(base::FilePath(), nullptr);
  bad_data.SetDownloadFileInfo(&download);
  EXPECT_FALSE(textfield_->CanDrop(bad_data));
}
#endif

TEST_F(TextfieldTest, DragAndDrop_InitiateDrag) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello string world"));

  // Ensure the textfield will provide selected text for drag data.
  base::string16 string;
  ui::OSExchangeData data;
  const gfx::Range kStringRange(6, 12);
  textfield_->SetSelectedRange(kStringRange);
  const gfx::Point kStringPoint(GetCursorPositionX(9), GetCursorYForTesting());
  textfield_->WriteDragDataForView(nullptr, kStringPoint, &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);

  // Ensure that disabled textfields do not support drag operations.
  textfield_->SetEnabled(false);
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(nullptr, kStringPoint));
  textfield_->SetEnabled(true);
  // Ensure that textfields without selections do not support drag operations.
  textfield_->ClearSelection();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(nullptr, kStringPoint));
  textfield_->SetSelectedRange(kStringRange);
  // Ensure that password textfields do not support drag operations.
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(nullptr, kStringPoint));
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  MoveMouseTo(kStringPoint);
  PressLeftMouseButton();
  // Ensure that textfields only initiate drag operations inside the selection.
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_->GetDragOperationsForView(nullptr, gfx::Point()));
  EXPECT_FALSE(
      textfield_->CanStartDragForView(nullptr, gfx::Point(), gfx::Point()));
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY,
            textfield_->GetDragOperationsForView(nullptr, kStringPoint));
  EXPECT_TRUE(
      textfield_->CanStartDragForView(nullptr, kStringPoint, gfx::Point()));
  // Ensure that textfields support local moves.
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
      textfield_->GetDragOperationsForView(textfield_, kStringPoint));
}

TEST_F(TextfieldTest, DragAndDrop_ToTheRight) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  const int cursor_y = GetCursorYForTesting();

  base::string16 string;
  ui::OSExchangeData data;
  int formats = 0;
  int operations = 0;
  std::set<ui::ClipboardFormatType> format_types;

  // Start dragging "ello".
  textfield_->SetSelectedRange(gfx::Range(1, 5));
  gfx::Point point(GetCursorPositionX(3), cursor_y);
  MoveMouseTo(point);
  PressLeftMouseButton();
  EXPECT_TRUE(textfield_->CanStartDragForView(textfield_, point, point));
  operations = textfield_->GetDragOperationsForView(textfield_, point);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
            operations);
  textfield_->WriteDragDataForView(nullptr, point, &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(format_types.empty());

  // Drop "ello" after "w".
  const gfx::PointF kDropPoint(GetCursorPositionX(7), cursor_y);
  EXPECT_TRUE(textfield_->CanDrop(data));
  ui::DropTargetEvent drop_a(data, kDropPoint, kDropPoint, operations);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop_a));
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnPerformDrop(drop_a));
  EXPECT_STR_EQ("h welloorld", textfield_->GetText());
  textfield_->OnDragDone();

  // Undo/Redo the drag&drop change.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("hello world", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("hello world", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("h welloorld", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("h welloorld", textfield_->GetText());
}

TEST_F(TextfieldTest, DragAndDrop_ToTheLeft) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  const int cursor_y = GetCursorYForTesting();

  base::string16 string;
  ui::OSExchangeData data;
  int formats = 0;
  int operations = 0;
  std::set<ui::ClipboardFormatType> format_types;

  // Start dragging " worl".
  textfield_->SetSelectedRange(gfx::Range(5, 10));
  gfx::Point point(GetCursorPositionX(7), cursor_y);
  MoveMouseTo(point);
  PressLeftMouseButton();
  EXPECT_TRUE(textfield_->CanStartDragForView(textfield_, point, gfx::Point()));
  operations = textfield_->GetDragOperationsForView(textfield_, point);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
            operations);
  textfield_->WriteDragDataForView(nullptr, point, &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(format_types.empty());

  // Drop " worl" after "h".
  EXPECT_TRUE(textfield_->CanDrop(data));
  gfx::PointF drop_point(GetCursorPositionX(1), cursor_y);
  ui::DropTargetEvent drop_a(data, drop_point, drop_point, operations);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop_a));
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnPerformDrop(drop_a));
  EXPECT_STR_EQ("h worlellod", textfield_->GetText());
  textfield_->OnDragDone();

  // Undo/Redo the drag&drop change.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("hello world", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("hello world", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("h worlellod", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("h worlellod", textfield_->GetText());
}

TEST_F(TextfieldTest, DragAndDrop_Canceled) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  const int cursor_y = GetCursorYForTesting();

  // Start dragging "worl".
  textfield_->SetSelectedRange(gfx::Range(6, 10));
  gfx::Point point(GetCursorPositionX(8), cursor_y);
  MoveMouseTo(point);
  PressLeftMouseButton();
  ui::OSExchangeData data;
  textfield_->WriteDragDataForView(nullptr, point, &data);
  EXPECT_TRUE(textfield_->CanDrop(data));
  // Drag the text over somewhere valid, outside the current selection.
  gfx::PointF drop_point(GetCursorPositionX(2), cursor_y);
  ui::DropTargetEvent drop(data, drop_point, drop_point,
                           ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop));
  // "Cancel" the drag, via move and release over the selection, and OnDragDone.
  gfx::Point drag_point(GetCursorPositionX(9), cursor_y);
  DragMouseTo(drag_point);
  ReleaseLeftMouseButton();
  EXPECT_EQ(ASCIIToUTF16("hello world"), textfield_->GetText());
}

TEST_F(TextfieldTest, ReadOnlyTest) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("read only"));
  textfield_->SetReadOnly(true);
  EXPECT_TRUE(textfield_->GetEnabled());
  EXPECT_TRUE(textfield_->IsFocusable());

  bool shift = false;
  SendHomeEvent(shift);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());
  SendEndEvent(shift);
  EXPECT_EQ(9U, textfield_->GetCursorPosition());

  SendKeyEvent(ui::VKEY_LEFT, shift, false);
  EXPECT_EQ(8U, textfield_->GetCursorPosition());
  SendWordEvent(ui::VKEY_LEFT, shift);
  EXPECT_EQ(5U, textfield_->GetCursorPosition());
  shift = true;
  SendWordEvent(ui::VKEY_LEFT, shift);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());
  EXPECT_STR_EQ("read ", textfield_->GetSelectedText());
  textfield_->SelectAll(false);
  EXPECT_STR_EQ("read only", textfield_->GetSelectedText());

  // Cut should be disabled.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "Test");
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_->ExecuteCommand(IDS_APP_CUT, 0);
  SendKeyEvent(ui::VKEY_X, false, true);
  SendAlternateCut();
  EXPECT_STR_EQ("Test", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_STR_EQ("read only", textfield_->GetText());

  // Paste should be disabled.
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_->ExecuteCommand(IDS_APP_PASTE, 0);
  SendKeyEvent(ui::VKEY_V, false, true);
  SendAlternatePaste();
  EXPECT_STR_EQ("read only", textfield_->GetText());

  // Copy should work normally.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "Test");
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_->ExecuteCommand(IDS_APP_COPY, 0);
  EXPECT_STR_EQ("read only", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "Test");
  SendKeyEvent(ui::VKEY_C, false, true);
  EXPECT_STR_EQ("read only", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "Test");
  SendAlternateCopy();
  EXPECT_STR_EQ("read only", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));

  // SetText should work even in read only mode.
  textfield_->SetText(ASCIIToUTF16(" four five six "));
  EXPECT_STR_EQ(" four five six ", textfield_->GetText());

  textfield_->SelectAll(false);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());

  // Text field is unmodifiable and selection shouldn't change.
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_T);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, TextInputClientTest) {
  InitTextfield();
  ui::TextInputClient* client = textfield_;
  EXPECT_TRUE(client);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, client->GetTextInputType());

  textfield_->SetText(ASCIIToUTF16("0123456789"));
  gfx::Range range;
  EXPECT_TRUE(client->GetTextRange(&range));
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(10U, range.end());

  EXPECT_TRUE(client->SetEditableSelectionRange(gfx::Range(1, 4)));
  EXPECT_TRUE(client->GetEditableSelectionRange(&range));
  EXPECT_EQ(gfx::Range(1, 4), range);

  base::string16 substring;
  EXPECT_TRUE(client->GetTextFromRange(range, &substring));
  EXPECT_STR_EQ("123", substring);

  EXPECT_TRUE(client->DeleteRange(range));
  EXPECT_STR_EQ("0456789", textfield_->GetText());

  ui::CompositionText composition;
  composition.text = UTF8ToUTF16("321");
  // Set composition through input method.
  input_method_->Clear();
  input_method_->SetCompositionTextForNextKey(composition);
  textfield_->clear();

  on_before_user_action_ = on_after_user_action_ = 0;
  DispatchMockInputMethodKeyEvent();

  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  EXPECT_TRUE(client->HasCompositionText());
  EXPECT_TRUE(client->GetCompositionTextRange(&range));
  EXPECT_STR_EQ("0321456789", textfield_->GetText());
  EXPECT_EQ(gfx::Range(1, 4), range);
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  input_method_->SetResultTextForNextKey(UTF8ToUTF16("123"));
  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  DispatchMockInputMethodKeyEvent();
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  EXPECT_FALSE(client->HasCompositionText());
  EXPECT_FALSE(input_method_->cancel_composition_called());
  EXPECT_STR_EQ("0123456789", textfield_->GetText());
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  input_method_->Clear();
  input_method_->SetCompositionTextForNextKey(composition);
  textfield_->clear();
  DispatchMockInputMethodKeyEvent();
  EXPECT_TRUE(client->HasCompositionText());
  EXPECT_STR_EQ("0123321456789", textfield_->GetText());

  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_FALSE(client->HasCompositionText());
  EXPECT_TRUE(input_method_->cancel_composition_called());
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_STR_EQ("0123321456789", textfield_->GetText());
  EXPECT_EQ(8U, textfield_->GetCursorPosition());
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  textfield_->clear();
  textfield_->SetText(ASCIIToUTF16("0123456789"));
  EXPECT_TRUE(client->SetEditableSelectionRange(gfx::Range(5, 5)));
  client->ExtendSelectionAndDelete(4, 2);
  EXPECT_STR_EQ("0789", textfield_->GetText());

  // On{Before,After}UserAction should be called by whatever user action
  // triggers clearing or setting a selection if appropriate.
  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  textfield_->ClearSelection();
  textfield_->SelectAll(false);
  EXPECT_EQ(0, on_before_user_action_);
  EXPECT_EQ(0, on_after_user_action_);

  input_method_->Clear();

  // Changing the Textfield to readonly shouldn't change the input client, since
  // it's still required for selections and clipboard copy.
  ui::TextInputClient* text_input_client = textfield_;
  EXPECT_TRUE(text_input_client);
  EXPECT_NE(ui::TEXT_INPUT_TYPE_NONE, text_input_client->GetTextInputType());
  textfield_->SetReadOnly(true);
  EXPECT_TRUE(input_method_->text_input_type_changed());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, text_input_client->GetTextInputType());

  input_method_->Clear();
  textfield_->SetReadOnly(false);
  EXPECT_TRUE(input_method_->text_input_type_changed());
  EXPECT_NE(ui::TEXT_INPUT_TYPE_NONE, text_input_client->GetTextInputType());

  input_method_->Clear();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_TRUE(input_method_->text_input_type_changed());
}

TEST_F(TextfieldTest, UndoRedoTest) {
  InitTextfield();
  SendKeyEvent(ui::VKEY_A);
  EXPECT_STR_EQ("a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("a", textfield_->GetText());

  // AppendText
  textfield_->AppendText(ASCIIToUTF16("b"));
  last_contents_.clear();  // AppendText doesn't call ContentsChanged.
  EXPECT_STR_EQ("ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("ab", textfield_->GetText());

  // SetText
  SendKeyEvent(ui::VKEY_C);
  // Undo'ing append moves the cursor to the end for now.
  // A no-op SetText won't add a new edit; see TextfieldModel::SetText.
  EXPECT_STR_EQ("abc", textfield_->GetText());
  textfield_->SetText(ASCIIToUTF16("abc"));
  EXPECT_STR_EQ("abc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("abc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("abc", textfield_->GetText());
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SetText(ASCIIToUTF16("123"));
  EXPECT_STR_EQ("123", textfield_->GetText());
  SendKeyEvent(ui::VKEY_END, false, false);
  SendKeyEvent(ui::VKEY_4, false, false);
  EXPECT_STR_EQ("1234", textfield_->GetText());
  last_contents_.clear();
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("123", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  // the insert edit "c" and set edit "123" are merged to single edit,
  // so text becomes "ab" after undo.
  EXPECT_STR_EQ("ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("123", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("1234", textfield_->GetText());

  // Undoing to the same text shouldn't call ContentsChanged.
  SendKeyEvent(ui::VKEY_A, false, true);  // select all
  SendKeyEvent(ui::VKEY_A);
  EXPECT_STR_EQ("a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  EXPECT_STR_EQ("abc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("1234", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("abc", textfield_->GetText());

  // Delete/Backspace
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("ab", textfield_->GetText());
  SendHomeEvent(false);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("b", textfield_->GetText());
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("b", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("abc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("b", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("", textfield_->GetText());
}

// Most platforms support Ctrl+Y as an alternative to Ctrl+Shift+Z, but on Mac
// Ctrl+Y is bound to "Yank" and Cmd+Y is bound to "Show full history". So, on
// Mac, Cmd+Shift+Z is sent for the tests above and the Ctrl+Y test below is
// skipped.
#if !defined(OS_MACOSX)

// Test that Ctrl+Y works for Redo, as well as Ctrl+Shift+Z.
TEST_F(TextfieldTest, RedoWithCtrlY) {
  InitTextfield();
  SendKeyEvent(ui::VKEY_A);
  EXPECT_STR_EQ("a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_STR_EQ("a", textfield_->GetText());
}

#endif  // !defined(OS_MACOSX)

// Non-Mac platforms don't have a key binding for Yank. Since this test is only
// run on Mac, it uses some Mac specific key bindings.
#if defined(OS_MACOSX)

TEST_F(TextfieldTest, Yank) {
  InitTextfields(2);
  textfield_->SetText(ASCIIToUTF16("abcdef"));
  textfield_->SetSelectedRange(gfx::Range(2, 4));

  // Press Ctrl+Y to yank.
  SendKeyPress(ui::VKEY_Y, ui::EF_CONTROL_DOWN);

  // Initially the kill buffer should be empty. Hence yanking should delete the
  // selected text.
  EXPECT_STR_EQ("abef", textfield_->GetText());
  EXPECT_EQ(gfx::Range(2), textfield_->GetSelectedRange());

  // Press Ctrl+K to delete to end of paragraph. This should place the deleted
  // text in the kill buffer.
  SendKeyPress(ui::VKEY_K, ui::EF_CONTROL_DOWN);

  EXPECT_STR_EQ("ab", textfield_->GetText());
  EXPECT_EQ(gfx::Range(2), textfield_->GetSelectedRange());

  // Yank twice.
  SendKeyPress(ui::VKEY_Y, ui::EF_CONTROL_DOWN);
  SendKeyPress(ui::VKEY_Y, ui::EF_CONTROL_DOWN);
  EXPECT_STR_EQ("abefef", textfield_->GetText());
  EXPECT_EQ(gfx::Range(6), textfield_->GetSelectedRange());

  // Verify pressing backspace does not modify the kill buffer.
  SendKeyEvent(ui::VKEY_BACK);
  SendKeyPress(ui::VKEY_Y, ui::EF_CONTROL_DOWN);
  EXPECT_STR_EQ("abefeef", textfield_->GetText());
  EXPECT_EQ(gfx::Range(7), textfield_->GetSelectedRange());

  // Move focus to next textfield.
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(2, GetFocusedView()->GetID());
  Textfield* textfield2 = static_cast<Textfield*>(GetFocusedView());
  EXPECT_TRUE(textfield2->GetText().empty());

  // Verify yanked text persists across multiple textfields and that yanking
  // into a password textfield works.
  textfield2->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  SendKeyPress(ui::VKEY_Y, ui::EF_CONTROL_DOWN);
  EXPECT_STR_EQ("ef", textfield2->GetText());
  EXPECT_EQ(gfx::Range(2), textfield2->GetSelectedRange());

  // Verify deletion in a password textfield does not modify the kill buffer.
  textfield2->SetText(ASCIIToUTF16("hello"));
  textfield2->SetSelectedRange(gfx::Range(0));
  SendKeyPress(ui::VKEY_K, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(textfield2->GetText().empty());

  textfield_->RequestFocus();
  textfield_->SetSelectedRange(gfx::Range(0));
  SendKeyPress(ui::VKEY_Y, ui::EF_CONTROL_DOWN);
  EXPECT_STR_EQ("efabefeef", textfield_->GetText());
}

#endif  // defined(OS_MACOSX)

TEST_F(TextfieldTest, CutCopyPaste) {
  InitTextfield();
  // Ensure IDS_APP_CUT cuts.
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SelectAll(false);
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_->ExecuteCommand(IDS_APP_CUT, 0);
  EXPECT_STR_EQ("123", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_STR_EQ("", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[x] cuts and [Ctrl]+[Alt][x] does nothing.
  textfield_->SetText(ASCIIToUTF16("456"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_X, true, false, true, false);
  EXPECT_STR_EQ("123", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_STR_EQ("456", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_STR_EQ("456", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_STR_EQ("", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Ensure [Shift]+[Delete] cuts.
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SelectAll(false);
  SendAlternateCut();
  EXPECT_STR_EQ("123", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_STR_EQ("", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Reset clipboard text.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "");

  // Ensure [Shift]+[Delete] is a no-op in case there is no selection.
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SetSelectedRange(gfx::Range(0));
  SendAlternateCut();
  EXPECT_STR_EQ("", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_STR_EQ("123", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());

  // Ensure IDS_APP_COPY copies.
  textfield_->SetText(ASCIIToUTF16("789"));
  textfield_->SelectAll(false);
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_->ExecuteCommand(IDS_APP_COPY, 0);
  EXPECT_STR_EQ("789", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[c] copies and [Ctrl]+[Alt][c] does nothing.
  textfield_->SetText(ASCIIToUTF16("012"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_C, true, false, true, false);
  EXPECT_STR_EQ("789", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_C, false, true);
  EXPECT_STR_EQ("012", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[Insert] copies.
  textfield_->SetText(ASCIIToUTF16("345"));
  textfield_->SelectAll(false);
  SendAlternateCopy();
  EXPECT_STR_EQ("345", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_STR_EQ("345", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Ensure IDS_APP_PASTE, [Ctrl]+[V], and [Shift]+[Insert] pastes;
  // also ensure that [Ctrl]+[Alt]+[V] does nothing.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "abc");
  textfield_->SetText(base::string16());
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_->ExecuteCommand(IDS_APP_PASTE, 0);
  EXPECT_STR_EQ("abc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_V, false, true);
  EXPECT_STR_EQ("abcabc", textfield_->GetText());
  SendAlternatePaste();
  EXPECT_STR_EQ("abcabcabc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_V, true, false, true, false);
  EXPECT_STR_EQ("abcabcabc", textfield_->GetText());

  // Ensure [Ctrl]+[Shift]+[Insert] is a no-op.
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_INSERT, true, true);
  EXPECT_STR_EQ("abc", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_STR_EQ("abcabcabc", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());
}

TEST_F(TextfieldTest, CutCopyPasteWithEditCommand) {
  InitTextfield();
  // Target the "WIDGET". This means that, on Mac, keystrokes will be sent to a
  // dummy 'Edit' menu which will dispatch into the responder chain as a "cut:"
  // selector rather than a keydown. This has no effect on other platforms
  // (events elsewhere always dispatch via a ui::EventProcessor, which is
  // responsible for finding targets).
  event_generator_->set_target(ui::test::EventGenerator::Target::WIDGET);

  SendKeyEvent(ui::VKEY_O, false, false);      // Type "o".
  SendKeyEvent(ui::VKEY_A, false, true);       // Select it.
  SendKeyEvent(ui::VKEY_C, false, true);       // Copy it.
  SendKeyEvent(ui::VKEY_RIGHT, false, false);  // Deselect and navigate to end.
  EXPECT_STR_EQ("o", textfield_->GetText());
  SendKeyEvent(ui::VKEY_V, false, true);  // Paste it.
  EXPECT_STR_EQ("oo", textfield_->GetText());
  SendKeyEvent(ui::VKEY_H, false, false);  // Type "h".
  EXPECT_STR_EQ("ooh", textfield_->GetText());
  SendKeyEvent(ui::VKEY_LEFT, true, false);  // Select "h".
  SendKeyEvent(ui::VKEY_X, false, true);     // Cut it.
  EXPECT_STR_EQ("oo", textfield_->GetText());
}

TEST_F(TextfieldTest, OvertypeMode) {
  InitTextfield();
  // Overtype mode should be disabled (no-op [Insert]).
  textfield_->SetText(ASCIIToUTF16("2"));
  const bool shift = false;
  SendHomeEvent(shift);
  // Note: On Mac, there is no insert key. Insert sends kVK_Help. Currently,
  // since there is no overtype on toolkit-views, the behavior happens to match.
  // However, there's no enable-overtype equivalent key combination on OSX.
  SendKeyEvent(ui::VKEY_INSERT);
  SendKeyEvent(ui::VKEY_1, false, false);
  EXPECT_STR_EQ("12", textfield_->GetText());
}

TEST_F(TextfieldTest, TextCursorDisplayTest) {
  InitTextfield();
  // LTR-RTL string in LTR context.
  SendKeyEvent('a');
  EXPECT_STR_EQ("a", textfield_->GetText());
  int x = GetCursorBounds().x();
  int prev_x = x;

  SendKeyEvent('b');
  EXPECT_STR_EQ("ab", textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
  prev_x = x;

  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1"), textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1\x5E2"), textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);

  // RTL-LTR string in LTR context.
  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"\x05E1"), textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_EQ(GetDisplayRect().x(), x);
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"\x05E1\x05E2"), textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  SendKeyEvent('a');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2"
                        L"a"),
            textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2"
                        L"ab"),
            textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
}

TEST_F(TextfieldTest, TextCursorDisplayInRTLTest) {
  std::string locale = base::i18n::GetConfiguredLocale();
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield();
  // LTR-RTL string in RTL context.
  SendKeyEvent('a');
  EXPECT_STR_EQ("a", textfield_->GetText());
  int x = GetCursorBounds().x();
  EXPECT_EQ(GetDisplayRect().right() - 1, x);
  int prev_x = x;

  SendKeyEvent('b');
  EXPECT_STR_EQ("ab", textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1"), textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1\x5E2"), textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);

  // RTL-LTR string in RTL context.
  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"\x05E1"), textfield_->GetText());
  x = GetCursorBounds().x();
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"\x05E1\x05E2"), textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);
  prev_x = x;

  SendKeyEvent('a');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2"
                        L"a"),
            textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2"
                        L"ab"),
            textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextfieldTest, TextCursorPositionInRTLTest) {
  std::string locale = base::i18n::GetConfiguredLocale();
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield();
  // LTR-RTL string in RTL context.
  int text_cursor_position_prev = test_api_->GetCursorViewRect().x();
  SendKeyEvent('a');
  SendKeyEvent('b');
  EXPECT_STR_EQ("ab", textfield_->GetText());
  int text_cursor_position_new = test_api_->GetCursorViewRect().x();
  // Text cursor stays at same place after inserting new charactors in RTL mode.
  EXPECT_EQ(text_cursor_position_prev, text_cursor_position_new);

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextfieldTest, TextCursorPositionInLTRTest) {
  InitTextfield();

  // LTR-RTL string in LTR context.
  int text_cursor_position_prev = test_api_->GetCursorViewRect().x();
  SendKeyEvent('a');
  SendKeyEvent('b');
  EXPECT_STR_EQ("ab", textfield_->GetText());
  int text_cursor_position_new = test_api_->GetCursorViewRect().x();
  // Text cursor moves to right after inserting new charactors in LTR mode.
  EXPECT_LT(text_cursor_position_prev, text_cursor_position_new);
}

TEST_F(TextfieldTest, HitInsideTextAreaTest) {
  InitTextfield();
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));
  std::vector<gfx::Rect> cursor_bounds;

  // Save each cursor bound.
  gfx::SelectionModel sel(0, gfx::CURSOR_FORWARD);
  cursor_bounds.push_back(GetCursorBounds(sel));

  sel = gfx::SelectionModel(1, gfx::CURSOR_BACKWARD);
  gfx::Rect bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(1, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  // Check that a cursor at the end of the Latin portion of the text is at the
  // same position as a cursor placed at the end of the RTL Hebrew portion.
  sel = gfx::SelectionModel(2, gfx::CURSOR_BACKWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(4, gfx::CURSOR_BACKWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  sel = gfx::SelectionModel(3, gfx::CURSOR_BACKWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(3, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  sel = gfx::SelectionModel(2, gfx::CURSOR_FORWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(4, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  // Expected cursor position when clicking left and right of each character.
  size_t cursor_pos_expected[] = {0, 1, 1, 2, 4, 3, 3, 2};

  int index = 0;
  for (size_t i = 0; i < cursor_bounds.size() - 1; ++i) {
    int half_width = (cursor_bounds[i + 1].x() - cursor_bounds[i].x()) / 2;
    MouseClick(cursor_bounds[i], half_width / 2);
    EXPECT_EQ(cursor_pos_expected[index++], textfield_->GetCursorPosition());

    // To avoid trigger double click. Not using sleep() since it takes longer
    // for the test to run if using sleep().
    NonClientMouseClick();

    MouseClick(cursor_bounds[i + 1], -(half_width / 2));
    EXPECT_EQ(cursor_pos_expected[index++], textfield_->GetCursorPosition());

    NonClientMouseClick();
  }
}

TEST_F(TextfieldTest, HitOutsideTextAreaTest) {
  InitTextfield();

  // LTR-RTL string in LTR context.
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));

  const bool shift = false;
  SendHomeEvent(shift);
  gfx::Rect bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendEndEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  NonClientMouseClick();

  // RTL-LTR string in LTR context.
  textfield_->SetText(WideToUTF16(L"\x05E1\x5E2" L"ab"));

  SendHomeEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendEndEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());
}

TEST_F(TextfieldTest, HitOutsideTextAreaInRTLTest) {
  std::string locale = base::i18n::GetConfiguredLocale();
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield();

  // RTL-LTR string in RTL context.
  textfield_->SetText(WideToUTF16(L"\x05E1\x5E2" L"ab"));
  const bool shift = false;
  SendHomeEvent(shift);
  gfx::Rect bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendEndEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  NonClientMouseClick();

  // LTR-RTL string in RTL context.
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));
  SendHomeEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendEndEvent(shift);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextfieldTest, OverflowTest) {
  InitTextfield();

  base::string16 str;
  for (size_t i = 0; i < 500; ++i)
    SendKeyEvent('a');
  SendKeyEvent(kHebrewLetterSamekh);
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  // Test mouse pointing.
  MouseClick(GetCursorBounds(), -1);
  EXPECT_EQ(500U, textfield_->GetCursorPosition());

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);

  for (size_t i = 0; i < 500; ++i)
    SendKeyEvent(kHebrewLetterSamekh);
  SendKeyEvent('a');
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), -1);
  EXPECT_EQ(501U, textfield_->GetCursorPosition());
}

TEST_F(TextfieldTest, OverflowInRTLTest) {
  std::string locale = base::i18n::GetConfiguredLocale();
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield();

  base::string16 str;
  for (size_t i = 0; i < 500; ++i)
    SendKeyEvent('a');
  SendKeyEvent(kHebrewLetterSamekh);
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), 1);
  EXPECT_EQ(501U, textfield_->GetCursorPosition());

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);

  for (size_t i = 0; i < 500; ++i)
    SendKeyEvent(kHebrewLetterSamekh);
  SendKeyEvent('a');
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), 1);
  EXPECT_EQ(500U, textfield_->GetCursorPosition());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextfieldTest, GetCompositionCharacterBoundsTest) {
  InitTextfield();
  ui::CompositionText composition;
  composition.text = UTF8ToUTF16("abc123");
  const uint32_t char_count = static_cast<uint32_t>(composition.text.length());
  ui::TextInputClient* client = textfield_;

  // Compare the composition character bounds with surrounding cursor bounds.
  for (uint32_t i = 0; i < char_count; ++i) {
    composition.selection = gfx::Range(i);
    client->SetCompositionText(composition);
    gfx::Point cursor_origin = GetCursorBounds().origin();
    views::View::ConvertPointToScreen(textfield_, &cursor_origin);

    composition.selection = gfx::Range(i + 1);
    client->SetCompositionText(composition);
    gfx::Point next_cursor_bottom_left = GetCursorBounds().bottom_left();
    views::View::ConvertPointToScreen(textfield_, &next_cursor_bottom_left);

    gfx::Rect character;
    EXPECT_TRUE(client->GetCompositionCharacterBounds(i, &character));
    EXPECT_EQ(character.origin(), cursor_origin) << " i=" << i;
    EXPECT_EQ(character.bottom_right(), next_cursor_bottom_left) << " i=" << i;
  }

  // Return false if the index is out of range.
  gfx::Rect rect;
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count, &rect));
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count + 1, &rect));
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count + 100, &rect));
}

TEST_F(TextfieldTest, GetCompositionCharacterBounds_ComplexText) {
  InitTextfield();

  const base::char16 kUtf16Chars[] = {
    // U+0020 SPACE
    0x0020,
    // U+1F408 (CAT) as surrogate pair
    0xd83d, 0xdc08,
    // U+5642 as Ideographic Variation Sequences
    0x5642, 0xDB40, 0xDD00,
    // U+260E (BLACK TELEPHONE) as Emoji Variation Sequences
    0x260E, 0xFE0F,
    // U+0020 SPACE
    0x0020,
  };
  const size_t kUtf16CharsCount = base::size(kUtf16Chars);

  ui::CompositionText composition;
  composition.text.assign(kUtf16Chars, kUtf16Chars + kUtf16CharsCount);
  ui::TextInputClient* client = textfield_;
  client->SetCompositionText(composition);

  // Make sure GetCompositionCharacterBounds never fails for index.
  gfx::Rect rects[kUtf16CharsCount];
  for (uint32_t i = 0; i < kUtf16CharsCount; ++i)
    EXPECT_TRUE(client->GetCompositionCharacterBounds(i, &rects[i]));

  // Here we might expect the following results but it actually depends on how
  // Uniscribe or HarfBuzz treats them with given font.
  // - rects[1] == rects[2]
  // - rects[3] == rects[4] == rects[5]
  // - rects[6] == rects[7]
}

// The word we select by double clicking should remain selected regardless of
// where we drag the mouse afterwards without releasing the left button.
TEST_F(TextfieldTest, KeepInitiallySelectedWord) {
  InitTextfield();

  textfield_->SetText(ASCIIToUTF16("abc def ghi"));

  textfield_->SetSelectedRange(gfx::Range(5, 5));
  const gfx::Rect middle_cursor = GetCursorBounds();
  textfield_->SetSelectedRange(gfx::Range(0, 0));
  const gfx::Point beginning = GetCursorBounds().origin();

  // Double click, but do not release the left button.
  MouseClick(middle_cursor, 0);
  const gfx::Point middle(middle_cursor.x(),
                          middle_cursor.y() + middle_cursor.height() / 2);
  MoveMouseTo(middle);
  PressLeftMouseButton();
  EXPECT_EQ(gfx::Range(4, 7), textfield_->GetSelectedRange());

  // Drag the mouse to the beginning of the textfield.
  DragMouseTo(beginning);
  EXPECT_EQ(gfx::Range(7, 0), textfield_->GetSelectedRange());
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
TEST_F(TextfieldTest, SelectionClipboard) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("0123"));
  const int cursor_y = GetCursorYForTesting();
  gfx::Point point_1(GetCursorPositionX(1), cursor_y);
  gfx::Point point_2(GetCursorPositionX(2), cursor_y);
  gfx::Point point_3(GetCursorPositionX(3), cursor_y);
  gfx::Point point_4(GetCursorPositionX(4), cursor_y);

  // Text selected by the mouse should be placed on the selection clipboard.
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, point_1, point_1,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(press);
  ui::MouseEvent drag(ui::ET_MOUSE_DRAGGED, point_3, point_3,
                      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                      ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseDragged(drag);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, point_3, point_3,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release);
  EXPECT_EQ(gfx::Range(1, 3), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("12", GetClipboardText(ui::ClipboardBuffer::kSelection));

  // Select-all should update the selection clipboard.
  SendKeyEvent(ui::VKEY_A, false, true);
  EXPECT_EQ(gfx::Range(0, 4), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("0123", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());

  // Shift-click selection modifications should update the clipboard.
  NonClientMouseClick();
  ui::MouseEvent press_2(ui::ET_MOUSE_PRESSED, point_2, point_2,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  press_2.set_flags(press_2.flags() | ui::EF_SHIFT_DOWN);
  textfield_->OnMousePressed(press_2);
  ui::MouseEvent release_2(ui::ET_MOUSE_RELEASED, point_2, point_2,
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release_2);
  EXPECT_EQ(gfx::Range(0, 2), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("01", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());

  // Shift-Left/Right should update the selection clipboard.
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  EXPECT_STR_EQ("012", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_LEFT, true, false);
  EXPECT_STR_EQ("01", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_RIGHT, true, true);
  EXPECT_STR_EQ("0123", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());

  // Moving the cursor without a selection should not change the clipboard.
  SendKeyEvent(ui::VKEY_LEFT, false, false);
  EXPECT_EQ(gfx::Range(0, 0), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("0123", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());

  // Middle clicking should paste at the mouse (not cursor) location.
  // The cursor should be placed at the end of the pasted text.
  ui::MouseEvent middle(ui::ET_MOUSE_PRESSED, point_4, point_4,
                        ui::EventTimeForNow(), ui::EF_MIDDLE_MOUSE_BUTTON,
                        ui::EF_MIDDLE_MOUSE_BUTTON);
  textfield_->OnMousePressed(middle);
  EXPECT_STR_EQ("01230123", textfield_->GetText());
  EXPECT_EQ(gfx::Range(8, 8), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("0123", GetClipboardText(ui::ClipboardBuffer::kSelection));

  // Middle clicking on an unfocused textfield should focus it and paste.
  textfield_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(textfield_->HasFocus());
  textfield_->OnMousePressed(middle);
  EXPECT_TRUE(textfield_->HasFocus());
  EXPECT_STR_EQ("012301230123", textfield_->GetText());
  EXPECT_EQ(gfx::Range(8, 8), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("0123", GetClipboardText(ui::ClipboardBuffer::kSelection));

  // Middle clicking with an empty selection clipboard should still focus.
  SetClipboardText(ui::ClipboardBuffer::kSelection, std::string());
  textfield_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(textfield_->HasFocus());
  textfield_->OnMousePressed(middle);
  EXPECT_TRUE(textfield_->HasFocus());
  EXPECT_STR_EQ("012301230123", textfield_->GetText());
  EXPECT_EQ(gfx::Range(4, 4), textfield_->GetSelectedRange());
  EXPECT_TRUE(GetClipboardText(ui::ClipboardBuffer::kSelection).empty());

  // Middle clicking in the selection should insert the selection clipboard
  // contents into the middle of the selection, and move the cursor to the end
  // of the pasted content.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, "foo");
  textfield_->SetSelectedRange(gfx::Range(2, 6));
  textfield_->OnMousePressed(middle);
  EXPECT_STR_EQ("0123foo01230123", textfield_->GetText());
  EXPECT_EQ(gfx::Range(7, 7), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("foo", GetClipboardText(ui::ClipboardBuffer::kSelection));

  // Double and triple clicking should update the clipboard contents.
  textfield_->SetText(ASCIIToUTF16("ab cd ef"));
  gfx::Point word(GetCursorPositionX(4), cursor_y);
  ui::MouseEvent press_word(ui::ET_MOUSE_PRESSED, word, word,
                            ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                            ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(press_word);
  ui::MouseEvent release_word(ui::ET_MOUSE_RELEASED, word, word,
                              ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                              ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release_word);
  ui::MouseEvent double_click(ui::ET_MOUSE_PRESSED, word, word,
                              ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_DOUBLE_CLICK,
                              ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(double_click);
  textfield_->OnMouseReleased(release_word);
  EXPECT_EQ(gfx::Range(3, 5), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("cd", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());
  textfield_->OnMousePressed(press_word);
  textfield_->OnMouseReleased(release_word);
  EXPECT_EQ(gfx::Range(0, 8), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("ab cd ef", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());

  // Selecting a range of text without any user interaction should not change
  // the clipboard content.
  textfield_->SetSelectedRange(gfx::Range(0, 3));
  EXPECT_STR_EQ("ab ", textfield_->GetSelectedText());
  EXPECT_STR_EQ("ab cd ef", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());

  SetClipboardText(ui::ClipboardBuffer::kSelection, "other");
  textfield_->SelectAll(false);
  EXPECT_STR_EQ("other", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());
}

// Verify that the selection clipboard is not updated for selections on a
// password textfield.
TEST_F(TextfieldTest, SelectionClipboard_Password) {
  InitTextfields(2);
  textfield_->SetText(ASCIIToUTF16("abcd"));

  // Select-all should update the selection clipboard for a non-password
  // textfield.
  SendKeyEvent(ui::VKEY_A, false, true);
  EXPECT_EQ(gfx::Range(0, 4), textfield_->GetSelectedRange());
  EXPECT_STR_EQ("abcd", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());

  // Move focus to the next textfield.
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(2, GetFocusedView()->GetID());
  Textfield* textfield2 = static_cast<Textfield*>(GetFocusedView());

  // Select-all should not modify the selection clipboard for a password
  // textfield.
  textfield2->SetText(ASCIIToUTF16("1234"));
  textfield2->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  SendKeyEvent(ui::VKEY_A, false, true);
  EXPECT_EQ(gfx::Range(0, 4), textfield2->GetSelectedRange());
  EXPECT_STR_EQ("abcd", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());

  // Shift-Left/Right should not modify the selection clipboard for a password
  // textfield.
  SendKeyEvent(ui::VKEY_LEFT, true, false);
  EXPECT_EQ(gfx::Range(0, 3), textfield2->GetSelectedRange());
  EXPECT_STR_EQ("abcd", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());

  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  EXPECT_EQ(gfx::Range(0, 4), textfield2->GetSelectedRange());
  EXPECT_STR_EQ("abcd", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());
}
#endif

// Long_Press gesture in Textfield can initiate a drag and drop now.
TEST_F(TextfieldTest, TestLongPressInitiatesDragDrop) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("Hello string world"));

  // Ensure the textfield will provide selected text for drag data.
  textfield_->SetSelectedRange(gfx::Range(6, 12));
  const gfx::Point kStringPoint(GetCursorPositionX(9), GetCursorYForTesting());

  // Enable touch-drag-drop to make long press effective.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);

  // Create a long press event in the selected region should start a drag.
  GestureEventForTest long_press(
      kStringPoint.x(),
      kStringPoint.y(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  textfield_->OnGestureEvent(&long_press);
  EXPECT_TRUE(
      textfield_->CanStartDragForView(nullptr, kStringPoint, kStringPoint));
}

TEST_F(TextfieldTest, GetTextfieldBaseline_FontFallbackTest) {
  InitTextfield();
  textfield_->SetText(UTF8ToUTF16("abc"));
  const int old_baseline = textfield_->GetBaseline();

  // Set text which may fall back to a font which has taller baseline than
  // the default font.
  textfield_->SetText(UTF8ToUTF16("\xE0\xB9\x91"));
  const int new_baseline = textfield_->GetBaseline();

  // Regardless of the text, the baseline must be the same.
  EXPECT_EQ(new_baseline, old_baseline);
}

// Tests that a textfield view can be destroyed from OnKeyEvent() on its
// controller and it does not crash.
TEST_F(TextfieldTest, DestroyingTextfieldFromOnKeyEvent) {
  InitTextfield();

  // The controller assumes ownership of the textfield.
  TextfieldDestroyerController controller(textfield_);
  EXPECT_TRUE(controller.target());

  // Send a key to trigger OnKeyEvent().
  SendKeyEvent(ui::VKEY_RETURN);

  EXPECT_FALSE(controller.target());
}

TEST_F(TextfieldTest, CursorBlinkRestartsOnInsertOrReplace) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("abc"));
  EXPECT_TRUE(test_api_->IsCursorBlinkTimerRunning());
  textfield_->SetSelectedRange(gfx::Range(1, 2));
  EXPECT_FALSE(test_api_->IsCursorBlinkTimerRunning());
  textfield_->InsertOrReplaceText(base::ASCIIToUTF16("foo"));
  EXPECT_TRUE(test_api_->IsCursorBlinkTimerRunning());
}

#if defined(OS_CHROMEOS)
// Check that when accessibility virtual keyboard is enabled, windows are
// shifted up when focused and restored when focus is lost.
TEST_F(TextfieldTest, VirtualKeyboardFocusEnsureCaretNotInRect) {
  InitTextfield();

  aura::Window* root_window = GetRootWindow(widget_);
  int keyboard_height = 200;
  gfx::Rect root_bounds = root_window->bounds();
  gfx::Rect orig_widget_bounds = gfx::Rect(0, 300, 400, 200);
  gfx::Rect shifted_widget_bounds = gfx::Rect(0, 200, 400, 200);
  gfx::Rect keyboard_view_bounds =
      gfx::Rect(0, root_bounds.height() - keyboard_height, root_bounds.width(),
                keyboard_height);

  // Focus the window.
  widget_->SetBounds(orig_widget_bounds);
  input_method_->SetFocusedTextInputClient(textfield_);
  EXPECT_EQ(widget_->GetNativeView()->bounds(), orig_widget_bounds);

  // Simulate virtual keyboard.
  input_method_->SetOnScreenKeyboardBounds(keyboard_view_bounds);

  // Window should be shifted.
  EXPECT_EQ(widget_->GetNativeView()->bounds(), shifted_widget_bounds);

  // Detach the textfield from the IME
  input_method_->DetachTextInputClient(textfield_);
  wm::RestoreWindowBoundsOnClientFocusLost(
      widget_->GetNativeView()->GetToplevelWindow());

  // Window should be restored.
  EXPECT_EQ(widget_->GetNativeView()->bounds(), orig_widget_bounds);
}
#endif  // defined(OS_CHROMEOS)

class TextfieldTouchSelectionTest : public TextfieldTest {
 protected:
  // Simulates a complete tap.
  void Tap(const gfx::Point& point) {
    GestureEventForTest begin(
        point.x(), point.y(), ui::GestureEventDetails(ui::ET_GESTURE_BEGIN));
    textfield_->OnGestureEvent(&begin);

    GestureEventForTest tap_down(
        point.x(), point.y(), ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
    textfield_->OnGestureEvent(&tap_down);

    GestureEventForTest show_press(
        point.x(),
        point.y(),
        ui::GestureEventDetails(ui::ET_GESTURE_SHOW_PRESS));
    textfield_->OnGestureEvent(&show_press);

    ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP);
    tap_details.set_tap_count(1);
    GestureEventForTest tap(point.x(), point.y(), tap_details);
    textfield_->OnGestureEvent(&tap);

    GestureEventForTest end(
        point.x(), point.y(), ui::GestureEventDetails(ui::ET_GESTURE_END));
    textfield_->OnGestureEvent(&end);
  }
};

// Touch selection and dragging currently only works for chromeos.
#if defined(OS_CHROMEOS)
TEST_F(TextfieldTouchSelectionTest, TouchSelectionAndDraggingTest) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  EXPECT_FALSE(test_api_->touch_selection_controller());
  const int x = GetCursorPositionX(2);

  // Tapping on the textfield should turn on the TouchSelectionController.
  ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP);
  tap_details.set_tap_count(1);
  GestureEventForTest tap(x, 0, tap_details);
  textfield_->OnGestureEvent(&tap);
  EXPECT_TRUE(test_api_->touch_selection_controller());

  // Un-focusing the textfield should reset the TouchSelectionController
  textfield_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(test_api_->touch_selection_controller());
  textfield_->RequestFocus();

  // With touch editing enabled, long press should not show context menu.
  // Instead, select word and invoke TouchSelectionController.
  GestureEventForTest long_press_1(
      x, 0, ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  textfield_->OnGestureEvent(&long_press_1);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_TRUE(test_api_->touch_selection_controller());
  EXPECT_TRUE(long_press_1.handled());

  // With touch drag drop enabled, long pressing in the selected region should
  // start a drag and remove TouchSelectionController.
  ASSERT_TRUE(switches::IsTouchDragDropEnabled());
  GestureEventForTest long_press_2(
      x, 0, ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  textfield_->OnGestureEvent(&long_press_2);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_FALSE(test_api_->touch_selection_controller());
  EXPECT_FALSE(long_press_2.handled());

  // After disabling touch drag drop, long pressing again in the selection
  // region should not do anything.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableTouchDragDrop);
  ASSERT_FALSE(switches::IsTouchDragDropEnabled());
  GestureEventForTest long_press_3(
      x, 0, ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  textfield_->OnGestureEvent(&long_press_3);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_FALSE(test_api_->touch_selection_controller());
  EXPECT_FALSE(long_press_3.handled());
}
#endif

TEST_F(TextfieldTouchSelectionTest, TouchSelectionInUnfocusableTextfield) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  gfx::Point touch_point(GetCursorPositionX(2), 0);

  // Disable textfield and tap on it. Touch text selection should not get
  // activated.
  textfield_->SetEnabled(false);
  Tap(touch_point);
  EXPECT_FALSE(test_api_->touch_selection_controller());
  textfield_->SetEnabled(true);

  // Make textfield unfocusable and tap on it. Touch text selection should not
  // get activated.
  textfield_->SetFocusBehavior(View::FocusBehavior::NEVER);
  Tap(touch_point);
  EXPECT_FALSE(textfield_->HasFocus());
  EXPECT_FALSE(test_api_->touch_selection_controller());
  textfield_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if defined(OS_MACOSX)
#define MAYBE_TapOnSelection DISABLED_TapOnSelection
#else
#define MAYBE_TapOnSelection TapOnSelection
#endif

TEST_F(TextfieldTouchSelectionTest, MAYBE_TapOnSelection) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("hello world"));
  gfx::Range sel_range(2, 7);
  gfx::Range tap_range(5, 5);
  gfx::Rect tap_rect =
      GetCursorBounds(gfx::SelectionModel(tap_range, gfx::CURSOR_FORWARD));
  gfx::Point tap_point = tap_rect.CenterPoint();

  // Select range |sel_range| and check if touch selection handles are not
  // present and correct range is selected.
  textfield_->SetEditableSelectionRange(sel_range);
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_FALSE(test_api_->touch_selection_controller());
  EXPECT_EQ(sel_range, range);

  // Tap on selection and check if touch selectoin handles are shown, but
  // selection range is not modified.
  Tap(tap_point);
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_TRUE(test_api_->touch_selection_controller());
  EXPECT_EQ(sel_range, range);

  // Tap again on selection and check if touch selection handles are still
  // present and selection is changed to a cursor at tap location.
  Tap(tap_point);
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_TRUE(test_api_->touch_selection_controller());
  EXPECT_EQ(tap_range, range);
}

TEST_F(TextfieldTest, AccessiblePasswordTest) {
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16("password"));

  ui::AXNodeData node_data_regular;
  textfield_->GetAccessibleNodeData(&node_data_regular);
  EXPECT_EQ(ax::mojom::Role::kTextField, node_data_regular.role);
  EXPECT_EQ(ASCIIToUTF16("password"), node_data_regular.GetString16Attribute(
                                          ax::mojom::StringAttribute::kValue));
  EXPECT_FALSE(node_data_regular.HasState(ax::mojom::State::kProtected));

  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  ui::AXNodeData node_data_protected;
  textfield_->GetAccessibleNodeData(&node_data_protected);
  EXPECT_EQ(ax::mojom::Role::kTextField, node_data_protected.role);
  EXPECT_EQ(UTF8ToUTF16("••••••••"), node_data_protected.GetString16Attribute(
                                         ax::mojom::StringAttribute::kValue));
  EXPECT_TRUE(node_data_protected.HasState(ax::mojom::State::kProtected));
}

// Verify that cursor visibility is controlled by SetCursorEnabled.
TEST_F(TextfieldTest, CursorVisibility) {
  InitTextfield();

  textfield_->SetCursorEnabled(false);
  EXPECT_FALSE(test_api_->IsCursorVisible());

  textfield_->SetCursorEnabled(true);
  EXPECT_TRUE(test_api_->IsCursorVisible());
}

// Verify that cursor view height does not exceed the textfield height.
TEST_F(TextfieldTest, CursorViewHeight) {
  InitTextfield();
  textfield_->SetBounds(0, 0, 100, 100);
  textfield_->SetCursorEnabled(true);
  SendKeyEvent('a');
  EXPECT_TRUE(test_api_->IsCursorVisible());
  EXPECT_GT(textfield_->GetVisibleBounds().height(),
            test_api_->GetCursorViewRect().height());
  EXPECT_LE(test_api_->GetCursorViewRect().height(),
            GetCursorBounds().height());

  // set the cursor height to be higher than the textfield height, verify that
  // UpdateCursorViewPosition update cursor view height currectly.
  gfx::Rect cursor_bound(test_api_->GetCursorViewRect());
  cursor_bound.set_height(150);
  test_api_->SetCursorViewRect(cursor_bound);
  SendKeyEvent('b');
  EXPECT_GT(textfield_->GetVisibleBounds().height(),
            test_api_->GetCursorViewRect().height());
  EXPECT_LE(test_api_->GetCursorViewRect().height(),
            GetCursorBounds().height());
}

// Verify that cursor view height is independent of its parent view height.
TEST_F(TextfieldTest, CursorViewHeightAtDiffDSF) {
  InitTextfield();
  textfield_->SetBounds(0, 0, 100, 100);
  textfield_->SetCursorEnabled(true);
  SendKeyEvent('a');
  EXPECT_TRUE(test_api_->IsCursorVisible());
  int height = test_api_->GetCursorViewRect().height();

  // update the size of its parent view size and verify that the height of the
  // cursor view stays the same.
  View* parent = textfield_->parent();
  parent->SetBounds(0, 0, 50, height - 2);
  SendKeyEvent('b');
  EXPECT_EQ(height, test_api_->GetCursorViewRect().height());
}

// Check if the text cursor is always at the end of the textfield after the
// text overflows from the textfield. If the textfield size changes, check if
// the text cursor's location is updated accordingly.
TEST_F(TextfieldTest, TextfieldBoundsChangeTest) {
  InitTextfield();
  gfx::Size new_size = gfx::Size(30, 100);
  textfield_->SetSize(new_size);

  // Insert chars in |textfield_| to make it overflow.
  SendKeyEvent('a');
  SendKeyEvent('a');
  SendKeyEvent('a');
  SendKeyEvent('a');
  SendKeyEvent('a');
  SendKeyEvent('a');
  SendKeyEvent('a');

  // Check if the cursor continues pointing to the end of the textfield.
  int prev_x = GetCursorBounds().x();
  SendKeyEvent('a');
  EXPECT_EQ(prev_x, GetCursorBounds().x());
  EXPECT_TRUE(test_api_->IsCursorVisible());

  // Increase the textfield size and check if the cursor moves to the new end.
  textfield_->SetSize(gfx::Size(40, 100));
  EXPECT_LT(prev_x, GetCursorBounds().x());

  prev_x = GetCursorBounds().x();
  // Decrease the textfield size and check if the cursor moves to the new end.
  textfield_->SetSize(gfx::Size(30, 100));
  EXPECT_GT(prev_x, GetCursorBounds().x());
}

// Verify that after creating a new Textfield, the Textfield doesn't
// automatically receive focus and the text cursor is not visible.
TEST_F(TextfieldTest, TextfieldInitialization) {
  TestTextfield* new_textfield = new TestTextfield();
  new_textfield->set_controller(this);
  View* container = new View();
  Widget* widget(new Widget());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget->Init(std::move(params));
  widget->SetContentsView(container);
  container->AddChildView(new_textfield);

  new_textfield->SetBoundsRect(params.bounds);
  new_textfield->SetID(1);
  test_api_ = std::make_unique<TextfieldTestApi>(new_textfield);
  widget->Show();
  EXPECT_FALSE(new_textfield->HasFocus());
  EXPECT_FALSE(test_api_->IsCursorVisible());
  new_textfield->RequestFocus();
  EXPECT_TRUE(test_api_->IsCursorVisible());
  widget->Close();
}

// Verify that if a textfield gains focus during key dispatch that an edit
// command only results when the event is not consumed.
TEST_F(TextfieldTest, SwitchFocusInKeyDown) {
  InitTextfield();
  TextfieldFocuser* focuser = new TextfieldFocuser(textfield_);
  widget_->GetContentsView()->AddChildView(focuser);

  focuser->RequestFocus();
  EXPECT_EQ(focuser, GetFocusedView());
  SendKeyPress(ui::VKEY_SPACE, 0);
  EXPECT_EQ(textfield_, GetFocusedView());
  EXPECT_EQ(base::string16(), textfield_->GetText());

  focuser->set_consume(false);
  focuser->RequestFocus();
  EXPECT_EQ(focuser, GetFocusedView());
  SendKeyPress(ui::VKEY_SPACE, 0);
  EXPECT_EQ(textfield_, GetFocusedView());
  EXPECT_EQ(base::ASCIIToUTF16(" "), textfield_->GetText());
}

TEST_F(TextfieldTest, FocusChangesScrollToStart) {
  const std::string& kText = "abcdef";
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16(kText));
  EXPECT_EQ(base::ASCIIToUTF16(std::string()), textfield_->GetSelectedText());
  textfield_->AboutToRequestFocusFromTabTraversal(false);
  EXPECT_EQ(base::ASCIIToUTF16(kText), textfield_->GetSelectedText());
  if (PlatformStyle::kTextfieldScrollsToStartOnFocusChange)
    EXPECT_EQ(0U, textfield_->GetCursorPosition());
  else
    EXPECT_EQ(kText.size(), textfield_->GetCursorPosition());

  // The OnBlur() behavior below is only meaningful on platforms where textfield
  // focus moves on focus change.
  if (!PlatformStyle::kTextfieldScrollsToStartOnFocusChange)
    return;

  // The cursor is at the start (so that it scrolls in to view), but the
  // "Select All" is currently undirected. Shift+right will give it a direction
  // and scroll to the end.
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  EXPECT_EQ(kText.size(), textfield_->GetCursorPosition());

  // And a focus loss should scroll back to the start.
  textfield_->OnBlur();
  EXPECT_EQ(0U, textfield_->GetCursorPosition());
}

TEST_F(TextfieldTest, SendingDeletePreservesShiftFlag) {
  InitTextfield();
  SendKeyPress(ui::VKEY_DELETE, 0);
  EXPECT_EQ(0, textfield_->event_flags());
  textfield_->clear();

  // Ensure the shift modifier propagates for keys that may be subject to native
  // key mappings. E.g., on Mac, Delete and Shift+Delete are both
  // deleteForward:, but the shift modifier should propagate.
  SendKeyPress(ui::VKEY_DELETE, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(ui::EF_SHIFT_DOWN, textfield_->event_flags());
}

TEST_F(TextfieldTest, EmojiItem_EmptyField) {
  InitTextfield();
  EXPECT_TRUE(textfield_->context_menu_controller());

  // A normal empty field may show the Emoji option (if supported).
  ui::MenuModel* context_menu = GetContextMenuModel();
  EXPECT_TRUE(context_menu);
  EXPECT_GT(context_menu->GetItemCount(), 0);
  // Not all OS/versions support the emoji menu.
  EXPECT_EQ(ui::IsEmojiPanelSupported(),
            context_menu->GetLabelAt(0) ==
                l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_EMOJI));
}

TEST_F(TextfieldTest, EmojiItem_ReadonlyField) {
  InitTextfield();
  EXPECT_TRUE(textfield_->context_menu_controller());

  textfield_->SetReadOnly(true);
  // In no case is the emoji option showing on a read-only field.
  ui::MenuModel* context_menu = GetContextMenuModel();
  EXPECT_TRUE(context_menu);
  EXPECT_GT(context_menu->GetItemCount(), 0);
  EXPECT_NE(context_menu->GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_EMOJI));
}

TEST_F(TextfieldTest, EmojiItem_FieldWithText) {
  InitTextfield();
  EXPECT_TRUE(textfield_->context_menu_controller());

#if defined(OS_MACOSX)
  // On Mac, when there is text, the "Look up" item (+ separator) takes the top
  // position, and emoji comes after.
  constexpr int kExpectedEmojiIndex = 2;
#else
  constexpr int kExpectedEmojiIndex = 0;
#endif

  // A field with text may still show the Emoji option (if supported).
  textfield_->SetText(base::ASCIIToUTF16("some text"));
  textfield_->SelectAll(false);
  ui::MenuModel* context_menu = GetContextMenuModel();
  EXPECT_TRUE(context_menu);
  EXPECT_GT(context_menu->GetItemCount(), 0);
  // Not all OS/versions support the emoji menu.
  EXPECT_EQ(ui::IsEmojiPanelSupported(),
            context_menu->GetLabelAt(kExpectedEmojiIndex) ==
                l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_EMOJI));
}

#if defined(OS_MACOSX)
// Tests to see if the BiDi submenu items are updated correctly when the
// textfield's text direction is changed.
TEST_F(TextfieldTest, TextServicesContextMenuTextDirectionTest) {
  InitTextfield();
  EXPECT_TRUE(textfield_->context_menu_controller());

  EXPECT_TRUE(GetContextMenuModel());

  textfield_->ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection::LEFT_TO_RIGHT);
  test_api_->UpdateContextMenu();

  EXPECT_FALSE(test_api_->IsTextDirectionCheckedInContextMenu(
      base::i18n::TextDirection::UNKNOWN_DIRECTION));
  EXPECT_TRUE(test_api_->IsTextDirectionCheckedInContextMenu(
      base::i18n::TextDirection::LEFT_TO_RIGHT));
  EXPECT_FALSE(test_api_->IsTextDirectionCheckedInContextMenu(
      base::i18n::TextDirection::RIGHT_TO_LEFT));

  textfield_->ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection::RIGHT_TO_LEFT);
  test_api_->UpdateContextMenu();

  EXPECT_FALSE(test_api_->IsTextDirectionCheckedInContextMenu(
      base::i18n::TextDirection::UNKNOWN_DIRECTION));
  EXPECT_FALSE(test_api_->IsTextDirectionCheckedInContextMenu(
      base::i18n::TextDirection::LEFT_TO_RIGHT));
  EXPECT_TRUE(test_api_->IsTextDirectionCheckedInContextMenu(
      base::i18n::TextDirection::RIGHT_TO_LEFT));
}

// Tests to see if the look up item is updated when the textfield's selected
// text has changed.
TEST_F(TextfieldTest, LookUpItemUpdate) {
  InitTextfield();
  EXPECT_TRUE(textfield_->context_menu_controller());

  const base::string16 kTextOne = ASCIIToUTF16("crake");
  textfield_->SetText(kTextOne);
  textfield_->SelectAll(false);

  ui::MenuModel* context_menu = GetContextMenuModel();
  EXPECT_TRUE(context_menu);
  EXPECT_GT(context_menu->GetItemCount(), 0);
  EXPECT_EQ(context_menu->GetLabelAt(0),
            l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_LOOK_UP, kTextOne));

#if !defined(OS_MACOSX)
  // Mac context menus don't behave this way: it's not possible to update the
  // text while the menu is still "open", but also the selection can't change
  // while the menu is open (because the user can't interact with the rest of
  // the app).
  const base::string16 kTextTwo = ASCIIToUTF16("rail");
  textfield_->SetText(kTextTwo);
  textfield_->SelectAll(false);

  context_menu = GetContextMenuModel();
  EXPECT_TRUE(context_menu);
  EXPECT_GT(context_menu->GetItemCount(), 0);
  EXPECT_EQ(context_menu->GetLabelAt(0),
            l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_LOOK_UP, kTextTwo));
#endif
}

// Tests to see if the look up item is hidden for password fields.
TEST_F(TextfieldTest, LookUpPassword) {
  InitTextfield();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  const base::string16 kText = ASCIIToUTF16("Willie Wagtail");

  textfield_->SetText(kText);
  textfield_->SelectAll(false);

  ui::MenuModel* context_menu = GetContextMenuModel();
  EXPECT_TRUE(context_menu);
  EXPECT_GT(context_menu->GetItemCount(), 0);
  EXPECT_NE(context_menu->GetCommandIdAt(0), IDS_CONTENT_CONTEXT_LOOK_UP);
  EXPECT_NE(context_menu->GetLabelAt(0),
            l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_LOOK_UP, kText));
}

TEST_F(TextfieldTest, SecurePasswordInput) {
  InitTextfield();
  ASSERT_FALSE(ui::ScopedPasswordInputEnabler::IsPasswordInputEnabled());

  // Shouldn't enable secure input if it's not a password textfield.
  textfield_->OnFocus();
  EXPECT_FALSE(ui::ScopedPasswordInputEnabler::IsPasswordInputEnabled());

  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  // Single matched calls immediately update IsPasswordInputEnabled().
  textfield_->OnFocus();
  EXPECT_TRUE(ui::ScopedPasswordInputEnabler::IsPasswordInputEnabled());

  textfield_->OnBlur();
  EXPECT_FALSE(ui::ScopedPasswordInputEnabler::IsPasswordInputEnabled());
}
#endif  // defined(OS_MACOSX)

TEST_F(TextfieldTest, AccessibilitySelectionEvents) {
  const std::string& kText = "abcdef";
  InitTextfield();
  textfield_->SetText(ASCIIToUTF16(kText));
  EXPECT_TRUE(textfield_->HasFocus());
  int previous_selection_fired_count =
      textfield_->GetAccessibilitySelectionFiredCount();
  textfield_->SelectAll(false);
  EXPECT_LT(previous_selection_fired_count,
            textfield_->GetAccessibilitySelectionFiredCount());
  previous_selection_fired_count =
      textfield_->GetAccessibilitySelectionFiredCount();

  // No selection event when textfield blurred, even though text is
  // deselected.
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(textfield_->HasFocus());
  textfield_->ClearSelection();
  EXPECT_FALSE(textfield_->HasSelection());
  // Has not changed.
  EXPECT_EQ(previous_selection_fired_count,
            textfield_->GetAccessibilitySelectionFiredCount());
}

TEST_F(TextfieldTest, FocusReasonMouse) {
  InitTextfield();
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_NONE,
            textfield_->GetFocusReason());

  const auto& bounds = textfield_->bounds();
  MouseClick(bounds, 10);

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_MOUSE,
            textfield_->GetFocusReason());
}

TEST_F(TextfieldTest, FocusReasonTouchTap) {
  InitTextfield();
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_NONE,
            textfield_->GetFocusReason());

  ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP_DOWN);
  tap_details.set_primary_pointer_type(
      ui::EventPointerType::POINTER_TYPE_TOUCH);
  GestureEventForTest tap(GetCursorPositionX(0), 0, tap_details);
  textfield_->OnGestureEvent(&tap);

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_TOUCH,
            textfield_->GetFocusReason());
}

TEST_F(TextfieldTest, FocusReasonPenTap) {
  InitTextfield();
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_NONE,
            textfield_->GetFocusReason());

  ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP_DOWN);
  tap_details.set_primary_pointer_type(ui::EventPointerType::POINTER_TYPE_PEN);
  GestureEventForTest tap(GetCursorPositionX(0), 0, tap_details);
  textfield_->OnGestureEvent(&tap);

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_PEN,
            textfield_->GetFocusReason());
}

TEST_F(TextfieldTest, FocusReasonMultipleEvents) {
  InitTextfield();
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_NONE,
            textfield_->GetFocusReason());

  // Pen tap, followed by a touch tap
  {
    ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP_DOWN);
    tap_details.set_primary_pointer_type(
        ui::EventPointerType::POINTER_TYPE_PEN);
    GestureEventForTest tap(GetCursorPositionX(0), 0, tap_details);
    textfield_->OnGestureEvent(&tap);
  }

  {
    ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP_DOWN);
    tap_details.set_primary_pointer_type(
        ui::EventPointerType::POINTER_TYPE_TOUCH);
    GestureEventForTest tap(GetCursorPositionX(0), 0, tap_details);
    textfield_->OnGestureEvent(&tap);
  }

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_PEN,
            textfield_->GetFocusReason());
}

TEST_F(TextfieldTest, FocusReasonFocusBlurFocus) {
  InitTextfield();
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_NONE,
            textfield_->GetFocusReason());

  // Pen tap, blur, then programmatic focus.
  ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP_DOWN);
  tap_details.set_primary_pointer_type(ui::EventPointerType::POINTER_TYPE_PEN);
  GestureEventForTest tap(GetCursorPositionX(0), 0, tap_details);
  textfield_->OnGestureEvent(&tap);

  widget_->GetFocusManager()->ClearFocus();

  textfield_->RequestFocus();

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_OTHER,
            textfield_->GetFocusReason());
}

TEST_F(TextfieldTest, ChangeTextDirectionAndLayoutAlignmentTest) {
  InitTextfield();

  textfield_->ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(textfield_->GetTextDirection(),
            base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(textfield_->GetHorizontalAlignment(),
            gfx::HorizontalAlignment::ALIGN_RIGHT);

  textfield_->ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection::RIGHT_TO_LEFT);
  const base::string16& text = test_api_->GetRenderText()->GetDisplayText();
  base::i18n::TextDirection text_direction =
      base::i18n::GetFirstStrongCharacterDirection(text);
  EXPECT_EQ(textfield_->GetTextDirection(), text_direction);
  EXPECT_EQ(textfield_->GetHorizontalAlignment(),
            gfx::HorizontalAlignment::ALIGN_RIGHT);

  textfield_->ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection::LEFT_TO_RIGHT);
  EXPECT_EQ(textfield_->GetTextDirection(),
            base::i18n::TextDirection::LEFT_TO_RIGHT);
  EXPECT_EQ(textfield_->GetHorizontalAlignment(),
            gfx::HorizontalAlignment::ALIGN_LEFT);
}

TEST_F(TextfieldTest, TextChangedCallbackTest) {
  InitTextfield();

  bool text_changed = false;
  auto subscription = textfield_->AddTextChangedCallback(base::BindRepeating(
      [](bool* text_changed) { *text_changed = true; }, &text_changed));

  textfield_->SetText(ASCIIToUTF16("abc"));
  EXPECT_TRUE(text_changed);

  text_changed = false;
  textfield_->AppendText(ASCIIToUTF16("def"));
  EXPECT_TRUE(text_changed);

  // Undo should still cause callback.
  text_changed = false;
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_TRUE(text_changed);

  text_changed = false;
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_TRUE(text_changed);
}

}  // namespace views
