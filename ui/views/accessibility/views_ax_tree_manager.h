// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEWS_AX_TREE_MANAGER_H_
#define UI_VIEWS_ACCESSIBILITY_VIEWS_AX_TREE_MANAGER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/accessibility/ax_tree_source_views.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {

struct AXActionData;

}  // namespace ui

namespace views {

class AXAuraObjWrapper;
class View;

// Manages an accessibility tree that mirrors the Views tree for a particular
// widget.
//
// TODO(nektar): Enable navigating to parent and child accessibility trees for
// parent and child widgets, thereby allowing stiching the various accessibility
// trees into a unified platform tree.
//
// The Views tree is serialized into an AXTreeSource and immediately
// deserialized into an AXTree, both in the same process.
class VIEWS_EXPORT ViewsAXTreeManager : public ui::AXTreeManager,
                                        public ui::AXActionHandler,
                                        public AXEventObserver,
                                        public views::WidgetObserver {
 public:
  using GeneratedEventCallbackForTesting = base::RepeatingCallback<
      void(Widget*, ui::AXEventGenerator::Event, ui::AXNodeID)>;

  // Creates an instance of this class that manages an AXTree mirroring the
  // Views tree rooted at a given Widget.
  //
  // The provided |widget| doesn't own this class.
  explicit ViewsAXTreeManager(Widget* widget);

  ~ViewsAXTreeManager() override;
  ViewsAXTreeManager(const ViewsAXTreeManager& manager) = delete;
  ViewsAXTreeManager& operator=(const ViewsAXTreeManager& manager) = delete;

  // Returns a reference to the managed AXTree.
  const ui::AXTree& ax_tree() const { return ax_tree_; }

  // For testing only, register a function to be called when a generated event
  // is fired from this ViewsAXTreeManager.
  void SetGeneratedEventCallbackForTesting(
      const GeneratedEventCallbackForTesting& callback);

  // For testing only, unregister the function that was previously registered to
  // be called when a generated event is fired from this ViewsAXTreeManager.
  void UnsetGeneratedEventCallbackForTesting();

  // AXTreeManager implementation.
  ui::AXNode* GetNodeFromTree(const ui::AXTreeID tree_id,
                              const ui::AXNodeID node_id) const override;
  ui::AXNode* GetNodeFromTree(const ui::AXNodeID node_id) const override;
  ui::AXTreeID GetTreeID() const override;
  ui::AXTreeID GetParentTreeID() const override;
  ui::AXNode* GetRootAsAXNode() const override;
  ui::AXNode* GetParentNodeFromParentTreeAsAXNode() const override;

  // AXActionHandlerBase implementation.
  void PerformAction(const ui::AXActionData& data) override;

  // AXEventObserver implementation.
  void OnViewEvent(views::View* view, ax::mojom::Event event) override;

  // WidgetObserver implementation.
  void OnWidgetDestroyed(Widget* widget) override;
  void OnWidgetClosing(Widget* widget) override;

 private:
  using ViewsAXTreeSerializer = ui::AXTreeSerializer<AXAuraObjWrapper*>;

  void SerializeTreeUpdates();
  void UnserializeTreeUpdates(const std::vector<ui::AXTreeUpdate>& updates);

  // Determines the platform node which corresponds to the given |node| and
  // fires the given |event| on it.
  //
  // TODO(nektar): Implement this other than for testing.
  void FireGeneratedEvent(const ui::AXEventGenerator::Event& event,
                          const ui::AXNode& node) const;

  // The Widget for which this class manages an AXTree.
  //
  // Weak, a Widget doesn't own this class.
  Widget* widget_;

  // Set to true if we are still waiting for a task to serialize all previously
  // modified nodes.
  bool waiting_to_serialize_ = false;

  // The set of nodes in the source tree that might have been modified after an
  // event has been fired on them.
  std::set<ui::AXNodeID> modified_nodes_;

  // The cache that maps objects in the Views tree to AXAuraObjWrapper objects
  // that are used to serialize the Views tree.
  AXAuraObjCache cache_;

  // The ID for this AXTree.
  ui::AXTreeID tree_id_;

  // The AXTree that mirrors the Views tree and which is created by
  // deserializing the updates from |tree_source_|.
  ui::AXTree ax_tree_;

  // The tree source that enables us to serialize the Views tree.
  AXTreeSourceViews tree_source_;

  // The serializer that serializes the Views tree into one or more
  // AXTreeUpdate.
  ViewsAXTreeSerializer tree_serializer_;

  // For automatically generating events based on changes to |tree_|.
  ui::AXEventGenerator event_generator_;

  // For testing only: A function to call when a generated event is fired.
  GeneratedEventCallbackForTesting generated_event_callback_for_testing_;

  // To prevent any use-after-free, members below this line should be declared
  // last.
  base::ScopedObservation<AXEventManager, AXEventObserver>
      views_event_observer_{this};
  base::ScopedObservation<Widget, views::WidgetObserver> widget_observer_{this};
  base::WeakPtrFactory<ViewsAXTreeManager> weak_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEWS_AX_TREE_MANAGER_H_
