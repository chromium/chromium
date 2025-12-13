// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/views_ax_manager.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "components/crash/core/common/crash_key.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/accessibility_alert_window.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_root_obj_wrapper.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace views {

void ViewsAXManager::Enable() {
  is_enabled_ = true;
  Reset(/*reset_serializer=*/false);

  // Seed the views::AXAuraObjCache with per-display root windows so
  // GetTopLevelWindows() returns the correct values when accessibility is
  // enabled with multiple displays connected.
  if (send_window_state_on_enable_) {
    for (aura::WindowTreeHost* host :
         aura::Env::GetInstance()->window_tree_hosts()) {
      cache_->OnRootWindowObjCreated(host->window());
    }
  }

  // Send this event immediately to push the initial desktop tree state.
  pending_events_.push_back({tree_source_->GetRoot()->GetUniqueId(),
                             ax::mojom::Event::kLoadComplete, -1,
                             currently_performing_action_});
  pending_data_updates_.insert(tree_source_->GetRoot()->GetUniqueId());
  SendPendingUpdate();

  // Intentionally not reset at shutdown since we cannot rely on the shutdown
  // ordering of two base::Singletons.
  cache_->SetDelegate(this);

  if (!display::Screen::Get()) {
    return;
  }

  const display::Display& display = display::Screen::Get()->GetPrimaryDisplay();
  aura::Window* root_window = nullptr;
  for (aura::WindowTreeHost* host :
       aura::Env::GetInstance()->window_tree_hosts()) {
    if (display.id() == host->GetDisplayId()) {
      root_window = host->window();
      break;
    }
  }

  aura::Window* active_window = nullptr;
  if (root_window) {
    active_window = ::wm::GetActivationClient(root_window)->GetActiveWindow();
  }

  if (active_window) {
    views::AXAuraObjWrapper* focus = cache_->GetOrCreate(active_window);
    if (focus) {
      PostEvent(focus->GetUniqueId(), ax::mojom::Event::kChildrenChanged);
    }
  }
}

void ViewsAXManager::Disable() {
  is_enabled_ = false;

  tree_source_.reset();
  tree_serializer_.reset();
  alert_window_.reset();
  cache_ = std::make_unique<views::AXAuraObjCache>();
}

void ViewsAXManager::HandleAlert(const std::string& text) {
  if (!is_enabled_) {
    return;
  }

  if (alert_window_.get()) {
    alert_window_->HandleAlert(text);
  }
}

void ViewsAXManager::SetA11yOverrideWindow(aura::Window* a11y_override_window) {
  if (cache_) {
    cache_->SetA11yOverrideWindow(a11y_override_window);
  }
}

void ViewsAXManager::OnDataChanged(views::View* view) {
  CHECK(view);
  if (!is_enabled_) {
    return;
  }

  DCHECK(tree_source_.get());

  views::Widget* widget = view->GetWidget();
  if (widget && !widget->GetNativeView()) {
    return;
  }

  views::AXAuraObjWrapper* obj = cache_->GetOrCreate(view);
  if (!obj) {
    return;
  }

  pending_data_updates_.insert(obj->GetUniqueId());

  if (processing_update_posted_) {
    return;
  }

  processing_update_posted_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ViewsAXManager::SendPendingUpdate,
                                base::Unretained(this)));
}

void ViewsAXManager::OnVirtualViewDataChanged(
    views::AXVirtualView* virtual_view) {
  CHECK(virtual_view);

  if (!is_enabled_) {
    return;
  }

  DCHECK(tree_source_.get());

  views::AXAuraObjWrapper* obj = virtual_view->GetOrCreateWrapper(cache_.get());
  if (!obj) {
    return;
  }

  pending_data_updates_.insert(obj->GetUniqueId());

  if (processing_update_posted_) {
    return;
  }

  processing_update_posted_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ViewsAXManager::SendPendingUpdate,
                                base::Unretained(this)));
}

