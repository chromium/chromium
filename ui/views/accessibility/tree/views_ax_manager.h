// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_VIEWS_AX_MANAGER_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_VIEWS_AX_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_tree_source_views.h"
#include "ui/views/accessibility/ax_update_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace base {
template <typename T>
class NoDestructor;
}

namespace ui {
struct AXEvent;
}

namespace views {
class AccessibilityAlertWindow;
class AXAuraObjWrapper;
class AXVirtualView;
class View;

using AuraAXTreeSerializer = ui::AXTreeSerializer<
    views::AXAuraObjWrapper*,
    std::vector<raw_ptr<views::AXAuraObjWrapper, VectorExperimental>>,
    ui::AXTreeUpdate*,
    ui::AXTreeData*,
    ui::AXNodeData>;

// This class manages a tree of AXNodes for all Chromium Views, serializing
// updates, handling accessibility events, performing hit tests, etc.
// TODO(https://crbug.com/40672441): This is currently aura-only, but it will be
// modified to work with Views on macOS as well.
class VIEWS_EXPORT ViewsAXManager : public ui::AXActionHandler,
                                    public views::AXAuraObjCache::Delegate,
                                    public views::AXUpdateObserver {
 public:
  ViewsAXManager(const ViewsAXManager&) = delete;
  ViewsAXManager& operator=(const ViewsAXManager&) = delete;

  // Enables platform accessibility support for views.
  virtual void Enable();

  // Disables platform accessibility support for views.
  virtual void Disable();

  // Handle a textual alert.
  void HandleAlert(const std::string& text);

  void SetA11yOverrideWindow(aura::Window* a11y_override_window);

  // ui::AXActionHandler:
  void PerformAction(const ui::AXActionData& data) override;

  // views::AXAuraObjCache::Delegate:
  void OnChildWindowRemoved(views::AXAuraObjWrapper* parent) override;
  void OnEvent(views::AXAuraObjWrapper* aura_obj,
               ax::mojom::Event event_type) override;

  // views::AXUpdateObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override;
  void OnVirtualViewEvent(views::AXVirtualView* virtual_view,
                          ax::mojom::Event event_type) override;

  void OnDataChanged(views::View* view) override;
  void OnVirtualViewDataChanged(views::AXVirtualView* virtual_view) override;

  bool is_enabled() const { return is_enabled_; }

  void set_ax_aura_obj_cache_for_testing(
      std::unique_ptr<views::AXAuraObjCache> cache) {
    cache_ = std::move(cache);
  }

 protected:
  friend class base::NoDestructor<ViewsAXManager>;
  ViewsAXManager();
  ~ViewsAXManager() override;

  // Resets internal state, optionally resetting the serializer too to save
  // memory.
  virtual void Reset(bool reset_serializer);

  void PostEvent(int id,
                 ax::mojom::Event event_type,
                 int action_request_id = -1,
                 bool from_user = false);

  virtual void SendPendingUpdate();

  // Subclasses override this to do final dispatching of events.
  virtual void DispatchAccessibilityEvents(
      const ui::AXTreeID& tree_id,
      std::vector<ui::AXTreeUpdate> tree_updates,
      const gfx::Point& mouse_location,
      std::vector<ui::AXEvent> events);

  views::AXTreeSourceViews* GetTreeSource() { return tree_source_.get(); }

  bool send_window_state_on_enable_ = true;

 private:
  void PerformHitTest(const ui::AXActionData& data);

  // Logs an error with details about a serialization failure.
  void OnSerializeFailure(ax::mojom::Event event_type,
                          const ui::AXTreeUpdate& update);

  // Whether accessibility tree support for views is enabled.
  bool is_enabled_ = false;

  std::unique_ptr<views::AXAuraObjCache> cache_;

  // Holds the active views-based tree. A tree consists of all views descendant
  // to a `Widget` (see `AXTreeSourceViews`).
  std::unique_ptr<views::AXTreeSourceViews> tree_source_;

  // Serializes incremental updates on the currently active `tree_source_`.
  std::unique_ptr<AuraAXTreeSerializer> tree_serializer_;

  std::unique_ptr<views::AccessibilityAlertWindow> alert_window_;

  // Indicates whether we have already posted an event or data changed task to
  // SendPendingUpdate().
  bool processing_update_posted_ = false;

  ax::mojom::Action currently_performing_action_ = ax::mojom::Action::kNone;

  struct Event {
    int id;
    ax::mojom::Event event_type;
    int action_request_id;
    ax::mojom::Action currently_performing_action;
    bool from_user;
  };
  std::vector<Event> pending_events_;

  std::unordered_set<ui::AXNodeID> pending_data_updates_;
};
}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_VIEWS_AX_MANAGER_H_
