// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/input_method_win_tsf.h"

#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/base/ime/win/tsf_bridge.h"
#include "ui/base/ime/win/tsf_event_router.h"

namespace ui {

class InputMethodWinTSF::TSFEventObserver : public TSFEventRouterObserver {
 public:
  TSFEventObserver() = default;

  TSFEventObserver(const TSFEventObserver&) = delete;
  TSFEventObserver& operator=(const TSFEventObserver&) = delete;

  // Returns true if we know for sure that a candidate window (or IME suggest,
  // etc.) is open.
  bool IsCandidatePopupOpen() const { return is_candidate_popup_open_; }

  // Overridden from TSFEventRouterObserver:
  void OnCandidateWindowCountChanged(size_t window_count) override {
    is_candidate_popup_open_ = (window_count != 0);
  }

 private:
  // True if we know for sure that a candidate window is open.
  bool is_candidate_popup_open_ = false;
};

InputMethodWinTSF::InputMethodWinTSF(
    ImeKeyEventDispatcher* ime_key_event_dispatcher,
    HWND attached_window_handle)
    : InputMethodWinBase(ime_key_event_dispatcher, attached_window_handle),
      tsf_event_observer_(new TSFEventObserver()),
      tsf_event_router_(new TSFEventRouter(tsf_event_observer_.get())) {}

InputMethodWinTSF::~InputMethodWinTSF() {
  // A pointer to |this| might have been passed to ui::TSFBridge::GetInstance()
  // in InputMethodWinTSF::OnFocus(). This TSFBridge can sometime be called
  // asynchronously with the following stack:
  //   chrome.dll               ui::TSFTextStore::DispatchKeyEvent
  //   chrome.dll               ui::TSFTextStore::RequestLock
  //   textinputframework.dll   SafeRequestLock
  //   textinputframework.dll   CInputContext::RequestLock
  //
  // This will cause the TSFBridge to try to access this pointer, which could
  // cause a UAF if this object is already destroyed. To avoid this, we need to
  // explicitly remove the dispatcher before destroying this object. The code in
  // ui::TSFTextStore::DispatchKeyEvent does properly check the pointer before
  // trying to use it. Note that everything happens on the same thread here.
  //
  // See crbug.com/41488962
  if (ui::TSFBridge::GetInstance()) {
    ui::TSFBridge::GetInstance()->RemoveImeKeyEventDispatcher(
        InputMethodBase::ime_key_event_dispatcher());
  }
}

void InputMethodWinTSF::OnFocus() {
  InputMethodBase::OnFocus();
  if (!ui::TSFBridge::GetInstance()) {
    // TSFBridge can be null for tests.
    return;
  }
  tsf_event_router_->SetManager(
      ui::TSFBridge::GetInstance()->GetThreadManager().Get());
  ui::TSFBridge::GetInstance()->SetImeKeyEventDispatcher(
      InputMethodBase::ime_key_event_dispatcher());
}

void InputMethodWinTSF::OnBlur() {
  InputMethodBase::OnBlur();
  if (!ui::TSFBridge::GetInstance()) {
    // TSFBridge can be null for tests.
    return;
  }
  tsf_event_router_->SetManager(nullptr);
  ui::TSFBridge::GetInstance()->RemoveImeKeyEventDispatcher(
      InputMethodBase::ime_key_event_dispatcher());
}

bool InputMethodWinTSF::OnUntranslatedIMEMessage(
    const CHROME_MSG event,
    InputMethod::NativeEventResult* result) {
  LRESULT original_result = 0;
  BOOL handled = FALSE;
  // Even when TSF is enabled, following IMM32/Win32 messages must be handled.
  switch (event.message) {
    case WM_IME_REQUEST:
      // Some TSF-native TIPs (Text Input Processors) such as ATOK and Mozc
      // still rely on WM_IME_REQUEST message to implement reverse conversion.
      original_result =
          OnImeRequest(event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_CHAR:
    case WM_SYSCHAR:
      // ui::InputMethod interface is responsible for handling Win32 character
      // messages. For instance, we will be here in the following cases.
      // - TIP is not activated. (e.g, the current language profile is English)
      // - TIP does not handle and WM_KEYDOWN and WM_KEYDOWN is translated into
      //   WM_CHAR by TranslateMessage API. (e.g, TIP is turned off)
      // - Another application sends WM_CHAR through SendMessage API.
      original_result = OnChar(event.hwnd, event.message, event.wParam,
                               event.lParam, event, &handled);
      break;
  }

  if (result)
    *result = original_result;
  return !!handled;
}

void InputMethodWinTSF::OnTextInputTypeChanged(TextInputClient* client) {
  InputMethodBase::OnTextInputTypeChanged(client);
  if (!ui::TSFBridge::GetInstance() || !IsTextInputClientFocused(client) ||
      !IsWindowFocused(client)) {
    return;
  }
  ui::TSFBridge::GetInstance()->CancelComposition();
  ui::TSFBridge::GetInstance()->OnTextInputTypeChanged(client);
}

void InputMethodWinTSF::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!ui::TSFBridge::GetInstance() || !IsTextInputClientFocused(client) ||
      !IsWindowFocused(client)) {
    return;
  }
  NotifyTextInputCaretBoundsChanged(client);
  ui::TSFBridge::GetInstance()->OnTextLayoutChanged();
}