void ViewsAXManager::PerformAction(const ui::AXActionData& data) {
  if (!is_enabled_) {
    return;
  }

  base::AutoReset<ax::mojom::Action> reset_action(&currently_performing_action_,
                                                  data.action);

  DCHECK(tree_source_.get());

  base::AutoReset<ax::mojom::Action> reset_currently_performing_action(
      &currently_performing_action_, data.action);

  // Exclude the do default action, which can trigger too many important events
  // that should not be ignored by clients like focus.
  if (data.action == ax::mojom::Action::kDoDefault) {
    currently_performing_action_ = ax::mojom::Action::kNone;
  }

  // Unlike all of the other actions, a hit test requires determining the
  // node to perform the action on first.
  if (data.action == ax::mojom::Action::kHitTest) {
    PerformHitTest(data);
    return;
  }

  tree_source_->HandleAccessibleAction(data);
}

void ViewsAXManager::OnViewEvent(views::View* view,
                                 ax::mojom::Event event_type) {
  CHECK(view);

  if (!is_enabled_) {
    return;
  }

  views::AXAuraObjWrapper* obj = cache_->GetOrCreate(view);
  if (!obj) {
    return;
  }

  PostEvent(obj->GetUniqueId(), event_type);
}

void ViewsAXManager::OnVirtualViewEvent(views::AXVirtualView* virtual_view,
                                        ax::mojom::Event event_type) {
  CHECK(virtual_view);

  if (!is_enabled_) {
    return;
  }

  views::AXAuraObjWrapper* obj = virtual_view->GetOrCreateWrapper(cache_.get());
  if (!obj) {
    return;
  }

  PostEvent(obj->GetUniqueId(), event_type);
}

void ViewsAXManager::OnChildWindowRemoved(views::AXAuraObjWrapper* parent) {
  if (!is_enabled_) {
    return;
  }

  DCHECK(tree_source_.get());

  if (!parent) {
    parent = tree_source_->GetRoot();
  }

  PostEvent(parent->GetUniqueId(), ax::mojom::Event::kChildrenChanged);
}

void ViewsAXManager::OnEvent(views::AXAuraObjWrapper* aura_obj,
                             ax::mojom::Event event_type) {
  if (!is_enabled_) {
    return;
  }

  PostEvent(aura_obj->GetUniqueId(), event_type);
}

void ViewsAXManager::Reset(bool reset_serializer) {
  if (!tree_source_) {
    auto desktop_root = std::make_unique<AXRootObjWrapper>(this, cache_.get());
    tree_source_ = std::make_unique<views::AXTreeSourceViews>(
        desktop_root->GetUniqueId(), ax_tree_id(), cache_.get());
    cache_->CreateOrReplace(std::move(desktop_root));
  }
  if (reset_serializer) {
    tree_serializer_.reset();
    alert_window_.reset();
  } else {
    tree_serializer_ =
        std::make_unique<AuraAXTreeSerializer>(tree_source_.get());

    const auto& hosts = aura::Env::GetInstance()->window_tree_hosts();
    if (!hosts.empty()) {
      alert_window_ = std::make_unique<views::AccessibilityAlertWindow>(
          hosts[0]->window(), cache_.get());
    }
  }
}

void ViewsAXManager::PostEvent(int id,
                               ax::mojom::Event event_type,
                               int action_request_id,
                               bool from_user) {
  pending_events_.push_back({id, event_type, action_request_id,
                             currently_performing_action_, from_user});
  pending_data_updates_.insert(id);

  if (processing_update_posted_) {
    return;
  }

  processing_update_posted_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ViewsAXManager::SendPendingUpdate,
                                base::Unretained(this)));
}

