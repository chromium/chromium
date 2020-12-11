// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/views_ax_tree_manager.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

ViewsAXTreeManager::ViewsAXTreeManager(Widget* widget)
    : widget_(widget),
      tree_source_(cache_.GetOrCreate(widget),
                   ui::AXTreeID::CreateNewAXTreeID(),
                   &cache_),
      tree_serializer_(&tree_source_),
      event_generator_(&ax_tree_) {
  DCHECK(widget);
  ui::AXTreeManagerMap::GetInstance().AddTreeManager(GetTreeID(), this);
  views_event_observer_.Add(AXEventManager::Get());
  View* root_view = widget->GetRootView();
  if (root_view)
    root_view->NotifyAccessibilityEvent(ax::mojom::Event::kLoadComplete, true);
}

ViewsAXTreeManager::~ViewsAXTreeManager() {
  event_generator_.ReleaseTree();
  views_event_observer_.RemoveAll();
  ui::AXTreeManagerMap::GetInstance().RemoveTreeManager(GetTreeID());
}

void ViewsAXTreeManager::SetGeneratedEventCallbackForTesting(
    const GeneratedEventCallbackForTesting& callback) {
  generated_event_callback_for_testing_ = callback;
}

void ViewsAXTreeManager::UnsetGeneratedEventCallbackForTesting() {
  generated_event_callback_for_testing_.Reset();
}

ui::AXNode* ViewsAXTreeManager::GetNodeFromTree(
    const ui::AXTreeID tree_id,
    const ui::AXNode::AXID node_id) const {
  const ui::AXTreeManager* manager =
      ui::AXTreeManagerMap::GetInstance().GetManager(tree_id);
  return manager ? manager->GetNodeFromTree(node_id) : nullptr;
}

ui::AXNode* ViewsAXTreeManager::GetNodeFromTree(
    const ui::AXNode::AXID node_id) const {
  return ax_tree_.GetFromId(node_id);
}

ui::AXTreeID ViewsAXTreeManager::GetTreeID() const {
  return ax_tree_.GetAXTreeID();
}

ui::AXTreeID ViewsAXTreeManager::GetParentTreeID() const {
  // TODO(nektar): Implement stiching of AXTrees, e.g. a dialog to the main
  // window.
  return ui::AXTreeIDUnknown();
}

ui::AXNode* ViewsAXTreeManager::GetRootAsAXNode() const {
  return ax_tree_.root();
}

ui::AXNode* ViewsAXTreeManager::GetParentNodeFromParentTreeAsAXNode() const {
  // TODO(nektar): Implement stiching of AXTrees, e.g. a dialog to the main
  // window.
  return nullptr;
}

void ViewsAXTreeManager::OnViewEvent(View* view, ax::mojom::Event event) {
  DCHECK(view);
  AXAuraObjWrapper* wrapper = cache_.GetOrCreate(view);
  if (!wrapper)
    return;
  modified_nodes_.insert(wrapper->GetUniqueId());

  if (waiting_to_serialize_)
    return;
  waiting_to_serialize_ = true;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ViewsAXTreeManager::SerializeTreeUpdates,
                                weak_factory_.GetWeakPtr()));
}

void ViewsAXTreeManager::PerformAction(const ui::AXActionData& data) {
  tree_source_.HandleAccessibleAction(data);
}

void ViewsAXTreeManager::SerializeTreeUpdates() {
  // Better to set this flag to false early in case this method, or any method
  // it calls, causes an event to get fired.
  waiting_to_serialize_ = false;

  // Make sure the focused node is serialized.
  AXAuraObjWrapper* focused_wrapper = cache_.GetFocus();
  if (focused_wrapper)
    modified_nodes_.insert(focused_wrapper->GetUniqueId());

  std::vector<ui::AXTreeUpdate> updates;
  for (const ui::AXNode::AXID node_id : modified_nodes_) {
    AXAuraObjWrapper* wrapper = cache_.Get(node_id);
    if (!wrapper)
      continue;

    ui::AXTreeUpdate update;
    if (!tree_serializer_.SerializeChanges(wrapper, &update)) {
      std::string error;
      ui::AXTreeSourceChecker<AXAuraObjWrapper*, ui::AXNodeData, ui::AXTreeData>
          checker(&tree_source_);
      checker.CheckAndGetErrorString(&error);
      NOTREACHED() << error << '\n' << update.ToString();
      return;
    }

    updates.push_back(update);
  }

  UnserializeTreeUpdates(updates);
}

void ViewsAXTreeManager::UnserializeTreeUpdates(
    const std::vector<ui::AXTreeUpdate>& updates) {
  for (const ui::AXTreeUpdate& update : updates) {
    if (!ax_tree_.Unserialize(update)) {
      NOTREACHED() << ax_tree_.error();
      return;
    }
  }

  // Unserializing the updates into our AXTree should have prompted our
  // AXEventGenerator to generate events based on the updates.
  for (const ui::AXEventGenerator::TargetedEvent& targeted_event :
       event_generator_) {
    FireGeneratedEvent(targeted_event.event_params.event, *targeted_event.node);
  }
  event_generator_.ClearEvents();
}

void ViewsAXTreeManager::FireGeneratedEvent(
    const ui::AXEventGenerator::Event& event,
    const ui::AXNode& node) const {
  if (!generated_event_callback_for_testing_.is_null())
    generated_event_callback_for_testing_.Run(widget_, event, node.id());
  // TODO(nektar): Implement this other than "for testing".
}

}  // namespace views
