// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_SOURCE_OBSERVER_H_
#define UI_ACCESSIBILITY_AX_TREE_SOURCE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/accessibility/ax_base_export.h"

namespace ui {

template <typename AXNodeSource,
          typename AXTreeDataType,
          typename AXNodeDataType>
class AXTreeSource;

// This is an interface for a class that observes changes to an `AXTreeSource`.
template <typename AXNodeSource,
          typename AXTreeDataType,
          typename AXNodeDataType>
class AX_BASE_EXPORT AXTreeSourceObserver : public base::CheckedObserver {
 public:
  ~AXTreeSourceObserver() override = default;

  virtual void OnNodeAdded(
      const AXTreeSource<AXNodeSource, AXTreeDataType, AXNodeDataType>&
          tree_source,
      const AXNodeSource& node_source) = 0;
  virtual void OnNodeUpdated(
      const AXTreeSource<AXNodeSource, AXTreeDataType, AXNodeDataType>&
          tree_source,
      const AXNodeSource& node_source) = 0;
  virtual void OnNodeRemoved(
      const AXTreeSource<AXNodeSource, AXTreeDataType, AXNodeDataType>&
          tree_source,
      const AXNodeSource& node_source) = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_SOURCE_OBSERVER_H_
