// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_COMBOBOX_MODEL_OBSERVER_H_
#define UI_BASE_MODELS_COMBOBOX_MODEL_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace ui {

class ComboboxModel;

// Observer for the ComboboxModel.
class COMPONENT_EXPORT(UI_BASE) ComboboxModelObserver
    : public base::CheckedObserver {
 public:
  // Invoked when `model` has changed in some way. The observer should assume
  // everything changed.
  virtual void OnComboboxModelChanged(ComboboxModel* model) = 0;

  // Invoked when `model` is destroyed. The observer should stop observing.
  virtual void OnComboboxModelDestroying(ComboboxModel* model) = 0;

 protected:
  ~ComboboxModelObserver() override = default;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_COMBOBOX_MODEL_OBSERVER_H_
