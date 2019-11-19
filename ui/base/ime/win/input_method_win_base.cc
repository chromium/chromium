// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/input_method_win_base.h"

#include <stddef.h>
#include <stdint.h>
#include <cwctype>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/win/windows_version.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/on_screen_keyboard_display_manager_input_pane.h"
#include "ui/base/ime/win/on_screen_keyboard_display_manager_tab_tip.h"
#include "ui/base/ime/win/tsf_input_scope.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/win/screen_win.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
namespace {

// Extra number of chars before and after selection (or composition) range which
// is returned to IME for improving conversion accuracy.
constexpr size_t kExtraNumberOfChars = 20;

std::unique_ptr<InputMethodKeyboardController> CreateKeyboardController(
    HWND toplevel_window_handle) {
  if (base::FeatureList::IsEnabled(features::kInputPaneOnScreenKeyboard) &&
      base::win::GetVersion() >= base::win::Version::WIN10_RS4) {
    return std::make_unique<OnScreenKeyboardDisplayManagerInputPane>(
        toplevel_window_handle);
  } else if (base::win::GetVersion() >= base::win::Version::WIN8) {
    return std::make_unique<OnScreenKeyboardDisplayManagerTabTip>(
        toplevel_window_handle);
  }
  return nullptr;
}

// Checks if a given primary language ID is a RTL language.
bool IsRTLPrimaryLangID(LANGID lang) {
  switch (lang) {
    case LANG_ARABIC:
    case LANG_HEBREW:
    case LANG_PERSIAN:
    case LANG_SYRIAC:
    case LANG_UIGHUR:
    case LANG_URDU:
      return true;
    default:
      return false;
  }
}

// Checks if there is any RTL keyboard layout installed in the system.
bool IsRTLKeyboardLayoutInstalled() {
  static enum {
    RTL_KEYBOARD_LAYOUT_NOT_INITIALIZED,
    RTL_KEYBOARD_LAYOUT_INSTALLED,
    RTL_KEYBOARD_LAYOUT_NOT_INSTALLED,
    RTL_KEYBOARD_LAYOUT_ERROR,
  } layout = RTL_KEYBOARD_LAYOUT_NOT_INITIALIZED;

  // Cache the result value.
  if (layout != RTL_KEYBOARD_LAYOUT_NOT_INITIALIZED)
    return layout == RTL_KEYBOARD_LAYOUT_INSTALLED;

  // Retrieve the number of layouts installed in this system.
  int size = GetKeyboardLayoutList(0, NULL);
  if (size <= 0) {
    layout = RTL_KEYBOARD_LAYOUT_ERROR;
    return false;
  }

  // Retrieve the keyboard layouts in an array and check if there is an RTL
  // layout in it.
  std::unique_ptr<HKL[]> layouts(new HKL[size]);
  ::GetKeyboardLayoutList(size, layouts.get());
  for (int i = 0; i < size; ++i) {
    if (IsRTLPrimaryLangID(
            PRIMARYLANGID(reinterpret_cast<uintptr_t>(layouts[i])))) {
      layout = RTL_KEYBOARD_LAYOUT_INSTALLED;
      return true;
    }
  }

  layout = RTL_KEYBOARD_LAYOUT_NOT_INSTALLED;
  return false;
}

// Checks if the user pressed both Ctrl and right or left Shift keys to
// request to change the text direction and layout alignment explicitly.
// Returns true if only a Ctrl key and a Shift key are down. The desired text
// direction will be stored in |*direction|.
bool IsCtrlShiftPressed(base::i18n::TextDirection* direction) {
  uint8_t keystate[256];
  if (!::GetKeyboardState(&keystate[0]))
    return false;

  // To check if a user is pressing only a control key and a right-shift key
  // (or a left-shift key), we use the steps below:
  // 1. Check if a user is pressing a control key and a right-shift key (or
  //    a left-shift key).
  // 2. If the condition 1 is true, we should check if there are any other
  //    keys pressed at the same time.
  //    To ignore the keys checked in 1, we set their status to 0 before
  //    checking the key status.
  const int kKeyDownMask = 0x80;
  if ((keystate[VK_CONTROL] & kKeyDownMask) == 0)
    return false;

  if (keystate[VK_RSHIFT] & kKeyDownMask) {
    keystate[VK_RSHIFT] = 0;
    *direction = base::i18n::RIGHT_TO_LEFT;
  } else if (keystate[VK_LSHIFT] & kKeyDownMask) {
    keystate[VK_LSHIFT] = 0;
    *direction = base::i18n::LEFT_TO_RIGHT;
  } else {
    return false;
  }

  // Scan the key status to find pressed keys. We should abandon changing the
  // text direction when there are other pressed keys.
  // This code is executed only when a user is pressing a control key and a
  // right-shift key (or a left-shift key), i.e. we should ignore the status of
  // the keys: VK_SHIFT, VK_CONTROL, VK_RCONTROL, and VK_LCONTROL.
  // So, we reset their status to 0 and ignore them.
  keystate[VK_SHIFT] = 0;
  keystate[VK_CONTROL] = 0;
  keystate[VK_RCONTROL] = 0;
  keystate[VK_LCONTROL] = 0;
  // Oddly, pressing F10 in another application seemingly breaks all subsequent
  // calls to GetKeyboardState regarding the state of the F22 key. Perhaps this
  // defect is limited to my keyboard driver, but ignoring F22 should be okay.
  keystate[VK_F22] = 0;
  for (int i = 0; i <= VK_PACKET; ++i) {
    if (keystate[i] & kKeyDownMask)
      return false;
  }
  return true;
}

ui::EventDispatchDetails DispatcherDestroyedDetails() {
  ui::EventDispatchDetails dispatcher_details;
  dispatcher_details.dispatcher_destroyed = true;
  return dispatcher_details;
}

}  // namespace

