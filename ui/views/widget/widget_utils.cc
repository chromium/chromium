// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_utils.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace views {

namespace {

std::vector<ui::Layer*> GetLayerPath(ui::Layer* layer) {
  std::vector<ui::Layer*> path;
  for (ui::Layer* l = layer; l; l = l->parent()) {
    path.push_back(l);
  }
  std::reverse(path.begin(), path.end());
  return path;
}

}  // namespace

WidgetOpenTimer::WidgetOpenTimer(Callback callback)
    : callback_(std::move(callback)) {}

WidgetOpenTimer::~WidgetOpenTimer() = default;

void WidgetOpenTimer::OnWidgetDestroying(Widget* widget) {
  DCHECK(open_timer_.has_value());
  DCHECK(observed_widget_.IsObservingSource(widget));
  callback_.Run(open_timer_->Elapsed());
  open_timer_.reset();
  observed_widget_.Reset();
}

void WidgetOpenTimer::Reset(Widget* widget) {
  DCHECK(!open_timer_.has_value());
  DCHECK(!observed_widget_.IsObservingSource(widget));
  observed_widget_.Observe(widget);
  open_timer_ = base::ElapsedTimer();
}

gfx::NativeWindow GetRootWindow(const Widget* widget) {
  gfx::NativeWindow window = widget->GetNativeWindow();
#if defined(USE_AURA)
  window = window->GetRootWindow();
#endif
  return window;
}

LayerRelation GetLayerRelation(ui::Layer* first, ui::Layer* second) {
  LayerRelation relation;
  if (!first || !second) {
    return relation;
  }

  if (first == second) {
    relation.type = LayerRelation::Type::kFirstIsChildOfSecond;
    relation.common_parent = first;
    return relation;
  }

  std::vector<ui::Layer*> path_first = GetLayerPath(first);
  std::vector<ui::Layer*> path_second = GetLayerPath(second);

  // If they don't share the same root, they are disjoint.
  if (path_first.empty() || path_second.empty() ||
      path_first[0] != path_second[0]) {
    return relation;
  }

  // Find the last common ancestor.
  size_t i = 0;
  while (i < path_first.size() && i < path_second.size() &&
         path_first[i] == path_second[i]) {
    i++;
  }
  i--;  // Step back to the last common ancestor.

  ui::Layer* common_parent = path_first[i];
  relation.common_parent = common_parent;

  if (i == (path_first.size() - 1)) {
    // First is an ancestor of second.
    relation.type = LayerRelation::Type::kSecondIsChildOfFirst;
    relation.ancestor_of_second = path_second[i + 1];
  } else if (i == (path_second.size() - 1)) {
    // Second is an ancestor of first.
    relation.type = LayerRelation::Type::kFirstIsChildOfSecond;
    relation.ancestor_of_first = path_first[i + 1];
  } else {
    // Sibling branches.
    relation.type = LayerRelation::Type::kSiblings;
    relation.ancestor_of_first = path_first[i + 1];
    relation.ancestor_of_second = path_second[i + 1];
  }

  return relation;
}

}  // namespace views
