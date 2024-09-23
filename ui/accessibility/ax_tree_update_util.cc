// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_update_util.h"

#include "base/metrics/histogram_macros.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

// Two tree updates can be merged into one if the second one
// doesn't clear a subtree, doesn't have new tree data, and
// doesn't have a new root id - in other words the second tree
// update consists of only changes to nodes.
bool AXTreeUpdatesCanBeMerged(const AXTreeUpdate& u1, const AXTreeUpdate& u2) {
  if (u2.node_id_to_clear)
    return false;

  if (u2.has_tree_data && u2.tree_data != u1.tree_data)
    return false;

  if (u2.root_id != u1.root_id)
    return false;

  return true;
}

bool MergeAXTreeUpdates(const std::vector<AXTreeUpdate>& src,
                        std::vector<AXTreeUpdate>* dst) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Accessibility.Performance.MergeAXTreeUpdates");

  size_t merge_count = 0;
  for (size_t i = 1; i < src.size(); i++) {
    if (AXTreeUpdatesCanBeMerged(src[i - 1], src[i]))
      merge_count++;
  }

  // Doing a single merge isn't necessarily worth it because
  // copying the tree updates takes time too so the total
  // savings is less. But two more more merges is probably
  // worth the overhead of copying.
  if (merge_count < 2)
    return false;

  dst->resize(src.size() - merge_count);
  if (features::IsUseMoveNotCopyInMergeTreeUpdateEnabled()) {
    (*dst)[0] = std::move(const_cast<AXTreeUpdate&>(src[0]));
  } else {
    (*dst)[0] = src[0];
  }
  size_t dst_index = 0;
  for (size_t i = 1; i < src.size(); i++) {
    if (AXTreeUpdatesCanBeMerged(src[i - 1], src[i])) {
      std::vector<AXNodeData>& dst_nodes = (*dst)[dst_index].nodes;
      if (features::IsUseMoveNotCopyInMergeTreeUpdateEnabled()) {
        std::vector<AXNodeData>& src_nodes =
            const_cast<std::vector<AXNodeData>&>(src[i].nodes);
        for (auto& src_node : src_nodes) {
          dst_nodes.emplace_back(std::move(src_node));
        }
      } else {
        const std::vector<AXNodeData>& src_nodes = src[i].nodes;
        dst_nodes.insert(dst_nodes.end(), src_nodes.begin(), src_nodes.end());
      }
    } else {
      dst_index++;
      (*dst)[dst_index] = src[i];
    }
  }

  return true;
}

}  // namespace ui
