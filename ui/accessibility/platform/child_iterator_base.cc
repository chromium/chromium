// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/child_iterator_base.h"

#include "ui/accessibility/platform/ax_platform_node.h"

namespace ui {

ChildIteratorBase::ChildIteratorBase(const AXPlatformNodeDelegate* parent,
                                     size_t index)
    : parent_(parent), index_(index) {
  DCHECK(parent);
  DCHECK_LE(index, parent->GetChildCount());
}

ChildIteratorBase::ChildIteratorBase(const ChildIteratorBase& it)
    : parent_(it.parent_), index_(it.index_) {
  DCHECK(parent_);
}

ChildIteratorBase& ChildIteratorBase::operator++() {
  index_++;
  return *this;
}

ChildIteratorBase ChildIteratorBase::operator++(int) {
  ChildIteratorBase previous_state = *this;
  index_++;
  return previous_state;
}

ChildIteratorBase& ChildIteratorBase::operator--() {
  DCHECK_GT(index_, 0u);
  index_--;
  return *this;
}

ChildIteratorBase ChildIteratorBase::operator--(int) {
  DCHECK_GT(index_, 0u);
  ChildIteratorBase previous_state = *this;
  index_--;
  return previous_state;
}

gfx::NativeViewAccessible ChildIteratorBase::GetNativeViewAccessible() const {
  if (index_ < parent_->GetChildCount())
    return parent_->ChildAtIndex(index_);

  return nullptr;
}

std::optional<size_t> ChildIteratorBase::GetIndexInParent() const {
  return index_;
}

AXPlatformNodeDelegate* ChildIteratorBase::get() const {
  AXPlatformNode* platform_node =
      AXPlatformNode::FromNativeViewAccessible(GetNativeViewAccessible());
  return platform_node ? platform_node->GetDelegate() : nullptr;
}

AXPlatformNodeDelegate& ChildIteratorBase::operator*() const {
  AXPlatformNode* platform_node =
      AXPlatformNode::FromNativeViewAccessible(GetNativeViewAccessible());
  DCHECK(platform_node && platform_node->GetDelegate());
  return *(platform_node->GetDelegate());
}

AXPlatformNodeDelegate* ChildIteratorBase::operator->() const {
  return get();
}

}  // namespace ui
