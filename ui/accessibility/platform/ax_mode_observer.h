// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_MODE_OBSERVER_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_MODE_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "ui/accessibility/ax_mode.h"

namespace ui {

enum class AssistiveTech;

class COMPONENT_EXPORT(AX_PLATFORM) AXModeObserver
    : public base::CheckedObserver {
 public:
  ~AXModeObserver() override;

  // Notifies when accessibility mode changes.
  virtual void OnAXModeAdded(AXMode mode) {}

  // Notifies when an assistive tech becomes active or inactive.
  virtual void OnAssistiveTechChanged(AssistiveTech assistive_tech) {}
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_MODE_OBSERVER_H_
