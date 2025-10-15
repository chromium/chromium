// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_ax_manager.h"

#if BUILDFLAG(IS_WIN)
#include <oleacc.h>
#endif  // BUILDFLAG(IS_WIN)

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/views/accessibility/tree/widget_view_ax_cache.h"
#include "ui/views/accessibility/view_accessibility.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "ui/views/widget/native_widget_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace views {

WidgetAXManager::WidgetAXManager(Widget* widget)
    : widget_(widget),
      ax_tree_id_(ui::AXTreeID::CreateNewAXTreeID()),
      cache_(std::make_unique<WidgetViewAXCache>()) {
  CHECK(::features::IsAccessibilityTreeForViewsEnabled())
      << "WidgetAXManager should only be created when the "
         "accessibility tree feature is enabled.";

  ui::AXPlatform::GetInstance().AddModeObserver(this);
}

WidgetAXManager::~WidgetAXManager() {
  ui::AXPlatform::GetInstance().RemoveModeObserver(this);
  ax_tree_manager_.reset();
}

void WidgetAXManager::Init() {
  CHECK(widget_->GetRootView());
  if (ui::AXPlatform::GetInstance().GetMode() == ui::AXMode::kNativeAPIs) {
    Enable();
  } else {
    if (widget_->is_top_level()) {
      InitAXTreeManager();
    }
  }
}

void WidgetAXManager::OnEvent(ViewAccessibility& view_ax,
                              ax::mojom::Event event_type) {
  if (!is_enabled_) {
    return;
  }

  pending_events_.push_back({view_ax.GetUniqueId(), event_type});
  pending_data_updates_.insert(view_ax.GetUniqueId());

  SchedulePendingUpdate();
}

void WidgetAXManager::OnDataChanged(ViewAccessibility& view_ax) {
  if (!is_enabled_) {
    return;
  }

  pending_data_updates_.insert(view_ax.GetUniqueId());

  SchedulePendingUpdate();
}

void WidgetAXManager::OnChildAdded(ViewAccessibility& child,
                                   ViewAccessibility& parent) {
  if (!is_enabled_) {
    return;
  }

  cache_->Insert(&child);
  pending_data_updates_.insert(parent.GetUniqueId());

  SchedulePendingUpdate();
}

void WidgetAXManager::OnChildRemoved(ViewAccessibility& child,
                                     ViewAccessibility& parent) {
  if (!is_enabled_) {
    return;
  }

  cache_->Remove(child.GetUniqueId());
  pending_data_updates_.insert(parent.GetUniqueId());

  SchedulePendingUpdate();
}

void WidgetAXManager::OnChildManagerAdded(WidgetAXManager& child_manager) {
  child_manager.parent_ax_tree_id_ = ax_tree_id_;
}

void WidgetAXManager::OnChildManagerRemoved(WidgetAXManager& child_manager) {
  child_manager.parent_ax_tree_id_ = ui::AXTreeID();
}

void WidgetAXManager::OnAXModeAdded(ui::AXMode mode) {
  if (mode.has_mode(ui::AXMode::kNativeAPIs)) {
    Enable();
  }
}

ui::AXPlatformNodeId WidgetAXManager::GetOrCreateAXNodeUniqueId(
    ui::AXNodeID ax_node_id) {
  // ViewAccessibility already generates a unique ID for each View. Return it.
  ViewAccessibility* view_ax = cache_->Get(ax_node_id);
  return view_ax ? view_ax->GetUniqueId() : ui::AXPlatformNodeId();
}

void WidgetAXManager::OnAXNodeDeleted(ui::AXNodeID ax_node_id) {
  // Do nothing. Those unique IDs aren't cached in WidgetAXManager, so they
  // don't need to be removed.
}

void WidgetAXManager::AccessibilityPerformAction(const ui::AXActionData& data) {
  tree_source_->HandleAccessibleAction(data);
}

bool WidgetAXManager::AccessibilityViewHasFocus() {
  return widget_ && widget_->IsActive();
}

void WidgetAXManager::AccessibilityViewSetFocus() {
  if (!widget_ || widget_->IsActive()) {
    return;
  }
  widget_->Activate();
}

gfx::Rect WidgetAXManager::AccessibilityGetViewBounds() {
  if (!widget_) {
    return gfx::Rect();
  }
  return widget_->GetWindowBoundsInScreen();
}

float WidgetAXManager::AccessibilityGetDeviceScaleFactor() {
  // TODO(crbug.com/40672441): Confirm that the DSF is always 1.0f once we
  // serialize the views and can test it manually.
  return 1.0f;
}

void WidgetAXManager::UnrecoverableAccessibilityError() {
  // TODO(accessibility): Implement.
}

