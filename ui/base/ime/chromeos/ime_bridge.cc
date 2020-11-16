// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/ime_bridge.h"

#include <map>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "build/build_config.h"

namespace ui {

static IMEBridge* g_ime_bridge = nullptr;

// An implementation of IMEBridge.
class IMEBridgeImpl : public IMEBridge {
 public:
  IMEBridgeImpl()
      : current_input_context_(ui::TEXT_INPUT_TYPE_NONE,
                               ui::TEXT_INPUT_MODE_DEFAULT,
                               0,
                               ui::TextInputClient::FOCUS_REASON_NONE,
                               false /* should_do_learning */) {}

  ~IMEBridgeImpl() override = default;

  // IMEBridge override.
  IMEInputContextHandlerInterface* GetInputContextHandler() const override {
    return input_context_handler_;
  }

  // IMEBridge override.
  void SetInputContextHandler(
      IMEInputContextHandlerInterface* handler) override {
    input_context_handler_ = handler;
    for (auto& observer : observers_)
      observer.OnInputContextHandlerChanged();
  }

  // IMEBridge override.
  void SetCurrentEngineHandler(IMEEngineHandlerInterface* handler) override {
    engine_handler_ = handler;
  }

  // IMEBridge override.
  IMEEngineHandlerInterface* GetCurrentEngineHandler() const override {
    return engine_handler_;
  }

  // IMEBridge override.
  void SetCurrentInputContext(
      const IMEEngineHandlerInterface::InputContext& input_context) override {
    current_input_context_ = input_context;
  }

  // IMEBridge override.
  const IMEEngineHandlerInterface::InputContext& GetCurrentInputContext()
      const override {
    return current_input_context_;
  }

  // IMEBridge override.
  void AddObserver(ui::IMEBridgeObserver* observer) override {
    observers_.AddObserver(observer);
  }

  // IMEBridge override.
  void RemoveObserver(ui::IMEBridgeObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  // IMEBridge override.
  void MaybeSwitchEngine() override {
    for (auto& observer : observers_)
      observer.OnRequestSwitchEngine();
  }

  // IMEBridge override.
  void SetCandidateWindowHandler(
      chromeos::IMECandidateWindowHandlerInterface* handler) override {
    candidate_window_handler_ = handler;
  }

  // IMEBridge override.
  chromeos::IMECandidateWindowHandlerInterface* GetCandidateWindowHandler()
      const override {
    return candidate_window_handler_;
  }

  // IMEBridge override.
  void SetAssistiveWindowHandler(
      chromeos::IMEAssistiveWindowHandlerInterface* handler) override {
    assistive_window_handler_ = handler;
  }

  // IMEBridge override.
  chromeos::IMEAssistiveWindowHandlerInterface* GetAssistiveWindowHandler()
      const override {
    return assistive_window_handler_;
  }

 private:
  IMEInputContextHandlerInterface* input_context_handler_ = nullptr;
  IMEEngineHandlerInterface* engine_handler_ = nullptr;
  base::ObserverList<IMEBridgeObserver> observers_;
  IMEEngineHandlerInterface::InputContext current_input_context_;

  chromeos::IMECandidateWindowHandlerInterface* candidate_window_handler_ =
      nullptr;
  chromeos::IMEAssistiveWindowHandlerInterface* assistive_window_handler_ =
      nullptr;

  DISALLOW_COPY_AND_ASSIGN(IMEBridgeImpl);
};

///////////////////////////////////////////////////////////////////////////////
// IMEBridge
IMEBridge::IMEBridge() = default;

IMEBridge::~IMEBridge() = default;

// static.
void IMEBridge::Initialize() {
  if (!g_ime_bridge)
    g_ime_bridge = new IMEBridgeImpl();
}

// static.
void IMEBridge::Shutdown() {
  delete g_ime_bridge;
  g_ime_bridge = nullptr;
}

// static.
IMEBridge* IMEBridge::Get() {
  return g_ime_bridge;
}

}  // namespace ui
