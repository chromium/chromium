// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/window_reorderer.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <vector>

#include "base/containers/adapters.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/view_constants_aura.h"

namespace views {

namespace {

// Sets |hosted_windows| to a mapping of the views with an associated window to
// the window that they are associated to. Only views associated to a child of
// |parent_window| are returned.
void GetViewsWithAssociatedWindow(
    const aura::Window& parent_window,
    std::map<views::View*, aura::Window*>* hosted_windows) {
  for (auto* child : parent_window.children()) {
    View* host_view = child->GetProperty(kHostViewKey);
    if (host_view)
      (*hosted_windows)[host_view] = child;
  }
}

// Sets |order| to the list of views whose layer / associated window's layer
// is a child of |parent_layer|. |order| is sorted in ascending z-order of
// the views.
// |hosts| are the views with an associated window whose layer is a child of
// |parent_layer|.
void GetOrderOfViewsWithLayers(
    views::View* view,
    ui::Layer* parent_layer,
    const std::map<views::View*, aura::Window*>& hosts,
    std::vector<views::View*>* order) {
  DCHECK(view);
  DCHECK(parent_layer);
  DCHECK(order);
  if (view->layer() && view->layer()->parent() == parent_layer) {
    order->push_back(view);
    // |hosts| may contain a child of |view|.
  } else if (hosts.find(view) != hosts.end()) {
    order->push_back(view);
  }

  for (views::View* child : view->children())
    GetOrderOfViewsWithLayers(child, parent_layer, hosts, order);
}

}  // namespace

// Class which reorders windows as a result of the kHostViewKey property being
// set on the window.
class WindowReorderer::AssociationObserver : public aura::WindowObserver {
 public:
  explicit AssociationObserver(WindowReorderer* reorderer);
  ~AssociationObserver() override;

  // Start/stop observing changes in the kHostViewKey property on |window|.
  void StartObserving(aura::Window* window);
  void StopObserving(aura::Window* window);

 private:
  // aura::WindowObserver overrides:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Not owned.
  WindowReorderer* reorderer_;

  std::set<aura::Window*> windows_;

  DISALLOW_COPY_AND_ASSIGN(AssociationObserver);
};

WindowReorderer::AssociationObserver::AssociationObserver(
    WindowReorderer* reorderer)
    : reorderer_(reorderer) {
}

WindowReorderer::AssociationObserver::~AssociationObserver() {
  while (!windows_.empty())
    StopObserving(*windows_.begin());
}

void WindowReorderer::AssociationObserver::StartObserving(
    aura::Window* window) {
  windows_.insert(window);
  window->AddObserver(this);
}

void WindowReorderer::AssociationObserver::StopObserving(
    aura::Window* window) {
  windows_.erase(window);
  window->RemoveObserver(this);
}

void WindowReorderer::AssociationObserver::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key == kHostViewKey)
    reorderer_->ReorderChildWindows();
}

void WindowReorderer::AssociationObserver::OnWindowDestroying(
    aura::Window* window) {
  windows_.erase(window);
  window->RemoveObserver(this);
}

WindowReorderer::WindowReorderer(aura::Window* parent_window,
                                 View* root_view)
    : parent_window_(parent_window),
      root_view_(root_view),
      association_observer_(new AssociationObserver(this)) {
  parent_window_->AddObserver(this);
  for (auto* window : parent_window_->children())
    association_observer_->StartObserving(window);
  ReorderChildWindows();
}

WindowReorderer::~WindowReorderer() {
  if (parent_window_) {
    parent_window_->RemoveObserver(this);
    // |association_observer_| stops observing any windows it is observing upon
    // destruction.
  }
}

void WindowReorderer::ReorderChildWindows() {
  if (!parent_window_)
    return;

  std::map<View*, aura::Window*> hosted_windows;
  GetViewsWithAssociatedWindow(*parent_window_, &hosted_windows);

  if (hosted_windows.empty()) {
    // Exit early if there are no views with associated windows.
    // View::ReorderLayers() should have already reordered the layers owned by
    // views.
    return;
  }

  // Compute the desired z-order of the layers based on the order of the views
  // with layers and views with associated windows in the view tree.
  std::vector<View*> view_with_layer_order;
  GetOrderOfViewsWithLayers(root_view_, parent_window_->layer(), hosted_windows,
      &view_with_layer_order);

  std::vector<ui::Layer*> children_layer_order;

  // For the sake of simplicity, reorder both the layers owned by views and the
  // layers of windows associated with a view. Iterate through
  // |view_with_layer_order| backwards and stack windows at the bottom so that
  // windows not associated to a view are stacked above windows with an
  // associated view.
  for (View* view : base::Reversed(view_with_layer_order)) {
    std::vector<ui::Layer*> layers;
    aura::Window* window = nullptr;

    auto hosted_window_it = hosted_windows.find(view);
    if (hosted_window_it != hosted_windows.end()) {
      window = hosted_window_it->second;
      layers.push_back(window->layer());
    } else {
      layers = view->GetLayersInOrder();
      std::reverse(layers.begin(), layers.end());
    }

    DCHECK(!layers.empty());
    if (window)
      parent_window_->StackChildAtBottom(window);

    for (ui::Layer* layer : layers)
      children_layer_order.emplace_back(layer);
  }
  std::reverse(children_layer_order.begin(), children_layer_order.end());
  parent_window_->layer()->StackChildrenAtBottom(children_layer_order);
}

void WindowReorderer::OnWindowAdded(aura::Window* new_window) {
  association_observer_->StartObserving(new_window);
  ReorderChildWindows();
}

void WindowReorderer::OnWillRemoveWindow(aura::Window* window) {
  association_observer_->StopObserving(window);
}

void WindowReorderer::OnWindowDestroying(aura::Window* window) {
  parent_window_->RemoveObserver(this);
  parent_window_ = nullptr;
  association_observer_.reset();
}

}  // namespace views
