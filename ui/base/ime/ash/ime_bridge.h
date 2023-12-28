// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_IME_BRIDGE_H_
#define UI_BASE_IME_ASH_IME_BRIDGE_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/base/ime/ash/ime_assistive_window_handler_interface.h"
#include "ui/base/ime/ash/ime_bridge_observer.h"
#include "ui/base/ime/ash/ime_candidate_window_handler_interface.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/ash/text_input_target.h"

class IMECandidateWindowHandlerInterface;
class IMEAssistiveWindowHandlerInterface;

namespace ash {

// IMEBridge provides access of each IME related handler. This class
// is used for IME implementation.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) IMEBridge {
 public:
  IMEBridge(const IMEBridge&) = delete;
  IMEBridge& operator=(const IMEBridge&) = delete;
  ~IMEBridge();

  // Constructs the global singleton (if not available yet) then returns it.
  // TODO(crbug/1279743): Use dependency injection instead of global singleton.
  static IMEBridge* Get();

  // Returns current TextInputTarget. This function returns nullptr if input
  // context is not ready to use.
  // TODO(b/245020074): Rename this method.
  TextInputTarget* GetInputContextHandler() const;

  // Updates current TextInputTarget. If there is no active input context,
  // pass nullptr for |handler|. Caller must release |handler|.
  // TODO(b/245020074): Rename this method.
  void SetInputContextHandler(TextInputTarget* handler);

  // Updates current TextInputMethod. If there is no active engine service, pass
  // nullptr for |handler|. Caller must release |handler|.
  // TODO(b/245020074): Rename this method.
  void SetCurrentEngineHandler(TextInputMethod* handler);

  // Returns current TextInputMethod. This function returns nullptr if current
  // engine is not ready to use.
  // TODO(b/245020074): Rename this method.
  TextInputMethod* GetCurrentEngineHandler() const;

  // Updates the current input context.
  // This is called from `InputMethodAsh`.
  void SetCurrentInputContext(
      const TextInputMethod::InputContext& input_context);

  // Returns the current input context.
  // This is called from InputMethodEngine.
  const TextInputMethod::InputContext& GetCurrentInputContext() const;

  // Add or remove observers of events such as switching engines, etc.
  void AddObserver(IMEBridgeObserver* observer);
  void RemoveObserver(IMEBridgeObserver* observer);

  // Returns current CandidateWindowHandler. This function returns nullptr if
  // current candidate window is not ready to use.
  IMECandidateWindowHandlerInterface* GetCandidateWindowHandler() const;

  // Updates current CandidatWindowHandler. If there is no active candidate
  // window service, pass nullptr for |handler|. Caller must release |handler|.
  void SetCandidateWindowHandler(IMECandidateWindowHandlerInterface* handler);

  IMEAssistiveWindowHandlerInterface* GetAssistiveWindowHandler() const;
  void SetAssistiveWindowHandler(IMEAssistiveWindowHandlerInterface* handler);

 private:
  IMEBridge();

  // TODO(b/245020074): Rename this member.
  raw_ptr<TextInputTarget, DanglingUntriaged> input_context_handler_ = nullptr;
  // TODO(b/245020074): Rename this member.
  raw_ptr<TextInputMethod, DanglingUntriaged> engine_handler_ = nullptr;
  base::ObserverList<IMEBridgeObserver> observers_;
  TextInputMethod::InputContext current_input_context_;

  raw_ptr<IMECandidateWindowHandlerInterface, LeakedDanglingUntriaged>
      candidate_window_handler_ = nullptr;
  raw_ptr<IMEAssistiveWindowHandlerInterface, DanglingUntriaged>
      assistive_window_handler_ = nullptr;
};

}  // namespace ash

#endif  // UI_BASE_IME_ASH_IME_BRIDGE_H_
