// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/transient_window_stacking_client.h"

#include <stddef.h>

#include <algorithm>

#include "ui/aura/client/transient_window_client.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

using aura::Window;

namespace wm {

namespace {

// Populates |ancestors| with all transient ancestors of |window| that are
// siblings of |window|. Returns true if any ancestors were found, false if not.
bool GetAllTransientAncestors(Window* window, Window::Windows* ancestors) {
  Window* parent = window->parent();
  for (; window; window = GetTransientParent(window)) {
    if (window->parent() == parent)
      ancestors->push_back(window);
  }
  return (!ancestors->empty());
}

// Replaces |window1| and |window2| with their possible transient ancestors that
// are still siblings (have a common transient parent).  |window1| and |window2|
// are not modified if such ancestors cannot be found.
void FindCommonTransientAncestor(Window** window1, Window** window2) {
  DCHECK(window1);
  DCHECK(window2);
  DCHECK(*window1);
  DCHECK(*window2);
  // Assemble chains of ancestors of both windows.
  Window::Windows ancestors1;
  Window::Windows ancestors2;
  if (!GetAllTransientAncestors(*window1, &ancestors1) ||
      !GetAllTransientAncestors(*window2, &ancestors2)) {
    return;
  }
  // Walk the two chains backwards and look for the first difference.
  auto it1 = ancestors1.rbegin();
  auto it2 = ancestors2.rbegin();
  for (; it1  != ancestors1.rend() && it2  != ancestors2.rend(); ++it1, ++it2) {
    if (*it1 != *it2) {
      *window1 = *it1;
      *window2 = *it2;
      break;
    }
  }
}

}  // namespace

// static
TransientWindowStackingClient* TransientWindowStackingClient::instance_ = NULL;

TransientWindowStackingClient::TransientWindowStackingClient() {
  instance_ = this;
}

TransientWindowStackingClient::~TransientWindowStackingClient() {
  if (instance_ == this)
    instance_ = NULL;
}

bool TransientWindowStackingClient::AdjustStacking(
    Window** child,
    Window** target,
    Window::StackDirection* direction) {
  const TransientWindowManager* transient_manager =
      TransientWindowManager::GetIfExists(*child);
  if (transient_manager && transient_manager->IsStackingTransient(*target))
    return true;

  // For windows that have transient children stack the transient ancestors that
  // are siblings. This prevents one transient group from being inserted in the
  // middle of another.
  FindCommonTransientAncestor(child, target);

  // When stacking above skip to the topmost transient descendant of the target.
  if (*direction == Window::STACK_ABOVE &&
      !HasTransientAncestor(*child, *target)) {
    const Window::Windows& siblings((*child)->parent()->children());
    size_t target_i =
        std::find(siblings.begin(), siblings.end(), *target) - siblings.begin();
    while (target_i + 1 < siblings.size() &&
           HasTransientAncestor(siblings[target_i + 1], *target)) {
      ++target_i;
    }
    *target = siblings[target_i];
  }

  return *child != *target;
}

}  // namespace wm
