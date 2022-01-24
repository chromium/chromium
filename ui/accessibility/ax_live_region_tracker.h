// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_LIVE_REGION_TRACKER_H_
#define UI_ACCESSIBILITY_AX_LIVE_REGION_TRACKER_H_

#include <map>
#include <set>

#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

// Class that works with `AXEventGenerator` to track live regions in
// an `AXTree`, by maintaining a map from each node to the root of
// its live region. This map is used to trigger events on live region
// roots when a node in a live region changes.
class AXLiveRegionTracker {
 public:
  static bool IsLiveRegionRoot(const AXNode& node);

  explicit AXLiveRegionTracker(const AXTree& tree);
  virtual ~AXLiveRegionTracker();
  AXLiveRegionTracker(const AXLiveRegionTracker& other) = delete;
  AXLiveRegionTracker& operator=(const AXLiveRegionTracker& other) = delete;

  // Walk upwards in the tree and determine the live root for this node,
  // overriding any previously assigned live root.
  void UpdateCachedLiveRootForNode(const AXNode& node);

  void OnNodeWillBeDeleted(const AXNode& node);
  void OnAtomicUpdateFinished();
  AXNode* GetLiveRoot(const AXNode& node) const;
  AXNode* GetLiveRootIfNotBusy(const AXNode& node) const;

  const std::set<AXNodeID>& live_region_roots_with_changes() const {
    return live_region_roots_with_changes_;
  }

 private:
  void WalkTreeAndAssignLiveRootsToNodes(const AXNode& node,
                                         const AXNode* current_root);
  void QueueChangeEventForDeletedNode(const AXNode& root);

  const AXTree& tree_;

  // Map from live region node to its live region root ID.
  std::map<const AXNodeID, AXNodeID> live_region_node_to_root_id_;
  std::set<AXNodeID> deleted_node_ids_;
  std::set<AXNodeID> live_region_roots_with_changes_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_LIVE_REGION_TRACKER_H_
