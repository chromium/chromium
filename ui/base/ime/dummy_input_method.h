// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_DUMMY_INPUT_METHOD_H_
#define UI_BASE_IME_DUMMY_INPUT_METHOD_H_

#include "build/build_config.h"
#include "ui/base/ime/input_method.h"

namespace ui {

class InputMethodObserver;

class DummyInputMethod : public InputMethod {
 public:
  DummyInputMethod();

  DummyInputMethod(const DummyInputMethod&) = delete;
  DummyInputMethod& operator=(const DummyInputMethod&) = delete;

  ~DummyInputMethod() override;

  // InputMethod overrides:
  void SetImeKeyEventDispatcher(
      ImeKeyEventDispatcher* ime_key_event_dispatcher) override;
  void OnFocus() override;
  void OnBlur() override;

#if BUILDFLAG(IS_WIN)
  bool OnUntranslatedIMEMessage(const CHROME_MSG event,
                                NativeEventResult* result) override;
  void OnInputLocaleChanged() override;
  bool IsInputLocaleCJK() const override;
  void OnUrlChanged() override;
#endif

  void SetFocusedTextInputClient(TextInputClient* client) override;
  void DetachTextInputClient(TextInputClient* client) override;
  TextInputClient* GetTextInputClient() const override;
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnTextInputTypeChanged(TextInputClient* client) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  TextInputType GetTextInputType() const override;
  bool IsCandidatePopupOpen() const override;
  void SetVirtualKeyboardVisibilityIfEnabled(bool should_show) override;

  void AddObserver(InputMethodObserver* observer) override;
  void RemoveObserver(InputMethodObserver* observer) override;
  VirtualKeyboardController* GetVirtualKeyboardController() override;
  void SetVirtualKeyboardControllerForTesting(
      std::unique_ptr<VirtualKeyboardController> controller) override;
};

}  // namespace ui

#endif  // UI_BASE_IME_DUMMY_INPUT_METHOD_H_
