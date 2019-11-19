// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_aura_obj_cache.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_view_obj_wrapper.h"
#include "ui/views/accessibility/ax_widget_obj_wrapper.h"
#include "ui/views/accessibility/ax_window_obj_wrapper.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
namespace {

aura::client::FocusClient* GetFocusClient(aura::Window* root_window) {
  if (!root_window)
    return nullptr;
  return aura::client::GetFocusClient(root_window);
}

}  // namespace

AXAuraObjWrapper* AXAuraObjCache::GetOrCreate(View* view) {
  // Avoid problems with transient focus events. https://crbug.com/729449
  if (!view->GetWidget())
    return nullptr;
  return CreateInternal<AXViewObjWrapper>(view, view_to_id_map_);
}

AXAuraObjWrapper* AXAuraObjCache::GetOrCreate(Widget* widget) {
  return CreateInternal<AXWidgetObjWrapper>(widget, widget_to_id_map_);
}

AXAuraObjWrapper* AXAuraObjCache::GetOrCreate(aura::Window* window) {
  return CreateInternal<AXWindowObjWrapper>(window, window_to_id_map_);
}

int32_t AXAuraObjCache::GetID(View* view) const {
  return GetIDInternal(view, view_to_id_map_);
}

int32_t AXAuraObjCache::GetID(Widget* widget) const {
  return GetIDInternal(widget, widget_to_id_map_);
}

int32_t AXAuraObjCache::GetID(aura::Window* window) const {
  return GetIDInternal(window, window_to_id_map_);
}

void AXAuraObjCache::Remove(View* view) {
  RemoveInternal(view, view_to_id_map_);
}

void AXAuraObjCache::RemoveViewSubtree(View* view) {
  Remove(view);
  for (View* child : view->children())
    RemoveViewSubtree(child);
}

void AXAuraObjCache::Remove(Widget* widget) {
  RemoveInternal(widget, widget_to_id_map_);

  // When an entire widget is deleted, it doesn't always send a notification
  // on each of its views, so we need to explore them recursively.
  auto* view = widget->GetRootView();
  if (view)
    RemoveViewSubtree(view);
}

void AXAuraObjCache::Remove(aura::Window* window, aura::Window* parent) {
  int id = GetIDInternal(parent, window_to_id_map_);
  AXAuraObjWrapper* parent_window_obj = Get(id);
  RemoveInternal(window, window_to_id_map_);
  if (parent && delegate_)
    delegate_->OnChildWindowRemoved(parent_window_obj);
}

AXAuraObjWrapper* AXAuraObjCache::Get(int32_t id) {
  auto it = cache_.find(id);
  return it != cache_.end() ? it->second.get() : nullptr;
}

void AXAuraObjCache::GetTopLevelWindows(
    std::vector<AXAuraObjWrapper*>* children) {
  for (aura::Window* root : root_windows_)
    children->push_back(GetOrCreate(root));
}

AXAuraObjWrapper* AXAuraObjCache::GetFocus() {
  View* focused_view = GetFocusedView();
  if (focused_view) {
    const ViewAccessibility& view_accessibility =
        focused_view->GetViewAccessibility();
    if (view_accessibility.FocusedVirtualChild())
      return view_accessibility.FocusedVirtualChild()->GetOrCreateWrapper(this);

    return GetOrCreate(focused_view);
  }
  return nullptr;
}

void AXAuraObjCache::OnFocusedViewChanged() {
  View* view = GetFocusedView();
  if (view)
    view->NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
}

void AXAuraObjCache::FireEvent(AXAuraObjWrapper* aura_obj,
                               ax::mojom::Event event_type) {
  if (delegate_)
    delegate_->OnEvent(aura_obj, event_type);
}

AXAuraObjCache::AXAuraObjCache() = default;

// Never runs because object is leaked.
AXAuraObjCache::~AXAuraObjCache() {
  if (!root_windows_.empty() && GetFocusClient(*root_windows_.begin()))
    GetFocusClient(*root_windows_.begin())->RemoveObserver(this);
}

View* AXAuraObjCache::GetFocusedView() {
  Widget* focused_widget = focused_widget_for_testing_;
  aura::Window* focused_window = nullptr;
  if (!focused_widget) {
    if (root_windows_.empty())
      return nullptr;
    aura::client::FocusClient* focus_client =
        GetFocusClient(*root_windows_.begin());
    if (!focus_client)
      return nullptr;

    focused_window = focus_client->GetFocusedWindow();
    if (!focused_window)
      return nullptr;

    focused_widget = Widget::GetWidgetForNativeView(focused_window);
    while (!focused_widget) {
      focused_window = focused_window->parent();
      if (!focused_window)
        break;

      focused_widget = Widget::GetWidgetForNativeView(focused_window);
    }
  }

  if (!focused_widget)
    return nullptr;

  FocusManager* focus_manager = focused_widget->GetFocusManager();
  if (!focus_manager)
    return nullptr;

  View* focused_view = focus_manager->GetFocusedView();
  if (focused_view)
    return focused_view;

  if (focused_window &&
      focused_window->GetProperty(
          aura::client::kAccessibilityFocusFallsbackToWidgetKey)) {
    // If focused widget has non client view, falls back to first child view of
    // its client view. We don't expect that non client view gets keyboard
    // focus.
    auto* non_client = focused_widget->non_client_view();
    auto* client = non_client ? non_client->client_view() : nullptr;
    return (client && !client->children().empty())
               ? client->children().front()
               : focused_widget->GetRootView();
  }

  return nullptr;
}

void AXAuraObjCache::OnWindowFocused(aura::Window* gained_focus,
                                     aura::Window* lost_focus) {
  OnFocusedViewChanged();
}

void AXAuraObjCache::OnRootWindowObjCreated(aura::Window* window) {
  if (root_windows_.empty() && GetFocusClient(window))
    GetFocusClient(window)->AddObserver(this);
  root_windows_.insert(window);
}

void AXAuraObjCache::OnRootWindowObjDestroyed(aura::Window* window) {
  root_windows_.erase(window);
  if (root_windows_.empty() && GetFocusClient(window))
    GetFocusClient(window)->RemoveObserver(this);
}

template <typename AuraViewWrapper, typename AuraView>
AXAuraObjWrapper* AXAuraObjCache::CreateInternal(
    AuraView* aura_view,
    std::map<AuraView*, int32_t>& aura_view_to_id_map) {
  if (!aura_view)
    return nullptr;

  auto it = aura_view_to_id_map.find(aura_view);

  if (it != aura_view_to_id_map.end())
    return Get(it->second);

  auto wrapper = std::make_unique<AuraViewWrapper>(this, aura_view);
  int32_t id = wrapper->GetUniqueId();
  aura_view_to_id_map[aura_view] = id;
  cache_[id] = std::move(wrapper);
  return cache_[id].get();
}

template <typename AuraView>
int32_t AXAuraObjCache::GetIDInternal(
    AuraView* aura_view,
    const std::map<AuraView*, int32_t>& aura_view_to_id_map) const {
  if (!aura_view)
    return ui::AXNode::kInvalidAXID;

  auto it = aura_view_to_id_map.find(aura_view);
  return it != aura_view_to_id_map.end() ? it->second
                                         : ui::AXNode::kInvalidAXID;
}

template <typename AuraView>
void AXAuraObjCache::RemoveInternal(
    AuraView* aura_view,
    std::map<AuraView*, int32_t>& aura_view_to_id_map) {
  int32_t id = GetID(aura_view);
  if (id == ui::AXNode::kInvalidAXID)
    return;
  aura_view_to_id_map.erase(aura_view);
  cache_.erase(id);
}

}  // namespace views