gfx::AcceleratedWidget WidgetAXManager::AccessibilityGetAcceleratedWidget() {
  // This method is only used on Windows, where we need the HWND to fire events.
#if BUILDFLAG(IS_WIN)
  if (!widget_) {
    return gfx::kNullAcceleratedWidget;
  }
  return HWNDForView(widget_->GetRootView());
#else
  return gfx::kNullAcceleratedWidget;
#endif  // BUILDFLAG(IS_WIN)
}

gfx::NativeViewAccessible
WidgetAXManager::AccessibilityGetNativeViewAccessible() {
  if (!widget_) {
    return gfx::NativeViewAccessible();
  }
#if BUILDFLAG(IS_MAC)
  // On macOS, the chromium accessibility tree is attached to an NSView. We must
  // return the NativeViewAccessible for the NSView to connect our internal tree
  // to the native one.
  if (auto* native_widget =
          static_cast<NativeWidgetMac*>(widget_->native_widget())) {
    return native_widget->GetNativeViewAccessibleForNSView();
  }
#elif BUILDFLAG(IS_WIN)
  // On Windows, we get the IAccessible for the HWND of the widget through an
  // OS API call.
  // TODO(accessibility): Consider caching the IAccessible object.
  HWND hwnd = HWNDForView(widget_->GetRootView());
  if (!hwnd) {
    return nullptr;
  }

  IAccessible* parent;
  if (SUCCEEDED(
          ::AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible,
                                       reinterpret_cast<void**>(&parent)))) {
    return parent;
  }
#endif
  return gfx::NativeViewAccessible();
}

gfx::NativeViewAccessible
WidgetAXManager::AccessibilityGetNativeViewAccessibleForWindow() {
  if (!widget_) {
    return gfx::NativeViewAccessible();
  }
#if BUILDFLAG(IS_MAC)
  // On macOS, the chromium accessibility tree is attached to an NSView itself
  // connected to an NSWindow. We must return the NativeViewAccessible for the
  // NSWindow to connect our internal tree to the native one.
  //
  // This function is only called on macOS to retrieve the NSWindow associated
  // with the node in the tree. No need to implement on other platforms for now.
  if (auto* native_widget =
          static_cast<NativeWidgetMac*>(widget_->native_widget())) {
    return native_widget->GetNativeViewAccessibleForNSWindow();
  }
#endif  // BUILDFLAG(IS_MAC)
  return gfx::NativeViewAccessible();
}

void WidgetAXManager::AccessibilityHitTest(
    const gfx::Point& point_in_view_pixels,
    const ax::mojom::Event& opt_event_to_fire,
    int opt_request_id,
    base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                            ui::AXNodeID hit_node_id)> opt_callback) {
  // TODO(accessibility): Implement.
}

gfx::NativeWindow WidgetAXManager::GetTopLevelNativeWindow() {
  if (!widget_) {
    return gfx::NativeWindow();
  }

  auto* top = widget_->GetTopLevelWidget();
  if (!top) {
    return gfx::NativeWindow();
  }

  return top->GetNativeWindow();
}

bool WidgetAXManager::CanFireAccessibilityEvents() const {
  return widget_ ? widget_->IsActive() : false;
}

bool WidgetAXManager::AccessibilityIsRootFrame() const {
  // This always returns false for WidgetAXManager, since the "frame" concept is
  // unique to web content.
  return false;
}

bool WidgetAXManager::ShouldSuppressAXLoadComplete() {
  return true;
}

content::WebContentsAccessibility*
WidgetAXManager::AccessibilityGetWebContentsAccessibility() {
  return nullptr;
}

bool WidgetAXManager::AccessibilityIsWebContentSource() {
  return false;
}

void WidgetAXManager::SchedulePendingUpdate() {
  if (processing_update_posted_ || !is_enabled_) {
    return;
  }

  processing_update_posted_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WidgetAXManager::SendPendingUpdate,
                                weak_factory_.GetWeakPtr()));
}

void WidgetAXManager::InitAXTreeManager() {
  CHECK(!ax_tree_manager_);
  ui::AXNodeData root_data;
  widget_->GetRootView()->GetViewAccessibility().GetAccessibleNodeData(
      &root_data);
  ui::AXTreeUpdate update;
  update.root_id = root_data.id;
  update.nodes.push_back(root_data);

  cache_->Init(widget_->GetRootView()->GetViewAccessibility(),
               false /* full_tree */);

  ax_tree_manager_.reset(
      ui::BrowserAccessibilityManager::Create(update, *this, this));
}

