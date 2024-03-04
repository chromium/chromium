// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_aura_obj_cache.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_view_obj_wrapper.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/ax_virtual_view_wrapper.h"
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

// A class which observes the destruction of the a11y override window. Done here
// since adding Window and WindowObserver includes are not allowed in the
// header.
class AXAuraObjCache::A11yOverrideWindowObserver : public aura::WindowObserver {
 public:
  explicit A11yOverrideWindowObserver(AXAuraObjCache* cache) : cache_(cache) {}
  A11yOverrideWindowObserver(const A11yOverrideWindowObserver&) = delete;
  A11yOverrideWindowObserver& operator=(const A11yOverrideWindowObserver&) =
      delete;
  ~A11yOverrideWindowObserver() override = default;

  void Observe() {
    observer_.Reset();
    aura::Window* a11y_override_window = cache_->a11y_override_window_;
    if (a11y_override_window)
      observer_.Observe(a11y_override_window);
  }

 private:
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(window);
    DCHECK_EQ(cache_->a11y_override_window_, window);
    cache_->a11y_override_window_ = nullptr;
    observer_.Reset();
  }

  // Pointer to the AXAuraObjCache object that owns |this|. Guaranteed not to be
  // null for the lifetime of this.
  const raw_ptr<AXAuraObjCache> cache_;

  base::ScopedObservation<aura::Window, aura::WindowObserver> observer_{this};
};

AXAuraObjWrapper* AXAuraObjCache::GetOrCreate(View* view) {
  // Avoid problems with transient focus events. https://crbug.com/729449
  if (!view->GetWidget())
    return nullptr;

  DCHECK(view_to_id_map_.find(view) != view_to_id_map_.end() ||
         // This is either a new view or we're erroneously here during ~View.
         view->life_cycle_state() == View::LifeCycleState::kAlive);

  return CreateInternal<AXViewObjWrapper>(view, &view_to_id_map_);
}

AXAuraObjWrapper* AXAuraObjCache::GetOrCreate(AXVirtualView* virtual_view) {
  if (!virtual_view->GetOwnerView() ||
      !virtual_view->GetOwnerView()->GetWidget())
    return nullptr;
  return CreateInternal<AXVirtualViewWrapper>(virtual_view,
                                              &virtual_view_to_id_map_);
}

AXAuraObjWrapper* AXAuraObjCache::GetOrCreate(Widget* widget) {
  return CreateInternal<AXWidgetObjWrapper>(widget, &widget_to_id_map_);
}

AXAuraObjWrapper* AXAuraObjCache::GetOrCreate(aura::Window* window) {
  return CreateInternal<AXWindowObjWrapper>(window, &window_to_id_map_);
}

void AXAuraObjCache::CreateOrReplace(std::unique_ptr<AXAuraObjWrapper> obj) {
  cache_[obj->GetUniqueId()] = std::move(obj);
}

int32_t AXAuraObjCache::GetID(View* view) const {
  return GetIDInternal(view, view_to_id_map_);
}

int32_t AXAuraObjCache::GetID(AXVirtualView* virtual_view) const {
  return GetIDInternal(virtual_view, virtual_view_to_id_map_);
}

int32_t AXAuraObjCache::GetID(Widget* widget) const {
  return GetIDInternal(widget, widget_to_id_map_);
}

int32_t AXAuraObjCache::GetID(aura::Window* window) const {
  return GetIDInternal(window, window_to_id_map_);
}

void AXAuraObjCache::Remove(View* view) {
  RemoveInternal(view, &view_to_id_map_);
}

void AXAuraObjCache::Remove(AXVirtualView* virtual_view) {
  RemoveInternal(virtual_view, &virtual_view_to_id_map_);
}

void AXAuraObjCache::RemoveViewSubtree(View* view) {
  Remove(view);
  for (View* child : view->children())
    RemoveViewSubtree(child);
}

void AXAuraObjCache::Remove(Widget* widget) {
  RemoveInternal(widget, &widget_to_id_map_);

  // When an entire widget is deleted, it doesn't always send a notification
  // on each of its views, so we need to explore them recursively.
  auto* view = widget->GetRootView();
  if (view)
    RemoveViewSubtree(view);
}

void AXAuraObjCache::Remove(aura::Window* window, aura::Window* parent) {
  int id = GetIDInternal(parent, window_to_id_map_);
  AXAuraObjWrapper* parent_window_obj = Get(id);
  RemoveInternal(window, &window_to_id_map_);
  if (parent && delegate_)
    delegate_->OnChildWindowRemoved(parent_window_obj);

  if (focused_window_ == window)
    focused_window_ = nullptr;
}

AXAuraObjWrapper* AXAuraObjCache::Get(int32_t id) {
  auto it = cache_.find(id);
  return it != cache_.end() ? it->second.get() : nullptr;
}

void AXAuraObjCache::GetTopLevelWindows(
    std::vector<raw_ptr<AXAuraObjWrapper, VectorExperimental>>* children) {
  for (aura::Window* root : root_windows_)
    children->push_back(GetOrCreate(root));
}

