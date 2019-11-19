// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_INPUT_METHOD_WIN_IMM32_H_
#define UI_BASE_IME_WIN_INPUT_METHOD_WIN_IMM32_H_

#include <windows.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/win/imm32_manager.h"
#include "ui/base/ime/win/input_method_win_base.h"

namespace ui {

// A common InputMethod implementation based on IMM32.
class COMPONENT_EXPORT(UI_BASE_IME_WIN) InputMethodWinImm32
    : public InputMethodWinBase {
 public:
  InputMethodWinImm32(internal::InputMethodDelegate* delegate,
                      HWND toplevel_window_handle);
  ~InputMethodWinImm32() override;

  // Overridden from InputMethodBase:
  void OnFocus() override;
  void ConfirmCompositionText(bool reset_engine, bool keep_selection) override;

  // Overridden from InputMethod:
  bool OnUntranslatedIMEMessage(const MSG event,
                                NativeEventResult* result) override;
  void OnTextInputTypeChanged(const TextInputClient* client) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  void OnInputLocaleChanged() override;
  bool IsInputLocaleCJK() const override;
  bool IsCandidatePopupOpen() const override;

 protected:
  // Overridden from InputMethodBase:
  // If a derived class overrides this method, it should call parent's
  // implementation.
  void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                 TextInputClient* focused) override;
  void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                TextInputClient* focused) override;

 private:
  LRESULT OnImeSetContext(HWND window_handle,
                          UINT message,
                          WPARAM wparam,
                          LPARAM lparam,
                          BOOL* handled);
  LRESULT OnImeStartComposition(HWND window_handle,
                                UINT message,
                                WPARAM wparam,
                                LPARAM lparam,
                                BOOL* handled);
  LRESULT OnImeComposition(HWND window_handle,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam,
                           BOOL* handled);
  LRESULT OnImeEndComposition(HWND window_handle,
                              UINT message,
                              WPARAM wparam,
                              LPARAM lparam,
                              BOOL* handled);
  LRESULT OnImeNotify(UINT message,
                      WPARAM wparam,
                      LPARAM lparam,
                      BOOL* handled);

  void RefreshInputLanguage();

  // Enables or disables the IME according to the current text input type.
  void UpdateIMEState();

  // Windows IMM32 wrapper.
  // (See "ui/base/ime/win/ime_input.h" for its details.)
  ui::IMM32Manager imm32_manager_;

  // True when an IME should be allowed to process key events.
  bool enabled_;

  // True if we know for sure that a candidate window is open.
  bool is_candidate_popup_open_;

  // Window handle where composition is on-going. NULL when there is no
  // composition.
  HWND composing_window_handle_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodWinImm32);
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_INPUT_METHOD_WIN_IMM32_H_