void InputMethodWinTSF::CancelComposition(const TextInputClient* client) {
  if (ui::TSFBridge::GetInstance() && IsTextInputClientFocused(client) &&
      IsWindowFocused(client)) {
    ui::TSFBridge::GetInstance()->CancelComposition();
  }
}

void InputMethodWinTSF::DetachTextInputClient(TextInputClient* client) {
  if (!ui::TSFBridge::GetInstance()) {
    // TSFBridge can be null for tests.
    return;
  }
  InputMethodWinBase::DetachTextInputClient(client);
  ui::TSFBridge::GetInstance()->RemoveFocusedClient(client);
}

void InputMethodWinTSF::OnInputLocaleChanged() {}

bool InputMethodWinTSF::IsInputLocaleCJK() const {
  if (!ui::TSFBridge::GetInstance()) {
    return false;
  }

  return ui::TSFBridge::GetInstance()->IsInputLanguageCJK();
}

bool InputMethodWinTSF::IsCandidatePopupOpen() const {
  return tsf_event_observer_->IsCandidatePopupOpen();
}

void InputMethodWinTSF::OnWillChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  if (ui::TSFBridge::GetInstance() && IsWindowFocused(focused_before)) {
    ConfirmCompositionText();
    ui::TSFBridge::GetInstance()->RemoveFocusedClient(focused_before);
  }
}

void InputMethodWinTSF::OnDidChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  if (ui::TSFBridge::GetInstance() && IsWindowFocused(focused) &&
      IsTextInputClientFocused(focused)) {
    ui::TSFBridge::GetInstance()->SetFocusedClient(attached_window_handle_,
                                                   focused);
    // Force to update the input type since client's TextInputStateChanged()
    // function might not be called if text input types before the client loses
    // focus and after it acquires focus again are the same.
    OnTextInputTypeChanged(focused);

    // Force to update caret bounds, in case the client thinks that the caret
    // bounds has not changed.
    OnCaretBoundsChanged(focused);
  }
  InputMethodWinBase::OnDidChangeFocusedClient(focused_before, focused);
}

void InputMethodWinTSF::ConfirmCompositionText() {
  if (IsTextInputTypeNone())
    return;

  if (ui::TSFBridge::GetInstance())
    ui::TSFBridge::GetInstance()->ConfirmComposition();
}

void InputMethodWinTSF::OnUrlChanged() {
  if (!ui::TSFBridge::GetInstance()) {
    return;
  }

  ui::TSFBridge::GetInstance()->OnUrlChanged();
}

}  // namespace ui
