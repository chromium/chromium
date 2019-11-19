// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IME_BRIDGE_OBSERVER_H_
#define UI_BASE_IME_IME_BRIDGE_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace ui {

// A interface to observe changes in the IMEBridge.
class COMPONENT_EXPORT(UI_BASE_IME) IMEBridgeObserver
    : public base::CheckedObserver {
 public:
  // Called when requesting to switch the engine handler from ui::InputMethod.
  virtual void OnRequestSwitchEngine() = 0;

  // Called when the input context handler has changed, a signal of IME change.
  virtual void OnInputContextHandlerChanged() = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_IME_BRIDGE_OBSERVER_H_
