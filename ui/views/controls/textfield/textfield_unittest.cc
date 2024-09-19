// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield_unittest.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/render_text_test_api.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/touch_selection/touch_selection_metrics.h"
#include "ui/views/accessibility/atomic_view_ax_tree_manager.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/accessibility/view_ax_platform_node_delegate.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield_model.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/fake_linux_ui.h"
#include "ui/linux/linux_ui.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/aura/window.h"
#include "ui/wm/core/ime_util_chromeos.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/base/cocoa/secure_password_input.h"
#include "ui/base/cocoa/text_services_context_menu.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/events/ozone/layout/keyboard_layout_engine_test_utils.h"
#endif

namespace views::test {

const char16_t kHebrewLetterSamekh = 0x05E1;

// Convenience to make constructing a GestureEvent simpler.
ui::GestureEvent
CreateTestGestureEvent(int x, int y, const ui::GestureEventDetails& details) {
  return ui::GestureEvent(x, y, ui::EF_NONE, base::TimeTicks(), details);
}

// This controller will happily destroy the target field passed on
// construction when a key event is triggered.
class TextfieldDestroyerController : public TextfieldController {
 public:
  explicit TextfieldDestroyerController(Textfield* target) : target_(target) {
    target_->set_controller(this);
  }

  TextfieldDestroyerController(const TextfieldDestroyerController&) = delete;
  TextfieldDestroyerController& operator=(const TextfieldDestroyerController&) =
      delete;

  Textfield* target() { return target_.get(); }

  // TextfieldController:
  bool HandleKeyEvent(Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (target_)
      target_->OnBlur();
    target_.reset();
    return false;
  }

 private:
  std::unique_ptr<Textfield> target_;
};

// Class that focuses a textfield when it sees a KeyDown event.
class TextfieldFocuser : public View {
  METADATA_HEADER(TextfieldFocuser, View)

 public:
  explicit TextfieldFocuser(Textfield* textfield) : textfield_(*textfield) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }

  TextfieldFocuser(const TextfieldFocuser&) = delete;
  TextfieldFocuser& operator=(const TextfieldFocuser&) = delete;

  void set_consume(bool consume) { consume_ = consume; }

  // View:
  bool OnKeyPressed(const ui::KeyEvent& event) override {
    textfield_->RequestFocus();
    return consume_;
  }

 private:
  bool consume_ = true;
  const raw_ref<Textfield> textfield_;
};

BEGIN_METADATA(TextfieldFocuser)
END_METADATA

class MockInputMethod : public ui::InputMethodBase {
 public:
  MockInputMethod();

  MockInputMethod(const MockInputMethod&) = delete;
  MockInputMethod& operator=(const MockInputMethod&) = delete;

  ~MockInputMethod() override;

  // InputMethod:
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* key) override;
  void OnTextInputTypeChanged(ui::TextInputClient* client) override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void CancelComposition(const ui::TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;
  void SetVirtualKeyboardVisibilityIfEnabled(bool visibility) override {
    if (visibility)
      count_show_virtual_keyboard_++;
  }

#if BUILDFLAG(IS_WIN)
  bool OnUntranslatedIMEMessage(
      const CHROME_MSG event,
      InputMethod::NativeEventResult* result) override {
    return false;
  }
  void OnInputLocaleChanged() override {}
  bool IsInputLocaleCJK() const override { return false; }
  void OnUrlChanged() override {}
#endif

  bool untranslated_ime_message_called() const {
    return untranslated_ime_message_called_;
  }
  bool text_input_type_changed() const { return text_input_type_changed_; }
  bool cancel_composition_called() const { return cancel_composition_called_; }
  int count_show_virtual_keyboard() const {
    return count_show_virtual_keyboard_;
  }

  // Clears all internal states and result.
  void Clear();

  void SetCompositionTextForNextKey(const ui::CompositionText& composition);
  void SetResultTextForNextKey(const std::u16string& result);

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
  std::u16string result_text_;

  // Record call state of corresponding methods. They will be set to false
  // automatically before dispatching a key event.
  bool untranslated_ime_message_called_ = false;
  bool text_input_type_changed_ = false;
  bool cancel_composition_called_ = false;

  int count_show_virtual_keyboard_ = 0;
};

MockInputMethod::MockInputMethod() : ui::InputMethodBase(nullptr) {}

MockInputMethod::~MockInputMethod() = default;

ui::EventDispatchDetails MockInputMethod::DispatchKeyEvent(ui::KeyEvent* key) {
// On Mac, emulate InputMethodMac behavior for character events. Composition
// still needs to be mocked, since it's not possible to generate test events
// which trigger the appropriate NSResponder action messages for composition.
#if BUILDFLAG(IS_MAC)
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
    ui::KeyEvent mock_key(ui::EventType::kKeyPressed, ui::VKEY_PROCESSKEY,
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
        client->InsertText(result_text_,
                           ui::TextInputClient::InsertTextCursorBehavior::
                               kMoveCursorAfterText);
      if (composition_.text.length())
        client->SetCompositionText(composition_);
      else
        client->ClearCompositionText();
    } else if (key->type() == ui::EventType::kKeyPressed) {
      char16_t ch = key->GetCharacter();
      if (ch)
        client->InsertChar(*key);
    }
  }

  ClearComposition();

  return dispatch_details;
}

void MockInputMethod::OnTextInputTypeChanged(ui::TextInputClient* client) {
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

void MockInputMethod::SetResultTextForNextKey(const std::u16string& result) {
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
  METADATA_HEADER(TestTextfield, views::Textfield)

 public:
  TestTextfield() = default;

  TestTextfield(const TestTextfield&) = delete;
  TestTextfield& operator=(const TestTextfield&) = delete;

  ~TestTextfield() override = default;

  // ui::TextInputClient:
  void InsertChar(const ui::KeyEvent& e) override {
    views::Textfield::InsertChar(e);
#if BUILDFLAG(IS_MAC)
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
    accessibility_events_.push_back(event_type);
  }

  std::vector<ax::mojom::Event> GetAccessibilityEventsOfTypes(
      const std::vector<ax::mojom::Event>& event_types) {
    std::vector<ax::mojom::Event> filtered_events;
    for (const auto& event : accessibility_events_) {
      if (std::find(event_types.begin(), event_types.end(), event) !=
          event_types.end()) {
        filtered_events.push_back(event);
      }
    }
    return filtered_events;
  }

  void ClearAccessibilityEvents() { accessibility_events_.clear(); }

 private:
  // views::View:
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
    if (event->type() == ui::EventType::kKeyReleased) {
      EXPECT_FALSE(key_handled_);
    }
  }

  bool key_handled_ = false;
  bool key_received_ = false;
  int event_flags_ = 0;
  std::vector<ax::mojom::Event> accessibility_events_;

  base::WeakPtrFactory<TestTextfield> weak_ptr_factory_{this};
};

BEGIN_METADATA(TestTextfield)
END_METADATA

TextfieldTest::TextfieldTest() {
  ui::SetUpInputMethodForTesting(new MockInputMethod());
}

TextfieldTest::~TextfieldTest() = default;

void TextfieldTest::SetUp() {
  // OS clipboard is a global resource, which causes flakiness when unit tests
  // run in parallel. So, use a per-instance test clipboard.
  ui::Clipboard::SetClipboardForCurrentThread(
      std::make_unique<ui::TestClipboard>());
  ViewsTestBase::SetUp();

#if BUILDFLAG(IS_OZONE)
  // Setting up the keyboard layout engine depends on the implementation and may
  // be asynchronous.  We ensure that it is ready to use so that tests could
  // handle key events properly.
  ui::WaitUntilLayoutEngineIsReadyForTest();
#endif
}

void TextfieldTest::TearDown() {
  textfield_ = nullptr;
  event_target_ = nullptr;
  if (widget_)
    widget_->Close();
  // Clear kill buffer used for "Yank" text editing command so that no state
  // persists between tests.
  TextfieldModel::ClearKillBuffer();
  ViewsTestBase::TearDown();
}

ui::ClipboardBuffer TextfieldTest::GetAndResetCopiedToClipboard() {
  return std::exchange(copied_to_clipboard_, ui::ClipboardBuffer::kMaxValue);
}

std::u16string TextfieldTest::GetClipboardText(
    ui::ClipboardBuffer clipboard_buffer) {
  std::u16string text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      clipboard_buffer, /* data_dst = */ nullptr, &text);
  return text;
}

void TextfieldTest::SetClipboardText(ui::ClipboardBuffer clipboard_buffer,
                                     const std::u16string& text) {
  ui::ScopedClipboardWriter(clipboard_buffer).WriteText(text);
}

void TextfieldTest::ContentsChanged(Textfield* sender,
                                    const std::u16string& new_contents) {
  // Paste calls TextfieldController::ContentsChanged() explicitly even if the
  // paste action did not change the content. So |new_contents| may match
  // |last_contents_|. For more info, see http://crbug.com/79002
  last_contents_ = new_contents;
}

void TextfieldTest::OnBeforeUserAction(Textfield* sender) {
  ++on_before_user_action_;
}

void TextfieldTest::OnAfterUserAction(Textfield* sender) {
  ++on_after_user_action_;
}

void TextfieldTest::OnAfterCutOrCopy(ui::ClipboardBuffer clipboard_type) {
  copied_to_clipboard_ = clipboard_type;
}

void TextfieldTest::InitTextfield(int count) {
  ASSERT_FALSE(textfield_);
  textfield_ = PrepareTextfields(count, std::make_unique<TestTextfield>(),
                                 gfx::Rect(100, 100, 200, 200));
}

void TextfieldTest::PrepareTextfieldsInternal(int count,
                                              Textfield* textfield,
                                              View* container,
                                              gfx::Rect bounds) {
  input_method()->SetImeKeyEventDispatcher(
      test::WidgetTest::GetImeKeyEventDispatcherForWidget(widget_.get()));

  textfield->set_controller(this);
  textfield->SetBoundsRect(bounds);
  textfield->SetID(1);

  for (int i = 1; i < count; ++i) {
    Textfield* child = container->AddChildView(std::make_unique<Textfield>());
    child->SetID(i + 1);
  }

  TextfieldTestApi(textfield).model()->ClearEditHistory();

  // Since the window type is activatable, showing the widget will also
  // activate it. Calling Activate directly is insufficient, since that does
  // not also _focus_ an aura::Window (i.e. using the FocusClient). Both the
  // widget and the textfield must have focus to properly handle input.
  widget_->Show();
  textfield->RequestFocus();

  event_generator_ =
      std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_.get()));
  event_generator_->set_target(ui::test::EventGenerator::Target::WINDOW);
  event_target_ = textfield;
}

ui::MenuModel* TextfieldTest::GetContextMenuModel() {
  GetTextfieldTestApi().UpdateContextMenu();
  return GetTextfieldTestApi().context_menu_contents();
}

bool TextfieldTest::TestingNativeMac() const {
#if BUILDFLAG(IS_MAC)
  return true;
#else
  return false;
#endif
}

bool TextfieldTest::TestingNativeCrOs() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void TextfieldTest::SendKeyPress(ui::KeyboardCode key_code, int flags) {
  event_generator_->PressKey(key_code, flags);
}

void TextfieldTest::SendKeyEvent(ui::KeyboardCode key_code,
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

  int flags =
      (shift ? ui::EF_SHIFT_DOWN : 0) | (control ? ui::EF_CONTROL_DOWN : 0) |
      (alt ? ui::EF_ALT_DOWN : 0) | (command ? ui::EF_COMMAND_DOWN : 0) |
      (caps_lock ? ui::EF_CAPS_LOCK_ON : 0);

  SendKeyPress(key_code, flags);
}

void TextfieldTest::SendKeyEvent(ui::KeyboardCode key_code,
                                 bool shift,
                                 bool control_or_command) {
  SendKeyEvent(key_code, false, shift, control_or_command, false);
}

void TextfieldTest::SendKeyEvent(ui::KeyboardCode key_code) {
  SendKeyEvent(key_code, false, false);
}

void TextfieldTest::SendKeyEvent(char16_t ch) {
  SendKeyEvent(ch, ui::EF_NONE, false);
}

void TextfieldTest::SendKeyEvent(char16_t ch, int flags) {
  SendKeyEvent(ch, flags, false);
}

void TextfieldTest::SendKeyEvent(char16_t ch, int flags, bool from_vk) {
  if (ch < 0x80) {
    ui::KeyboardCode code =
        ch == ' ' ? ui::VKEY_SPACE
                  : static_cast<ui::KeyboardCode>(ui::VKEY_A + ch - 'a');
    SendKeyPress(code, flags);
  } else {
    // For unicode characters, assume they come from IME rather than the
    // keyboard. So they are dispatched directly to the input method. But on
    // Mac, key events don't pass through InputMethod. Hence they are
    // dispatched regularly.
    ui::KeyEvent event = ui::KeyEvent::FromCharacter(ch, ui::VKEY_UNKNOWN,
                                                     ui::DomCode::NONE, flags);
    if (from_vk) {
      ui::Event::Properties properties;
      properties[ui::kPropertyFromVK] =
          std::vector<uint8_t>(ui::kPropertyFromVKSize);
      event.SetProperties(properties);
    }
#if BUILDFLAG(IS_MAC)
    event_generator_->Dispatch(&event);
#else
    input_method()->DispatchKeyEvent(&event);
#endif
  }
}

void TextfieldTest::DispatchMockInputMethodKeyEvent() {
  // Send a key to trigger MockInputMethod::DispatchKeyEvent(). Note the
  // specific VKEY isn't used (MockInputMethod will mock a ui::VKEY_PROCESSKEY
  // whenever it has a test composition). However, on Mac, it can't be a letter
  // (e.g. VKEY_A) since all native character events on Mac are unicode events
  // and don't have a meaningful ui::KeyEvent that would trigger
  // DispatchKeyEvent(). It also can't be VKEY_ENTER, since those key events may
  // need to be suppressed when interacting with real system IME.
  SendKeyEvent(ui::VKEY_INSERT);
}

// Sends a platform-specific move (and select) to the logical start of line.
// Eg. this should move (and select) to the right end of line for RTL text.
void TextfieldTest::SendHomeEvent(bool shift) {
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
void TextfieldTest::SendEndEvent(bool shift) {
  if (TestingNativeMac()) {
    SendKeyEvent(ui::VKEY_DOWN, shift, true);  // Cmd+Down.
    return;
  }
  SendKeyEvent(ui::VKEY_END, shift, false);
}

// Sends {delete, move, select} word {forward, backward}.
void TextfieldTest::SendWordEvent(ui::KeyboardCode key, bool shift) {
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
void TextfieldTest::SendAlternateCut() {
  if (TestingNativeMac())
    SendKeyEvent(ui::VKEY_X, false, true);
  else
    SendKeyEvent(ui::VKEY_DELETE, true, false);
}

// Sends Ctrl+Insert if supported, otherwise Cmd+C again.
void TextfieldTest::SendAlternateCopy() {
  if (TestingNativeMac())
    SendKeyEvent(ui::VKEY_C, false, true);
  else
    SendKeyEvent(ui::VKEY_INSERT, false, true);
}

// Sends Shift+Insert if supported, otherwise Cmd+V again.
void TextfieldTest::SendAlternatePaste() {
  if (TestingNativeMac())
    SendKeyEvent(ui::VKEY_V, false, true);
  else
    SendKeyEvent(ui::VKEY_INSERT, true, false);
}

View* TextfieldTest::GetFocusedView() {
  return widget_->GetFocusManager()->GetFocusedView();
}

int TextfieldTest::GetCursorPositionX(int cursor_pos) {
  return GetTextfieldTestApi()
      .GetRenderText()
      ->GetCursorBounds(gfx::SelectionModel(cursor_pos, gfx::CURSOR_FORWARD),
                        false)
      .x();
}

int TextfieldTest::GetCursorYForTesting() {
  return GetTextfieldTestApi().GetRenderText()->GetLineOffset(0).y() + 1;
}

gfx::Rect TextfieldTest::GetCursorBounds() {
  return GetTextfieldTestApi().GetRenderText()->GetUpdatedCursorBounds();
}

// Gets the cursor bounds of |sel|.
gfx::Rect TextfieldTest::GetCursorBounds(const gfx::SelectionModel& sel) {
  return GetTextfieldTestApi().GetRenderText()->GetCursorBounds(sel, true);
}

gfx::Rect TextfieldTest::GetDisplayRect() {
  return GetTextfieldTestApi().GetRenderText()->display_rect();
}

gfx::Rect TextfieldTest::GetCursorViewRect() {
  return GetTextfieldTestApi().GetCursorViewRect();
}

// Performs a mouse click on the point whose x-axis is |bound|'s x plus
// |x_offset| and y-axis is in the middle of |bound|'s vertical range.
void TextfieldTest::MouseClick(const gfx::Rect bound, int x_offset) {
  gfx::Point point(bound.x() + x_offset, bound.y() + bound.height() / 2);
  ui::MouseEvent click(ui::EventType::kMousePressed, point, point,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  event_target_->OnMousePressed(click);
  ui::MouseEvent release(ui::EventType::kMouseReleased, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  event_target_->OnMouseReleased(release);
}

// This is to avoid double/triple click.
void TextfieldTest::NonClientMouseClick() {
  ui::MouseEvent click(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(),
                       int{ui::EF_LEFT_MOUSE_BUTTON} | ui::EF_IS_NON_CLIENT,
                       ui::EF_LEFT_MOUSE_BUTTON);
  event_target_->OnMousePressed(click);
  ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(),
                         int{ui::EF_LEFT_MOUSE_BUTTON} | ui::EF_IS_NON_CLIENT,
                         ui::EF_LEFT_MOUSE_BUTTON);
  event_target_->OnMouseReleased(release);
}

void TextfieldTest::VerifyTextfieldContextMenuContents(
    bool textfield_has_selection,
    bool can_undo,
    ui::MenuModel* menu) {
  const auto& text = textfield_->GetText();
  const bool is_all_selected =
      !text.empty() && textfield_->GetSelectedRange().length() == text.length();

  int menu_index = 0;

#if BUILDFLAG(IS_MAC)
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
  EXPECT_EQ(textfield_has_selection, menu->IsEnabledAt(menu_index++ /* CUT */));
  EXPECT_EQ(textfield_has_selection,
            menu->IsEnabledAt(menu_index++ /* COPY */));
  EXPECT_NE(GetClipboardText(ui::ClipboardBuffer::kCopyPaste).empty(),
            menu->IsEnabledAt(menu_index++ /* PASTE */));
  EXPECT_EQ(textfield_has_selection,
            menu->IsEnabledAt(menu_index++ /* DELETE */));
  EXPECT_TRUE(menu->IsEnabledAt(menu_index++ /* Separator */));
  EXPECT_EQ(!is_all_selected, menu->IsEnabledAt(menu_index++ /* SELECT ALL */));
}

void TextfieldTest::PressMouseButton(ui::EventFlags mouse_button_flags) {
  ui::MouseEvent press(ui::EventType::kMousePressed, mouse_position_,
                       mouse_position_, ui::EventTimeForNow(),
                       mouse_button_flags, mouse_button_flags);
  event_target_->OnMousePressed(press);
}

void TextfieldTest::ReleaseMouseButton(ui::EventFlags mouse_button_flags) {
  ui::MouseEvent release(ui::EventType::kMouseReleased, mouse_position_,
                         mouse_position_, ui::EventTimeForNow(),
                         mouse_button_flags, mouse_button_flags);
  event_target_->OnMouseReleased(release);
}

void TextfieldTest::PressLeftMouseButton() {
  PressMouseButton(ui::EF_LEFT_MOUSE_BUTTON);
}

void TextfieldTest::ReleaseLeftMouseButton() {
  ReleaseMouseButton(ui::EF_LEFT_MOUSE_BUTTON);
}

void TextfieldTest::ClickLeftMouseButton() {
  PressLeftMouseButton();
  ReleaseLeftMouseButton();
}

void TextfieldTest::ClickRightMouseButton() {
  PressMouseButton(ui::EF_RIGHT_MOUSE_BUTTON);
  ReleaseMouseButton(ui::EF_RIGHT_MOUSE_BUTTON);
}

void TextfieldTest::DragMouseTo(const gfx::Point& where) {
  mouse_position_ = where;
  ui::MouseEvent drag(ui::EventType::kMouseDragged, where, where,
                      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  event_target_->OnMouseDragged(drag);
}

void TextfieldTest::MoveMouseTo(const gfx::Point& where) {
  mouse_position_ = where;
}

// Taps on the textfield.
void TextfieldTest::TapAtCursor(ui::EventPointerType pointer_type) {
  ui::GestureEventDetails tap_down_details(ui::EventType::kGestureTapDown);
  tap_down_details.set_primary_pointer_type(pointer_type);
  ui::GestureEvent tap_down =
      CreateTestGestureEvent(GetCursorPositionX(0), 0, tap_down_details);
  textfield_->OnGestureEvent(&tap_down);

  ui::GestureEventDetails tap_up_details(ui::EventType::kGestureTap);
  tap_up_details.set_primary_pointer_type(pointer_type);
  ui::GestureEvent tap_up =
      CreateTestGestureEvent(GetCursorPositionX(0), 0, tap_up_details);
  textfield_->OnGestureEvent(&tap_up);
}

TextfieldTestApi TextfieldTest::GetTextfieldTestApi() {
  return TextfieldTestApi(textfield_);
}

MockInputMethod* TextfieldTest::input_method() {
  return static_cast<MockInputMethod*>(widget_->GetInputMethod());
}

TextfieldModel* TextfieldTest::model() {
  return GetTextfieldTestApi().model();
}

TEST_F(TextfieldTest, ModelChangesTest) {
  InitTextfield();

  // TextfieldController::ContentsChanged() shouldn't be called when changing
  // text programmatically.
  last_contents_.clear();
  textfield_->SetText(u"this is");
  EXPECT_EQ(u"this is", model()->text());
  EXPECT_EQ(u"this is", textfield_->GetText());
  EXPECT_TRUE(last_contents_.empty());

  textfield_->AppendText(u" a test");
  EXPECT_EQ(u"this is a test", model()->text());
  EXPECT_EQ(u"this is a test", textfield_->GetText());
  EXPECT_TRUE(last_contents_.empty());

  EXPECT_EQ(std::u16string(), textfield_->GetSelectedText());
  textfield_->SelectAll(false);
  EXPECT_EQ(u"this is a test", textfield_->GetSelectedText());
  EXPECT_TRUE(last_contents_.empty());

  textfield_->SetTextWithoutCaretBoundsChangeNotification(u"another test", 3);
  EXPECT_EQ(u"another test", model()->text());
  EXPECT_EQ(u"another test", textfield_->GetText());
  EXPECT_EQ(textfield_->GetCursorPosition(), 3u);
  EXPECT_TRUE(last_contents_.empty());
}

TEST_F(TextfieldTest, Scroll) {
  InitTextfield();

  // Size the textfield wide enough to hold 10 characters.
  gfx::test::RenderTextTestApi render_text_test_api(
      GetTextfieldTestApi().GetRenderText());
  constexpr int kGlyphWidth = 10;
  render_text_test_api.SetGlyphWidth(kGlyphWidth);
  constexpr int kCursorWidth = 1;
  GetTextfieldTestApi().GetRenderText()->SetDisplayRect(
      gfx::Rect(kGlyphWidth * 10 + kCursorWidth, 20));
  textfield_->SetTextWithoutCaretBoundsChangeNotification(
      u"0123456789_123456789_123456789", 0);
  GetTextfieldTestApi().SetDisplayOffsetX(0);

  // Empty Scroll() call should have no effect.
  textfield_->Scroll({});
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), 0);

  // Selected range should scroll cursor into view.
  textfield_->SetSelectedRange({0, 20});
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), -100);

  // Selected range should override new cursor position.
  GetTextfieldTestApi().SetDisplayOffsetX(0);
  textfield_->SetTextWithoutCaretBoundsChangeNotification(
      u"0123456789_123456789_123456789", 30);
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), -100);

  // Scroll positions should affect scroll.
  textfield_->SetSelectedRange(gfx::Range());
  GetTextfieldTestApi().SetDisplayOffsetX(0);
  textfield_->Scroll({30});
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), -200);

  // Should scroll right no more than necessary.
  GetTextfieldTestApi().SetDisplayOffsetX(0);
  textfield_->Scroll({15});
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), -50);

  // Should scroll left no more than necessary.
  GetTextfieldTestApi().SetDisplayOffsetX(-200);  // Scroll all the way right.
  textfield_->Scroll({15});
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), -150);

  // Should not scroll if position is already in view.
  GetTextfieldTestApi().SetDisplayOffsetX(
      -100);  // Scroll the middle 10 chars into view.
  textfield_->Scroll({15});
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), -100);

  // With multiple scroll positions, the Last scroll position takes priority.
  GetTextfieldTestApi().SetDisplayOffsetX(0);
  textfield_->Scroll({30, 0});
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), 0);
  textfield_->Scroll({30, 0, 20});
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), -100);

  // With multiple scroll positions, the previous scroll positions should be
  // scrolled to anyways.
  GetTextfieldTestApi().SetDisplayOffsetX(0);
  textfield_->Scroll({30, 20});
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), -200);

  // Only the selection end should affect scrolling.
  GetTextfieldTestApi().SetDisplayOffsetX(0);
  textfield_->Scroll({20});
  textfield_->SetSelectedRange({30, 20});
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), -100);
}

