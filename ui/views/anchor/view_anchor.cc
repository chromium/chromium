// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/anchor/view_anchor.h"

#include <memory>

#include "ui/views/widget/widget.h"

namespace views {

ViewAnchorImpl::ViewAnchorImpl(View* anchor_view)
    : view_tracker_(anchor_view) {}

ViewAnchorImpl::~ViewAnchorImpl() = default;

std::unique_ptr<ui::AnchorImpl> ViewAnchorImpl::Clone() const {
  return std::make_unique<ViewAnchorImpl>(const_cast<View*>(view()));
}

bool ViewAnchorImpl::IsEmpty() const {
  return view() == nullptr;
}

gfx::Rect ViewAnchorImpl::GetScreenBounds() const {
  return view() ? view()->GetAnchorBoundsInScreen() : gfx::Rect();
}

Widget* ViewAnchorImpl::GetWidget() {
  return view() ? view()->GetWidget() : nullptr;
}

bool ViewAnchorImpl::IsView() const {
  return true;
}

views::View* ViewAnchorImpl::GetView() {
  return view();
}

}  // namespace views

namespace ui {

Anchor::Anchor(views::View* anchor_view)
    : impl_(std::make_unique<views::ViewAnchorImpl>(anchor_view)) {}

}  // namespace ui
