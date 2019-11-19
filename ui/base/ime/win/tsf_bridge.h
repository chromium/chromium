// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_TSF_BRIDGE_H_
#define UI_BASE_IME_WIN_TSF_BRIDGE_H_

#include <msctf.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/component_export.h"
#include "base/macros.h"

namespace ui {
class TextInputClient;

// TSFBridge provides high level IME related operations on top of Text Services
// Framework (TSF). TSFBridge is managed by TLS because TSF related stuff is
// associated with each thread and not allowed to access across thread boundary.
// To be consistent with IMM32 behavior, TSFBridge is shared in the same thread.
// TSFBridge is used by the web content text inputting field, for example
// DisableIME() should be called if a password field is focused.
//
// TSFBridge also manages connectivity between TSFTextStore which is the backend
// of text inputting and current focused TextInputClient.
//
// All methods in this class must be used in UI thread.
class COMPONENT_EXPORT(UI_BASE_IME_WIN) TSFBridge {
 public:
  virtual ~TSFBridge();

  // Returns the thread local TSFBridge instance. Initialize() must be called
  // first. Do not cache this pointer and use it after TSFBridge Shutdown().
  static TSFBridge* GetInstance();

  // Sets the thread local instance. Must be called before any calls to
  // GetInstance().
  static void Initialize();

  // Injects an alternative TSFBridge such as MockTSFBridge for testing. The
  // injected object should be released by the caller. This function returns
  // previous TSFBridge pointer with ownership.
  static TSFBridge* ReplaceForTesting(TSFBridge* bridge);

  // Destroys the thread local instance.
  static void Shutdown();

  // Handles TextInputTypeChanged event. RWHVW is responsible for calling this
  // handler whenever renderer's input text type is changed. Does nothing
  // unless |client| is focused.
  virtual void OnTextInputTypeChanged(const TextInputClient* client) = 0;

  // Sends an event to TSF manager that the text layout should be updated.
  virtual void OnTextLayoutChanged() = 0;

  // Cancels the ongoing composition if exists.
  // Returns true if there is no composition.
  // Returns false if an edit session is on-going.
  // Returns false if an error occures.
  virtual bool CancelComposition() = 0;

  // Confirms the ongoing composition if exists.
  // Returns true if there is no composition.
  // Returns false if an edit session is on-going.
  // Returns false if an error occures.
  virtual bool ConfirmComposition() = 0;

  // Sets currently focused TextInputClient.
  // Caller must free |client|.
  virtual void SetFocusedClient(HWND focused_window,
                                TextInputClient* client) = 0;

  // Removes currently focused TextInputClient.
  // Caller must free |client|.
  virtual void RemoveFocusedClient(TextInputClient* client) = 0;

  // Lets TSFTextstore see InputMethodDelegate instance when in focus.
  virtual void SetInputMethodDelegate(
      internal::InputMethodDelegate* delegate) = 0;

  // Remove InputMethodDelegate instance from TSFTextStore when not in focus.
  virtual void RemoveInputMethodDelegate() = 0;

  // Returns whether the system's input language is CJK.
  virtual bool IsInputLanguageCJK() = 0;

  // Obtains current thread manager.
  virtual Microsoft::WRL::ComPtr<ITfThreadMgr> GetThreadManager() = 0;

  // Returns the focused text input client.
  virtual TextInputClient* GetFocusedTextInputClient() const = 0;

  // Sets the input panel policy in TSFTextStore so that input service
  // could invoke the software input panel (SIP) on Windows.
  // input_panel_policy_manual equals to false would make the SIP policy
  // to automatic meaning TSF would raise/dismiss the SIP based on TSFTextStore
  // focus and other heuristics that input service have added on Windows to
  // provide a consistent behavior across all apps on Windows.
  // input_panel_policy_manual equals to true would make the SIP policy to
  // manual meaning TSF wouldn't raise/dismiss the SIP automatically. This is
  // used to control the SIP behavior based on user interaction with the page.
  virtual void SetInputPanelPolicy(bool input_panel_policy_manual) = 0;

 protected:
  // Uses GetInstance() instead.
  TSFBridge();

 private:
  DISALLOW_COPY_AND_ASSIGN(TSFBridge);
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_TSF_BRIDGE_H_
