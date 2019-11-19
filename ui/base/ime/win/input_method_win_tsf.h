// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_INPUT_METHOD_WIN_TSF_H_
#define UI_BASE_IME_WIN_INPUT_METHOD_WIN_TSF_H_

#include <windows.h>

#include <string>

#include "base/component_export.h"
#include "ui/base/ime/win/input_method_win_base.h"

namespace ui {

class TSFEventRouter;

// An InputMethod implementation based on Windows TSF API.
class COMPONENT_EXPORT(UI_BASE_IME_WIN) InputMethodWinTSF
    : public InputMethodWinBase {
 public:
  InputMethodWinTSF(internal::InputMethodDelegate* delegate,
                    HWND toplevel_window_handle);
  ~InputMethodWinTSF() override;

  // Overridden from InputMethod:
  void OnFocus() override;
  void OnBlur() override;
  bool OnUntranslatedIMEMessage(const MSG event,
                                NativeEventResult* result) override;
  void OnTextInputTypeChanged(const TextInputClient* client) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  void DetachTextInputClient(TextInputClient* client) override;
  bool IsInputLocaleCJK() const override;
  bool IsCandidatePopupOpen() const override;

  // Overridden from InputMethodBase:
  void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                 TextInputClient* focused) override;
  void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                TextInputClient* focused) override;
  void ConfirmCompositionText(bool reset_engine, bool keep_selection) override;

  void ShowVirtualKeyboardIfEnabled() override;

 private:
  class TSFEventObserver;

  // TSF event router and observer.
  std::unique_ptr<TSFEventObserver> tsf_event_observer_;
  std::unique_ptr<TSFEventRouter> tsf_event_router_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodWinTSF);
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_INPUT_METHOD_WIN_TSF_H_