InputMethodWinBase::InputMethodWinBase(internal::InputMethodDelegate* delegate,
                                       HWND toplevel_window_handle)
    : InputMethodBase(delegate,
                      CreateKeyboardController(toplevel_window_handle)),
      toplevel_window_handle_(toplevel_window_handle),
      pending_requested_direction_(base::i18n::UNKNOWN_DIRECTION) {}

InputMethodWinBase::~InputMethodWinBase() {}

void InputMethodWinBase::OnDidChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  if (focused_before != focused)
    accept_carriage_return_ = false;
}

ui::EventDispatchDetails InputMethodWinBase::DispatchKeyEvent(
    ui::KeyEvent* event) {
  MSG native_key_event = MSGFromKeyEvent(event);
  if (native_key_event.message == WM_CHAR) {
    auto ref = weak_ptr_factory_.GetWeakPtr();
    BOOL handled = FALSE;
    OnChar(native_key_event.hwnd, native_key_event.message,
           native_key_event.wParam, native_key_event.lParam, native_key_event,
           &handled);
    if (!ref)
      return DispatcherDestroyedDetails();
    if (handled)
      event->StopPropagation();
    return ui::EventDispatchDetails();
  }

  std::vector<MSG> char_msgs;
  // Combines the WM_KEY* and WM_CHAR messages in the event processing flow
  // which is necessary to let Chrome IME extension to process the key event
  // and perform corresponding IME actions.
  // Chrome IME extension may wants to consume certain key events based on
  // the character information of WM_CHAR messages. Holding WM_KEY* messages
  // until WM_CHAR is processed by the IME extension is not feasible because
  // there is no way to know whether there will or not be a WM_CHAR following
  // the WM_KEY*.
  // Chrome never handles dead chars so it is safe to remove/ignore
  // WM_*DEADCHAR messages.
  MSG msg;
  while (::PeekMessage(&msg, native_key_event.hwnd, WM_CHAR, WM_DEADCHAR,
                       PM_REMOVE)) {
    if (msg.message == WM_CHAR)
      char_msgs.push_back(msg);
  }
  while (::PeekMessage(&msg, native_key_event.hwnd, WM_SYSCHAR, WM_SYSDEADCHAR,
                       PM_REMOVE)) {
    if (msg.message == WM_SYSCHAR)
      char_msgs.push_back(msg);
  }

  // Handles ctrl-shift key to change text direction and layout alignment.
  if (IsRTLKeyboardLayoutInstalled() && !IsTextInputTypeNone()) {
    ui::KeyboardCode code = event->key_code();
    if (event->type() == ui::ET_KEY_PRESSED) {
      if (code == ui::VKEY_SHIFT) {
        base::i18n::TextDirection dir;
        if (IsCtrlShiftPressed(&dir))
          pending_requested_direction_ = dir;
      } else if (code != ui::VKEY_CONTROL) {
        pending_requested_direction_ = base::i18n::UNKNOWN_DIRECTION;
      }
    } else if (event->type() == ui::ET_KEY_RELEASED &&
               (code == ui::VKEY_SHIFT || code == ui::VKEY_CONTROL) &&
               pending_requested_direction_ != base::i18n::UNKNOWN_DIRECTION) {
      GetTextInputClient()->ChangeTextDirectionAndLayoutAlignment(
          pending_requested_direction_);
      pending_requested_direction_ = base::i18n::UNKNOWN_DIRECTION;
    }
  }

  // If only 1 WM_CHAR per the key event, set it as the character of it.
  if (char_msgs.size() == 1 &&
      !std::iswcntrl(static_cast<wint_t>(char_msgs[0].wParam)))
    event->set_character(static_cast<base::char16>(char_msgs[0].wParam));

  // Dispatches the key events to the Chrome IME extension which is listening to
  // key events on the following two situations:
  // 1) |char_msgs| is empty when the event is non-character key.
  // 2) |char_msgs|.size() == 1 when the event is character key and the WM_CHAR
  // messages have been combined in the event processing flow.
  if (char_msgs.size() <= 1 && GetEngine()) {
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback =
        base::BindOnce(&InputMethodWinBase::ProcessKeyEventDone,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Owned(new ui::KeyEvent(*event)),
                       base::Owned(new std::vector<MSG>(char_msgs)));
    GetEngine()->ProcessKeyEvent(*event, std::move(callback));
    return ui::EventDispatchDetails();
  }

  return ProcessUnhandledKeyEvent(event, &char_msgs);
}