TEST_F(TextfieldTest, ScrollUpdatesScrollXAccessibilityAttribute) {
  InitTextfield();
  // Size the textfield wide enough to hold 10 characters.
  gfx::test::RenderTextTestApi render_text_test_api(
      GetTextfieldTestApi().GetRenderText());
  constexpr int kGlyphWidth = 10;
  render_text_test_api.SetGlyphWidth(kGlyphWidth);
  constexpr int kCursorWidth = 1;
  GetTextfieldTestApi().GetRenderText()->SetDisplayRect(
      gfx::Rect(kGlyphWidth * 10 + kCursorWidth, 20));
  textfield_->SetTextWithoutCaretBoundsChangeNotification(
      u"0123456789_123456789_123456789", 0);
  GetTextfieldTestApi().SetDisplayOffsetX(0);

  ui::AXNodeData textfield_node_data;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(
      &textfield_node_data);
  int scroll_x =
      textfield_node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), scroll_x);

  textfield_->SetSelectedRange({0, 20});
  textfield_->Scroll({20});
  textfield_node_data = ui::AXNodeData();
  textfield_->GetViewAccessibility().GetAccessibleNodeData(
      &textfield_node_data);
  EXPECT_EQ(
      GetTextfieldTestApi().GetDisplayOffsetX(),
      textfield_node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollX));
  EXPECT_NE(scroll_x, textfield_node_data.GetIntAttribute(
                          ax::mojom::IntAttribute::kScrollX));
}

TEST_F(TextfieldTest,
       SetTextWithoutCaretBoundsChangeNotification_ModelEditHistory) {
  InitTextfield();

  // The cursor and selected range should reflect the selected range.
  textfield_->SetTextWithoutCaretBoundsChangeNotification(
      u"0123456789_123456789_123456789", 20);
  textfield_->SetSelectedRange({10, 15});
  EXPECT_EQ(textfield_->GetCursorPosition(), 15u);
  EXPECT_EQ(textfield_->GetSelectedRange(), gfx::Range(10, 15));

  // After undo, the cursor and selected range should reflect the state prior to
  // the edit.
  textfield_->InsertOrReplaceText(u"xyz");               // 2nd edit
  SendKeyEvent(ui::VKEY_Z, false, true);                 // Undo 2nd edit
  EXPECT_EQ(textfield_->GetCursorPosition(), 15u);
  EXPECT_EQ(textfield_->GetSelectedRange(), gfx::Range(10, 15));

  // After redo, the cursor and selected range should reflect the
  // |cursor_position| parameter.
  SendKeyEvent(ui::VKEY_Z, false, true);  // Undo 2nd edit
  SendKeyEvent(ui::VKEY_Z, false, true);  // Undo 1st edit
  SendKeyEvent(ui::VKEY_Z, true, true);   // Redo 1st edit
  EXPECT_EQ(textfield_->GetCursorPosition(), 20u);
  EXPECT_EQ(textfield_->GetSelectedRange(), gfx::Range(20, 20));

  // After undo, the cursor and selected range should reflect the state prior to
  // the edit, even if that differs than the state after the current (1st) edit.
  textfield_->InsertOrReplaceText(u"xyz");               // (2')nd edit
  SendKeyEvent(ui::VKEY_Z, false, true);                 // Undo (2')nd edit
  EXPECT_EQ(textfield_->GetCursorPosition(), 20u);
  EXPECT_EQ(textfield_->GetSelectedRange(), gfx::Range(20, 20));
}

TEST_F(TextfieldTest, KeyTest) {
  InitTextfield();
  // Event flags:  key,    alt,   shift, ctrl,  caps-lock.
  SendKeyEvent(ui::VKEY_T, false, true, false, false);
  SendKeyEvent(ui::VKEY_E, false, false, false, false);
  SendKeyEvent(ui::VKEY_X, false, true, false, true);
  SendKeyEvent(ui::VKEY_T, false, false, false, true);
  SendKeyEvent(ui::VKEY_1, false, true, false, false);
  SendKeyEvent(ui::VKEY_1, false, false, false, false);
  SendKeyEvent(ui::VKEY_1, false, true, false, true);
  SendKeyEvent(ui::VKEY_1, false, false, false, true);

  // On Mac, Caps+Shift remains uppercase.
  if (TestingNativeMac())
    EXPECT_EQ(u"TeXT!1!1", textfield_->GetText());
  else
    EXPECT_EQ(u"TexT!1!1", textfield_->GetText());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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

  EXPECT_EQ(
      u"\x0448\x044C"
      u"im",
      textfield_->GetText());
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_KeysWithModifiersTest KeysWithModifiersTest
#else
// TODO(crbug.com/41274325): Implement keyboard layout changing for other
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
    EXPECT_EQ(u"TeTEx34", textfield_->GetText());
  else if (TestingNativeMac())
    EXPECT_EQ(u"TheTxE134", textfield_->GetText());
  else
    EXPECT_EQ(u"TeTEx234", textfield_->GetText());
}

TEST_F(TextfieldTest, ControlAndSelectTest) {
  // Insert a test string in a textfield.
  InitTextfield();
  textfield_->SetText(u"one two three");
  SendHomeEvent(false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);

  EXPECT_EQ(u"one", textfield_->GetSelectedText());

  // Test word select.
  SendWordEvent(ui::VKEY_RIGHT, true);
#if BUILDFLAG(IS_WIN)  // Windows breaks on word starts and includes spaces.
  EXPECT_EQ(u"one ", textfield_->GetSelectedText());
  SendWordEvent(ui::VKEY_RIGHT, true);
  EXPECT_EQ(u"one two ", textfield_->GetSelectedText());
#else  // Non-Windows breaks on word ends and does NOT include spaces.
  EXPECT_EQ(u"one two", textfield_->GetSelectedText());
#endif
  SendWordEvent(ui::VKEY_RIGHT, true);
  EXPECT_EQ(u"one two three", textfield_->GetSelectedText());
  SendWordEvent(ui::VKEY_LEFT, true);
  EXPECT_EQ(u"one two ", textfield_->GetSelectedText());
  SendWordEvent(ui::VKEY_LEFT, true);
  EXPECT_EQ(u"one ", textfield_->GetSelectedText());

  // Replace the selected text.
  SendKeyEvent(ui::VKEY_Z, true, false);
  SendKeyEvent(ui::VKEY_E, true, false);
  SendKeyEvent(ui::VKEY_R, true, false);
  SendKeyEvent(ui::VKEY_O, true, false);
  SendKeyEvent(ui::VKEY_SPACE, false, false);
  EXPECT_EQ(u"ZERO two three", textfield_->GetText());

  SendEndEvent(true);
  EXPECT_EQ(u"two three", textfield_->GetSelectedText());
  SendHomeEvent(true);

// On Mac, the existing selection should be extended.
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(u"ZERO two three", textfield_->GetSelectedText());
#else
  EXPECT_EQ(u"ZERO ", textfield_->GetSelectedText());
#endif
}

TEST_F(TextfieldTest, WordSelection) {
  InitTextfield();
  textfield_->SetText(u"12 34567 89");

  // Place the cursor after "5".
  textfield_->SetEditableSelectionRange(gfx::Range(6));

  // Select word towards right.
  SendWordEvent(ui::VKEY_RIGHT, true);
#if BUILDFLAG(IS_WIN)  // Select word right includes space/punctuation.
  EXPECT_EQ(u"67 ", textfield_->GetSelectedText());
#else  // Non-Win: select word right does NOT include space/punctuation.
  EXPECT_EQ(u"67", textfield_->GetSelectedText());
#endif
  SendWordEvent(ui::VKEY_RIGHT, true);
  EXPECT_EQ(u"67 89", textfield_->GetSelectedText());

  // Select word towards left.
  SendWordEvent(ui::VKEY_LEFT, true);
  EXPECT_EQ(u"67 ", textfield_->GetSelectedText());
  SendWordEvent(ui::VKEY_LEFT, true);

// On Mac, the selection should reduce to a caret when the selection direction
// changes for a word selection.
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(gfx::Range(6), textfield_->GetSelectedRange());
#else
  EXPECT_EQ(u"345", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(6, 3), textfield_->GetSelectedRange());
#endif

  SendWordEvent(ui::VKEY_LEFT, true);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(u"345", textfield_->GetSelectedText());
#else
  EXPECT_EQ(u"12 345", textfield_->GetSelectedText());
#endif
  EXPECT_TRUE(textfield_->GetSelectedRange().is_reversed());

  SendWordEvent(ui::VKEY_LEFT, true);
  EXPECT_EQ(u"12 345", textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, LineSelection) {
  InitTextfield();
  textfield_->SetText(u"12 34567 89");

  // Place the cursor after "5".
  textfield_->SetEditableSelectionRange(gfx::Range(6));

  // Select line towards right.
  SendEndEvent(true);
  EXPECT_EQ(u"67 89", textfield_->GetSelectedText());

  // Select line towards left. On Mac, the existing selection should be extended
  // to cover the whole line.
  SendHomeEvent(true);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(textfield_->GetText(), textfield_->GetSelectedText());
#else
  EXPECT_EQ(u"12 345", textfield_->GetSelectedText());
#endif
  EXPECT_TRUE(textfield_->GetSelectedRange().is_reversed());

  // Select line towards right.
  SendEndEvent(true);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(textfield_->GetText(), textfield_->GetSelectedText());
#else
  EXPECT_EQ(u"67 89", textfield_->GetSelectedText());
#endif
  EXPECT_FALSE(textfield_->GetSelectedRange().is_reversed());
}

TEST_F(TextfieldTest, MoveUpDownAndModifySelection) {
  InitTextfield();
  textfield_->SetText(u"12 34567 89");
  textfield_->SetEditableSelectionRange(gfx::Range(6));

  // Up/Down keys won't be handled except on Mac where they map to move
  // commands.
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_TRUE(textfield_->key_received());
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_EQ(gfx::Range(0), textfield_->GetSelectedRange());
#else
  EXPECT_FALSE(textfield_->key_handled());
#endif
  textfield_->clear();

  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(textfield_->key_received());
#if BUILDFLAG(IS_MAC)
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
#if BUILDFLAG(IS_MAC)
  textfield_->SetText(u"12 34567 89");
  textfield_->SetEditableSelectionRange(gfx::Range(6));

  EXPECT_TRUE(
      textfield_->IsTextEditCommandEnabled(ui::TextEditCommand::MOVE_PAGE_UP));
  EXPECT_TRUE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::MOVE_PAGE_DOWN));
  EXPECT_TRUE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION));
  EXPECT_TRUE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION));

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PAGE_UP);
  EXPECT_EQ(gfx::Range(0), textfield_->GetSelectedRange());

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PAGE_DOWN);
  EXPECT_EQ(gfx::Range(11), textfield_->GetSelectedRange());

  textfield_->SetEditableSelectionRange(gfx::Range(6));
  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION);
  EXPECT_EQ(gfx::Range(6, 0), textfield_->GetSelectedRange());

  GetTextfieldTestApi().ExecuteTextEditCommand(
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
  textfield_->SetText(u"12 34567 89");
  textfield_->SetEditableSelectionRange(gfx::Range(6));

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PARAGRAPH_FORWARD_AND_MODIFY_SELECTION);
  EXPECT_EQ(gfx::Range(6, 11), textfield_->GetSelectedRange());

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PARAGRAPH_BACKWARD_AND_MODIFY_SELECTION);
// On Mac, the selection should reduce to a caret when the selection direction
// is reversed for MOVE_PARAGRAPH_[FORWARD/BACKWARD]_AND_MODIFY_SELECTION.
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(gfx::Range(6), textfield_->GetSelectedRange());
#else
  EXPECT_EQ(gfx::Range(6, 0), textfield_->GetSelectedRange());
#endif

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PARAGRAPH_BACKWARD_AND_MODIFY_SELECTION);
  EXPECT_EQ(gfx::Range(6, 0), textfield_->GetSelectedRange());

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_PARAGRAPH_FORWARD_AND_MODIFY_SELECTION);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(gfx::Range(6), textfield_->GetSelectedRange());
#else
  EXPECT_EQ(gfx::Range(6, 11), textfield_->GetSelectedRange());
#endif
}

TEST_F(TextfieldTest, ModifySelectionWithMultipleSelections) {
  InitTextfield();
  textfield_->SetText(u"0123456 89");
  textfield_->SetSelectedRange(gfx::Range(3, 5));
  textfield_->AddSecondarySelectedRange(gfx::Range(8, 9));

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_RIGHT_AND_MODIFY_SELECTION);
  EXPECT_EQ(gfx::Range(3, 6), textfield_->GetSelectedRange());
  EXPECT_EQ(6U, textfield_->GetCursorPosition());
  EXPECT_EQ(0U, textfield_->GetSelectionModel().secondary_selections().size());
}

TEST_F(TextfieldTest, InsertionDeletionTest) {
  // Insert a test string in a textfield.
  InitTextfield();
  for (size_t i = 0; i < 10; ++i)
    SendKeyEvent(static_cast<ui::KeyboardCode>(ui::VKEY_A + i));
  EXPECT_EQ(u"abcdefghij", textfield_->GetText());

  // Test the delete and backspace keys.
  textfield_->SetSelectedRange(gfx::Range(5));
  for (size_t i = 0; i < 3; ++i)
    SendKeyEvent(ui::VKEY_BACK);
  EXPECT_EQ(u"abfghij", textfield_->GetText());
  for (size_t i = 0; i < 3; ++i)
    SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_EQ(u"abij", textfield_->GetText());

  // Select all and replace with "k".
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_K);
  EXPECT_EQ(u"k", textfield_->GetText());

  // Delete the previous word from cursor.
  bool shift = false;
  textfield_->SetText(u"one two three four");
  SendEndEvent(shift);
  SendWordEvent(ui::VKEY_BACK, shift);
  EXPECT_EQ(u"one two three ", textfield_->GetText());

  // Delete to a line break on Linux and ChromeOS, to a word break on Windows
  // and Mac.
  SendWordEvent(ui::VKEY_LEFT, shift);
  shift = true;
  SendWordEvent(ui::VKEY_BACK, shift);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(u"three ", textfield_->GetText());
#else
  EXPECT_EQ(u"one three ", textfield_->GetText());
#endif

  // Delete the next word from cursor.
  textfield_->SetText(u"one two three four");
  shift = false;
  SendHomeEvent(shift);
  SendWordEvent(ui::VKEY_DELETE, shift);
#if BUILDFLAG(IS_WIN)  // Delete word incldes space/punctuation.
  EXPECT_EQ(u"two three four", textfield_->GetText());
#else  // Non-Windows: delete word does NOT include space/punctuation.
  EXPECT_EQ(u" two three four", textfield_->GetText());