void ViewsAXManager::SendPendingUpdate() {
  processing_update_posted_ = false;
  if (!is_enabled_ || !tree_serializer_) {
    return;
  }

  std::vector<ui::AXTreeUpdate> tree_updates;
  std::vector<ui::AXEvent> events;

  auto pending_events_copy = std::move(pending_events_);
  auto pending_data_changes_copy = std::move(pending_data_updates_);
  pending_events_.clear();
  pending_data_updates_.clear();

  for (auto& event_copy : pending_events_copy) {
    const int id = event_copy.id;
    const ax::mojom::Event event_type = event_copy.event_type;
    auto* aura_obj = cache_->Get(id);

    // Some events are important enough where even if their ax obj was
    // destroyed, they still need to be fired.
    if (event_type == ax::mojom::Event::kMenuEnd && !aura_obj) {
      aura_obj = tree_source_->GetRoot();
    }

    if (!aura_obj) {
      continue;
    }

    // We can't defer serialization until the loop of the
    // `pending_data_changes_copy` since we need to first fire the event but
    // only if the object in the client tree, and we need to serialize to know
    // this.
    ui::AXTreeUpdate update;
    if (!tree_serializer_->SerializeChanges(aura_obj, &update)) {
      OnSerializeFailure(event_type, update);
      return;
    }
    tree_updates.push_back(std::move(update));
    pending_data_changes_copy.erase(id);

    // Fire the event on the node, but only if it's actually in the tree.
    // Sometimes we get events fired on nodes with an ancestor that's
    // marked invisible, for example. In those cases we should still
    // call SerializeChanges (because the change may have affected the
    // ancestor) but we shouldn't fire the event on the node not in the tree.
    if (tree_serializer_->IsInClientTree(aura_obj)) {
      ui::AXEvent event;
      event.id = aura_obj->GetUniqueId();
      event.event_type = event_type;
      if (event_copy.currently_performing_action != ax::mojom::Action::kNone) {
        event.event_from = ax::mojom::EventFrom::kAction;
        event.event_from_action = event_copy.currently_performing_action;
      } else if (event_copy.from_user) {
        event.event_from = ax::mojom::EventFrom::kUser;
      }
      event.action_request_id = event_copy.action_request_id;
      events.push_back(std::move(event));
    }
  }

  // We must now serialize any changes that were not associated with an event.
  ui::AXTreeUpdate update;
  for (auto id : pending_data_changes_copy) {
    auto* aura_obj = cache_->Get(id);
    if (!aura_obj) {
      continue;
    }

    if (!tree_serializer_->SerializeChanges(aura_obj, &update)) {
      OnSerializeFailure(ax::mojom::Event::kChildrenChanged, update);
      return;
    }
    tree_updates.push_back(std::move(update));
  }

  // Make sure the focused node is serialized.
  views::AXAuraObjWrapper* focus = cache_->GetFocus();
  if (focus) {
    ui::AXTreeUpdate focused_node_update;
    tree_serializer_->SerializeChanges(focus, &focused_node_update);
    tree_updates.push_back(std::move(focused_node_update));
  }

  gfx::Point mouse_location = aura::Env::GetInstance()->last_mouse_location();
  DispatchAccessibilityEvents(ax_tree_id(), std::move(tree_updates),
                              mouse_location, std::move(events));
}

void ViewsAXManager::DispatchAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    std::vector<ui::AXTreeUpdate> tree_updates,
    const gfx::Point& mouse_location,
    std::vector<ui::AXEvent> events) {
  // By default, do nothing. Should be overridden by subclasses.
}

ViewsAXManager::ViewsAXManager()
    : cache_(std::make_unique<views::AXAuraObjCache>()) {
  views::AXUpdateNotifier::Get()->AddObserver(this);
}

// Never runs because object is leaked.
ViewsAXManager::~ViewsAXManager() = default;

