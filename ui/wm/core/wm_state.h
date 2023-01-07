// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WM_STATE_H_
#define UI_WM_CORE_WM_STATE_H_

#include <memory>

#include "base/component_export.h"

namespace wm {

class CaptureController;
class TransientWindowController;
class TransientWindowStackingClient;

// Installs state needed by the window manager.
class COMPONENT_EXPORT(UI_WM) WMState {
 public:
  WMState();

  WMState(const WMState&) = delete;
  WMState& operator=(const WMState&) = delete;

  ~WMState();

 private:
  std::unique_ptr<TransientWindowStackingClient> window_stacking_client_;
  std::unique_ptr<TransientWindowController> transient_window_client_;
  // NOTE: this is really only needed in ash
  std::unique_ptr<CaptureController> capture_controller_;
};

}  // namespace wm

#endif  // UI_WM_CORE_WM_STATE_H_
