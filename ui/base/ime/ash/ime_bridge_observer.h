// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_IME_BRIDGE_OBSERVER_H_
#define UI_BASE_IME_ASH_IME_BRIDGE_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace ash {

// A interface to observe changes in the IMEBridge.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) IMEBridgeObserver
    : public base::CheckedObserver {
 public:
  // Called when the input context handler has changed, a signal of IME change.
  virtual void OnInputContextHandlerChanged() = 0;
};

}  // namespace ash

#endif  // UI_BASE_IME_ASH_IME_BRIDGE_OBSERVER_H_
