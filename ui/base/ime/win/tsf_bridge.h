// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_TSF_BRIDGE_H_
#define UI_BASE_IME_WIN_TSF_BRIDGE_H_

#include <windows.h>

#include <msctf.h>
#include <wrl/client.h>

#include <memory>

#include "base/component_export.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"

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
  TSFBridge(const TSFBridge&) = delete;
  TSFBridge& operator=(const TSFBridge&) = delete;

  virtual ~TSFBridge() = default;

  // Returns the thread local TSFBridge instance. Initialize() must be called
  // first. Do not cache this pointer and use it after TSFBridge Shutdown().
  static TSFBridge* GetInstance();

  // Sets the thread local instance. Must be called before any calls to
  // GetInstance().
  static HRESULT Initialize();

  // Sets the thread local instance for testing only. Must be called before
  // any calls to GetInstance().
  static void InitializeForTesting();

  // Sets the new instance of TSFBridge in the thread-local storage (such as
  // MockTSFBridge for testing). This function replaces previous TSFBridge
  // instance with the newInstance and also deletes the old instance.
  static void ReplaceThreadLocalTSFBridge(
      std::unique_ptr<TSFBridge> new_instance);

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

  // Lets TSFTextstore see ImeKeyEventDispatcher instance when in focus.
  virtual void SetImeKeyEventDispatcher(
      ImeKeyEventDispatcher* ime_key_event_dispatcher) = 0;

  // Remove ImeKeyEventDispatcher instance from TSFTextStore when not in focus.
  virtual void RemoveImeKeyEventDispatcher(
      ImeKeyEventDispatcher* ime_key_event_dispatcher) = 0;

  // Returns whether the system's input language is CJK.
  virtual bool IsInputLanguageCJK() = 0;

  // Obtains current thread manager.
  virtual Microsoft::WRL::ComPtr<ITfThreadMgr> GetThreadManager() = 0;

  // Returns the focused text input client.
  virtual TextInputClient* GetFocusedTextInputClient() const = 0;

  // Notify TSF when a frame with a committed Url has been focused.
  virtual void OnUrlChanged() = 0;

 protected:
  // Uses GetInstance() instead.
  TSFBridge() = default;
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_TSF_BRIDGE_H_
