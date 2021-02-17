// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_utils.h"

namespace views {

ViewDebugWrapperImpl::ViewDebugWrapperImpl(View* view) : view_(view) {}

ViewDebugWrapperImpl::~ViewDebugWrapperImpl() = default;

std::string ViewDebugWrapperImpl::GetViewClassName() {
  return view_->GetClassName();
}

int ViewDebugWrapperImpl::GetID() {
  return view_->GetID();
}

debug::ViewDebugWrapper::BoundsTuple ViewDebugWrapperImpl::GetBounds() {
  const auto& bounds = view_->bounds();
  return BoundsTuple(bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

bool ViewDebugWrapperImpl::GetVisible() {
  return view_->GetVisible();
}

bool ViewDebugWrapperImpl::GetNeedsLayout() {
  return view_->needs_layout();
}

bool ViewDebugWrapperImpl::GetEnabled() {
  return view_->GetEnabled();
}

std::vector<debug::ViewDebugWrapper*> ViewDebugWrapperImpl::GetChildren() {
  children_.clear();
  for (auto* child : view_->children())
    children_.push_back(std::make_unique<ViewDebugWrapperImpl>(child));

  std::vector<debug::ViewDebugWrapper*> child_ptrs;
  for (auto& child : children_)
    child_ptrs.push_back(child.get());
  return child_ptrs;
}

void ViewDebugWrapperImpl::ForAllProperties(PropCallback callback) {
  views::View* view = const_cast<views::View*>(view_);
  for (auto* member : *(view->GetClassMetaData())) {
    auto flags = member->GetPropertyFlags();
    if (!!(flags & views::metadata::PropertyFlags::kSerializable)) {
      callback.Run(member->member_name(),
                   base::UTF16ToUTF8(member->GetValueAsString(view)));
    }
  }
}

void PrintViewHierarchy(View* view, bool verbose, int depth) {
  ViewDebugWrapperImpl debug_view(view);
  std::ostringstream out;
  debug::PrintViewHierarchy(&out, &debug_view, verbose, depth);
  LOG(ERROR) << '\n' << out.str();
}

}  // namespace views