#endif
  // Delete to a line break on Linux and ChromeOS, to a word break on Windows
  // and Mac.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  shift = true;
  SendWordEvent(ui::VKEY_DELETE, shift);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(u" two", textfield_->GetText());
#elif BUILDFLAG(IS_WIN)
  EXPECT_EQ(u"two four", textfield_->GetText());
#else
  EXPECT_EQ(u" two four", textfield_->GetText());
#endif
}

// Test that deletion operations behave correctly with an active selection.
TEST_F(TextfieldTest, DeletionWithSelection) {
  struct TestCase {
    ui::KeyboardCode key;
    bool shift;
  };

  constexpr auto kTestCases = std::to_array<TestCase>({
      {ui::VKEY_BACK, false},
      {ui::VKEY_BACK, true},
      {ui::VKEY_DELETE, false},
      {ui::VKEY_DELETE, true},
  });

  InitTextfield();
  // [Ctrl] ([Alt] on Mac) + [Delete]/[Backspace] should delete the active
  // selection, regardless of [Shift].
  for (size_t i = 0; i < kTestCases.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    textfield_->SetText(u"one two three");
    textfield_->SetSelectedRange(gfx::Range(2, 6));
    // Make selection as - on|e tw|o three.
    SendWordEvent(kTestCases[i].key, kTestCases[i].shift);
    // Verify state is on|o three.
    EXPECT_EQ(u"ono three", textfield_->GetText());
    EXPECT_EQ(gfx::Range(2), textfield_->GetSelectedRange());
  }
}

// Test that deletion operations behave correctly with multiple selections.
TEST_F(TextfieldTest, DeletionWithMultipleSelections) {
  struct TestCase {
    ui::KeyboardCode key;
    bool shift;
  };

  constexpr auto kTestCases = std::to_array<TestCase>({
      {ui::VKEY_BACK, false},
      {ui::VKEY_BACK, true},
      {ui::VKEY_DELETE, false},
      {ui::VKEY_DELETE, true},
  });

  InitTextfield();
  // [Ctrl] ([Alt] on Mac) + [Delete]/[Backspace] should delete the active
  // selection, regardless of [Shift].
  for (size_t i = 0; i < kTestCases.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    textfield_->SetText(u"one two three");
    // Select: o[ne] [two] th[re]e
    textfield_->SetSelectedRange(gfx::Range(4, 7));
    textfield_->AddSecondarySelectedRange(gfx::Range(10, 12));
    textfield_->AddSecondarySelectedRange(gfx::Range(1, 3));
    SendWordEvent(kTestCases[i].key, kTestCases[i].shift);
    EXPECT_EQ(u"o  the", textfield_->GetText());
    EXPECT_EQ(gfx::Range(2), textfield_->GetSelectedRange());
    EXPECT_EQ(0U,
              textfield_->GetSelectionModel().secondary_selections().size());
  }
}

// Test deletions not covered by other tests with key events.
TEST_F(TextfieldTest, DeletionWithEditCommands) {
  struct TestCase {
    ui::TextEditCommand command;
    const char16_t* expected;
  };

  constexpr auto kTestCases = std::to_array<TestCase>({
      {ui::TextEditCommand::DELETE_TO_BEGINNING_OF_LINE, u"two three"},
      {ui::TextEditCommand::DELETE_TO_BEGINNING_OF_PARAGRAPH, u"two three"},
      {ui::TextEditCommand::DELETE_TO_END_OF_LINE, u"one "},
      {ui::TextEditCommand::DELETE_TO_END_OF_PARAGRAPH, u"one "},
  });

  InitTextfield();
  for (size_t i = 0; i < kTestCases.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    textfield_->SetText(u"one two three");
    textfield_->SetSelectedRange(gfx::Range(4));
    GetTextfieldTestApi().ExecuteTextEditCommand(kTestCases[i].command);
    EXPECT_EQ(kTestCases[i].expected, textfield_->GetText());
  }
}

TEST_F(TextfieldTest, PasswordTest) {
  InitTextfield();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());
  EXPECT_TRUE(textfield_->GetEnabled());
  EXPECT_TRUE(textfield_->IsFocusable());

  last_contents_.clear();
  textfield_->SetText(u"password");
  // Ensure GetText() and the callback returns the actual text instead of "*".
  EXPECT_EQ(u"password", textfield_->GetText());
  EXPECT_TRUE(last_contents_.empty());
  model()->SelectAll(false);
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, u"foo");

  // Cut and copy should be disabled.
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(Textfield::kCut));
  textfield_->ExecuteCommand(Textfield::kCut, 0);
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(Textfield::kCopy));
  textfield_->ExecuteCommand(Textfield::kCopy, 0);
  SendKeyEvent(ui::VKEY_C, false, true);
  SendAlternateCopy();
  EXPECT_EQ(u"foo", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(u"password", textfield_->GetText());
  // [Shift]+[Delete] should just delete without copying text to the clipboard.
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_DELETE, true, false);

  // Paste should work normally.
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(Textfield::kPaste));
  textfield_->ExecuteCommand(Textfield::kPaste, 0);
  SendKeyEvent(ui::VKEY_V, false, true);
  SendAlternatePaste();
  EXPECT_EQ(u"foo", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(u"foofoofoo", textfield_->GetText());
}

TEST_F(TextfieldTest, PasswordSelectWordTest) {
  InitTextfield();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  textfield_->SetText(u"password word test");

  // Select word command should be disabled.
  textfield_->SetEditableSelectionRange(gfx::Range(2));
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(Textfield::kSelectWord));
  textfield_->ExecuteCommand(Textfield::kPaste, 0);
  EXPECT_EQ(u"", textfield_->GetSelectedText());

  // Select word should select whole text instead of the nearest word.
  textfield_->SelectWord();
  EXPECT_EQ(u"password word test", textfield_->GetSelectedText());
}

// Check that text insertion works appropriately for password and read-only
// textfields.
TEST_F(TextfieldTest, TextInputType_InsertionTest) {
  InitTextfield();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, textfield_->GetTextInputType());

  SendKeyEvent(ui::VKEY_A);
  EXPECT_FALSE(textfield_->GetPasswordCharRevealIndex().has_value());
  SendKeyEvent(kHebrewLetterSamekh, ui::EF_NONE, true /* from_vk */);
#if !BUILDFLAG(IS_MAC)
  // Don't verifies the password character reveal on MacOS, because on MacOS,
  // the text insertion is not done through TextInputClient::InsertChar().
  EXPECT_EQ(1u, textfield_->GetPasswordCharRevealIndex());
#endif
  SendKeyEvent(ui::VKEY_B);
  EXPECT_FALSE(textfield_->GetPasswordCharRevealIndex().has_value());

  EXPECT_EQ(
      u"a\x05E1"
      u"b",
      textfield_->GetText());

  textfield_->SetReadOnly(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, textfield_->GetTextInputType());
  SendKeyEvent(ui::VKEY_C);

  // No text should be inserted for read only textfields.
  EXPECT_EQ(
      u"a\x05E1"
      u"b",
      textfield_->GetText());
}

TEST_F(TextfieldTest, ShouldDoLearning) {
  InitTextfield();

  // Defaults to false.
  EXPECT_EQ(false, textfield_->ShouldDoLearning());

  // The value can be set.
  textfield_->SetShouldDoLearning(true);
  EXPECT_EQ(true, textfield_->ShouldDoLearning());
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

#if BUILDFLAG(IS_LINUX)
  // Install a TextEditKeyBindingsDelegateAuraLinux that does nothing.
  class TestDelegate : public ui::FakeLinuxUi {
   public:
    TestDelegate() = default;

    TestDelegate(const TestDelegate&) = delete;
    TestDelegate& operator=(const TestDelegate&) = delete;

    ~TestDelegate() override = default;

    bool GetTextEditCommandsForEvent(
        const ui::Event& event,
        int text_flags,
        std::vector<ui::TextEditCommandAuraLinux>* commands) override {
      return false;
    }
  };

  auto test_delegate = std::make_unique<TestDelegate>();
  auto* old_linux_ui = ui::LinuxUi::SetInstance(test_delegate.get());
#endif

  SendKeyEvent(ui::VKEY_A, false, false);
  EXPECT_EQ(u"a", textfield_->GetText());
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
  EXPECT_EQ(u"a", textfield_->GetText());
  textfield_->clear();

#if BUILDFLAG(IS_LINUX)
  ui::LinuxUi::SetInstance(old_linux_ui);
#endif
}

TEST_F(TextfieldTest, CursorMovement) {
  InitTextfield();

  // Test with trailing whitespace.
  textfield_->SetText(u"one two hre ");

  // Send the cursor at the end.
  SendKeyEvent(ui::VKEY_END);

  // Ctrl+Left should move the cursor just before the last word.
  const bool shift = false;
  SendWordEvent(ui::VKEY_LEFT, shift);
  SendKeyEvent(ui::VKEY_T);
  EXPECT_EQ(u"one two thre ", textfield_->GetText());
  EXPECT_EQ(u"one two thre ", last_contents_);

#if BUILDFLAG(IS_WIN)  // Move right by word includes space/punctuation.
  // Ctrl+Right should move the cursor to the end of the last word.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  SendKeyEvent(ui::VKEY_E);
  EXPECT_EQ(u"one two thre e", textfield_->GetText());
  EXPECT_EQ(u"one two thre e", last_contents_);

  // Ctrl+Right again should not move the cursor, because
  // it is aleady at the end.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_EQ(u"one two thre ", textfield_->GetText());
  EXPECT_EQ(u"one two thre ", last_contents_);
#else  // Non-Windows: move right by word does NOT include space/punctuation.
  // Ctrl+Right should move the cursor to the end of the last word.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  SendKeyEvent(ui::VKEY_E);
  EXPECT_EQ(u"one two three ", textfield_->GetText());
  EXPECT_EQ(u"one two three ", last_contents_);

  // Ctrl+Right again should move the cursor to the end.
  SendWordEvent(ui::VKEY_RIGHT, shift);
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_EQ(u"one two three", textfield_->GetText());
  EXPECT_EQ(u"one two three", last_contents_);
#endif
  // Test with leading whitespace.
  textfield_->SetText(u" ne two");

  // Send the cursor at the beginning.
  SendHomeEvent(shift);

  // Ctrl+Right, then Ctrl+Left should move the cursor to the beginning of the
  // first word.
  SendWordEvent(ui::VKEY_RIGHT, shift);
#if BUILDFLAG(IS_WIN)  // Windows breaks on word start, move further to pass
                       // "ne".
  SendWordEvent(ui::VKEY_RIGHT, shift);
#endif
  SendWordEvent(ui::VKEY_LEFT, shift);
  SendKeyEvent(ui::VKEY_O);
  EXPECT_EQ(u" one two", textfield_->GetText());
  EXPECT_EQ(u" one two", last_contents_);

  // Ctrl+Left to move the cursor to the beginning of the first word.
  SendWordEvent(ui::VKEY_LEFT, shift);
  // Ctrl+Left again should move the cursor back to the very beginning.
  SendWordEvent(ui::VKEY_LEFT, shift);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_EQ(u"one two", textfield_->GetText());
  EXPECT_EQ(u"one two", last_contents_);
}

TEST_F(TextfieldTest, CursorMovementWithMultipleSelections) {
  InitTextfield();
  textfield_->SetText(u"012 456 890 234 678");
  //                                    [p]     [s]
  textfield_->SetSelectedRange({4, 7});
  textfield_->AddSecondarySelectedRange({12, 15});

  GetTextfieldTestApi().ExecuteTextEditCommand(ui::TextEditCommand::MOVE_LEFT);
  EXPECT_EQ(gfx::Range(4, 4), textfield_->GetSelectedRange());
  EXPECT_EQ(0U, textfield_->GetSelectionModel().secondary_selections().size());

  textfield_->SetSelectedRange({4, 7});
  textfield_->AddSecondarySelectedRange({12, 15});

  GetTextfieldTestApi().ExecuteTextEditCommand(ui::TextEditCommand::MOVE_RIGHT);
  EXPECT_EQ(gfx::Range(7, 7), textfield_->GetSelectedRange());
  EXPECT_EQ(0U, textfield_->GetSelectionModel().secondary_selections().size());
}

TEST_F(TextfieldTest, ShouldShowCursor) {
  InitTextfield();
  textfield_->SetText(u"word1 word2");

  // should show cursor when there's no primary selection
  textfield_->SetSelectedRange({4, 4});
  EXPECT_TRUE(GetTextfieldTestApi().ShouldShowCursor());
  textfield_->AddSecondarySelectedRange({1, 3});
  EXPECT_TRUE(GetTextfieldTestApi().ShouldShowCursor());

  // should not show cursor when there's a primary selection
  textfield_->SetSelectedRange({4, 7});
  EXPECT_FALSE(GetTextfieldTestApi().ShouldShowCursor());
  textfield_->AddSecondarySelectedRange({1, 3});
  EXPECT_FALSE(GetTextfieldTestApi().ShouldShowCursor());
}

#if BUILDFLAG(IS_MAC)
TEST_F(TextfieldTest, MacCursorAlphaTest) {
  InitTextfield();

  const int cursor_y = GetCursorYForTesting();
  MoveMouseTo(gfx::Point(GetCursorPositionX(0), cursor_y));
  ClickRightMouseButton();
  EXPECT_TRUE(textfield_->HasFocus());

  const float kOpaque = 1.0;
  EXPECT_FLOAT_EQ(kOpaque, GetTextfieldTestApi().CursorLayerOpacity());

  GetTextfieldTestApi().FlashCursor();

  const float kAlmostTransparent = 1.0 / 255.0;
  EXPECT_FLOAT_EQ(kAlmostTransparent,
                  GetTextfieldTestApi().CursorLayerOpacity());

  GetTextfieldTestApi().FlashCursor();

  EXPECT_FLOAT_EQ(kOpaque, GetTextfieldTestApi().CursorLayerOpacity());

  const float kTransparent = 0.0;
  GetTextfieldTestApi().SetCursorLayerOpacity(kTransparent);
  ASSERT_FLOAT_EQ(kTransparent, GetTextfieldTestApi().CursorLayerOpacity());

  GetTextfieldTestApi().UpdateCursorVisibility();
  EXPECT_FLOAT_EQ(kOpaque, GetTextfieldTestApi().CursorLayerOpacity());
}
#endif

TEST_F(TextfieldTest, FocusTraversalTest) {
  InitTextfield(3);
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
  textfield_->SetText(u"hello world");
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
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, u"Test");
  VerifyTextfieldContextMenuContents(true, true, GetContextMenuModel());
}

TEST_F(TextfieldTest, DoubleAndTripleClickTest) {
  InitTextfield();
  textfield_->SetText(u"hello world");

  // Test for double click.
  MoveMouseTo(gfx::Point(0, GetCursorYForTesting()));
  ClickLeftMouseButton();
  EXPECT_TRUE(textfield_->GetSelectedText().empty());
  ClickLeftMouseButton();
  EXPECT_EQ(u"hello", textfield_->GetSelectedText());

  // Test for triple click.
  ClickLeftMouseButton();
  EXPECT_EQ(u"hello world", textfield_->GetSelectedText());

  // Another click should reset back to double click.
  ClickLeftMouseButton();
  EXPECT_EQ(u"hello", textfield_->GetSelectedText());
}

// Tests text selection behavior on a right click.
TEST_F(TextfieldTest, SelectionOnRightClick) {
  InitTextfield();
  textfield_->SetText(u"hello world");

  // Verify right clicking within the selection does not alter the selection.
  textfield_->SetSelectedRange(gfx::Range(1, 5));
  EXPECT_EQ(u"ello", textfield_->GetSelectedText());
  const int cursor_y = GetCursorYForTesting();
  MoveMouseTo(gfx::Point(GetCursorPositionX(3), cursor_y));
  ClickRightMouseButton();
  EXPECT_EQ(u"ello", textfield_->GetSelectedText());

  // Verify right clicking outside the selection, selects the word under the
  // cursor on platforms where this is expected.
  MoveMouseTo(gfx::Point(GetCursorPositionX(8), cursor_y));
  const char16_t* expected_right_click_word =
      PlatformStyle::kSelectWordOnRightClick ? u"world" : u"ello";
  ClickRightMouseButton();
  EXPECT_EQ(expected_right_click_word, textfield_->GetSelectedText());

  // Verify right clicking inside an unfocused textfield selects all the text on
  // platforms where this is expected. Else the older selection is retained.
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(textfield_->HasFocus());
  MoveMouseTo(gfx::Point(GetCursorPositionX(0), cursor_y));
  ClickRightMouseButton();
  EXPECT_TRUE(textfield_->HasFocus());
  const char16_t* expected_right_click_unfocused =
      PlatformStyle::kSelectAllOnRightClickWhenUnfocused
          ? u"hello world"
          : expected_right_click_word;
  EXPECT_EQ(expected_right_click_unfocused, textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, DragToSelect) {
  InitTextfield();
  textfield_->SetText(u"hello world");
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
  std::u16string text_left = textfield_->GetSelectedText();
  EXPECT_EQ(u"hello", text_left);

  // Check that dragging right selects the rest of the string.
  DragMouseTo(end_point);
  std::u16string text_right = textfield_->GetSelectedText();
  EXPECT_EQ(u" world", text_right);

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
  textfield_->SetText(u"hello world");
  const std::u16string expected_left =
      gfx::RenderText::kDragToEndIfOutsideVerticalBounds ? u"hello" : u"lo";
  const std::u16string expected_right =
      gfx::RenderText::kDragToEndIfOutsideVerticalBounds ? u" world" : u" w";
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

#if BUILDFLAG(IS_WIN)
TEST_F(TextfieldTest, DragAndDrop_AcceptDrop) {
  InitTextfield();
  textfield_->SetText(u"hello world");

  ui::OSExchangeData data;
  std::u16string string(u"string ");
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
  ui::DropTargetEvent drop(
      data, drop_point, drop_point,
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE,
            textfield_->OnDragUpdated(drop));
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  auto cb = textfield_->GetDropCallback(drop);
  std::move(cb).Run(drop, output_drag_op, /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ(ui::mojom::DragOperation::kCopy, output_drag_op);
  EXPECT_EQ(u"hello string world", textfield_->GetText());

  // Ensure that textfields do not accept non-OSExchangeData::STRING types.
  ui::OSExchangeData bad_data;
  bad_data.SetFilename(base::FilePath(FILE_PATH_LITERAL("x")));
  ui::ClipboardFormatType fmt = ui::ClipboardFormatType::BitmapType();
  bad_data.SetPickledData(fmt, base::Pickle());
  bad_data.SetFileContents(base::FilePath(L"x"), "x");
  bad_data.SetHtml(std::u16string(u"x"), GURL("x.org"));
  ui::DownloadFileInfo download(base::FilePath(), nullptr);
  bad_data.provider().SetDownloadFileInfo(&download);
  EXPECT_FALSE(textfield_->CanDrop(bad_data));
}
#endif

TEST_F(TextfieldTest, DragAndDrop_InitiateDrag) {
  InitTextfield();
  textfield_->SetText(u"hello string world");

  // Ensure the textfield will provide selected text for drag data.
  ui::OSExchangeData data;
  const gfx::Range kStringRange(6, 12);
  textfield_->SetSelectedRange(kStringRange);
  const gfx::Point kStringPoint(GetCursorPositionX(9), GetCursorYForTesting());
  textfield_->WriteDragDataForView(nullptr, kStringPoint, &data);
  std::optional<std::u16string> string = data.GetString();
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
  textfield_->SetText(u"hello world");
  const int cursor_y = GetCursorYForTesting();

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
  std::optional<std::u16string> string = data.GetString();
  EXPECT_EQ(textfield_->GetSelectedText(), string);
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(format_types.empty());

  // Drop "ello" after "w".
  const gfx::PointF kDropPoint(GetCursorPositionX(7), cursor_y);
  EXPECT_TRUE(textfield_->CanDrop(data));
  ui::DropTargetEvent drop_a(data, kDropPoint, kDropPoint, operations);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop_a));
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  auto cb = textfield_->GetDropCallback(drop_a);
  std::move(cb).Run(drop_a, output_drag_op, /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ(ui::mojom::DragOperation::kMove, output_drag_op);
  EXPECT_EQ(u"h welloorld", textfield_->GetText());
  textfield_->OnDragDone();

  // Undo/Redo the drag&drop change.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"hello world", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"hello world", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"h welloorld", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"h welloorld", textfield_->GetText());
}

TEST_F(TextfieldTest, DragAndDrop_ToTheLeft) {
  InitTextfield();
  textfield_->SetText(u"hello world");
  const int cursor_y = GetCursorYForTesting();

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
  std::optional<std::u16string> string = data.GetString();
  EXPECT_EQ(textfield_->GetSelectedText(), string);
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(format_types.empty());

  // Drop " worl" after "h".
  EXPECT_TRUE(textfield_->CanDrop(data));
  gfx::PointF drop_point(GetCursorPositionX(1), cursor_y);
  ui::DropTargetEvent drop_a(data, drop_point, drop_point, operations);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop_a));
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  auto cb = textfield_->GetDropCallback(drop_a);
  std::move(cb).Run(drop_a, output_drag_op, /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ(ui::mojom::DragOperation::kMove, output_drag_op);
  EXPECT_EQ(u"h worlellod", textfield_->GetText());
  textfield_->OnDragDone();

  // Undo/Redo the drag&drop change.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"hello world", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"hello world", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"h worlellod", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"h worlellod", textfield_->GetText());
}

TEST_F(TextfieldTest, DropCallbackCancelled) {
  InitTextfield();
  textfield_->SetText(u"hello world");
  const int cursor_y = GetCursorYForTesting();

  ui::OSExchangeData data;
  int formats = 0;
  int operations = 0;
  std::set<ui::ClipboardFormatType> format_types;

  // Start dragging "hello".
  textfield_->SetSelectedRange(gfx::Range(0, 5));
  gfx::Point point(GetCursorPositionX(3), cursor_y);
  MoveMouseTo(point);
  PressLeftMouseButton();
  EXPECT_TRUE(textfield_->CanStartDragForView(textfield_, point, gfx::Point()));
  operations = textfield_->GetDragOperationsForView(textfield_, point);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
            operations);
  textfield_->WriteDragDataForView(nullptr, point, &data);
  std::optional<std::u16string> string = data.GetString();
  EXPECT_EQ(textfield_->GetSelectedText(), string);
  EXPECT_TRUE(textfield_->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(format_types.empty());

  // Drop "hello" after "d". The drop callback should do nothing because
  // `textfield_` is mutated before the callback is run.
  EXPECT_TRUE(textfield_->CanDrop(data));
  gfx::PointF drop_point(GetCursorPositionX(11), cursor_y);
  ui::DropTargetEvent drop_a(data, drop_point, drop_point, operations);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_->OnDragUpdated(drop_a));
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  auto cb = textfield_->GetDropCallback(drop_a);
  textfield_->AppendText(u"new text");
  std::move(cb).Run(drop_a, output_drag_op, /*drag_image_layer_owner=*/nullptr);
  EXPECT_EQ(ui::mojom::DragOperation::kNone, output_drag_op);
  EXPECT_EQ(u"hello worldnew text", textfield_->GetText());
}

