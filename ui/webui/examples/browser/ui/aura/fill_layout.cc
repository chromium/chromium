// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/ui/aura/fill_layout.h"

#include "ui/aura/window.h"

namespace webui_examples {

FillLayout::FillLayout(aura::Window* root) : root_(root) {}

FillLayout::~FillLayout() = default;

void FillLayout::OnWindowResized() {
  if (root_->bounds().IsEmpty())
    return;
  for (aura::Window* child : root_->children())
    SetChildBoundsDirect(child, gfx::Rect(root_->bounds().size()));
}

void FillLayout::OnWindowAddedToLayout(aura::Window* child) {
  child->SetBounds(root_->bounds());
}

void FillLayout::OnWillRemoveWindowFromLayout(aura::Window* child) {}

void FillLayout::OnWindowRemovedFromLayout(aura::Window* child) {}

void FillLayout::OnChildWindowVisibilityChanged(aura::Window* child,
                                                bool visible) {}

void FillLayout::SetChildBounds(aura::Window* child,
                                const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
}

}  // namespace webui_examples
