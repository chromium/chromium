// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_H_
#define UI_BASE_IME_INPUT_METHOD_H_

#include <stdint.h>

#include "build/build_config.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class VirtualKeyboardController;
class InputMethodObserver;
class KeyEvent;
class ImeKeyEventDispatcher;
class TextInputClient;

// An interface implemented by an object that encapsulates a native input method
// service provided by the underlying operating system, and acts as a "system
// wide" input method for all Chrome windows. A class that implements this
// interface should behave as follows:
// - Receives a keyboard event directly from a message dispatcher for the
//   system through the InputMethod::DispatchKeyEvent API, and forwards it to
//   an underlying input method for the OS.
// - The input method should handle the key event either of the following ways:
//   1) Send the original key down event to the focused window, which is e.g.
//      a NativeWidgetAura (NWA) or a RenderWidgetHostViewAura (RWHVA), using
//      ImeKeyEventDispatcher API, then send a Char event using
//      TextInputClient::InsertChar API to a text input client, which is, again,
//      e.g. NWA or RWHVA, and then send the original key up event to the same
//      window.
//   2) Send VKEY_PROCESSKEY event to the window using the DispatchKeyEvent API,
//      then update IME status (e.g. composition text) using TextInputClient,
//      and then send the original key up event to the window.
// - Keeps track of the focused TextInputClient to see which client can call
//   APIs, OnTextInputTypeChanged, OnCaretBoundsChanged, and CancelComposition,
//   that change the state of the input method.
// In Aura environment, aura::WindowTreeHost creates an instance of
// ui::InputMethod and owns it.
class InputMethod {
 public:
#if BUILDFLAG(IS_WIN)
  typedef LRESULT NativeEventResult;
#else
  typedef int32_t NativeEventResult;
#endif

  virtual ~InputMethod() = default;

  // Sets the key event dispatcher used by this InputMethod instance. It should
  // only be called by an object which manages the whole UI.
  virtual void SetImeKeyEventDispatcher(
      ImeKeyEventDispatcher* ime_key_event_dispatcher) = 0;

  // Called when the top-level system window gets keyboard focus.
  virtual void OnFocus() = 0;

  // Called when the top-level system window loses keyboard focus.
  virtual void OnBlur() = 0;

#if BUILDFLAG(IS_WIN)
  // Called when the focused window receives native IME messages that are not
  // translated into other predefined event callbacks. Currently this method is
  // used only for IME functionalities specific to Windows.
  virtual bool OnUntranslatedIMEMessage(const CHROME_MSG event,
                                        NativeEventResult* result) = 0;

  // Called by the focused client whenever its input locale is changed.
  // This method is currently used only on Windows.
  // This method does not take a parameter of TextInputClient for historical
  // reasons.
  // TODO(ime): Consider to take a parameter of TextInputClient.
  virtual void OnInputLocaleChanged() = 0;

  // Returns whether the system input locale is in CJK languages.
  // This is only used in Windows platforms.
  virtual bool IsInputLocaleCJK() const = 0;

  // Called when a frame with a committed Url has received focus.
  virtual void OnUrlChanged() = 0;
#endif

  // Sets the text input client which receives text input events such as
  // SetCompositionText(). |client| can be NULL. A gfx::NativeWindow which
  // implementes TextInputClient interface, e.g. NWA and RWHVA, should register
  // itself by calling the method when it is focused, and unregister itself by
  // calling the method with NULL when it is unfocused.
  virtual void SetFocusedTextInputClient(TextInputClient* client) = 0;

  // Detaches and forgets the |client| regardless of whether it has the focus or
  // not.  This method is meant to be called when the |client| is going to be
  // destroyed.
  virtual void DetachTextInputClient(TextInputClient* client) = 0;

  // Gets the current text input client. Returns NULL when no client is set.
  virtual TextInputClient* GetTextInputClient() const = 0;

  // Dispatches a key event to the input method. The key event will be
  // dispatched back to the caller via
  // ui::InputMethodDelegate::DispatchKeyEventPostIME(), once it's processed by
  // the input method. It should only be called by a message dispatcher.
  [[nodiscard]] virtual ui::EventDispatchDetails DispatchKeyEvent(
      ui::KeyEvent* event) = 0;

  // Called by the focused client whenever its text input type is changed.
  // Before calling this method, the focused client must confirm or clear
  // existing composition text and call InputMethod::CancelComposition() when
  // necessary. Otherwise unexpected behavior may happen. This method has no
  // effect if the client is not the focused client.
  virtual void OnTextInputTypeChanged(TextInputClient* client) = 0;

  // Called by the focused client whenever its caret bounds is changed.
  // This method has no effect if the client is not the focused client.
  virtual void OnCaretBoundsChanged(const TextInputClient* client) = 0;

  // Called by the focused client to ask the input method cancel the ongoing
  // composition session. This method has no effect if the client is not the
  // focused client.
  virtual void CancelComposition(const TextInputClient* client) = 0;

  // TODO(yoichio): Following 3 methods(GetTextInputType, GetTextInputMode and
  // CanComposeInline) calls client's same method and returns its value. It is
  // not InputMethod itself's infomation. So rename these to
  // GetClientTextInputType and so on.
  // Gets the text input type of the focused text input client. Returns
  // ui::TEXT_INPUT_TYPE_NONE if there is no focused client.
  virtual TextInputType GetTextInputType() const = 0;

  // Returns true if we know for sure that a candidate window (or IME suggest,
  // etc.) is open.  Returns false if no popup window is open or the detection
  // of IME popups is not supported.
  virtual bool IsCandidatePopupOpen() const = 0;

  // Sets visibility of the virtual keyboard, if enabled already.
  virtual void SetVirtualKeyboardVisibilityIfEnabled(bool should_show) = 0;

  // Management of the observer list.
  virtual void AddObserver(InputMethodObserver* observer) = 0;
  virtual void RemoveObserver(InputMethodObserver* observer) = 0;

  // Set screen bounds of the virtual keyboard.
  virtual void SetVirtualKeyboardBounds(const gfx::Rect& new_bounds) {}

  // Return the keyboard controller.
  virtual VirtualKeyboardController* GetVirtualKeyboardController() = 0;

  // Sets a keyboard controller for testing.
  virtual void SetVirtualKeyboardControllerForTesting(
      std::unique_ptr<VirtualKeyboardController> controller) = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_H_
