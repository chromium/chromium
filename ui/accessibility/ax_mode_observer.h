// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_MODE_OBSERVER_H_
#define UI_ACCESSIBILITY_AX_MODE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_mode.h"

namespace ui {

class AX_EXPORT AXModeObserver : public base::CheckedObserver {
 public:
  ~AXModeObserver() override;

  // Notifies when accessibility mode changes.
  virtual void OnAXModeAdded(AXMode mode) = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_MODE_OBSERVER_H_
