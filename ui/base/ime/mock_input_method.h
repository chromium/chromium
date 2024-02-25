// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_MOCK_INPUT_METHOD_H_
#define UI_BASE_IME_MOCK_INPUT_METHOD_H_

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/virtual_keyboard_controller_stub.h"

namespace ui {

class KeyEvent;
class TextInputClient;

// A mock ui::InputMethod implementation for testing. You can get the instance
// of this class as the global input method with calling
// SetUpInputMethodFactoryForTesting() which is declared in
// ui/base/ime/init/input_method_factory.h
class COMPONENT_EXPORT(UI_BASE_IME) MockInputMethod : public InputMethod {
 public:
  explicit MockInputMethod(ImeKeyEventDispatcher* ime_key_event_dispatcher);

  MockInputMethod(const MockInputMethod&) = delete;
  MockInputMethod& operator=(const MockInputMethod&) = delete;

  ~MockInputMethod() override;

  // Overriden from InputMethod.
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

 private:
  raw_ptr<TextInputClient, DanglingUntriaged> text_input_client_ = nullptr;
  base::ObserverList<InputMethodObserver>::Unchecked observer_list_;
  raw_ptr<ImeKeyEventDispatcher> ime_key_event_dispatcher_;

  VirtualKeyboardControllerStub keyboard_controller_;
};

}  // namespace ui

#endif  // UI_BASE_IME_MOCK_INPUT_METHOD_H_