bool InputMethodWinBase::IsWindowFocused(const TextInputClient* client) const {
  if (!client)
    return false;
  // When Aura is enabled, |attached_window_handle| should always be a top-level
  // window. So we can safely assume that |attached_window_handle| is ready for
  // receiving keyboard input as long as it is an active window. This works well
  // even when the |attached_window_handle| becomes active but has not received
  // WM_FOCUS yet.
  return toplevel_window_handle_ &&
         GetActiveWindow() == toplevel_window_handle_;
}

LRESULT InputMethodWinBase::OnChar(HWND window_handle,
                                   UINT message,
                                   WPARAM wparam,
                                   LPARAM lparam,
                                   const MSG& event,
                                   BOOL* handled) {
  *handled = TRUE;

  // We need to send character events to the focused text input client event if
  // its text input type is ui::TEXT_INPUT_TYPE_NONE.
  if (GetTextInputClient()) {
    const base::char16 kCarriageReturn = L'\r';
    const base::char16 ch = static_cast<base::char16>(wparam);
    // A mask to determine the previous key state from |lparam|. The value is 1
    // if the key is down before the message is sent, or it is 0 if the key is
    // up.
    const uint32_t kPrevKeyDownBit = 0x40000000;
    if (ch == kCarriageReturn && !(lparam & kPrevKeyDownBit))
      accept_carriage_return_ = true;
    // Conditionally ignore '\r' events to work around https://crbug.com/319100.
    // TODO(yukawa, IME): Figure out long-term solution.
    if (ch != kCarriageReturn || accept_carriage_return_) {
      ui::KeyEvent char_event = ui::KeyEventFromMSG(event);
      GetTextInputClient()->InsertChar(char_event);
    }
  }

  return 0;
}

LRESULT InputMethodWinBase::OnImeRequest(UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam,
                                         BOOL* handled) {
  *handled = FALSE;

  // Should not receive WM_IME_REQUEST message, if IME is disabled.
  const ui::TextInputType type = GetTextInputType();
  if (type == ui::TEXT_INPUT_TYPE_NONE ||
      type == ui::TEXT_INPUT_TYPE_PASSWORD) {
    return 0;
  }

  switch (wparam) {
    case IMR_RECONVERTSTRING:
      *handled = TRUE;
      return OnReconvertString(reinterpret_cast<RECONVERTSTRING*>(lparam));
    case IMR_DOCUMENTFEED:
      *handled = TRUE;
      return OnDocumentFeed(reinterpret_cast<RECONVERTSTRING*>(lparam));
    case IMR_QUERYCHARPOSITION:
      *handled = TRUE;
      return OnQueryCharPosition(reinterpret_cast<IMECHARPOSITION*>(lparam));
    default:
      return 0;
  }
}

