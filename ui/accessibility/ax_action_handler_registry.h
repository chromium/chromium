// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ACTION_HANDLER_REGISTRY_H_
#define UI_ACCESSIBILITY_AX_ACTION_HANDLER_REGISTRY_H_

#include <cstdint>
#include <map>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_tree_id.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace ui {

class AXActionHandlerBase;

// An observer is informed of all automation actions.
class AXActionHandlerObserver : public base::CheckedObserver {
 public:
  // This method is intended to route actions to their final destinations. The
  // routing is asynchronous and we do not know which observers intend to
  // respond to which actions -- so we forward all actions to all observers.
  // Only the observer that owns the unique |tree_id| will perform the action.
  virtual void PerformAction(const AXActionData& action_data) {}

  // Informs the observer that a tree has been removed.
  virtual void TreeRemoved(AXTreeID tree_id) {}
};

// This class generates and saves a runtime id for an accessibility tree.
// It provides a few distinct forms of generating an id:
//     - from a frame id (which consists of a process and routing id)
//     - from a backing |AXActionHandlerBase| object
//
// The first form allows underlying instances to change but refer to the same
// frame.
// The second form allows this registry to track the object for later retrieval.
class AX_BASE_EXPORT AXActionHandlerRegistry final {
 public:
  using FrameID = std::pair<int, int>;

  // Get the single instance of this class.
  static AXActionHandlerRegistry* GetInstance();

  virtual ~AXActionHandlerRegistry();
  AXActionHandlerRegistry(const AXActionHandlerRegistry&) = delete;
  AXActionHandlerRegistry& operator=(const AXActionHandlerRegistry&) = delete;

  // Gets the frame id based on an ax tree id.
  FrameID GetFrameID(const AXTreeID& ax_tree_id);

  // Gets an ax tree id from a frame id.
  AXTreeID GetAXTreeID(FrameID frame_id);

  // Retrieve an |AXActionHandlerBase| based on an ax tree id.
  AXActionHandlerBase* GetActionHandler(AXTreeID ax_tree_id);

  // Set a mapping between an AXTreeID and AXActionHandlerBase explicitly.
  void SetAXTreeID(const AXTreeID& ax_tree_id,
                   AXActionHandlerBase* action_handler);

  // Removes an ax tree id, and its associated delegate and frame id (if it
  // exists).
  void RemoveAXTreeID(AXTreeID ax_tree_id);

  // Associate a frame id with an ax tree id.
  void SetFrameIDForAXTreeID(const FrameID& frame_id,
                             const AXTreeID& ax_tree_id);

  void AddObserver(AXActionHandlerObserver* observer);
  void RemoveObserver(AXActionHandlerObserver* observer);

  // Calls PerformAction on all observers.
  void PerformAction(const AXActionData& action_data);

 private:
  friend base::NoDestructor<AXActionHandlerRegistry>;

  // Allows registration of tree ids meant to be internally by AXActionHandler*.
  // These typically involve the creation of a new tree id.
  friend AXActionHandler;
  friend AXActionHandlerBase;

  AXActionHandlerRegistry();

  // Get or create a ax tree id keyed on |handler|.
  AXTreeID GetOrCreateAXTreeID(AXActionHandlerBase* handler);

  // Maps an accessibility tree to its frame via ids.
  std::map<AXTreeID, FrameID> ax_tree_to_frame_id_map_;

  // Maps frames to an accessibility tree via ids.
  std::map<FrameID, AXTreeID> frame_to_ax_tree_id_map_;

  // Maps an id to its handler.
  std::map<AXTreeID, raw_ptr<AXActionHandlerBase, CtnExperimental>>
      id_to_action_handler_;

  // Tracks all observers.
  base::ObserverList<AXActionHandlerObserver> observers_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ACTION_HANDLER_REGISTRY_H_
