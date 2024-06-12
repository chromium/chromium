// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/sublevel_manager.h"

#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"

namespace {

bool ShouldStackAboveParent(views::Widget* widget) {
#if !BUILDFLAG(IS_MAC)
  return false;
#else
  // macOS bug: a child widget might be rendered behind its parent in fullscreen
  // if the child is not explicitly StackAbove()'ed its parent.
  int level = 0;
  views::Widget* root = widget;
  while (root->parent()) {
    root = root->parent();
    // StackAbove() will make `widget` visible. We don't want this when its
    // ancestor is invisible.
    if (!root->IsVisible()) {
      return false;
    }
    level++;
  }
  // Only StackAbove() when `widget` is a grandchild (or deeper) of the root
  // fullscreen window.
  return level > 1 && root->IsFullscreen();
#endif
}

}  // namespace

namespace views {

SublevelManager::SublevelManager(Widget* owner, int sublevel)
    : owner_(owner), sublevel_(sublevel) {
  owner_observation_.Observe(owner);
}

SublevelManager::~SublevelManager() = default;

void SublevelManager::TrackChildWidget(Widget* child) {
  DCHECK_EQ(0, base::ranges::count(children_, child));
  DCHECK(child->parent() == owner_);
  children_.push_back(child);
}

void SublevelManager::UntrackChildWidget(Widget* child) {
  // During shutdown a child might get untracked more than once by the same
  // parent. We don't want to DCHECK on that.
  children_.erase(base::ranges::remove(children_, child), std::end(children_));
}

void SublevelManager::SetSublevel(int sublevel) {
  sublevel_ = sublevel;
  EnsureOwnerSublevel();
}

int SublevelManager::GetSublevel() const {
  return sublevel_;
}

void SublevelManager::EnsureOwnerSublevel() {
  // Walk through the path to the root and ensure sublevel on every widget
  // on the path. This is to work around the behavior on some platforms
  // where showing an activatable widget brings its ancestors to the front.
  Widget* parent = owner_->parent();
  Widget* child = owner_;
  while (parent && parent->GetSublevelManager()->IsTrackingChildWidget(child)) {
    parent->GetSublevelManager()->OrderChildWidget(child);
    child = parent;
    parent = parent->parent();
  }
}

void SublevelManager::EnsureOwnerTreeSublevel() {
  for (Widget* child : children_) {
    child->GetSublevelManager()->EnsureOwnerTreeSublevel();
  }

  if (Widget* parent = owner_->parent()) {
    parent->GetSublevelManager()->OrderChildWidget(owner_);
  }
}

void SublevelManager::OrderChildWidget(Widget* child) {
  DCHECK_EQ(1, base::ranges::count(children_, child));
  children_.erase(base::ranges::remove(children_, child), std::end(children_));

  if (ShouldStackAboveParent(child)) {
    child->StackAboveWidget(owner_);
  }

  ui::ZOrderLevel child_level = child->GetZOrderLevel();
  auto insert_it = FindInsertPosition(child);

  // Stacking above an invisible widget is a no-op on Mac. Therefore, find only
  // visible ones.
  auto find_visible_widget_of_same_level = [child_level](Widget* widget) {
    return widget->IsVisible() && widget->GetZOrderLevel() == child_level;
  };

  auto prev_it = base::ranges::find_if(std::make_reverse_iterator(insert_it),
                                       std::crend(children_),
                                       find_visible_widget_of_same_level);

  if (prev_it == children_.rend()) {
    // x11 bug: stacking above the base `owner_` will cause `child` to become
    // unresponsive after the base widget is minimized. As a workaround, we
    // position `child` relative to the next child widget.

    // Find the closest next widget at the same level.
    auto next_it = base::ranges::find_if(insert_it, std::cend(children_),
                                         find_visible_widget_of_same_level);

    // Put `child` below `next_it`.
    if (next_it != std::end(children_)) {
      child->StackAboveWidget(*next_it);
      (*next_it)->StackAboveWidget(child);
    }
  } else {
    child->StackAboveWidget(*prev_it);
  }

  children_.insert(insert_it, child);
}

void SublevelManager::OnWidgetDestroying(Widget* owner) {
  DCHECK(owner == owner_);
  if (owner->parent())
    owner->parent()->GetSublevelManager()->UntrackChildWidget(owner);
}

bool SublevelManager::IsTrackingChildWidget(Widget* child) {
  return base::ranges::find(children_, child) != children_.end();
}

SublevelManager::ChildIterator SublevelManager::FindInsertPosition(
    Widget* child) const {
  ui::ZOrderLevel child_level = child->GetZOrderLevel();
  int child_sublevel = child->GetZOrderSublevel();
  return base::ranges::find_if(children_, [&](Widget* widget) {
    return widget->GetZOrderLevel() == child_level &&
           widget->GetZOrderSublevel() > child_sublevel;
  });
}

}  // namespace views