AXAuraObjWrapper* AXAuraObjCache::GetFocus() {
  View* focused_view = GetFocusedView();
  while (focused_view &&
         (focused_view->GetViewAccessibility().GetIsIgnored() ||
          focused_view->GetViewAccessibility().propagate_focus_to_ancestor())) {
    focused_view = focused_view->parent();
  }

  if (!focused_view)
    return nullptr;

  if (focused_view->GetViewAccessibility().FocusedVirtualChild()) {
    return focused_view->GetViewAccessibility()
        .FocusedVirtualChild()
        ->GetOrCreateWrapper(this);
  }

  return GetOrCreate(focused_view);
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

AXAuraObjCache::AXAuraObjCache()
    : a11y_override_window_observer_(
          std::make_unique<A11yOverrideWindowObserver>(this)) {}

AXAuraObjCache::~AXAuraObjCache() {
  // Remove all focus observers from |root_windows_|.
  for (aura::Window* window : root_windows_) {
    aura::client::FocusClient* focus_client = GetFocusClient(window);
    if (focus_client)
      focus_client->RemoveObserver(this);
  }
  root_windows_.clear();

  for (auto& entry : virtual_view_to_id_map_)
    entry.first->set_cache(nullptr);
}

View* AXAuraObjCache::GetFocusedView() {
  Widget* focused_widget = focused_widget_for_testing_;
  aura::Window* focused_window =
      focused_widget ? focused_widget->GetNativeWindow() : nullptr;
  if (!focused_widget) {
    // Uses the a11y override window for focus if it exists, otherwise gets the
    // last focused window.
    focused_window = a11y_override_window_ ? a11y_override_window_.get()
                                           : focused_window_.get();

    // Finally, fallback to searching for the focus.
    if (!focused_window) {
      for (aura::Window* window : root_windows_) {
        auto* focus_client = GetFocusClient(window);
        if (focus_client &&
            (focused_window = GetFocusClient(window)->GetFocusedWindow())) {
          break;
        }
      }
    }

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

  // No view has focus, but a child tree might have focus.
  if (focused_window) {
    auto* non_client = focused_widget->non_client_view();
    auto* client = non_client ? non_client->client_view() : nullptr;
    if (client && !client->children().empty()) {
      const ViewAccessibility& host_accessibility =
          client->children().front()->GetViewAccessibility();
      ui::AXNodeData host_data;
      host_accessibility.GetAccessibleNodeData(&host_data);
      if (host_accessibility.GetChildTreeID() != ui::AXTreeIDUnknown() ||
          !host_data
               .GetStringAttribute(
                   ax::mojom::StringAttribute::kChildTreeNodeAppId)
               .empty()) {
        return client->children().front();
      }
    }
  }

  return focused_widget->GetRootView();
}

void AXAuraObjCache::OnWindowFocused(aura::Window* gained_focus,
                                     aura::Window* lost_focus) {
  focused_window_ = gained_focus;
  OnFocusedViewChanged();
}

void AXAuraObjCache::OnRootWindowObjCreated(aura::Window* window) {
  if (root_windows_.empty() && GetFocusClient(window))
    GetFocusClient(window)->AddObserver(this);

  // Do not allow duplicate entries.
  if (!base::Contains(root_windows_, window)) {
    root_windows_.push_back(window);
  }
}

void AXAuraObjCache::OnRootWindowObjDestroyed(aura::Window* window) {
  std::erase_if(root_windows_, [window](aura::Window* current_window) {
    return current_window == window;
  });
  if (root_windows_.empty() && GetFocusClient(window))
    GetFocusClient(window)->RemoveObserver(this);

  if (focused_window_ == window)
    focused_window_ = nullptr;
}

void AXAuraObjCache::SetA11yOverrideWindow(aura::Window* a11y_override_window) {
  a11y_override_window_ = a11y_override_window;
  a11y_override_window_observer_->Observe();
}

template <typename AuraViewWrapper, typename AuraView>
AXAuraObjWrapper* AXAuraObjCache::CreateInternal(
    AuraView* aura_view,
    std::map<AuraView*, int32_t>* aura_view_to_id_map) {
  if (!aura_view)
    return nullptr;

  auto it = aura_view_to_id_map->find(aura_view);

  if (it != aura_view_to_id_map->end())
    return Get(it->second);

  auto wrapper = std::make_unique<AuraViewWrapper>(this, aura_view);
  ui::AXNodeID id = wrapper->GetUniqueId();

  // Ensure this |AuraView| is not already in the cache. This must happen after
  // |GetUniqueId|, as that can alter the cache such that the |find| call above
  // may have missed.
  DCHECK(aura_view_to_id_map->find(aura_view) == aura_view_to_id_map->end());

  (*aura_view_to_id_map)[aura_view] = id;
  cache_[id] = std::move(wrapper);
  return cache_[id].get();
}

template <typename AuraView>
int32_t AXAuraObjCache::GetIDInternal(
    AuraView* aura_view,
    const std::map<AuraView*, int32_t>& aura_view_to_id_map) const {
  if (!aura_view)
    return ui::kInvalidAXNodeID;

  auto it = aura_view_to_id_map.find(aura_view);
  return it != aura_view_to_id_map.end() ? it->second : ui::kInvalidAXNodeID;
}

template <typename AuraView>
void AXAuraObjCache::RemoveInternal(
    AuraView* aura_view,
    std::map<AuraView*, int32_t>* aura_view_to_id_map) {
  int32_t id = GetID(aura_view);
  if (id == ui::kInvalidAXNodeID)
    return;
  aura_view_to_id_map->erase(aura_view);
  cache_.erase(id);
}

}  // namespace views