TEST_F(TextfieldTest, DragAndDrop_Canceled) {
  InitTextfield();
  textfield_->SetText(u"hello world");
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
  EXPECT_EQ(u"hello world", textfield_->GetText());
}

TEST_F(TextfieldTest, ReadOnlyTest) {
  InitTextfield();
  textfield_->SetText(u"read only");
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
  EXPECT_EQ(u"read ", textfield_->GetSelectedText());
  textfield_->SelectAll(false);
  EXPECT_EQ(u"read only", textfield_->GetSelectedText());

  // Cut should be disabled.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, u"Test");
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(Textfield::kCut));
  textfield_->ExecuteCommand(Textfield::kCut, 0);
  SendKeyEvent(ui::VKEY_X, false, true);
  SendAlternateCut();
  EXPECT_EQ(u"Test", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(u"read only", textfield_->GetText());

  // Paste should be disabled.
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(Textfield::kPaste));
  textfield_->ExecuteCommand(Textfield::kPaste, 0);
  SendKeyEvent(ui::VKEY_V, false, true);
  SendAlternatePaste();
  EXPECT_EQ(u"read only", textfield_->GetText());

  // Copy should work normally.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, u"Test");
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(Textfield::kCopy));
  textfield_->ExecuteCommand(Textfield::kCopy, 0);
  EXPECT_EQ(u"read only", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, u"Test");
  SendKeyEvent(ui::VKEY_C, false, true);
  EXPECT_EQ(u"read only", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, u"Test");
  SendAlternateCopy();
  EXPECT_EQ(u"read only", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));

  // SetText should work even in read only mode.
  textfield_->SetText(u" four five six ");
  EXPECT_EQ(u" four five six ", textfield_->GetText());

  textfield_->SelectAll(false);
  EXPECT_EQ(u" four five six ", textfield_->GetSelectedText());

  // Text field is unmodifiable and selection shouldn't change.
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_EQ(u" four five six ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_EQ(u" four five six ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_T);
  EXPECT_EQ(u" four five six ", textfield_->GetSelectedText());
}

TEST_F(TextfieldTest, TextInputClientTest) {
  InitTextfield();
  ui::TextInputClient* client = textfield_;
  EXPECT_TRUE(client);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, client->GetTextInputType());

  textfield_->SetText(u"0123456789");
  gfx::Range range;
  EXPECT_TRUE(client->GetTextRange(&range));
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(10U, range.end());

  EXPECT_TRUE(client->SetEditableSelectionRange(gfx::Range(1, 4)));
  EXPECT_TRUE(client->GetEditableSelectionRange(&range));
  EXPECT_EQ(gfx::Range(1, 4), range);

  std::u16string substring;
  EXPECT_TRUE(client->GetTextFromRange(range, &substring));
  EXPECT_EQ(u"123", substring);

#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(client->DeleteRange(range));
  EXPECT_EQ(u"0456789", textfield_->GetText());
#endif

  ui::CompositionText composition;
  composition.text = u"321";
  // Set composition through input method.
  input_method()->Clear();
  input_method()->SetCompositionTextForNextKey(composition);
  textfield_->clear();

  on_before_user_action_ = on_after_user_action_ = 0;
  DispatchMockInputMethodKeyEvent();

  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  EXPECT_TRUE(client->HasCompositionText());
  EXPECT_TRUE(client->GetCompositionTextRange(&range));
  EXPECT_EQ(u"0321456789", textfield_->GetText());
  EXPECT_EQ(gfx::Range(1, 4), range);
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  input_method()->SetResultTextForNextKey(u"123");
  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  DispatchMockInputMethodKeyEvent();
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  EXPECT_FALSE(client->HasCompositionText());
  EXPECT_FALSE(input_method()->cancel_composition_called());
  EXPECT_EQ(u"0123456789", textfield_->GetText());
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  input_method()->Clear();
  input_method()->SetCompositionTextForNextKey(composition);
  textfield_->clear();
  DispatchMockInputMethodKeyEvent();
  EXPECT_TRUE(client->HasCompositionText());
  EXPECT_EQ(u"0123321456789", textfield_->GetText());

  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_FALSE(client->HasCompositionText());
  EXPECT_TRUE(input_method()->cancel_composition_called());
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_EQ(u"0123321456789", textfield_->GetText());
  EXPECT_EQ(8U, textfield_->GetCursorPosition());
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  textfield_->clear();
  textfield_->SetText(u"0123456789");
  EXPECT_TRUE(client->SetEditableSelectionRange(gfx::Range(5, 5)));
  client->ExtendSelectionAndDelete(4, 2);
  EXPECT_EQ(u"0789", textfield_->GetText());

  // On{Before,After}UserAction should be called by whatever user action
  // triggers clearing or setting a selection if appropriate.
  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  textfield_->ClearSelection();
  textfield_->SelectAll(false);
  EXPECT_EQ(0, on_before_user_action_);
  EXPECT_EQ(0, on_after_user_action_);

  input_method()->Clear();

  // Changing the Textfield to readonly shouldn't change the input client, since
  // it's still required for selections and clipboard copy.
  ui::TextInputClient* text_input_client = textfield_;
  EXPECT_TRUE(text_input_client);
  EXPECT_NE(ui::TEXT_INPUT_TYPE_NONE, text_input_client->GetTextInputType());
  textfield_->SetReadOnly(true);
  EXPECT_TRUE(input_method()->text_input_type_changed());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, text_input_client->GetTextInputType());

  input_method()->Clear();
  textfield_->SetReadOnly(false);
  EXPECT_TRUE(input_method()->text_input_type_changed());
  EXPECT_NE(ui::TEXT_INPUT_TYPE_NONE, text_input_client->GetTextInputType());

  input_method()->Clear();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_TRUE(input_method()->text_input_type_changed());
}