void WidgetAXManager::Enable() {
  is_enabled_ = true;
  tree_source_ = std::make_unique<ViewAccessibilityAXTreeSource>(
      widget_->GetRootView()->GetViewAccessibility().GetUniqueId(), ax_tree_id_,
      cache_.get());
  tree_serializer_ =
      std::make_unique<ViewAccessibilityAXTreeSerializer>(tree_source_.get());

  // It's possible the AXTreeManager was already created if this widget is
  // top-level and accessibility wasn't enabled before. Ensure it is created
  // now.
  if (!ax_tree_manager_) {
    InitAXTreeManager();
  }
  cache_->Init(widget_->GetRootView()->GetViewAccessibility());

  // Fully serialize the tree starting from the root immediately.
  pending_data_updates_.insert(
      widget_->GetRootView()->GetViewAccessibility().GetUniqueId());
  SendPendingUpdate();
}

void WidgetAXManager::SendPendingUpdate() {
  std::optional<ui::AXUpdatesAndEvents> maybe_updates_and_events;
  // Always invoke the test callback on exit.
  auto exit_cleanup = absl::MakeCleanup([this, &maybe_updates_and_events]() {
    if (!updates_and_events_callback_for_testing_.is_null()) {
      updates_and_events_callback_for_testing_.Run(maybe_updates_and_events);
    }
  });

  processing_update_posted_ = false;
  if (!is_enabled_) {
    return;
  }

  std::vector<ui::AXTreeUpdate> tree_updates;
  std::vector<ui::AXEvent> events;

  auto pending_events_copy = std::move(pending_events_);
  auto pending_data_changes_copy = std::move(pending_data_updates_);
  pending_events_.clear();
  pending_data_updates_.clear();

  absl::flat_hash_set<ui::AXNodeID> already_serialized_ids;

  // Serialize all changes first. This is necessary to ensure the nodes are
  // added in the cache (through the SerializeChanges call below).
  for (auto& id : pending_data_changes_copy) {
    if (already_serialized_ids.contains(id)) {
      // Don't serialize already serialized nodes a second time.
      continue;
    }

    ViewAccessibility* view_ax = cache_->Get(id);
    if (!view_ax) {
      continue;
    }

    ui::AXTreeUpdate update;
    if (!tree_serializer_->SerializeChanges(view_ax, &update)) {
      return;
    }

    for (auto& node : update.nodes) {
      already_serialized_ids.insert(node.id);
    }

    tree_updates.push_back(std::move(update));
  }

  // Serialize the events after.
  for (auto& event_copy : pending_events_copy) {
    const int id = event_copy.id;
    const ax::mojom::Event event_type = event_copy.event_type;

    ViewAccessibility* view_ax = cache_->Get(id);
    if (!view_ax) {
      continue;
    }

    // We must fire the event if the node is in the client tree. To determine
    // if it is, we need to serialize the node first. We might have serialized
    // it already in the previous loop, so don't serialize it again if
    // it's already in the pending data changes. This happens when a view
    // updates its data and fires an event related to that data change. In the
    // pending_data_changes_copy loop above, we would have already serialized
    // the node in such a case -- no need to do it again.
    if (!pending_data_changes_copy.contains(id)) {
      if (already_serialized_ids.contains(id)) {
        // Don't serialize already serialized nodes a second time.
        continue;
      }

      ui::AXTreeUpdate update;
      if (!tree_serializer_->SerializeChanges(view_ax, &update)) {
        return;
      }
      tree_updates.push_back(std::move(update));
    }

    // Fire the event on the node, but only if it's actually in the tree.
    // Sometimes we get events fired on nodes with an ancestor that's
    // marked invisible, for example. In those cases we should still
    // call SerializeChanges (because the change may have affected the
    // ancestor) but we shouldn't fire the event on the node not in the tree.
    //
    // TODO(crbug.com/40672441): Add test once we have a way to dump events
    // for views.
    if (tree_serializer_->IsInClientTree(view_ax)) {
      ui::AXEvent event;
      event.id = view_ax->GetUniqueId();
      event.event_type = event_type;
      events.push_back(std::move(event));
    }
  }

  // TODO(crbug.com/40672441): Make sure the focused node is serialized.

  if (tree_updates.empty() && events.empty()) {
    // Nothing to do, no updates or events.
    return;
  }

#if DCHECK_IS_ON()
  for (const auto& update : tree_updates) {
    for (const auto& node : update.nodes) {
      DCHECK(cache_->Get(node.id))
          << "Unknown serialized node. All nodes we serialize should be known "
             "to the WidgetAXManager.";
    }
  }
#endif  // DCHECK_IS_ON()

  maybe_updates_and_events.emplace();
  maybe_updates_and_events->ax_tree_id = ax_tree_id_;
  maybe_updates_and_events->updates = std::move(tree_updates);
  maybe_updates_and_events->events = std::move(events);

  ax_tree_manager_->OnAccessibilityEvents(*maybe_updates_and_events);
}

}  // namespace views