void ViewsAXManager::PerformHitTest(const ui::AXActionData& original_action) {
  ui::AXActionData action = original_action;
  // Get the display nearest the point.
  const display::Display& display =
      display::Screen::Get()->GetDisplayNearestPoint(action.target_point);

  // Require a window in `display`; prefer it also be focused.
  aura::Window* root_window = nullptr;
  for (aura::WindowTreeHost* host :
       aura::Env::GetInstance()->window_tree_hosts()) {
    if (display.id() == host->GetDisplayId()) {
      root_window = host->window();
      if (aura::client::GetFocusClient(root_window)->GetFocusedWindow()) {
        break;
      }
    }
  }

  if (!root_window) {
    return;
  }

  // Convert to the root window's coordinates.
  gfx::Point point_in_window(action.target_point);
  ::wm::ConvertPointFromScreen(root_window, &point_in_window);

  // Determine which aura Window is associated with the target point.
  aura::Window* window = root_window->GetEventHandlerForPoint(point_in_window);
  if (!window) {
    return;
  }

  // Convert point to local coordinates of the hit window within the root
  // window.
  aura::Window::ConvertPointToTarget(root_window, window, &point_in_window);
  action.target_point = point_in_window;

  // Check for a AX node tree in a remote process (e.g. renderer, mojo app).
  ui::AXTreeID child_ax_tree_id;
  std::string* child_ax_tree_id_ptr = window->GetProperty(ui::kChildAXTreeID);
  if (child_ax_tree_id_ptr) {
    child_ax_tree_id = ui::AXTreeID::FromString(*child_ax_tree_id_ptr);
  }

  // If the window has a child AX tree ID, forward the action to the
  // associated AXActionHandlerBase.
  if (child_ax_tree_id != ui::AXTreeIDUnknown()) {
    ui::AXActionHandlerRegistry* registry =
        ui::AXActionHandlerRegistry::GetInstance();
    ui::AXActionHandlerBase* action_handler =
        registry->GetActionHandler(child_ax_tree_id);
    CHECK(action_handler);

    // Convert to pixels for the RenderFrameHost HitTest, if required.
    if (action_handler->RequiresPerformActionPointInPixels()) {
      // The point is in DIPs, so multiply by the device scale factor to
      // get pixels. Don't apply magnification as the action_handler doesn't
      // know about magnification scale (that's applied later in the stack).
      // Specifically, we cannot use WindowTreeHost::ConvertDIPToPixels as that
      // will re-apply the magnification transform. The local point has
      // already been un-transformed when it was converted to local coordinates.
      float device_scale_factor = window->GetHost()->device_scale_factor();
      action.target_point.set_x(action.target_point.x() * device_scale_factor);
      action.target_point.set_y(action.target_point.y() * device_scale_factor);
    }

    action_handler->PerformAction(action);
    return;
  }

  // Fire an event directly on either a view or window.
  views::AXAuraObjWrapper* obj_to_send_event = nullptr;

  // If the window doesn't have a child tree ID, try to fire the event
  // on a View.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (widget) {
    views::View* root_view = widget->GetRootView();
    views::View* hit_view =
        root_view->GetEventHandlerForPoint(action.target_point);
    if (hit_view) {
      obj_to_send_event = cache_->GetOrCreate(hit_view);
    }
  }

  // Otherwise, fire the event directly on the Window.
  if (!obj_to_send_event) {
    obj_to_send_event = cache_->GetOrCreate(window);
  }
  if (obj_to_send_event) {
    PostEvent(obj_to_send_event->GetUniqueId(), action.hit_test_event_to_fire,
              action.request_id);
  }
}

void ViewsAXManager::OnSerializeFailure(ax::mojom::Event event_type,
                                        const ui::AXTreeUpdate& update) {
  std::string error_string;
  ui::AXTreeSourceChecker<views::AXAuraObjWrapper*> checker(tree_source_.get());
  checker.CheckAndGetErrorString(&error_string);

  // Add a crash key so we can figure out why this is happening.
  static crash_reporter::CrashKeyString<256> ax_tree_source_error(
      "ax_tree_source_error");
  ax_tree_source_error.Set(error_string);

  LOG(ERROR) << "Unable to serialize accessibility event!\n"
             << "Event type: " << event_type << "\n"
             << "Error: " << error_string;
}

}  // namespace views
