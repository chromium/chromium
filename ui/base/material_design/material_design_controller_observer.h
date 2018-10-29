// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_OBSERVER_H_
#define UI_BASE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/base/ui_base_export.h"

namespace ui {

class UI_BASE_EXPORT MaterialDesignControllerObserver
    : public base::CheckedObserver {
 public:
  virtual void OnTouchUiChanged() = 0;

 protected:
  ~MaterialDesignControllerObserver() override {}
};

}  // namespace ui

#endif  // UI_BASE_MATERIAL_DESIGN_MATERIAL_DESIGN_CONTROLLER_OBSERVER_H_