TEST_F(TextfieldTest, UndoRedoTest) {
  InitTextfield();
  SendKeyEvent(ui::VKEY_A);
  EXPECT_EQ(u"a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"a", textfield_->GetText());

  // AppendText
  textfield_->AppendText(u"b");
  last_contents_.clear();  // AppendText doesn't call ContentsChanged.
  EXPECT_EQ(u"ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"ab", textfield_->GetText());

  // SetText
  SendKeyEvent(ui::VKEY_C);
  // Undo'ing append moves the cursor to the end for now.
  // A no-op SetText won't add a new edit; see TextfieldModel::SetText.
  EXPECT_EQ(u"abc", textfield_->GetText());
  textfield_->SetText(u"abc");
  EXPECT_EQ(u"abc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"abc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"abc", textfield_->GetText());
  textfield_->SetText(u"123");
  textfield_->SetText(u"123");
  EXPECT_EQ(u"123", textfield_->GetText());
  SendKeyEvent(ui::VKEY_END, false, false);
  SendKeyEvent(ui::VKEY_4, false, false);
  EXPECT_EQ(u"1234", textfield_->GetText());
  last_contents_.clear();
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"123", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  // the insert edit "c" and set edit "123" are merged to single edit,
  // so text becomes "ab" after undo.
  EXPECT_EQ(u"ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"123", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"1234", textfield_->GetText());

  // Undoing to the same text shouldn't call ContentsChanged.
  SendKeyEvent(ui::VKEY_A, false, true);  // select all
  SendKeyEvent(ui::VKEY_A);
  EXPECT_EQ(u"a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  EXPECT_EQ(u"abc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"1234", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"abc", textfield_->GetText());

  // Delete/Backspace
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_EQ(u"ab", textfield_->GetText());
  SendHomeEvent(false);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_EQ(u"b", textfield_->GetText());
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_EQ(u"", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"b", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"abc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"ab", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"b", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"", textfield_->GetText());
}

// Most platforms support Ctrl+Y as an alternative to Ctrl+Shift+Z, but on Mac
// Ctrl+Y is bound to "Yank" and Cmd+Y is bound to "Show full history". So, on
// Mac, Cmd+Shift+Z is sent for the tests above and the Ctrl+Y test below is
// skipped.
#if !BUILDFLAG(IS_MAC)

// Test that Ctrl+Y works for Redo, as well as Ctrl+Shift+Z.
TEST_F(TextfieldTest, RedoWithCtrlY) {
  InitTextfield();
  SendKeyEvent(ui::VKEY_A);
  EXPECT_EQ(u"a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_EQ(u"a", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_EQ(u"", textfield_->GetText());
  SendKeyEvent(ui::VKEY_Z, true, true);
  EXPECT_EQ(u"a", textfield_->GetText());
}

#endif  // !BUILDFLAG(IS_MAC)

// Non-Mac platforms don't have a key binding for Yank. Since this test is only
// run on Mac, it uses some Mac specific key bindings.
#if BUILDFLAG(IS_MAC)

TEST_F(TextfieldTest, Yank) {
  InitTextfield(2);
  textfield_->SetText(u"abcdef");
  textfield_->SetSelectedRange(gfx::Range(2, 4));

  // Press Ctrl+Y to yank.
  SendKeyPress(ui::VKEY_Y, ui::EF_CONTROL_DOWN);

  // Initially the kill buffer should be empty. Hence yanking should delete the
  // selected text.
  EXPECT_EQ(u"abef", textfield_->GetText());
  EXPECT_EQ(gfx::Range(2), textfield_->GetSelectedRange());

  // Press Ctrl+K to delete to end of paragraph. This should place the deleted
  // text in the kill buffer.
  SendKeyPress(ui::VKEY_K, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(u"ab", textfield_->GetText());
  EXPECT_EQ(gfx::Range(2), textfield_->GetSelectedRange());

  // Yank twice.
  SendKeyPress(ui::VKEY_Y, ui::EF_CONTROL_DOWN);
  SendKeyPress(ui::VKEY_Y, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(u"abefef", textfield_->GetText());
  EXPECT_EQ(gfx::Range(6), textfield_->GetSelectedRange());

  // Verify pressing backspace does not modify the kill buffer.
  SendKeyEvent(ui::VKEY_BACK);
  SendKeyPress(ui::VKEY_Y, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(u"abefeef", textfield_->GetText());
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
  EXPECT_EQ(u"ef", textfield2->GetText());
  EXPECT_EQ(gfx::Range(2), textfield2->GetSelectedRange());

  // Verify deletion in a password textfield does not modify the kill buffer.
  textfield2->SetText(u"hello");
  textfield2->SetSelectedRange(gfx::Range(0));
  SendKeyPress(ui::VKEY_K, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(textfield2->GetText().empty());

  textfield_->RequestFocus();
  textfield_->SetSelectedRange(gfx::Range(0));
  SendKeyPress(ui::VKEY_Y, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(u"efabefeef", textfield_->GetText());
}

#endif  // BUILDFLAG(IS_MAC)

TEST_F(TextfieldTest, CutCopyPaste) {
  InitTextfield();
  // Ensure kCut cuts.
  textfield_->SetText(u"123");
  textfield_->SelectAll(false);
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(Textfield::kCut));
  textfield_->ExecuteCommand(Textfield::kCut, 0);
  EXPECT_EQ(u"123", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(u"", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[x] cuts and [Ctrl]+[Alt][x] does nothing.
  textfield_->SetText(u"456");
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_X, true, false, true, false);
  EXPECT_EQ(u"123", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(u"456", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_EQ(u"456", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(u"", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Ensure [Shift]+[Delete] cuts.
  textfield_->SetText(u"123");
  textfield_->SelectAll(false);
  SendAlternateCut();
  EXPECT_EQ(u"123", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(u"", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Reset clipboard text.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, u"");

  // Ensure [Shift]+[Delete] is a no-op in case there is no selection.
  textfield_->SetText(u"123");
  textfield_->SetSelectedRange(gfx::Range(0));
  SendAlternateCut();
  EXPECT_EQ(u"", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(u"123", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());

  // Ensure kCopy copies.
  textfield_->SetText(u"789");
  textfield_->SelectAll(false);
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(Textfield::kCopy));
  textfield_->ExecuteCommand(Textfield::kCopy, 0);
  EXPECT_EQ(u"789", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[c] copies and [Ctrl]+[Alt][c] does nothing.
  textfield_->SetText(u"012");
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_C, true, false, true, false);
  EXPECT_EQ(u"789", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_C, false, true);
  EXPECT_EQ(u"012", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Ensure [Ctrl]+[Insert] copies.
  textfield_->SetText(u"345");
  textfield_->SelectAll(false);
  SendAlternateCopy();
  EXPECT_EQ(u"345", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(u"345", textfield_->GetText());
  EXPECT_EQ(ui::ClipboardBuffer::kCopyPaste, GetAndResetCopiedToClipboard());

  // Ensure kPaste, [Ctrl]+[V], and [Shift]+[Insert] pastes;
  // also ensure that [Ctrl]+[Alt]+[V] does nothing.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, u"abc");
  textfield_->SetText(std::u16string());
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(Textfield::kPaste));
  textfield_->ExecuteCommand(Textfield::kPaste, 0);
  EXPECT_EQ(u"abc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_V, false, true);
  EXPECT_EQ(u"abcabc", textfield_->GetText());
  SendAlternatePaste();
  EXPECT_EQ(u"abcabcabc", textfield_->GetText());
  SendKeyEvent(ui::VKEY_V, true, false, true, false);
  EXPECT_EQ(u"abcabcabc", textfield_->GetText());

  // Ensure [Ctrl]+[Shift]+[Insert] is a no-op.
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_INSERT, true, true);
  EXPECT_EQ(u"abc", GetClipboardText(ui::ClipboardBuffer::kCopyPaste));
  EXPECT_EQ(u"abcabcabc", textfield_->GetText());
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
  EXPECT_EQ(u"o", textfield_->GetText());
  SendKeyEvent(ui::VKEY_V, false, true);  // Paste it.
  EXPECT_EQ(u"oo", textfield_->GetText());
  SendKeyEvent(ui::VKEY_H, false, false);  // Type "h".
  EXPECT_EQ(u"ooh", textfield_->GetText());
  SendKeyEvent(ui::VKEY_LEFT, true, false);  // Select "h".
  SendKeyEvent(ui::VKEY_X, false, true);     // Cut it.
  EXPECT_EQ(u"oo", textfield_->GetText());
}

TEST_F(TextfieldTest, SelectWordFromEmptySelection) {
  InitTextfield();
  textfield_->SetText(u"ab cde.123 4");

  // Place the cursor at the beginning of the text.
  textfield_->SetEditableSelectionRange(gfx::Range(0));
  textfield_->SelectWord();
  EXPECT_EQ(u"ab", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(0, 2), textfield_->GetSelectedRange());

  // Place the cursor after "c".
  textfield_->SetEditableSelectionRange(gfx::Range(4));
  textfield_->SelectWord();
  EXPECT_EQ(u"cde", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(3, 6), textfield_->GetSelectedRange());

  // Place the cursor after "2".
  textfield_->SetEditableSelectionRange(gfx::Range(9));
  textfield_->SelectWord();
  EXPECT_EQ(u"123", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(7, 10), textfield_->GetSelectedRange());

  // Place the cursor at the end of the text.
  textfield_->SetEditableSelectionRange(gfx::Range(12));
  textfield_->SelectWord();
  EXPECT_EQ(u"4", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(11, 12), textfield_->GetSelectedRange());
}

TEST_F(TextfieldTest, SelectWordFromNonEmptySelection) {
  InitTextfield();
  textfield_->SetText(u"ab cde.123 4");

  // Select "b".
  textfield_->SetEditableSelectionRange(gfx::Range(1, 2));
  textfield_->SelectWord();
  EXPECT_EQ(u"ab", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(0, 2), textfield_->GetSelectedRange());

  // Select "b c"
  textfield_->SetEditableSelectionRange(gfx::Range(1, 4));
  textfield_->SelectWord();
  EXPECT_EQ(u"ab cde", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(0, 6), textfield_->GetSelectedRange());

  // Select "e."
  textfield_->SetEditableSelectionRange(gfx::Range(5, 7));
  textfield_->SelectWord();
  EXPECT_EQ(u"cde.", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(3, 7), textfield_->GetSelectedRange());

  // Select "e.1"
  textfield_->SetEditableSelectionRange(gfx::Range(5, 8));
  textfield_->SelectWord();
  EXPECT_EQ(u"cde.123", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(3, 10), textfield_->GetSelectedRange());
}

TEST_F(TextfieldTest, SelectWordFromNonAlphaNumericFragment) {
  InitTextfield();
  textfield_->SetText(u"  HELLO  !!  WO     RLD");

  // Place the cursor within "  !!  ".
  textfield_->SetEditableSelectionRange(gfx::Range(8));
  textfield_->SelectWord();
  EXPECT_EQ(u"  !!  ", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(7, 13), textfield_->GetSelectedRange());

  textfield_->SetEditableSelectionRange(gfx::Range(10));
  textfield_->SelectWord();
  EXPECT_EQ(u"  !!  ", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(7, 13), textfield_->GetSelectedRange());
}

TEST_F(TextfieldTest, SelectWordFromWhitespaceFragment) {
  InitTextfield();
  textfield_->SetText(u"  HELLO  !!  WO     RLD");
  textfield_->SetEditableSelectionRange(gfx::Range(17));
  textfield_->SelectWord();
  EXPECT_EQ(u"     ", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(15, 20), textfield_->GetSelectedRange());
}

TEST_F(TextfieldTest, SelectCommands) {
  InitTextfield();
  textfield_->SetText(u"hello string world");

  // Select all and select word commands should both be enabled when there is no
  // selection.
  textfield_->SetEditableSelectionRange(gfx::Range(8));
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(Textfield::kSelectAll));
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(Textfield::kSelectWord));
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());

  // Select word at current position. Select word command should now be disabled
  // since there is already a selection.
  textfield_->ExecuteCommand(Textfield::kSelectWord, 0);
  EXPECT_EQ(u"string", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(6, 12), textfield_->GetSelectedRange());
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(Textfield::kSelectAll));
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(Textfield::kSelectWord));
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());

  // Select all text. Select all and select word commands should now both be
  // disabled.
  textfield_->ExecuteCommand(Textfield::kSelectAll, 0);
  EXPECT_EQ(u"hello string world", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(0, 18), textfield_->GetSelectedRange());
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(Textfield::kSelectAll));
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(Textfield::kSelectWord));
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());
}

// No touch on desktop Mac.
#if !BUILDFLAG(IS_MAC)
TEST_F(TextfieldTest, SelectCommandsFromTouchEvent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"hello string world");

  // Select all and select word commands should both be enabled when there is no
  // selection.
  textfield_->SetEditableSelectionRange(gfx::Range(8));
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(Textfield::kSelectAll));
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(Textfield::kSelectWord));
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());

  // Select word at current position. Select word command should now be disabled
  // since there is already a selection.
  textfield_->ExecuteCommand(Textfield::kSelectWord, ui::EF_FROM_TOUCH);
  EXPECT_EQ(u"string", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(6, 12), textfield_->GetSelectedRange());
  EXPECT_TRUE(textfield_->IsCommandIdEnabled(Textfield::kSelectAll));
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(Textfield::kSelectWord));
  EXPECT_TRUE(GetTextfieldTestApi().touch_selection_controller());

  // Select all text. Select all and select word commands should now both be
  // disabled.
  textfield_->ExecuteCommand(Textfield::kSelectAll, ui::EF_FROM_TOUCH);
  EXPECT_EQ(u"hello string world", textfield_->GetSelectedText());
  EXPECT_EQ(gfx::Range(0, 18), textfield_->GetSelectedRange());
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(Textfield::kSelectAll));
  EXPECT_FALSE(textfield_->IsCommandIdEnabled(Textfield::kSelectWord));
  EXPECT_TRUE(GetTextfieldTestApi().touch_selection_controller());
}
#endif

TEST_F(TextfieldTest, OvertypeMode) {
  InitTextfield();
  // Overtype mode should be disabled (no-op [Insert]).
  textfield_->SetText(u"2");
  const bool shift = false;
  SendHomeEvent(shift);
  // Note: On Mac, there is no insert key. Insert sends kVK_Help. Currently,
  // since there is no overtype on toolkit-views, the behavior happens to match.
  // However, there's no enable-overtype equivalent key combination on OSX.
  SendKeyEvent(ui::VKEY_INSERT);
  SendKeyEvent(ui::VKEY_1, false, false);
  EXPECT_EQ(u"12", textfield_->GetText());
}

TEST_F(TextfieldTest, TextCursorDisplayTest) {
  InitTextfield();
  // LTR-RTL string in LTR context.
  SendKeyEvent('a');
  EXPECT_EQ(u"a", textfield_->GetText());
  int x = GetCursorBounds().x();
  int prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(u"ab", textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
  prev_x = x;

  SendKeyEvent(0x05E1);
  EXPECT_EQ(u"ab\x05E1", textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  SendKeyEvent(0x05E2);
  EXPECT_EQ(u"ab\x05E1\x5E2", textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);

  // RTL-LTR string in LTR context.
  SendKeyEvent(0x05E1);
  EXPECT_EQ(u"\x05E1", textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_EQ(GetDisplayRect().x(), x);
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(u"\x05E1\x05E2", textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  SendKeyEvent('a');
  EXPECT_EQ(
      u"\x05E1\x5E2"
      u"a",
      textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(
      u"\x05E1\x5E2"
      u"ab",
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
  EXPECT_EQ(u"a", textfield_->GetText());
  int x = GetCursorBounds().x();
  EXPECT_EQ(GetDisplayRect().right() - 1, x);
  int prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(u"ab", textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));

  SendKeyEvent(0x05E1);
  EXPECT_EQ(u"ab\x05E1", textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(u"ab\x05E1\x5E2", textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);

  // RTL-LTR string in RTL context.
  SendKeyEvent(0x05E1);
  EXPECT_EQ(u"\x05E1", textfield_->GetText());
  x = GetCursorBounds().x();
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(u"\x05E1\x05E2", textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);
  prev_x = x;

  SendKeyEvent('a');
  EXPECT_EQ(
      u"\x05E1\x5E2"
      u"a",
      textfield_->GetText());
  x = GetCursorBounds().x();
  EXPECT_GE(1, std::abs(x - prev_x));
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(
      u"\x05E1\x5E2"
      u"ab",
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
  int text_cursor_position_prev = GetTextfieldTestApi().GetCursorViewRect().x();
  SendKeyEvent('a');
  SendKeyEvent('b');
  EXPECT_EQ(u"ab", textfield_->GetText());
  int text_cursor_position_new = GetTextfieldTestApi().GetCursorViewRect().x();
  // Text cursor stays at same place after inserting new characters in RTL mode.
  EXPECT_EQ(text_cursor_position_prev, text_cursor_position_new);

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(TextfieldTest, TextCursorPositionInLTRTest) {
  InitTextfield();

  // LTR-RTL string in LTR context.
  int text_cursor_position_prev = GetTextfieldTestApi().GetCursorViewRect().x();
  SendKeyEvent('a');
  SendKeyEvent('b');
  EXPECT_EQ(u"ab", textfield_->GetText());
  int text_cursor_position_new = GetTextfieldTestApi().GetCursorViewRect().x();
  // Text cursor moves to right after inserting new characters in LTR mode.
  EXPECT_LT(text_cursor_position_prev, text_cursor_position_new);
}

TEST_F(TextfieldTest, HitInsideTextAreaTest) {
  InitTextfield();
  textfield_->SetText(u"ab\x05E1\x5E2");
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
  constexpr auto cursor_pos_expected =
      std::to_array<size_t>({0, 1, 1, 2, 4, 3, 3, 2});

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
  textfield_->SetText(u"ab\x05E1\x5E2");

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
  textfield_->SetText(
      u"\x05E1\x5E2"
      u"ab");

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
  textfield_->SetText(
      u"\x05E1\x5E2"
      u"ab");
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
  textfield_->SetText(u"ab\x05E1\x5E2");
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

// TODO(https://crbug.com/361276581, https://crbug.com/361247468): Flakes on
// Fuschia cast Debug bots.
#if BUILDFLAG(IS_FUCHSIA) && !defined(NDEBUG)
#define MAYBE_OverflowTest DISABLED_OverflowTest
#define MAYBE_OverflowInRTLTest DISABLED_OverflowInRTLTest
#else
#define MAYBE_OverflowTest OverflowTest
#define MAYBE_OverflowInRTLTest OverflowInRTLTest
#endif
TEST_F(TextfieldTest, MAYBE_OverflowTest) {
  InitTextfield();

  std::u16string str;
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

TEST_F(TextfieldTest, MAYBE_OverflowInRTLTest) {
  std::string locale = base::i18n::GetConfiguredLocale();
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield();

  std::u16string str;
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

TEST_F(TextfieldTest, PasswordProtected) {
  InitTextfield();
  ui::AXNodeData data;

  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kProtected));

  data = ui::AXNodeData();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_NONE);
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(data.HasState(ax::mojom::State::kProtected));
}

TEST_F(TextfieldTest, CommitComposingTextTest) {
  InitTextfield();
  ui::CompositionText composition;
  composition.text = u"abc123";
  textfield_->SetCompositionText(composition);
  size_t composed_text_length =
      textfield_->ConfirmCompositionText(/* keep_selection */ false);

  EXPECT_EQ(composed_text_length, 6u);
}

TEST_F(TextfieldTest, CommitEmptyComposingTextTest) {
  InitTextfield();
  ui::CompositionText composition;
  composition.text = u"";
  textfield_->SetCompositionText(composition);
  size_t composed_text_length =
      textfield_->ConfirmCompositionText(/* keep_selection */ false);

  EXPECT_EQ(composed_text_length, 0u);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// SetCompositionFromExistingText is only available on Windows and Chrome OS.
TEST_F(TextfieldTest, SetCompositionFromExistingTextTest) {
  InitTextfield();
  textfield_->SetText(u"abc");

  textfield_->SetCompositionFromExistingText(gfx::Range(1, 3), {});

  gfx::Range actual_range;
  EXPECT_TRUE(textfield_->HasCompositionText());
  EXPECT_TRUE(textfield_->GetCompositionTextRange(&actual_range));
  EXPECT_EQ(actual_range, gfx::Range(1, 3));
}
#endif

TEST_F(TextfieldTest, GetCompositionCharacterBoundsTest) {
  InitTextfield();
  ui::CompositionText composition;
  composition.text = u"abc123";
  const uint32_t char_count = static_cast<uint32_t>(composition.text.length());

  // Compare the composition character bounds with surrounding cursor bounds.
  for (uint32_t i = 0; i < char_count; ++i) {
    composition.selection = gfx::Range(i);
    textfield_->SetCompositionText(composition);
    gfx::Point cursor_origin = GetCursorBounds().origin();
    views::View::ConvertPointToScreen(textfield_, &cursor_origin);

    composition.selection = gfx::Range(i + 1);
    textfield_->SetCompositionText(composition);
    gfx::Point next_cursor_bottom_left = GetCursorBounds().bottom_left();
    views::View::ConvertPointToScreen(textfield_, &next_cursor_bottom_left);

    gfx::Rect character;
    EXPECT_TRUE(textfield_->GetCompositionCharacterBounds(i, &character));
    EXPECT_EQ(character.origin(), cursor_origin) << " i=" << i;
    EXPECT_EQ(character.bottom_right(), next_cursor_bottom_left) << " i=" << i;
  }

  // Return false if the index is out of range.
  gfx::Rect rect;
  EXPECT_FALSE(textfield_->GetCompositionCharacterBounds(char_count, &rect));
  EXPECT_FALSE(
      textfield_->GetCompositionCharacterBounds(char_count + 1, &rect));
  EXPECT_FALSE(
      textfield_->GetCompositionCharacterBounds(char_count + 100, &rect));
}

TEST_F(TextfieldTest, GetCompositionCharacterBounds_ComplexText) {
  InitTextfield();

  constexpr auto kUtf16Chars = std::to_array<char16_t>({
      // U+0020 SPACE
      0x0020,
      // U+1F408 (CAT) as surrogate pair
      0xd83d,
      0xdc08,
      // U+5642 as Ideographic Variation Sequences
      0x5642,
      0xDB40,
      0xDD00,
      // U+260E (BLACK TELEPHONE) as Emoji Variation Sequences
      0x260E,
      0xFE0F,
      // U+0020 SPACE
      0x0020,
  });

  ui::CompositionText composition;
  composition.text.assign(kUtf16Chars.data(), kUtf16Chars.size());
  textfield_->SetCompositionText(composition);

  // Make sure GetCompositionCharacterBounds never fails for index.
  std::array<gfx::Rect, kUtf16Chars.size()> rects;
  for (uint32_t i = 0; i < kUtf16Chars.size(); ++i) {
    EXPECT_TRUE(textfield_->GetCompositionCharacterBounds(i, &rects[i]));
  }

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

  textfield_->SetText(u"abc def ghi");

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

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(TextfieldTest, SelectionClipboard) {
  InitTextfield();
  textfield_->SetText(u"0123");
  const int cursor_y = GetCursorYForTesting();
  gfx::Point point_1(GetCursorPositionX(1), cursor_y);
  gfx::Point point_2(GetCursorPositionX(2), cursor_y);
  gfx::Point point_3(GetCursorPositionX(3), cursor_y);
  gfx::Point point_4(GetCursorPositionX(4), cursor_y);

  // Text selected by the mouse should be placed on the selection clipboard.
  ui::MouseEvent press(ui::EventType::kMousePressed, point_1, point_1,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(press);
  ui::MouseEvent drag(ui::EventType::kMouseDragged, point_3, point_3,
                      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                      ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseDragged(drag);
  ui::MouseEvent release(ui::EventType::kMouseReleased, point_3, point_3,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release);
  EXPECT_EQ(gfx::Range(1, 3), textfield_->GetSelectedRange());
  EXPECT_EQ(u"12", GetClipboardText(ui::ClipboardBuffer::kSelection));

  // Select-all should update the selection clipboard.
  SendKeyEvent(ui::VKEY_A, false, true);
  EXPECT_EQ(gfx::Range(0, 4), textfield_->GetSelectedRange());
  EXPECT_EQ(u"0123", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());

  // Shift-click selection modifications should update the clipboard.
  NonClientMouseClick();
  ui::MouseEvent press_2(ui::EventType::kMousePressed, point_2, point_2,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  press_2.SetFlags(press_2.flags() | ui::EF_SHIFT_DOWN);
  textfield_->OnMousePressed(press_2);
  ui::MouseEvent release_2(ui::EventType::kMouseReleased, point_2, point_2,
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release_2);
  EXPECT_EQ(gfx::Range(0, 2), textfield_->GetSelectedRange());
  EXPECT_EQ(u"01", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());

  // Shift-Left/Right should update the selection clipboard.
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  EXPECT_EQ(u"012", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_LEFT, true, false);
  EXPECT_EQ(u"01", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());
  SendKeyEvent(ui::VKEY_RIGHT, true, true);
  EXPECT_EQ(u"0123", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());

  // Moving the cursor without a selection should not change the clipboard.
  SendKeyEvent(ui::VKEY_LEFT, false, false);
  EXPECT_EQ(gfx::Range(0, 0), textfield_->GetSelectedRange());
  EXPECT_EQ(u"0123", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());

  // Middle clicking should paste at the mouse (not cursor) location.
  // The cursor should be placed at the end of the pasted text.
  ui::MouseEvent middle(ui::EventType::kMousePressed, point_4, point_4,
                        ui::EventTimeForNow(), ui::EF_MIDDLE_MOUSE_BUTTON,
                        ui::EF_MIDDLE_MOUSE_BUTTON);
  textfield_->OnMousePressed(middle);
  EXPECT_EQ(u"01230123", textfield_->GetText());
  EXPECT_EQ(gfx::Range(8, 8), textfield_->GetSelectedRange());
  EXPECT_EQ(u"0123", GetClipboardText(ui::ClipboardBuffer::kSelection));

  // Middle clicking on an unfocused textfield should focus it and paste.
  textfield_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(textfield_->HasFocus());
  textfield_->OnMousePressed(middle);
  EXPECT_TRUE(textfield_->HasFocus());
  EXPECT_EQ(u"012301230123", textfield_->GetText());
  EXPECT_EQ(gfx::Range(8, 8), textfield_->GetSelectedRange());
  EXPECT_EQ(u"0123", GetClipboardText(ui::ClipboardBuffer::kSelection));

  // Middle clicking with an empty selection clipboard should still focus.
  SetClipboardText(ui::ClipboardBuffer::kSelection, std::u16string());
  textfield_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(textfield_->HasFocus());
  textfield_->OnMousePressed(middle);
  EXPECT_TRUE(textfield_->HasFocus());
  EXPECT_EQ(u"012301230123", textfield_->GetText());
  EXPECT_EQ(gfx::Range(4, 4), textfield_->GetSelectedRange());
  EXPECT_TRUE(GetClipboardText(ui::ClipboardBuffer::kSelection).empty());

  // Middle clicking in the selection should insert the selection clipboard
  // contents into the middle of the selection, and move the cursor to the end
  // of the pasted content.
  SetClipboardText(ui::ClipboardBuffer::kCopyPaste, u"foo");
  textfield_->SetSelectedRange(gfx::Range(2, 6));
  textfield_->OnMousePressed(middle);
  EXPECT_EQ(u"0123foo01230123", textfield_->GetText());
  EXPECT_EQ(gfx::Range(7, 7), textfield_->GetSelectedRange());
  EXPECT_EQ(u"foo", GetClipboardText(ui::ClipboardBuffer::kSelection));

  // Double and triple clicking should update the clipboard contents.
  textfield_->SetText(u"ab cd ef");
  gfx::Point word(GetCursorPositionX(4), cursor_y);
  ui::MouseEvent press_word(ui::EventType::kMousePressed, word, word,
                            ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                            ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(press_word);
  ui::MouseEvent release_word(ui::EventType::kMouseReleased, word, word,
                              ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                              ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMouseReleased(release_word);
  ui::MouseEvent double_click(ui::EventType::kMousePressed, word, word,
                              ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_DOUBLE_CLICK,
                              ui::EF_LEFT_MOUSE_BUTTON);
  textfield_->OnMousePressed(double_click);
  textfield_->OnMouseReleased(release_word);
  EXPECT_EQ(gfx::Range(3, 5), textfield_->GetSelectedRange());
  EXPECT_EQ(u"cd", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());
  textfield_->OnMousePressed(press_word);
  textfield_->OnMouseReleased(release_word);
  EXPECT_EQ(gfx::Range(0, 8), textfield_->GetSelectedRange());
  EXPECT_EQ(u"ab cd ef", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());

  // Selecting a range of text without any user interaction should not change
  // the clipboard content.
  textfield_->SetSelectedRange(gfx::Range(0, 3));
  EXPECT_EQ(u"ab ", textfield_->GetSelectedText());
  EXPECT_EQ(u"ab cd ef", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());

  SetClipboardText(ui::ClipboardBuffer::kSelection, u"other");
  textfield_->SelectAll(false);
  EXPECT_EQ(u"other", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());
}

// Verify that the selection clipboard is not updated for selections on a
// password textfield.
TEST_F(TextfieldTest, SelectionClipboard_Password) {
  InitTextfield(2);
  textfield_->SetText(u"abcd");

  // Select-all should update the selection clipboard for a non-password
  // textfield.
  SendKeyEvent(ui::VKEY_A, false, true);
  EXPECT_EQ(gfx::Range(0, 4), textfield_->GetSelectedRange());
  EXPECT_EQ(u"abcd", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kSelection, GetAndResetCopiedToClipboard());

  // Move focus to the next textfield.
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(2, GetFocusedView()->GetID());
  Textfield* textfield2 = static_cast<Textfield*>(GetFocusedView());

  // Select-all should not modify the selection clipboard for a password
  // textfield.
  textfield2->SetText(u"1234");
  textfield2->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  SendKeyEvent(ui::VKEY_A, false, true);
  EXPECT_EQ(gfx::Range(0, 4), textfield2->GetSelectedRange());
  EXPECT_EQ(u"abcd", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());

  // Shift-Left/Right should not modify the selection clipboard for a password
  // textfield.
  SendKeyEvent(ui::VKEY_LEFT, true, false);
  EXPECT_EQ(gfx::Range(0, 3), textfield2->GetSelectedRange());
  EXPECT_EQ(u"abcd", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());

  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  EXPECT_EQ(gfx::Range(0, 4), textfield2->GetSelectedRange());
  EXPECT_EQ(u"abcd", GetClipboardText(ui::ClipboardBuffer::kSelection));
  EXPECT_EQ(ui::ClipboardBuffer::kMaxValue, GetAndResetCopiedToClipboard());
}
#endif

// Long_Press gesture in Textfield can initiate a drag and drop now.
TEST_F(TextfieldTest, TestLongPressInitiatesDragDrop) {
  InitTextfield();
  textfield_->SetText(u"Hello string world");

  // Ensure the textfield will provide selected text for drag data.
  textfield_->SetSelectedRange(gfx::Range(6, 12));
  const gfx::Point kStringPoint(GetCursorPositionX(9), GetCursorYForTesting());

  // Enable touch-drag-drop to make long press effective.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);

  // Create a long press event in the selected region should start a drag.
  ui::GestureEvent long_press = CreateTestGestureEvent(
      kStringPoint.x(), kStringPoint.y(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  textfield_->OnGestureEvent(&long_press);
  EXPECT_TRUE(
      textfield_->CanStartDragForView(nullptr, kStringPoint, kStringPoint));
}

// No touch on desktop Mac.
#if !BUILDFLAG(IS_MAC)
TEST_F(TextfieldTest, ScrollToAdjustDisplayOffset) {
  InitTextfield();

  // Size the textfield wide enough to hold 10 characters.
  constexpr int kGlyphWidth = 10;
  gfx::test::RenderTextTestApi(GetTextfieldTestApi().GetRenderText())
      .SetGlyphWidth(kGlyphWidth);
  constexpr int kCursorWidth = 1;
  GetTextfieldTestApi().GetRenderText()->SetDisplayRect(
      gfx::Rect(kGlyphWidth * 10 + kCursorWidth, 20));
  textfield_->SetTextWithoutCaretBoundsChangeNotification(
      u"0123456789_123456789_123456789", 0);
  GetTextfieldTestApi().SetDisplayOffsetX(0);
  constexpr gfx::Range kSelectionRange(2, 7);
  textfield_->SetEditableSelectionRange(kSelectionRange);

  // Scroll starting at a vertical angle to adjust the display offset.
  constexpr int kDisplayOffsetXAdjustment = -30;
  const gfx::Point kScrollStart = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(5), GetCursorYForTesting()});
  const gfx::Point kScrollEnd =
      kScrollStart + gfx::Vector2d(kDisplayOffsetXAdjustment, 60);
  event_generator_->GestureScrollSequence(kScrollStart, kScrollEnd,
                                          /*duration=*/base::Milliseconds(50),
                                          /*steps=*/5);

  // Display offset should have updated without the selection changing.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, kSelectionRange);
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(),
            kDisplayOffsetXAdjustment);
}

TEST_F(TextfieldTest, TwoFingerScroll) {
  InitTextfield();

  // Size the textfield wide enough to hold 10 characters.
  constexpr int kGlyphWidth = 10;
  gfx::test::RenderTextTestApi(GetTextfieldTestApi().GetRenderText())
      .SetGlyphWidth(kGlyphWidth);
  constexpr int kCursorWidth = 1;
  GetTextfieldTestApi().GetRenderText()->SetDisplayRect(
      gfx::Rect(kGlyphWidth * 10 + kCursorWidth, 20));
  textfield_->SetTextWithoutCaretBoundsChangeNotification(
      u"0123456789_123456789_123456789", 0);
  GetTextfieldTestApi().SetDisplayOffsetX(0);
  constexpr gfx::Range kSelectionRange(2, 7);
  textfield_->SetEditableSelectionRange(kSelectionRange);

  // Scroll with two fingers to adjust the display offset.
  constexpr int kDisplayOffsetXAdjustment = -30;
  const gfx::Point kStart1 = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(5), GetCursorYForTesting()});
  const gfx::Point kStart2 = kStart1 + gfx::Vector2d(20, 0);
  const gfx::Point kStart[] = {kStart1, kStart2};
  event_generator_->GestureMultiFingerScroll(
      /*count=*/2, kStart,
      /*event_separation_time_ms=*/50,
      /*steps=*/5, /*move_x=*/kDisplayOffsetXAdjustment,
      /*move_y=*/0);

  // Display offset should have updated without the selection changing.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, kSelectionRange);
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(),
            kDisplayOffsetXAdjustment);
}

TEST_F(TextfieldTest, ScrollToPlaceCursor) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"Hello string world");

  // Scroll in a horizontal direction to move the cursor.
  constexpr int kCursorStartPos = 2;
  constexpr int kCursorEndPos = 15;
  const gfx::Point kScrollStart = views::View::ConvertPointToScreen(
      textfield_,
      {GetCursorPositionX(kCursorStartPos), GetCursorYForTesting()});
  const gfx::Point kScrollEnd = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(kCursorEndPos), GetCursorYForTesting()});
  event_generator_->GestureScrollSequence(kScrollStart, kScrollEnd,
                                          /*duration=*/base::Milliseconds(50),
                                          /*steps=*/5);

  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(kCursorEndPos));
}

TEST_F(TextfieldTest, ScrollToPlaceCursorAdjustsDisplayOffset) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();

  // Size the textfield wide enough to hold 10 characters.
  gfx::test::RenderTextTestApi render_text_test_api(
      GetTextfieldTestApi().GetRenderText());
  constexpr int kGlyphWidth = 10;
  render_text_test_api.SetGlyphWidth(kGlyphWidth);
  constexpr int kCursorWidth = 1;
  GetTextfieldTestApi().GetRenderText()->SetDisplayRect(
      gfx::Rect(kGlyphWidth * 10 + kCursorWidth, 20));
  textfield_->SetTextWithoutCaretBoundsChangeNotification(
      u"0123456789_123456789_123456789", 0);
  GetTextfieldTestApi().SetDisplayOffsetX(0);

  // Scroll in a horizontal direction to move the cursor.
  const gfx::Point kScrollStart = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(2), GetCursorYForTesting()});
  const gfx::Point kScrollEnd = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(30), GetCursorYForTesting()});
  event_generator_->GestureScrollSequence(kScrollStart, kScrollEnd,
                                          /*duration=*/base::Milliseconds(50),
                                          /*steps=*/5);

  // Cursor should have moved and display should be offset so that the cursor is
  // visible in the textfield.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(30));
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), -200);
}

