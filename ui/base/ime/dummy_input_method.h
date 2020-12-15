// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_DUMMY_INPUT_METHOD_H_
#define UI_BASE_IME_DUMMY_INPUT_METHOD_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/base/ime/input_method.h"

namespace ui {

class InputMethodObserver;

class DummyInputMethod : public InputMethod {
 public:
  DummyInputMethod();
  ~DummyInputMethod() override;

  // InputMethod overrides:
  void SetDelegate(internal::InputMethodDelegate* delegate) override;
  void OnFocus() override;
  void OnBlur() override;

#if defined(OS_WIN)
  bool OnUntranslatedIMEMessage(const MSG event,
                                NativeEventResult* result) override;
#endif

  void SetFocusedTextInputClient(TextInputClient* client) override;
  void DetachTextInputClient(TextInputClient* client) override;
  TextInputClient* GetTextInputClient() const override;
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnTextInputTypeChanged(const TextInputClient* client) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  void OnInputLocaleChanged() override;
  bool IsInputLocaleCJK() const override;
  TextInputType GetTextInputType() const override;
  TextInputMode GetTextInputMode() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  bool IsCandidatePopupOpen() const override;
  bool GetClientShouldDoLearning() override;
  void ShowVirtualKeyboardIfEnabled() override;

  void AddObserver(InputMethodObserver* observer) override;
  void RemoveObserver(InputMethodObserver* observer) override;
  VirtualKeyboardController* GetVirtualKeyboardController() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyInputMethod);
};

}  // namespace ui

#endif  // UI_BASE_IME_DUMMY_INPUT_METHOD_H_
