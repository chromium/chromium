// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_
#define UI_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_

#include <map>
#include <string>
#include <utility>

#include "base/macros.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree_id.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ui {

class AXHostDelegate;

// This class generates and saves a runtime id for an accessibility tree.
// It provides a few distinct forms of generating an id:
//     - from a frame id (which consists of a process and routing id)
//     - from a backing |AXHostDelegate| object
//
// The first form allows underlying instances to change but refer to the same
// frame.
// The second form allows this registry to track the object for later retrieval.
class AX_EXPORT AXTreeIDRegistry {
 public:
  using FrameID = std::pair<int, int>;

  // Get the single instance of this class.
  static AXTreeIDRegistry* GetInstance();

  // Methods for FrameID ax tree id generation, and retrieval.
  AXTreeID GetOrCreateAXTreeID(int process_id, int routing_id);
  FrameID GetFrameID(AXTreeID ax_tree_id);

  // Retrieve an |AXHostDelegate|.
  AXHostDelegate* GetHostDelegate(AXTreeID ax_tree_id);

  void RemoveAXTreeID(AXTreeID ax_tree_id);

 private:
  friend struct base::DefaultSingletonTraits<AXTreeIDRegistry>;
  friend AXHostDelegate;

  // Methods for AXHostDelegate ax tree id generation.
  AXTreeID GetOrCreateAXTreeID(AXHostDelegate* delegate);
  void SetDelegateForID(AXHostDelegate* delegate, AXTreeID id);

  AXTreeIDRegistry();
  virtual ~AXTreeIDRegistry();

  // Tracks the current unique ax tree id.
  int ax_tree_id_counter_;

  // Maps an accessibility tree to its frame via ids.
  std::map<AXTreeID, FrameID> ax_tree_to_frame_id_map_;

  // Maps frames to an accessibility tree via ids.
  std::map<FrameID, AXTreeID> frame_to_ax_tree_id_map_;

  // Maps an id to its host delegate.
  std::map<AXTreeID, AXHostDelegate*> id_to_host_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeIDRegistry);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_
