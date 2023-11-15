// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_MANAGER_OBSERVER_H_
#define UI_DISPLAY_MANAGER_DISPLAY_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/display/manager/display_manager_export.h"

namespace display {

class DISPLAY_MANAGER_EXPORT DisplayManagerObserver
    : public base::CheckedObserver {
 public:
  // Called before the DisplayManager begins processing a change / update to
  // the current display configuration.
  virtual void OnWillProcessDisplayChanges() {}

  // Called after the display configuration changes processed by the
  // DisplayManager have completed.
  virtual void OnDidProcessDisplayChanges() {}
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_MANAGER_OBSERVER_H_
