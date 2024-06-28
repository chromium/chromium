// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_BASE_H_
#define UI_BASE_IME_INPUT_METHOD_BASE_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/event_dispatcher.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class InputMethodObserver;
class KeyEvent;
class TextInputClient;

// A helper class providing functionalities shared among ui::InputMethod
// implementations.
class COMPONENT_EXPORT(UI_BASE_IME) InputMethodBase : public InputMethod {
 public:
  InputMethodBase(const InputMethodBase&) = delete;
  InputMethodBase& operator=(const InputMethodBase&) = delete;

  ~InputMethodBase() override;

  // Overriden from InputMethod.
  void SetImeKeyEventDispatcher(
      ImeKeyEventDispatcher* ime_key_event_dispatcher) override;
  void OnFocus() override;
  void OnBlur() override;

  void SetFocusedTextInputClient(TextInputClient* client) override;
  void DetachTextInputClient(TextInputClient* client) override;
  TextInputClient* GetTextInputClient() const override;
  void SetVirtualKeyboardBounds(const gfx::Rect& new_bounds) override;

  // If a derived class overrides this method, it should call parent's
  // implementation.
  void OnTextInputTypeChanged(TextInputClient* client) override;
  TextInputType GetTextInputType() const override;
  void SetVirtualKeyboardVisibilityIfEnabled(bool should_show) override;

  void AddObserver(InputMethodObserver* observer) override;
  void RemoveObserver(InputMethodObserver* observer) override;

  VirtualKeyboardController* GetVirtualKeyboardController() override;
  void SetVirtualKeyboardControllerForTesting(
      std::unique_ptr<VirtualKeyboardController> controller) override;

 protected:
  explicit InputMethodBase(ImeKeyEventDispatcher* ime_key_event_dispatcher);
  InputMethodBase(ImeKeyEventDispatcher* ime_key_event_dispatcher,
                  std::unique_ptr<VirtualKeyboardController> controller);

  virtual void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                         TextInputClient* focused) {}
  virtual void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                        TextInputClient* focused) {}

  // Returns true if |client| is currently focused.
  bool IsTextInputClientFocused(const TextInputClient* client);

  // Checks if the focused text input client's text input type is
  // TEXT_INPUT_TYPE_NONE. Also returns true if there is no focused text
  // input client.
  bool IsTextInputTypeNone() const;

  // Convenience method to call the focused text input client's
  // OnInputMethodChanged() method. It'll only take effect if the current text
  // input type is not TEXT_INPUT_TYPE_NONE.
  void OnInputMethodChanged() const;

  [[nodiscard]] virtual ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) const;

  // Convenience method to notify all observers of TextInputClient changes.
  void NotifyTextInputStateChanged(const TextInputClient* client);

  // Convenience method to notify all observers of CaretBounds changes on
  // |client| which is the text input client with focus.
  void NotifyTextInputCaretBoundsChanged(const TextInputClient* client);

  ImeKeyEventDispatcher* ime_key_event_dispatcher() {
    return ime_key_event_dispatcher_;
  }

 private:
  raw_ptr<ImeKeyEventDispatcher, DanglingUntriaged> ime_key_event_dispatcher_;

  void SetFocusedTextInputClientInternal(TextInputClient* client);

  raw_ptr<TextInputClient, DanglingUntriaged> text_input_client_ = nullptr;

  base::ObserverList<InputMethodObserver>::Unchecked observer_list_;

  // Screen bounds of a on-screen keyboard.
  gfx::Rect keyboard_bounds_;

  std::unique_ptr<VirtualKeyboardController> keyboard_controller_;
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_BASE_H_