TEST_F(TextfieldTest, TwoFingerScrollUpdate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();

  // Size the textfield wide enough to hold 10 characters.
  gfx::test::RenderTextTestApi render_text_test_api(
      GetTextfieldTestApi().GetRenderText());
  constexpr int kGlyphWidth = 10;
  render_text_test_api.SetGlyphWidth(kGlyphWidth);
  constexpr int kCursorWidth = 1;
  GetTextfieldTestApi().GetRenderText()->SetDisplayRect(
      gfx::Rect(kGlyphWidth * 10 + kCursorWidth, 20));
  textfield_->SetTextWithoutCaretBoundsChangeNotification(
      u"0123456789_123456789_123456789", 0);
  GetTextfieldTestApi().SetDisplayOffsetX(0);
  textfield_->SetEditableSelectionRange(gfx::Range(2, 7));

  // Perform a scroll which starts with one finger then adds another finger
  // after a delay.
  const gfx::Point kStart1 = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(8), GetCursorYForTesting()});
  const gfx::Point kStart2 = kStart1 + gfx::Vector2d(20, 0);
  const gfx::Point kStart[] = {kStart1, kStart2};
  constexpr gfx::Vector2d kDelta[] = {
      gfx::Vector2d(-50, 0),
      gfx::Vector2d(-30, 0),
  };
  constexpr int kDelayAddingFingerMs[] = {0, 40};
  constexpr int kDelayReleasingFingerMs[] = {150, 150};
  event_generator_->GestureMultiFingerScrollWithDelays(
      /*count=*/2, kStart, kDelta, kDelayAddingFingerMs,
      kDelayReleasingFingerMs, /*event_separation_time_ms=*/20, /*steps=*/5);

  // Since the scroll started with one finger, the cursor should have moved.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(6));
  EXPECT_TRUE(GetTextfieldTestApi().touch_selection_controller());
  // Since a second finger was added, the display should also be slightly
  // offset.
  EXPECT_LT(GetTextfieldTestApi().GetDisplayOffsetX(), 0);
}

// TODO(crbug.com/40276114): Rewrite these long press tests when EventGenerator
// can generate long press gestures.
TEST_F(TextfieldTest, LongPressSelection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"Hello string world");

  // Perform a long press.
  const gfx::Point kLongPressPoint = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(2), GetCursorYForTesting()});
  ui::GestureEvent long_press = CreateTestGestureEvent(
      kLongPressPoint.x(), kLongPressPoint.y(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  event_generator_->Dispatch(&long_press);

  // Check that the nearest word is selected, but that the touch selection
  // controller is not activated yet.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(0, 5));
  EXPECT_EQ(textfield_->GetSelectedText(), u"Hello");
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());

  // Check that touch selection is activated after the long press is released.
  ui::GestureEvent long_tap = CreateTestGestureEvent(
      kLongPressPoint.x(), kLongPressPoint.y(),
      ui::GestureEventDetails(ui::EventType::kGestureLongTap));
  event_generator_->Dispatch(&long_tap);
  EXPECT_TRUE(GetTextfieldTestApi().touch_selection_controller());
}

TEST_F(TextfieldTest, LongPressDragSelectionLTRForward) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"Hello string world");

  // Perform a forwards long press and drag movement.
  const gfx::Point kLongPressPoint = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(2), GetCursorYForTesting()});
  event_generator_->PressTouch(kLongPressPoint);
  ui::GestureEvent long_press = CreateTestGestureEvent(
      kLongPressPoint.x(), kLongPressPoint.y(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  event_generator_->Dispatch(&long_press);
  event_generator_->MoveTouchBy(25, 0);
  event_generator_->ReleaseTouch();

  // Check that text is selected between the word boundaries around the start
  // and end of the drag movement.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(0, 12));
  EXPECT_EQ(textfield_->GetSelectedText(), u"Hello string");
}

TEST_F(TextfieldTest, LongPressDragSelectionLTRBackward) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"Hello string world");

  // Perform a backwards long press and drag movement.
  const gfx::Point kLongPressPoint = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(9), GetCursorYForTesting()});
  event_generator_->PressTouch(kLongPressPoint);
  ui::GestureEvent long_press = CreateTestGestureEvent(
      kLongPressPoint.x(), kLongPressPoint.y(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  event_generator_->Dispatch(&long_press);
  event_generator_->MoveTouchBy(-25, 0);
  event_generator_->ReleaseTouch();

  // Check that text is selected between the word boundaries around the start
  // and end of the drag movement. The selection range is reversed since the
  // left endpoint moved while the right endpoint was fixed.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(12, 0));
  EXPECT_EQ(textfield_->GetSelectedText(), u"Hello string");
}

TEST_F(TextfieldTest, LongPressDragSelectionRTLForward) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"مرحبا بالعالم مرحبا");

  // Perform a forwards long press and drag movement.
  const gfx::Point kLongPressPoint = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(9), GetCursorYForTesting()});
  event_generator_->PressTouch(kLongPressPoint);
  ui::GestureEvent long_press = CreateTestGestureEvent(
      kLongPressPoint.x(), kLongPressPoint.y(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  event_generator_->Dispatch(&long_press);
  event_generator_->MoveTouchBy(-25, 0);
  event_generator_->ReleaseTouch();

  // Check that text is selected between the word boundaries around the start
  // and end of the drag movement.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(6, 19));
  EXPECT_EQ(textfield_->GetSelectedText(), u"بالعالم مرحبا");
}

TEST_F(TextfieldTest, LongPressDragSelectionRTLBackward) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"مرحبا بالعالم مرحبا");

  // Perform a backwards long press and drag movement.
  const gfx::Point kLongPressPoint = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(9), GetCursorYForTesting()});
  event_generator_->PressTouch(kLongPressPoint);
  ui::GestureEvent long_press = CreateTestGestureEvent(
      kLongPressPoint.x(), kLongPressPoint.y(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  event_generator_->Dispatch(&long_press);
  event_generator_->MoveTouchBy(25, 0);
  event_generator_->ReleaseTouch();

  // Check that text is selected between the word boundaries around the start
  // and end of the drag movement. The selection range is reversed since the
  // right endpoint moved while the left endpoint was fixed.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(13, 0));
  EXPECT_EQ(textfield_->GetSelectedText(), u"مرحبا بالعالم");
}

TEST_F(TextfieldTest, DoubleTapSelection) {
  InitTextfield();
  textfield_->SetText(u"Hello string world");

  // Perform a double tap.
  const gfx::Point kTapPoint = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(2), GetCursorYForTesting()});
  event_generator_->GestureTapAt(kTapPoint);
  event_generator_->GestureTapAt(kTapPoint);

  // Check that nearest word is selected and that touch selection has been
  // activated.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(0, 5));
  EXPECT_EQ(textfield_->GetSelectedText(), u"Hello");
  EXPECT_TRUE(GetTextfieldTestApi().touch_selection_controller());
}

TEST_F(TextfieldTest, TripleTapSelection) {
  InitTextfield();
  textfield_->SetText(u"Hello string world");

  // Perform a triple tap.
  const gfx::Point kTapPoint = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(2), GetCursorYForTesting()});
  event_generator_->GestureTapAt(kTapPoint);
  event_generator_->GestureTapAt(kTapPoint);
  event_generator_->GestureTapAt(kTapPoint);

  // Check that all text is selected and that touch selection has been
  // activated.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(0, 18));
  EXPECT_EQ(textfield_->GetSelectedText(), u"Hello string world");
  EXPECT_TRUE(GetTextfieldTestApi().touch_selection_controller());
}

TEST_F(TextfieldTest, DoublePressDragSelection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"Hello string world");

  // Perform a double press and drag movement.
  const gfx::Point kDragStart = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(2), GetCursorYForTesting()});
  const gfx::Point kDragEnd = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(10), GetCursorYForTesting()});
  event_generator_->GestureTapAt(kDragStart);
  event_generator_->GestureScrollSequence(kDragStart, kDragEnd,
                                          /*duration=*/base::Milliseconds(50),
                                          /*steps=*/5);

  // Check that text is selected between the word boundaries around the start
  // and end of the drag movement.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(0, 12));
  EXPECT_EQ(textfield_->GetSelectedText(), u"Hello string");
}

TEST_F(TextfieldTest, TouchSelectionDraggingMetrics) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"Text in a textfield");
  const gfx::Point kDragStart = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(2), GetCursorYForTesting()});
  const gfx::Point kDragEnd = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(10), GetCursorYForTesting()});

  // Double press drag selection.
  event_generator_->GestureTapAt(kDragStart);
  event_generator_->GestureScrollSequence(kDragStart, kDragEnd,
                                          /*duration=*/base::Milliseconds(50),
                                          /*steps=*/5);
  histogram_tester.ExpectBucketCount(
      ui::kTouchSelectionDragTypeHistogramName,
      ui::TouchSelectionDragType::kDoublePressDrag, 1);
  histogram_tester.ExpectTotalCount(ui::kTouchSelectionDragTypeHistogramName,
                                    1);

  // Swipe to move cursor.
  event_generator_->GestureScrollSequence(kDragStart, kDragEnd,
                                          /*duration=*/base::Milliseconds(50),
                                          /*steps=*/5);
  histogram_tester.ExpectBucketCount(ui::kTouchSelectionDragTypeHistogramName,
                                     ui::TouchSelectionDragType::kCursorDrag,
                                     1);
  histogram_tester.ExpectTotalCount(ui::kTouchSelectionDragTypeHistogramName,
                                    2);

  // Long press drag selection.
  event_generator_->PressTouch(kDragStart);
  ui::GestureEvent long_press = CreateTestGestureEvent(
      kDragStart.x(), kDragStart.y(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  event_generator_->Dispatch(&long_press);
  event_generator_->MoveTouchBy(25, 0);
  event_generator_->ReleaseTouch();
  histogram_tester.ExpectBucketCount(ui::kTouchSelectionDragTypeHistogramName,
                                     ui::TouchSelectionDragType::kLongPressDrag,
                                     1);
  histogram_tester.ExpectTotalCount(ui::kTouchSelectionDragTypeHistogramName,
                                    3);
}
#endif  // !BUILDFLAG(IS_MAC)

TEST_F(TextfieldTest, GetTextfieldBaseline_FontFallbackTest) {
  InitTextfield();
  textfield_->SetText(u"abc");
  const int old_baseline = textfield_->GetBaseline();

  // Set text which may fall back to a font which has taller baseline than
  // the default font.
  textfield_->SetText(u"๑");
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

  // These two members point to the textfield that is to be destroyed -
  // therefore null them.
  textfield_ = nullptr;
  event_target_ = nullptr;

  // Send a key to trigger OnKeyEvent().
  SendKeyEvent(ui::VKEY_RETURN);

  EXPECT_FALSE(controller.target());
}

TEST_F(TextfieldTest, CursorBlinkRestartsOnInsertOrReplace) {
  InitTextfield();
  textfield_->SetText(u"abc");
  EXPECT_TRUE(GetTextfieldTestApi().IsCursorBlinkTimerRunning());
  textfield_->SetSelectedRange(gfx::Range(1, 2));
  EXPECT_FALSE(GetTextfieldTestApi().IsCursorBlinkTimerRunning());
  textfield_->InsertOrReplaceText(u"foo");
  EXPECT_TRUE(GetTextfieldTestApi().IsCursorBlinkTimerRunning());
}

TEST_F(TextfieldTest, InitialAccessibilityProperties) {
  InitTextfield();
  ui::AXNodeData data;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kTextField);
  EXPECT_TRUE(textfield_->GetViewAccessibility().IsLeaf());
  EXPECT_TRUE(data.HasState(ax::mojom::State::kEditable));
}

// Verifies setting the accessible name will call NotifyAccessibilityEvent.
TEST_F(TextfieldTest, SetAccessibleNameNotifiesAccessibilityEvent) {
  InitTextfield();
  std::u16string test_tooltip_text = u"Test Accessible Name";
  test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged));
  textfield_->GetViewAccessibility().SetName(test_tooltip_text);
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged));
  EXPECT_EQ(test_tooltip_text,
            textfield_->GetViewAccessibility().GetCachedName());
  ui::AXNodeData data;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  const std::string& name =
      data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  EXPECT_EQ(test_tooltip_text, base::ASCIIToUTF16(name));

  // `NameFrom::kAttribute` is appropriate when the name is explicitly set to
  // a developer-provided string (rather than a label, tooltip, or placeholder
  // for which there are other `NameFrom` values). `NameFrom::kContents` is
  // typically not an appropriate value.
  EXPECT_EQ(data.GetNameFrom(), ax::mojom::NameFrom::kAttribute);
}

#if BUILDFLAG(IS_WIN)
TEST_F(TextfieldTest, AccessibilityAttributes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kUiaProvider);
  InitTextfield();

  ViewAXPlatformNodeDelegate* delegate =
      static_cast<ViewAXPlatformNodeDelegate*>(
          &textfield_->GetViewAccessibility());

  textfield_->GetViewAccessibility().EnsureAtomicViewAXTreeManager();
  textfield_->SetText(u"this is the textfield");
  textfield_->SetBounds(1, 2, 3, 4);

  ui::AXNodeData actual =
      delegate->GetAtomicViewAXTreeManagerForTesting()->GetRoot()->data();

  EXPECT_EQ(ax::mojom::Role::kTextField, actual.role);
  EXPECT_TRUE(actual.HasState(ax::mojom::State::kEditable) &&
              actual.HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(textfield_->GetAccessibleName(),
            actual.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(textfield_->GetText(),
            actual.GetString16Attribute(ax::mojom::StringAttribute::kValue));
  EXPECT_EQ(
      textfield_->GetPlaceholderText(),
      actual.GetString16Attribute(ax::mojom::StringAttribute::kPlaceholder));

  EXPECT_EQ(static_cast<const int>(textfield_->GetSelectedRange().start()),
            actual.GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart));
  EXPECT_EQ(static_cast<const int>(textfield_->GetSelectedRange().end()),
            actual.GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd));

  EXPECT_EQ(gfx::Rect(1, 2, 3, 4),
            gfx::ToEnclosingRect(actual.relative_bounds.bounds));

  EXPECT_EQ(textfield_->GetBoundsInScreen(),
            delegate->GetBoundsRect(ui::AXCoordinateSystem::kScreenDIPs,
                                    ui::AXClippingBehavior::kUnclipped, nullptr));
}
#endif

TEST_F(TextfieldTest, AccessiblePlaceholderTest) {
  InitTextfield();

  ui::AXNodeData data;
  textfield_->SetPlaceholderText(u"Some placeholder");
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kPlaceholder),
            u"Some placeholder");

  data = ui::AXNodeData();
  textfield_->SetPlaceholderText(u"Updated placeholder");
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kPlaceholder),
            u"Updated placeholder");
}

