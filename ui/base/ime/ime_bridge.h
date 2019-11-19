// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IME_BRIDGE_H_
#define UI_BASE_IME_IME_BRIDGE_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/base/ime/ime_bridge_observer.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/ime_input_context_handler_interface.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/ime_candidate_window_handler_interface.h"

namespace chromeos {
class IMECandidateWindowHandlerInterface;
}
#endif

namespace ui {

// IMEBridge provides access of each IME related handler. This class
// is used for IME implementation.
class COMPONENT_EXPORT(UI_BASE_IME) IMEBridge {
 public:
  virtual ~IMEBridge();

  // Allocates the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Releases the global instance.
  static void Shutdown();

  // Returns IMEBridge global instance. Initialize() must be called first.
  static IMEBridge* Get();

  // Returns current InputContextHandler. This function returns NULL if input
  // context is not ready to use.
  virtual IMEInputContextHandlerInterface* GetInputContextHandler() const = 0;

  // Updates current InputContextHandler. If there is no active input context,
  // pass NULL for |handler|. Caller must release |handler|.
  virtual void SetInputContextHandler(
      IMEInputContextHandlerInterface* handler) = 0;

  // Updates current EngineHandler. If there is no active engine service, pass
  // NULL for |handler|. Caller must release |handler|.
  virtual void SetCurrentEngineHandler(IMEEngineHandlerInterface* handler) = 0;

  // Returns current EngineHandler. This function returns NULL if current engine
  // is not ready to use.
  virtual IMEEngineHandlerInterface* GetCurrentEngineHandler() const = 0;

  // Updates the current input context.
  // This is called from InputMethodChromeOS.
  virtual void SetCurrentInputContext(
      const IMEEngineHandlerInterface::InputContext& input_context) = 0;

  // Returns the current input context.
  // This is called from InputMethodEngine.
  virtual const IMEEngineHandlerInterface::InputContext&
  GetCurrentInputContext() const = 0;

  // Add or remove observers of events such as switching engines, etc.
  virtual void AddObserver(ui::IMEBridgeObserver* observer) = 0;
  virtual void RemoveObserver(ui::IMEBridgeObserver* observer) = 0;

  // Switches the engine handler upon top level window focus change.
  virtual void MaybeSwitchEngine() = 0;

#if defined(OS_CHROMEOS)
  // Returns current CandidateWindowHandler. This function returns NULL if
  // current candidate window is not ready to use.
  virtual chromeos::IMECandidateWindowHandlerInterface*
  GetCandidateWindowHandler() const = 0;

  // Updates current CandidatWindowHandler. If there is no active candidate
  // window service, pass NULL for |handler|. Caller must release |handler|.
  virtual void SetCandidateWindowHandler(
      chromeos::IMECandidateWindowHandlerInterface* handler) = 0;
#endif

 protected:
  IMEBridge();

 private:
  DISALLOW_COPY_AND_ASSIGN(IMEBridge);
};

}  // namespace ui

#endif  // UI_BASE_IME_IME_BRIDGE_H_
