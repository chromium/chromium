// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/window_reorderer.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "ui/aura/window.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/views/view.h"

namespace views {

namespace {

void CollectSortedWindowsFromLayer(
    ui::Layer* layer,
    const std::vector<std::pair<ui::Layer*, aura::Window*>>& layer_to_window,
    std::vector<aura::Window*>& sorted_windows) {
  auto it = std::ranges::find_if(layer_to_window, [layer](const auto& pair) {
    return pair.first == layer;
  });
  if (it != layer_to_window.end()) {
    sorted_windows.push_back(it->second);
  }

  // Traverse children in layer tree (pre-order, bottom-to-top).
  for (ui::Layer* child : layer->children()) {
    CollectSortedWindowsFromLayer(child, layer_to_window, sorted_windows);
  }
}

// Collects windows in `sorted_windows` in Z-order from bottom to top.
void CollectSortedWindows(
    View* view,
    const std::vector<std::pair<ui::Layer*, aura::Window*>>& layer_to_window,
    std::vector<aura::Window*>& sorted_windows) {
  if (view->layer()) {
    // Switch to layer tree traversal and prune View tree traversal for this
    // subtree.
    CollectSortedWindowsFromLayer(view->layer(), layer_to_window,
                                  sorted_windows);
    return;
  }

  // Continue View tree traversal.
  for (View* child : view->GetChildrenInZOrder()) {
    CollectSortedWindows(child, layer_to_window, sorted_windows);
  }
}

}  // namespace

WindowReorderer::WindowReorderer(aura::Window* parent_window, View* root_view) {
  parent_window_observation_.Observe(parent_window);
  view_observation_.Observe(root_view);
  ReorderChildWindows();
}

WindowReorderer::~WindowReorderer() = default;

void WindowReorderer::ReorderChildWindows() {
  if (!parent_window_observation_.IsObserving() ||
      !view_observation_.IsObserving()) {
    return;
  }

  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;
  aura::Window* parent_window = parent_window_observation_.GetSource();
  View* root_view = view_observation_.GetSource();

  // Block deletion of all child windows during reordering.
  aura::Window::ScopedDeleteBlocker blocked_windows(parent_window->children());

  const size_t original_children_size = parent_window->children().size();

  // 1) Collect associated child windows (windows whose layers are managed by
  // views) and their layers.
  std::vector<std::pair<ui::Layer*, aura::Window*>> layer_to_window;

  for (aura::Window* child : parent_window->children()) {
    if (!child->layer_managed_by_parent()) {
      CHECK(child->layer());
      layer_to_window.emplace_back(child->layer(), child);
    }
  }

  if (layer_to_window.empty()) {
    return;
  }

  // 2) Traverse and sort using hybrid View/Layer traversal.
  std::vector<aura::Window*> sorted_windows;
  CollectSortedWindows(root_view, layer_to_window, sorted_windows);

  // 3) Reorder using boundary-anchored algorithm.

  // This first places the bottom-most and top-most associated windows so that
  // windows below the bottom-most and windows above the top-most stay in their
  // relative order (Step 1 and Step 2), then reorders windows between these
  // two, skipping unassociated windows (Step 3).
  std::vector<aura::Window*> expected = sorted_windows;
  if (expected.empty()) {
    return;
  }

  const auto& children = parent_window->children();
  aura::Window* assoc_first = layer_to_window.front().second;
  aura::Window* assoc_last = layer_to_window.back().second;

  auto it_first = std::find(children.begin(), children.end(), assoc_first);
  auto it_last = std::find(children.begin(), children.end(), assoc_last);

  CHECK(it_first != children.end());
  CHECK(it_last != children.end());

  aura::Window* unassociated_bot =
      (it_first != children.begin()) ? *std::prev(it_first) : nullptr;
  aura::Window* unassociated_top =
      (it_last + 1 != children.end()) ? *std::next(it_last) : nullptr;

  // Step 1: Move bottom element to bottom and top element to top.
  if (expected.size() >= 1) {
    aura::Window* bot_expected = expected.front();
    if (unassociated_bot) {
      parent_window->StackChildAbove(bot_expected, unassociated_bot);
    } else {
      parent_window->StackChildAtBottom(bot_expected);
    }
    CHECK_EQ(original_children_size, parent_window->children().size());
  }

  if (expected.size() >= 2) {
    aura::Window* top_expected = expected.back();
    if (unassociated_top) {
      parent_window->StackChildBelow(top_expected, unassociated_top);
    } else {
      parent_window->StackChildAtTop(top_expected);
    }
    CHECK_EQ(original_children_size, parent_window->children().size());
  }

  // Step 2: Reorder middle elements.
  // TODO(oshima): Prevent modification (add/remove) to the parent's children
  // list to guard against unrelated modifications to the children stack during
  // reordering. Note that if the order has changed, it may misbehave, but it
  // will not result in Use-After-Free (UAF) due to the index-based access and
  // size check.
  if (expected.size() > 2) {
    auto it_bot =
        std::ranges::find(parent_window->children(), expected.front());
    CHECK(it_bot != parent_window->children().end());
    size_t live_idx = static_cast<size_t>(std::distance(
                          parent_window->children().begin(), it_bot)) +
                      1;

    size_t expected_idx = 1;
    while (expected_idx < expected.size() - 1) {
      if (live_idx >= parent_window->children().size()) {
        break;
      }
      aura::Window* child = parent_window->children()[live_idx];

      if (!std::ranges::contains(expected, child)) {
        live_idx++;
        continue;
      }

      aura::Window* win_exp = expected[expected_idx];
      if (child == win_exp) {
        expected_idx++;
        live_idx++;
      } else {
        aura::Window* prev_live = parent_window->children()[live_idx - 1];
        parent_window->StackChildAbove(win_exp, prev_live);
        CHECK_EQ(original_children_size, parent_window->children().size());
        CHECK_EQ(parent_window->children()[live_idx], win_exp);
        expected_idx++;
        live_idx++;
      }
    }
  }

#if DCHECK_IS_ON()
  // Verify that the actual order of participating windows in
  // parent_window->children() matches the order in `expected`.
  {
    size_t expected_idx = expected.size();
    for (auto it = parent_window->children().rbegin();
         it != parent_window->children().rend() && expected_idx != 0; ++it) {
      if (*it == expected[expected_idx - 1]) {
        expected_idx--;
      }
    }
    CHECK_EQ(expected_idx, 0u);
  }
#endif
}

void WindowReorderer::OnWindowDestroying(aura::Window* window) {
  parent_window_observation_.Reset();
}

void WindowReorderer::OnViewIsDeleting(View* observed_view) {
  view_observation_.Reset();
}

}  // namespace views