TEST_F(TextfieldTest, AccessibleNameFromLabel) {
  InitTextfield();

  const std::u16string label_text = u"Some label";
  View label;
  label.GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
  label.GetViewAccessibility().SetName(label_text);
  textfield_->GetViewAccessibility().SetName(label);

  // Use `ViewAccessibility::GetAccessibleNodeData` so that we can get the
  // label's accessible id to compare with the textfield's labelled-by id.
  ui::AXNodeData label_data;
  label.GetViewAccessibility().GetAccessibleNodeData(&label_data);

  ui::AXNodeData textfield_data;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&textfield_data);
  EXPECT_EQ(
      textfield_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      label_text);
  EXPECT_EQ(textfield_->GetViewAccessibility().GetCachedName(), label_text);
  EXPECT_EQ(textfield_data.GetNameFrom(), ax::mojom::NameFrom::kRelatedElement);
  EXPECT_EQ(textfield_data.GetIntListAttribute(
                ax::mojom::IntListAttribute::kLabelledbyIds)[0],
            label_data.id);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Check that when accessibility virtual keyboard is enabled, windows are
// shifted up when focused and restored when focus is lost.
TEST_F(TextfieldTest, VirtualKeyboardFocusEnsureCaretNotInRect) {
  InitTextfield();

  aura::Window* root_window = GetRootWindow(widget_.get());
  int keyboard_height = 200;
  gfx::Rect root_bounds = root_window->bounds();
  gfx::Rect orig_widget_bounds = gfx::Rect(0, 300, 400, 200);
  gfx::Rect shifted_widget_bounds = gfx::Rect(0, 200, 400, 200);
  gfx::Rect keyboard_view_bounds =
      gfx::Rect(0, root_bounds.height() - keyboard_height, root_bounds.width(),
                keyboard_height);

  // Focus the window.
  widget_->SetBounds(orig_widget_bounds);
  input_method()->SetFocusedTextInputClient(textfield_);
  EXPECT_EQ(widget_->GetNativeView()->bounds(), orig_widget_bounds);

  // Simulate virtual keyboard.
  input_method()->SetVirtualKeyboardBounds(keyboard_view_bounds);

  // Window should be shifted.
  EXPECT_EQ(widget_->GetNativeView()->bounds(), shifted_widget_bounds);

  // Detach the textfield from the IME
  input_method()->DetachTextInputClient(textfield_);
  wm::RestoreWindowBoundsOnClientFocusLost(
      widget_->GetNativeView()->GetToplevelWindow());

  // Window should be restored.
  EXPECT_EQ(widget_->GetNativeView()->bounds(), orig_widget_bounds);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !BUILDFLAG(IS_MAC)
TEST_F(TextfieldTest, TapActivatesTouchSelection) {
  InitTextfield();
  textfield_->SetText(u"hello world");
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());

  // Tapping in the textfield should activate touch selection.
  const gfx::Point kPointInTextfield = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(2), GetCursorYForTesting()});
  event_generator_->GestureTapAt(kPointInTextfield);
  EXPECT_TRUE(GetTextfieldTestApi().touch_selection_controller());
}

TEST_F(TextfieldTest, ClearingFocusDeactivatesTouchSelection) {
  InitTextfield();
  textfield_->SetText(u"hello world");
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());

  // Tap textfield to activate touch selection.
  const gfx::Point kPointInTextfield = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(2), GetCursorYForTesting()});
  event_generator_->GestureTapAt(kPointInTextfield);
  EXPECT_TRUE(GetTextfieldTestApi().touch_selection_controller());

  // Clearing focus should deactivate touch selection.
  textfield_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());
}

TEST_F(TextfieldTest, TapOnSelection) {
  InitTextfield();
  textfield_->SetText(u"hello world");

  // Select a range and check that touch selection handles are not present and
  // that the correct range is selected.
  constexpr gfx::Range kSelectionRange(2, 7);
  textfield_->SetEditableSelectionRange(kSelectionRange);
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());
  EXPECT_EQ(range, kSelectionRange);

  // Tap on the selection and check that touch selection handles are shown, but
  // the selection range is not modified.
  constexpr gfx::Range kTapRange(5, 5);
  const gfx::Rect kTapRect =
      GetCursorBounds(gfx::SelectionModel(kTapRange, gfx::CURSOR_FORWARD));
  const gfx::Point kTapPoint =
      views::View::ConvertPointToScreen(textfield_, kTapRect.CenterPoint());
  event_generator_->GestureTapAt(kTapPoint);
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_TRUE(GetTextfieldTestApi().touch_selection_controller());
  EXPECT_EQ(range, kSelectionRange);

  // Tap again on the selection and check that touch selection handles are still
  // present and that the selection is changed to a cursor at the tap location.
  // We advance the clock before tapping again to avoid the tap being treated as
  // a double tap.
  event_generator_->AdvanceClock(base::Milliseconds(1000));
  event_generator_->GestureTapAt(kTapPoint);
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_TRUE(GetTextfieldTestApi().touch_selection_controller());
  EXPECT_EQ(range, kTapRange);
}

// When touch drag drop is enabled, long pressing on selected text initiates
// drag-drop behaviour. So, long pressing on selected text should preserve the
// selection rather than selecting the nearest word and activating touch
// selection.
TEST_F(TextfieldTest, LongPressOnSelection) {
  InitTextfield();
  textfield_->SetText(u"Hello string world");
  constexpr gfx::Range kSelectionRange(2, 7);
  textfield_->SetEditableSelectionRange(kSelectionRange);
  EXPECT_EQ(textfield_->GetSelectedText(), u"llo s");

  // Long press on the selected text.
  const gfx::Point kLongPressPoint = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(3), GetCursorYForTesting()});
  event_generator_->PressTouch(kLongPressPoint);
  ui::GestureEvent long_press = CreateTestGestureEvent(
      kLongPressPoint.x(), kLongPressPoint.y(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  event_generator_->Dispatch(&long_press);

  // Check that the selection has not changed and that touch selection is not
  // activated.
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, kSelectionRange);
  EXPECT_EQ(textfield_->GetSelectedText(), u"llo s");
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());
}

TEST_F(TextfieldTest, TouchSelectionInUnfocusableTextfield) {
  InitTextfield();
  textfield_->SetText(u"hello world");

  // Disable textfield and tap on it. Touch text selection should not get
  // activated.
  textfield_->SetEnabled(false);
  const gfx::Point kTapPoint = views::View::ConvertPointToScreen(
      textfield_, {GetCursorPositionX(2), GetCursorYForTesting()});
  event_generator_->GestureTapAt(kTapPoint);
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());
  textfield_->SetEnabled(true);

  // Make textfield unfocusable and tap on it. Touch text selection should not
  // get activated.
  textfield_->SetFocusBehavior(View::FocusBehavior::NEVER);
  event_generator_->GestureTapAt(kTapPoint);
  EXPECT_FALSE(textfield_->HasFocus());
  EXPECT_FALSE(GetTextfieldTestApi().touch_selection_controller());
  textfield_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
}
#endif

TEST_F(TextfieldTest, MoveCaret) {
  InitTextfield();
  textfield_->SetText(u"hello world");
  const int cursor_y = GetCursorYForTesting();
  gfx::Range range;

  textfield_->MoveCaret(gfx::Point(GetCursorPositionX(3), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(3));

  textfield_->MoveCaret(gfx::Point(GetCursorPositionX(0), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(0));

  textfield_->MoveCaret(gfx::Point(GetCursorPositionX(11), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(11));
}

TEST_F(TextfieldTest, MoveRangeSelectionExtent) {
  InitTextfield();
  textfield_->SetText(u"hello world");
  const int cursor_y = GetCursorYForTesting();
  gfx::Range range;

  textfield_->SelectBetweenCoordinates(
      gfx::Point(GetCursorPositionX(2), cursor_y),
      gfx::Point(GetCursorPositionX(3), cursor_y));
  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(5), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 5));

  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(0), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 0));
}

TEST_F(TextfieldTest, MoveRangeSelectionExtentToTextEnd) {
  InitTextfield();
  textfield_->SetText(u"hello world a");
  const int cursor_y = GetCursorYForTesting();
  gfx::Range range;

  textfield_->SelectBetweenCoordinates(
      gfx::Point(GetCursorPositionX(2), cursor_y),
      gfx::Point(GetCursorPositionX(3), cursor_y));
  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(13), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 13));
}

TEST_F(TextfieldTest, MoveRangeSelectionExtentByCharacter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{::features::kTouchTextEditingRedesign});

  InitTextfield();
  textfield_->SetText(u"hello world");
  const int cursor_y = GetCursorYForTesting();
  gfx::Range range;

  textfield_->SelectBetweenCoordinates(
      gfx::Point(GetCursorPositionX(2), cursor_y),
      gfx::Point(GetCursorPositionX(3), cursor_y));
  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(4), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 4));
  EXPECT_EQ(textfield_->GetSelectedText(), u"ll");

  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(8), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 8));
  EXPECT_EQ(textfield_->GetSelectedText(), u"llo wo");

  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(1), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 1));
  EXPECT_EQ(textfield_->GetSelectedText(), u"e");
}

TEST_F(TextfieldTest, MoveRangeSelectionExtentExpandByWord) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"some textfield text");
  const int cursor_y = GetCursorYForTesting();
  textfield_->SelectBetweenCoordinates(
      gfx::Point(GetCursorPositionX(2), cursor_y),
      gfx::Point(GetCursorPositionX(3), cursor_y));

  // Expand the selection. The end of the selection should move to the nearest
  // word boundary.
  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(11), cursor_y));
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 14));
  EXPECT_EQ(textfield_->GetSelectedText(), u"me textfield");

  // Shrink then expand the selection again.
  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(8), cursor_y));
  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(18), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 19));
  EXPECT_EQ(textfield_->GetSelectedText(), u"me textfield text");
}

TEST_F(TextfieldTest, MoveRangeSelectionExtentShrinkByCharacter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"some textfield text");
  const int cursor_y = GetCursorYForTesting();
  textfield_->SelectBetweenCoordinates(
      gfx::Point(GetCursorPositionX(2), cursor_y),
      gfx::Point(GetCursorPositionX(12), cursor_y));

  // Shrink the selection.
  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(11), cursor_y));
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 11));
  EXPECT_EQ(textfield_->GetSelectedText(), u"me textfi");
}

TEST_F(TextfieldTest, MoveRangeSelectionExtentOffset) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"some textfield text");
  const int cursor_y = GetCursorYForTesting();
  textfield_->SelectBetweenCoordinates(
      gfx::Point(GetCursorPositionX(2), cursor_y),
      gfx::Point(GetCursorPositionX(3), cursor_y));

  // Expand the selection. The end of the selection should move to the nearest
  // word boundary.
  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(11), cursor_y));
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 14));
  EXPECT_EQ(textfield_->GetSelectedText(), u"me textfield");

  // Shrink the selection. The offset between the selection extent and the end
  // of the selection should be preserved.
  const int offset = GetCursorPositionX(14) - GetCursorPositionX(11);
  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(12) - offset, cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 12));
  EXPECT_EQ(textfield_->GetSelectedText(), u"me textfie");

  // Move the extent past the end of the selection. The offset should be reset
  // and the selection should expand.
  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(13), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 13));
  EXPECT_EQ(textfield_->GetSelectedText(), u"me textfiel");
}

TEST_F(TextfieldTest, MoveRangeSelectionExtentNonEmptySelection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kTouchTextEditingRedesign},
      /*disabled_features=*/{});

  InitTextfield();
  textfield_->SetText(u"some textfield text");
  const int cursor_y = GetCursorYForTesting();
  textfield_->SelectBetweenCoordinates(
      gfx::Point(GetCursorPositionX(2), cursor_y),
      gfx::Point(GetCursorPositionX(12), cursor_y));

  // Shrink the selection. Selection should not become empty.
  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(3), cursor_y));
  gfx::Range range;
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 3));
  EXPECT_EQ(textfield_->GetSelectedText(), u"m");

  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(2), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 3));
  EXPECT_EQ(textfield_->GetSelectedText(), u"m");

  textfield_->MoveRangeSelectionExtent(
      gfx::Point(GetCursorPositionX(1), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(2, 1));
  EXPECT_EQ(textfield_->GetSelectedText(), u"o");
}

TEST_F(TextfieldTest, SelectBetweenCoordinates) {
  InitTextfield();
  textfield_->SetText(u"hello world");
  const int cursor_y = GetCursorYForTesting();
  gfx::Range range;

  textfield_->SelectBetweenCoordinates(
      gfx::Point(GetCursorPositionX(1), cursor_y),
      gfx::Point(GetCursorPositionX(2), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(1, 2));

  textfield_->SelectBetweenCoordinates(
      gfx::Point(GetCursorPositionX(0), cursor_y),
      gfx::Point(GetCursorPositionX(11), cursor_y));
  textfield_->GetEditableSelectionRange(&range);
  EXPECT_EQ(range, gfx::Range(0, 11));
}

TEST_F(TextfieldTest, AccessiblePasswordTest) {
  InitTextfield();
  textfield_->SetText(u"password");

  ui::AXNodeData node_data_regular;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data_regular);
  EXPECT_EQ(ax::mojom::Role::kTextField, node_data_regular.role);
  EXPECT_EQ(u"password", node_data_regular.GetString16Attribute(
                             ax::mojom::StringAttribute::kValue));
  EXPECT_FALSE(node_data_regular.HasState(ax::mojom::State::kProtected));

  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  ui::AXNodeData node_data_protected;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(
      &node_data_protected);
  EXPECT_EQ(ax::mojom::Role::kTextField, node_data_protected.role);
  EXPECT_EQ(u"••••••••", node_data_protected.GetString16Attribute(
                             ax::mojom::StringAttribute::kValue));
  EXPECT_TRUE(node_data_protected.HasState(ax::mojom::State::kProtected));
}

TEST_F(TextfieldTest, AccessibleRole) {
  InitTextfield();

  ui::AXNodeData data;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kTextField);
  EXPECT_EQ(textfield_->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kTextField);

  textfield_->GetViewAccessibility().SetRole(ax::mojom::Role::kSearchBox);

  data = ui::AXNodeData();
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kSearchBox);
  EXPECT_EQ(textfield_->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kSearchBox);
}

TEST_F(TextfieldTest, AccessibleReadOnly) {
  InitTextfield();

  textfield_->SetReadOnly(true);

  ui::AXNodeData data;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetRestriction(), ax::mojom::Restriction::kReadOnly);

  textfield_->SetReadOnly(false);

  data = ui::AXNodeData();
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_NE(data.GetRestriction(), ax::mojom::Restriction::kReadOnly);

  // We should not override the disabled restriction with a readonly one.
  textfield_->SetEnabled(false);
  textfield_->SetReadOnly(true);

  data = ui::AXNodeData();
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetRestriction(), ax::mojom::Restriction::kDisabled);

  // If we re-enable the textfield, the readonly restriction should be applied.
  textfield_->SetEnabled(true);
  data = ui::AXNodeData();
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetRestriction(), ax::mojom::Restriction::kReadOnly);

  // If we start out with a disabled textfield and then set it to readonly and
  // then enable it again, the readonly restriction should be applied.
  textfield_->SetEnabled(false);
  textfield_->SetReadOnly(false);

  data = ui::AXNodeData();
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetRestriction(), ax::mojom::Restriction::kDisabled);

  textfield_->SetReadOnly(true);
  textfield_->SetEnabled(true);

  data = ui::AXNodeData();
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetRestriction(), ax::mojom::Restriction::kReadOnly);
}

// Verify that cursor visibility is controlled by SetCursorEnabled.
TEST_F(TextfieldTest, CursorVisibility) {
  InitTextfield();

  textfield_->SetCursorEnabled(false);
  EXPECT_FALSE(GetTextfieldTestApi().IsCursorVisible());

  textfield_->SetCursorEnabled(true);
  EXPECT_TRUE(GetTextfieldTestApi().IsCursorVisible());
}

TEST_F(TextfieldTest, AccessibleValue) {
  InitTextfield();
  textfield_->SetText(u"password");

  ui::AXNodeData node_data_regular;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data_regular);
  EXPECT_EQ(ax::mojom::Role::kTextField, node_data_regular.role);
  EXPECT_EQ(u"password", node_data_regular.GetString16Attribute(
                             ax::mojom::StringAttribute::kValue));
  EXPECT_FALSE(node_data_regular.HasState(ax::mojom::State::kProtected));

  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  ui::AXNodeData node_data_protected;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(
      &node_data_protected);
  EXPECT_EQ(ax::mojom::Role::kTextField, node_data_protected.role);
  EXPECT_EQ(u"••••••••", node_data_protected.GetString16Attribute(
                             ax::mojom::StringAttribute::kValue));
  EXPECT_TRUE(node_data_protected.HasState(ax::mojom::State::kProtected));

  textfield_->SetText(u"password");
  node_data_protected = ui::AXNodeData();
  textfield_->GetViewAccessibility().GetAccessibleNodeData(
      &node_data_protected);
  EXPECT_EQ(u"••••••••", node_data_protected.GetString16Attribute(
                             ax::mojom::StringAttribute::kValue));
}

// Tests that Textfield::FitToLocalBounds() sets the RenderText's display rect
// to the view's bounds, taking the border into account.
TEST_F(TextfieldTest, FitToLocalBounds) {
  const int kDisplayRectWidth = 100;
  const int kBorderWidth = 5;
  InitTextfield();
  textfield_->SetBounds(0, 0, kDisplayRectWidth, 100);
  textfield_->SetBorder(views::CreateEmptyBorder(kBorderWidth));
  GetTextfieldTestApi().GetRenderText()->SetDisplayRect(gfx::Rect(20, 20));
  ASSERT_EQ(20, GetTextfieldTestApi().GetRenderText()->display_rect().width());
  textfield_->FitToLocalBounds();
  EXPECT_EQ(kDisplayRectWidth - 2 * kBorderWidth,
            GetTextfieldTestApi().GetRenderText()->display_rect().width());
}

// Verify that cursor view height does not exceed the textfield height.
TEST_F(TextfieldTest, CursorViewHeight) {
  InitTextfield();
  textfield_->SetBounds(0, 0, 100, 100);
  textfield_->SetCursorEnabled(true);
  SendKeyEvent('a');
  EXPECT_TRUE(GetTextfieldTestApi().IsCursorVisible());
  EXPECT_GT(textfield_->GetVisibleBounds().height(),
            GetTextfieldTestApi().GetCursorViewRect().height());
  EXPECT_LE(GetTextfieldTestApi().GetCursorViewRect().height(),
            GetCursorBounds().height());

  // set the cursor height to be higher than the textfield height, verify that
  // UpdateCursorViewPosition update cursor view height correctly.
  gfx::Rect cursor_bound(GetTextfieldTestApi().GetCursorViewRect());
  cursor_bound.set_height(150);
  GetTextfieldTestApi().SetCursorViewRect(cursor_bound);
  SendKeyEvent('b');
  EXPECT_GT(textfield_->GetVisibleBounds().height(),
            GetTextfieldTestApi().GetCursorViewRect().height());
  EXPECT_LE(GetTextfieldTestApi().GetCursorViewRect().height(),
            GetCursorBounds().height());
}

// Verify that cursor view height is independent of its parent view height.
TEST_F(TextfieldTest, CursorViewHeightAtDiffDSF) {
  InitTextfield();
  textfield_->SetBounds(0, 0, 100, 100);
  textfield_->SetCursorEnabled(true);
  SendKeyEvent('a');
  EXPECT_TRUE(GetTextfieldTestApi().IsCursorVisible());
  int height = GetTextfieldTestApi().GetCursorViewRect().height();

  // update the size of its parent view size and verify that the height of the
  // cursor view stays the same.
  View* parent = textfield_->parent();
  parent->SetBounds(0, 0, 50, height - 2);
  SendKeyEvent('b');
  EXPECT_EQ(height, GetTextfieldTestApi().GetCursorViewRect().height());
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
  EXPECT_TRUE(GetTextfieldTestApi().IsCursorVisible());

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
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  {
    View* container = widget->SetContentsView(std::make_unique<View>());
    TestTextfield* new_textfield =
        container->AddChildView(std::make_unique<TestTextfield>());
    new_textfield->SetBoundsRect(gfx::Rect(100, 100, 100, 100));
    new_textfield->SetID(1);
    widget->Show();
    EXPECT_FALSE(new_textfield->HasFocus());
    EXPECT_FALSE(TextfieldTestApi(new_textfield).IsCursorVisible());
    new_textfield->RequestFocus();
    EXPECT_TRUE(TextfieldTestApi(new_textfield).IsCursorVisible());
  }
  widget->Close();
}

