// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_BASE_H_
#define UI_BASE_IME_INPUT_METHOD_BASE_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "ui/base/ime/ime_input_context_handler_interface.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/event_dispatcher.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class IMEEngineHandlerInterface;
class InputMethodObserver;
class KeyEvent;
class TextInputClient;

// A helper class providing functionalities shared among ui::InputMethod
// implementations.
class COMPONENT_EXPORT(UI_BASE_IME) InputMethodBase
    : public InputMethod,
      public base::SupportsWeakPtr<InputMethodBase>,
      public IMEInputContextHandlerInterface {
 public:
  ~InputMethodBase() override;

  // Overriden from InputMethod.
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
  void SetOnScreenKeyboardBounds(const gfx::Rect& new_bounds) override;

  // If a derived class overrides this method, it should call parent's
  // implementation.
  void OnTextInputTypeChanged(const TextInputClient* client) override;
  void OnInputLocaleChanged() override;
  bool IsInputLocaleCJK() const override;

  TextInputType GetTextInputType() const override;
  TextInputMode GetTextInputMode() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  bool GetClientShouldDoLearning() override;
  void ShowVirtualKeyboardIfEnabled() override;

  void AddObserver(InputMethodObserver* observer) override;
  void RemoveObserver(InputMethodObserver* observer) override;

  InputMethodKeyboardController* GetInputMethodKeyboardController() override;

 protected:
  explicit InputMethodBase(internal::InputMethodDelegate* delegate);
  InputMethodBase(internal::InputMethodDelegate* delegate,
                  std::unique_ptr<InputMethodKeyboardController> controller);

  virtual void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                         TextInputClient* focused) {}
  virtual void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                        TextInputClient* focused) {}

  // IMEInputContextHandlerInterface:
  void CommitText(const std::string& text) override;
  void UpdateCompositionText(const CompositionText& text,
                             uint32_t cursor_pos,
                             bool visible) override;

#if defined(OS_CHROMEOS)
  bool SetCompositionRange(
      uint32_t before,
      uint32_t after,
      const std::vector<ui::ImeTextSpan>& text_spans) override;
  bool SetSelectionRange(uint32_t start, uint32_t end) override;
#endif

  void DeleteSurroundingText(int32_t offset, uint32_t length) override;
  SurroundingTextInfo GetSurroundingTextInfo() override;
  void SendKeyEvent(KeyEvent* event) override;
  InputMethod* GetInputMethod() override;
  void ConfirmCompositionText(bool reset_engine, bool keep_selection) override;
  bool HasCompositionText() override;

  // Sends a fake key event for IME composing without physical key events.
  // Returns true if the faked key event is stopped propagation.
  bool SendFakeProcessKeyEvent(bool pressed) const;

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

  virtual ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) const WARN_UNUSED_RESULT;

  // Convenience method to notify all observers of TextInputClient changes.
  void NotifyTextInputStateChanged(const TextInputClient* client);

  // Convenience method to notify all observers of CaretBounds changes on
  // |client| which is the text input client with focus.
  void NotifyTextInputCaretBoundsChanged(const TextInputClient* client);

  // Gets the bounds of the composition text or cursor in |client|.
  std::vector<gfx::Rect> GetCompositionBounds(const TextInputClient* client);

  internal::InputMethodDelegate* delegate() const { return delegate_; }

  static IMEEngineHandlerInterface* GetEngine();

 private:
  internal::InputMethodDelegate* delegate_;

  // InputMethod:
  const std::vector<std::unique_ptr<ui::KeyEvent>>& GetKeyEventsForTesting()
      override;

  void SetFocusedTextInputClientInternal(TextInputClient* client);

  TextInputClient* text_input_client_ = nullptr;

  base::ObserverList<InputMethodObserver>::Unchecked observer_list_;

  std::vector<std::unique_ptr<ui::KeyEvent>> key_events_for_testing_;

  // Screen bounds of a on-screen keyboard.
  gfx::Rect keyboard_bounds_;

  std::unique_ptr<InputMethodKeyboardController> const keyboard_controller_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBase);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_BASE_H_
