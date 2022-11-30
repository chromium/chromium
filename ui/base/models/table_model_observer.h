// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_TABLE_MODEL_OBSERVER_H_
#define UI_BASE_MODELS_TABLE_MODEL_OBSERVER_H_

#include "base/component_export.h"

namespace ui {

// Observer for a TableModel. Anytime the model changes, it must notify its
// observer.
class COMPONENT_EXPORT(UI_BASE) TableModelObserver {
 public:
  // Invoked when the model has been completely changed.
  virtual void OnModelChanged() = 0;

  // Invoked when a range of items has changed.
  virtual void OnItemsChanged(size_t start, size_t length) = 0;

  // Invoked when new items have been added.
  virtual void OnItemsAdded(size_t start, size_t length) = 0;

  // Invoked when a range of items has been removed.
  virtual void OnItemsRemoved(size_t start, size_t length) = 0;

  // Invoked when a range of items has been moved to a different position.
  virtual void OnItemsMoved(size_t old_start, size_t length, size_t new_start) {
  }

 protected:
  virtual ~TableModelObserver() = default;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_TABLE_MODEL_OBSERVER_H_