// Verify that if a textfield gains focus during key dispatch that an edit
// command only results when the event is not consumed.
TEST_F(TextfieldTest, SwitchFocusInKeyDown) {
  InitTextfield();
  TextfieldFocuser* focuser = widget_->GetContentsView()->AddChildView(
      std::make_unique<TextfieldFocuser>(textfield_));

  focuser->RequestFocus();
  EXPECT_EQ(focuser, GetFocusedView());
  SendKeyPress(ui::VKEY_SPACE, 0);
  EXPECT_EQ(textfield_, GetFocusedView());
  EXPECT_EQ(std::u16string(), textfield_->GetText());

  focuser->set_consume(false);
  focuser->RequestFocus();
  EXPECT_EQ(focuser, GetFocusedView());
  SendKeyPress(ui::VKEY_SPACE, 0);
  EXPECT_EQ(textfield_, GetFocusedView());
  EXPECT_EQ(u" ", textfield_->GetText());
  // Remove to ensure that the pointer in the focuser does not become dangling.
  widget_->GetContentsView()->RemoveChildViewT(std::exchange(focuser, nullptr));
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
  EXPECT_GT(context_menu->GetItemCount(), 0u);
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
  EXPECT_GT(context_menu->GetItemCount(), 0u);
  EXPECT_NE(context_menu->GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_EMOJI));
}

TEST_F(TextfieldTest, EmojiItem_FieldWithText) {
  InitTextfield();
  EXPECT_TRUE(textfield_->context_menu_controller());

#if BUILDFLAG(IS_MAC)
  // On Mac, when there is text, the "Look up" item (+ separator) takes the top
  // position, and emoji comes after.
  constexpr int kExpectedEmojiIndex = 2;
#else
  constexpr int kExpectedEmojiIndex = 0;
#endif

  // A field with text may still show the Emoji option (if supported).
  textfield_->SetText(u"some text");
  textfield_->SelectAll(false);
  ui::MenuModel* context_menu = GetContextMenuModel();
  EXPECT_TRUE(context_menu);
  EXPECT_GT(context_menu->GetItemCount(), 0u);
  // Not all OS/versions support the emoji menu.
  EXPECT_EQ(ui::IsEmojiPanelSupported(),
            context_menu->GetLabelAt(kExpectedEmojiIndex) ==
                l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_EMOJI));
}

#if BUILDFLAG(IS_MAC)
// Tests to see if the BiDi submenu items are updated correctly when the
// textfield's text direction is changed.
TEST_F(TextfieldTest, TextServicesContextMenuTextDirectionTest) {
  InitTextfield();
  EXPECT_TRUE(textfield_->context_menu_controller());

  EXPECT_TRUE(GetContextMenuModel());

  textfield_->ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection::LEFT_TO_RIGHT);
  GetTextfieldTestApi().UpdateContextMenu();

  EXPECT_FALSE(textfield_->IsCommandIdChecked(
      ui::TextServicesContextMenu::kWritingDirectionDefault));
  EXPECT_TRUE(textfield_->IsCommandIdChecked(
      ui::TextServicesContextMenu::kWritingDirectionLtr));
  EXPECT_FALSE(textfield_->IsCommandIdChecked(
      ui::TextServicesContextMenu::kWritingDirectionRtl));

  textfield_->ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection::RIGHT_TO_LEFT);
  GetTextfieldTestApi().UpdateContextMenu();

  EXPECT_FALSE(textfield_->IsCommandIdChecked(
      ui::TextServicesContextMenu::kWritingDirectionDefault));
  EXPECT_FALSE(textfield_->IsCommandIdChecked(
      ui::TextServicesContextMenu::kWritingDirectionLtr));
  EXPECT_TRUE(textfield_->IsCommandIdChecked(
      ui::TextServicesContextMenu::kWritingDirectionRtl));
}

// Tests to see if the look up item is hidden for password fields.
TEST_F(TextfieldTest, LookUpPassword) {
  InitTextfield();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  const std::u16string kText = u"Willie Wagtail";

  textfield_->SetText(kText);
  textfield_->SelectAll(false);

  ui::MenuModel* context_menu = GetContextMenuModel();
  EXPECT_TRUE(context_menu);
  EXPECT_GT(context_menu->GetItemCount(), 0u);
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
#endif  // BUILDFLAG(IS_MAC)

TEST_F(TextfieldTest, AccessibilitySelectionEvents) {
  const std::u16string kText = u"abcdef";
  InitTextfield();
  textfield_->SetText(kText);
  EXPECT_TRUE(textfield_->HasFocus());

  std::vector<ax::mojom::Event> event_type = {
      ax::mojom::Event::kTextSelectionChanged};
  std::vector<ax::mojom::Event> previous_selection_events =
      textfield_->GetAccessibilityEventsOfTypes(event_type);

  textfield_->SelectAll(false);

  std::vector<ax::mojom::Event> selection_events =
      textfield_->GetAccessibilityEventsOfTypes(event_type);
  EXPECT_LT(previous_selection_events.size(), selection_events.size());
  previous_selection_events = selection_events;

  // Validate that there's no selection event fired when the textfield blurred,
  // even though the text lost selection.
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(textfield_->HasFocus());
  textfield_->ClearSelection();
  EXPECT_FALSE(textfield_->HasSelection());

  selection_events = textfield_->GetAccessibilityEventsOfTypes(event_type);
  // Has not changed.
  EXPECT_EQ(previous_selection_events.size(), selection_events.size());
}

TEST_F(TextfieldTest, AccessibilitySelectionEventsOnInitialFocus) {
  // Initialize the textfield so we have text to select.
  const std::u16string kText = u"abcdef";
  InitTextfield();
  textfield_->SetText(kText);

  // Ensure focus isn't on the textfield yet.
  widget_->GetFocusManager()->ClearFocus();
  // Clear all the accessibility events we got so far.
  textfield_->ClearAccessibilityEvents();

  // Setting the focus should fire a focus event and a text selection event, in
  // that order.
  textfield_->RequestFocus();
  std::vector<ax::mojom::Event> events =
      textfield_->GetAccessibilityEventsOfTypes(
          {ax::mojom::Event::kFocus, ax::mojom::Event::kTextSelectionChanged});

  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ax::mojom::Event::kFocus, events[0]);
  EXPECT_EQ(ax::mojom::Event::kTextSelectionChanged, events[1]);
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

  TapAtCursor(ui::EventPointerType::kTouch);
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_TOUCH,
            textfield_->GetFocusReason());
}

TEST_F(TextfieldTest, FocusReasonPenTap) {
  InitTextfield();
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_NONE,
            textfield_->GetFocusReason());

  TapAtCursor(ui::EventPointerType::kPen);
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_PEN,
            textfield_->GetFocusReason());
}

TEST_F(TextfieldTest, FocusReasonMultipleEvents) {
  InitTextfield();
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_NONE,
            textfield_->GetFocusReason());

  // Pen tap, followed by a touch tap.
  TapAtCursor(ui::EventPointerType::kPen);
  TapAtCursor(ui::EventPointerType::kTouch);
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_PEN,
            textfield_->GetFocusReason());
}

TEST_F(TextfieldTest, FocusReasonFocusBlurFocus) {
  InitTextfield();
  widget_->GetFocusManager()->ClearFocus();
  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_NONE,
            textfield_->GetFocusReason());

  // Pen tap, blur, then programmatic focus.
  TapAtCursor(ui::EventPointerType::kPen);
  widget_->GetFocusManager()->ClearFocus();
  textfield_->RequestFocus();

  EXPECT_EQ(ui::TextInputClient::FOCUS_REASON_OTHER,
            textfield_->GetFocusReason());
}

TEST_F(TextfieldTest, KeyboardObserverForPenInput) {
  InitTextfield();

  TapAtCursor(ui::EventPointerType::kPen);
  EXPECT_EQ(1, input_method()->count_show_virtual_keyboard());
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
  const std::u16string& text =
      GetTextfieldTestApi().GetRenderText()->GetDisplayText();
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

  // If the text is center-aligned, only the text direction should change.
  textfield_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  textfield_->ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(textfield_->GetTextDirection(),
            base::i18n::TextDirection::RIGHT_TO_LEFT);
  EXPECT_EQ(textfield_->GetHorizontalAlignment(),
            gfx::HorizontalAlignment::ALIGN_CENTER);

  // If the text is aligned to the text direction, its alignment should change
  // iff the text direction changes. We test both scenarios.
  auto dir = base::i18n::TextDirection::RIGHT_TO_LEFT;
  auto opposite_dir = base::i18n::TextDirection::LEFT_TO_RIGHT;
  EXPECT_EQ(textfield_->GetTextDirection(), dir);
  textfield_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  textfield_->ChangeTextDirectionAndLayoutAlignment(opposite_dir);
  EXPECT_EQ(textfield_->GetTextDirection(), opposite_dir);
  EXPECT_NE(textfield_->GetHorizontalAlignment(), gfx::ALIGN_TO_HEAD);

  dir = base::i18n::TextDirection::LEFT_TO_RIGHT;
  EXPECT_EQ(textfield_->GetTextDirection(), dir);
  textfield_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  textfield_->ChangeTextDirectionAndLayoutAlignment(dir);
  EXPECT_EQ(textfield_->GetTextDirection(), dir);
  EXPECT_EQ(textfield_->GetHorizontalAlignment(), gfx::ALIGN_TO_HEAD);
}

TEST_F(TextfieldTest, AccessibilityTextDirection) {
  InitTextfield();
  ui::AXNodeData node_data = ui::AXNodeData();
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kTextDirection),
            static_cast<int32_t>(ax::mojom::WritingDirection::kLtr));

  textfield_->ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection::RIGHT_TO_LEFT);
  node_data = ui::AXNodeData();  // Reset the node data.
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kTextDirection),
            static_cast<int32_t>(ax::mojom::WritingDirection::kRtl));

  textfield_->ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection::LEFT_TO_RIGHT);
  node_data = ui::AXNodeData();
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kTextDirection),
            static_cast<int32_t>(ax::mojom::WritingDirection::kLtr));
}

TEST_F(TextfieldTest, TextChangedCallbackTest) {
  InitTextfield();

  bool text_changed = false;
  auto subscription = textfield_->AddTextChangedCallback(base::BindRepeating(
      [](bool* text_changed) { *text_changed = true; }, &text_changed));

  textfield_->SetText(u"abc");
  EXPECT_TRUE(text_changed);

  text_changed = false;
  textfield_->AppendText(u"def");
  EXPECT_TRUE(text_changed);

  // Undo should still cause callback.
  text_changed = false;
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_TRUE(text_changed);

  text_changed = false;
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_TRUE(text_changed);
}

// Tests that invalid characters like non-displayable characters are filtered
// out when inserted into the text field.
TEST_F(TextfieldTest, InsertInvalidCharsTest) {
  InitTextfield();

  textfield_->InsertText(
      u"\babc\ndef\t",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  EXPECT_EQ(textfield_->GetText(), u"abcdef");
}

TEST_F(TextfieldTest, ScrollCommands) {
  InitTextfield();

  // Scroll commands are only available on Mac.
#if BUILDFLAG(IS_MAC)
  textfield_->SetText(u"12 34567 89");
  textfield_->SetEditableSelectionRange(gfx::Range(6));

  EXPECT_TRUE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::SCROLL_PAGE_UP));
  EXPECT_TRUE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::SCROLL_PAGE_DOWN));
  EXPECT_TRUE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::SCROLL_TO_BEGINNING_OF_DOCUMENT));
  EXPECT_TRUE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::SCROLL_TO_END_OF_DOCUMENT));

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::SCROLL_PAGE_UP);
  EXPECT_EQ(textfield_->GetCursorPosition(), 0u);

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::SCROLL_PAGE_DOWN);
  EXPECT_EQ(textfield_->GetCursorPosition(), 11u);

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::SCROLL_TO_BEGINNING_OF_DOCUMENT);
  EXPECT_EQ(textfield_->GetCursorPosition(), 0u);

  GetTextfieldTestApi().ExecuteTextEditCommand(
      ui::TextEditCommand::SCROLL_TO_END_OF_DOCUMENT);
  EXPECT_EQ(textfield_->GetCursorPosition(), 11u);
#else
  EXPECT_FALSE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::SCROLL_PAGE_UP));
  EXPECT_FALSE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::SCROLL_PAGE_DOWN));
  EXPECT_FALSE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::SCROLL_TO_BEGINNING_OF_DOCUMENT));
  EXPECT_FALSE(textfield_->IsTextEditCommandEnabled(
      ui::TextEditCommand::SCROLL_TO_END_OF_DOCUMENT));
#endif
}

TEST_F(TextfieldTest, AccessibleTextDirectionRTL) {
  InitTextfield();
  textfield_->SetText(u"abc");

  ui::AXNodeData node_data;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kTextDirection),
            static_cast<int32_t>(ax::mojom::WritingDirection::kLtr));

  textfield_->SetText(u"اللغة العربيي");

  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kTextDirection),
            static_cast<int32_t>(ax::mojom::WritingDirection::kRtl));
}

TEST_F(TextfieldTest, AccessibleDefaultActionVerb) {
  InitTextfield();
  ui::AXNodeData data;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(),
            ax::mojom::DefaultActionVerb::kActivate);

  data = ui::AXNodeData();
  textfield_->SetEnabled(false);
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(
      data.HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));

  data = ui::AXNodeData();
  textfield_->SetEnabled(true);
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(),
            ax::mojom::DefaultActionVerb::kActivate);
}

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
TEST_F(TextfieldTest, WordOffsets) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kUiaProvider);
  InitTextfield();
  ui::AXNodeData node_data;
  textfield_->SetText(u"abc 12 34 def hij :' $*() ");
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  std::vector<int32_t> expected_starts = {0, 4, 7, 10, 14};
  std::vector<int32_t> expected_ends = {3, 6, 9, 13, 17};
  EXPECT_EQ(
      node_data.GetIntListAttribute(ax::mojom::IntListAttribute::kWordStarts),
      expected_starts);
  EXPECT_EQ(
      node_data.GetIntListAttribute(ax::mojom::IntListAttribute::kWordEnds),
      expected_ends);
}

TEST_F(TextfieldTest, AccessibleGraphemeOffsets) {
  struct TestCase {
    std::u16string text;
    std::vector<int32_t> expected_offsets;
  };

  const auto kTestCases = std::to_array<TestCase>({
      {std::u16string(), {}},
      // LTR.
      {u"asdfghkl:/", {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100}},
      // RTL: should render left-to-right as "<space>43210 \n cba9876".
      // Note this used to say "Arabic language", in Arabic, but the last
      // character in the string (\u0629) got fancy in an updated Mac font, so
      // now the penultimate character repeats.
      //
      // TODO(accessibility): This is not the correct order of grapheme offsets.
      // Blink returns the offsets from the right boundary when in RTL and so
      // should we for Views.
      {u"اللغة العربيي",
       {120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 0, 10}},
      // LTR कि (DEVANAGARI KA with VOWEL I) (2-char grapheme), LTR abc, and LTR
      // कि.
      {u"\u0915\u093fabc\u0915\u093f", {0, 20, 30, 40, 50, 70}},
      // LTR ab, LTR कि (DEVANAGARI KA with VOWEL I) (2-char grapheme), LTR cd.
      {u"ab\u0915\u093fcd", {0, 10, 20, 40, 50, 60}},
      // LTR ab, 𝄞 'MUSICAL SYMBOL G CLEF' U+1D11E (surrogate pair), LTR cd.
      // Windows requires wide strings for \Unnnnnnnn universal character names.
      {u"ab\U0001D11Ecd", {0, 10, 20, 30, 40, 50}},
  });

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kUiaProvider);

  InitTextfield();

  // Set the glyph width to a fixed value to avoid flakiness and dependency on
  // each platform's default font size.
  constexpr int kGlyphWidth = 10;
  gfx::test::RenderTextTestApi(GetTextfieldTestApi().GetRenderText())
      .SetGlyphWidth(kGlyphWidth);
  GetTextfieldTestApi().GetRenderText()->SetDisplayRect(
      gfx::Rect(0, 0, 20 * kGlyphWidth, 100));

  for (size_t i = 0; i < kTestCases.size(); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    textfield_->SetText(kTestCases[i].text);

    ui::AXNodeData node_data;
    textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    EXPECT_EQ(node_data.GetIntListAttribute(
                  ax::mojom::IntListAttribute::kCharacterOffsets),
              kTestCases[i].expected_offsets);
  }
}

TEST_F(TextfieldTest, AccessibleGraphemeOffsetsObscured) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kUiaProvider);
  InitTextfield();
  textfield_->SetText(u"abcdef");

  ASSERT_FALSE(GetTextfieldTestApi().GetRenderText()->obscured());

  ui::AXNodeData node_data;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  std::vector<int32_t> non_obscured_offsets = node_data.GetIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets);

  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_NE(node_data.GetIntListAttribute(
                ax::mojom::IntListAttribute::kCharacterOffsets),
            non_obscured_offsets);
}

TEST_F(TextfieldTest, AccessibleGraphemeOffsetsElidedTail) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kUiaProvider);
  InitTextfield();

  constexpr int kGlyphWidth = 10;

  GetTextfieldTestApi().GetRenderText()->SetDisplayRect(
      gfx::Rect(0, 0, 5 * kGlyphWidth, 100));
  GetTextfieldTestApi().GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
  gfx::test::RenderTextTestApi(GetTextfieldTestApi().GetRenderText())
      .SetGlyphWidth(kGlyphWidth);

  textfield_->SetText(u"abcdef");

  ui::AXNodeData node_data;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  std::vector<int32_t> expected_offsets = {0, 10, 20, 30, 40, 40, 40};
  EXPECT_EQ(node_data.GetIntListAttribute(
                ax::mojom::IntListAttribute::kCharacterOffsets),
            expected_offsets);
}

TEST_F(TextfieldTest, AccessibleGraphemeOffsetsIndependentOfDisplayOffset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kUiaProvider);
  InitTextfield();

  // Size the textfield wide enough to hold 10 characters.
  gfx::test::RenderTextTestApi render_text_test_api(
      GetTextfieldTestApi().GetRenderText());
  constexpr int kGlyphWidth = 10;
  render_text_test_api.SetGlyphWidth(kGlyphWidth);
  GetTextfieldTestApi().GetRenderText()->SetDisplayRect(
      gfx::Rect(kGlyphWidth * 10, 20));
  textfield_->SetTextWithoutCaretBoundsChangeNotification(
      u"3.141592653589793238462", 0);
  GetTextfieldTestApi().SetDisplayOffsetX(0);

  ui::AXNodeData node_data;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  std::vector<int32_t> expected_offsets = {
      0,   10,  20,  30,  40,  50,  60,  70,  80,  90,  100, 110,
      120, 130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230};
  EXPECT_EQ(node_data.GetIntListAttribute(
                ax::mojom::IntListAttribute::kCharacterOffsets),
            expected_offsets);
  GetTextfieldTestApi().SetDisplayOffsetX(-100);
  EXPECT_EQ(GetTextfieldTestApi().GetDisplayOffsetX(), -100);

  ui::AXNodeData node_data_2;
  textfield_->GetViewAccessibility().GetAccessibleNodeData(&node_data_2);
  // The offsets should be the same.
  EXPECT_EQ(node_data_2.GetIntListAttribute(
                ax::mojom::IntListAttribute::kCharacterOffsets),
            expected_offsets);
}
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

}  // namespace views::test