LRESULT InputMethodWinBase::OnDocumentFeed(RECONVERTSTRING* reconv) {
  ui::TextInputClient* client = GetTextInputClient();
  if (!client)
    return 0;

  gfx::Range text_range;
  if (!client->GetTextRange(&text_range) || text_range.is_empty())
    return 0;

  bool result = false;
  gfx::Range target_range;
  if (client->HasCompositionText())
    result = client->GetCompositionTextRange(&target_range);

  if (!result || target_range.is_empty()) {
    if (!client->GetEditableSelectionRange(&target_range) ||
        !target_range.IsValid()) {
      return 0;
    }
  }

  if (!text_range.Contains(target_range))
    return 0;

  if (target_range.GetMin() - text_range.start() > kExtraNumberOfChars)
    text_range.set_start(target_range.GetMin() - kExtraNumberOfChars);

  if (text_range.end() - target_range.GetMax() > kExtraNumberOfChars)
    text_range.set_end(target_range.GetMax() + kExtraNumberOfChars);

  size_t len = text_range.length();
  size_t need_size = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (!reconv)
    return need_size;

  if (reconv->dwSize < need_size)
    return 0;

  base::string16 text;
  if (!GetTextInputClient()->GetTextFromRange(text_range, &text))
    return 0;
  DCHECK_EQ(text_range.length(), text.length());

  reconv->dwVersion = 0;
  reconv->dwStrLen = len;
  reconv->dwStrOffset = sizeof(RECONVERTSTRING);
  reconv->dwCompStrLen =
      client->HasCompositionText() ? target_range.length() : 0;
  reconv->dwCompStrOffset =
      (target_range.GetMin() - text_range.start()) * sizeof(WCHAR);
  reconv->dwTargetStrLen = target_range.length();
  reconv->dwTargetStrOffset = reconv->dwCompStrOffset;

  memcpy((char*)reconv + sizeof(RECONVERTSTRING), text.c_str(),
         len * sizeof(WCHAR));

  // According to Microsoft API document, IMR_RECONVERTSTRING and
  // IMR_DOCUMENTFEED should return reconv, but some applications return
  // need_size.
  return reinterpret_cast<LRESULT>(reconv);
}

LRESULT InputMethodWinBase::OnReconvertString(RECONVERTSTRING* reconv) {
  ui::TextInputClient* client = GetTextInputClient();
  if (!client)
    return 0;

  // If there is a composition string already, we don't allow reconversion.
  if (client->HasCompositionText())
    return 0;

  gfx::Range text_range;
  if (!client->GetTextRange(&text_range) || text_range.is_empty())
    return 0;

  gfx::Range selection_range;
  if (!client->GetEditableSelectionRange(&selection_range) ||
      selection_range.is_empty()) {
    return 0;
  }

  DCHECK(text_range.Contains(selection_range));

  size_t len = selection_range.length();
  size_t need_size = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (!reconv)
    return need_size;

  if (reconv->dwSize < need_size)
    return 0;

  // TODO(penghuang): Return some extra context to help improve IME's
  // reconversion accuracy.
  base::string16 text;
  if (!GetTextInputClient()->GetTextFromRange(selection_range, &text))
    return 0;
  DCHECK_EQ(selection_range.length(), text.length());

  reconv->dwVersion = 0;
  reconv->dwStrLen = len;
  reconv->dwStrOffset = sizeof(RECONVERTSTRING);
  reconv->dwCompStrLen = len;
  reconv->dwCompStrOffset = 0;
  reconv->dwTargetStrLen = len;
  reconv->dwTargetStrOffset = 0;

  memcpy(reinterpret_cast<char*>(reconv) + sizeof(RECONVERTSTRING),
         text.c_str(), len * sizeof(WCHAR));

  // According to Microsoft API document, IMR_RECONVERTSTRING and
  // IMR_DOCUMENTFEED should return reconv, but some applications return
  // need_size.
  return reinterpret_cast<LRESULT>(reconv);
}

LRESULT InputMethodWinBase::OnQueryCharPosition(IMECHARPOSITION* char_positon) {
  if (!char_positon)
    return 0;

  if (char_positon->dwSize < sizeof(IMECHARPOSITION))
    return 0;

  ui::TextInputClient* client = GetTextInputClient();
  if (!client)
    return 0;

  // Tentatively assume that the returned value from |client| is DIP (Density
  // Independent Pixel). See the comment in text_input_client.h and
  // http://crbug.com/360334.
  gfx::Rect dip_rect;
  if (client->HasCompositionText()) {
    if (!client->GetCompositionCharacterBounds(char_positon->dwCharPos,
                                               &dip_rect)) {
      return 0;
    }
  } else {
    // If there is no composition and the first character is queried, returns
    // the caret bounds. This behavior is the same to that of RichEdit control.
    if (char_positon->dwCharPos != 0)
      return 0;
    dip_rect = client->GetCaretBounds();
  }
  const gfx::Rect rect = display::win::ScreenWin::DIPToScreenRect(
      toplevel_window_handle_, dip_rect);

  char_positon->pt.x = rect.x();
  char_positon->pt.y = rect.y();
  char_positon->cLineHeight = rect.height();
  return 1;  // returns non-zero value when succeeded.
}

void InputMethodWinBase::ProcessKeyEventDone(ui::KeyEvent* event,
                                             const std::vector<MSG>* char_msgs,
                                             bool is_handled) {
  if (is_handled)
    return;
  ProcessUnhandledKeyEvent(event, char_msgs);
}

ui::EventDispatchDetails InputMethodWinBase::ProcessUnhandledKeyEvent(
    ui::KeyEvent* event,
    const std::vector<MSG>* char_msgs) {
  DCHECK(event);
  ui::EventDispatchDetails details = DispatchKeyEventPostIME(event);
  if (details.dispatcher_destroyed || details.target_destroyed ||
      event->stopped_propagation()) {
    return details;
  }

  BOOL handled;
  for (const auto& msg : (*char_msgs)) {
    auto ref = weak_ptr_factory_.GetWeakPtr();
    OnChar(msg.hwnd, msg.message, msg.wParam, msg.lParam, msg, &handled);
    if (!ref)
      return DispatcherDestroyedDetails();
  }
  return details;
}

void InputMethodWinBase::UpdateCompositionBoundsForEngine(
    const TextInputClient* client) {
  TextInputType text_input_type = GetTextInputType();
  if (client == GetTextInputClient() &&
      text_input_type != TEXT_INPUT_TYPE_NONE &&
      text_input_type != TEXT_INPUT_TYPE_PASSWORD && GetEngine()) {
    GetEngine()->SetCompositionBounds(GetCompositionBounds(client));
  }
}

void InputMethodWinBase::ResetEngine() {
  if (GetEngine())
    GetEngine()->Reset();
}

void InputMethodWinBase::CancelCompositionForEngine() {
  TextInputType text_input_type = GetTextInputType();
  if (text_input_type != TEXT_INPUT_TYPE_NONE &&
      text_input_type != TEXT_INPUT_TYPE_PASSWORD) {
    InputMethodWinBase::ResetEngine();
  }
}

void InputMethodWinBase::UpdateEngineFocusAndInputContext() {
  if (!ui::IMEBridge::Get())  // IMEBridge could be null for tests.
    return;

  const TextInputType old_text_input_type =
      ui::IMEBridge::Get()->GetCurrentInputContext().type;
  ui::IMEEngineHandlerInterface::InputContext context(
      GetTextInputType(), GetTextInputMode(), GetTextInputFlags(),
      ui::TextInputClient::FOCUS_REASON_OTHER, GetClientShouldDoLearning());
  ui::IMEBridge::Get()->SetCurrentInputContext(context);

  // Update IME Engine state.
  ui::IMEEngineHandlerInterface* engine = GetEngine();
  if (engine) {
    if (old_text_input_type != ui::TEXT_INPUT_TYPE_NONE)
      engine->FocusOut();
    if (GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE)
      engine->FocusIn(context);
  }
}

}  // namespace ui
