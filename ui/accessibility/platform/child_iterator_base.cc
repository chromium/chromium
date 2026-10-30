// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/child_iterator_base.h"

#include "ui/accessibility/platform/ax_platform_node.h"

namespace ui {

ChildIteratorBase::ChildIteratorBase(const AXPlatformNodeDelegate* parent,
                                     size_t index)
    : parent_(parent), index_(index) {
  CHECK(parent, base::NotFatalUntil::M163);
  CHECK_LE(index, parent->GetChildCount(), base::NotFatalUntil::M163);
}

ChildIteratorBase::ChildIteratorBase(const ChildIteratorBase& it)
    : parent_(it.parent_), index_(it.index_) {
  CHECK(parent_, base::NotFatalUntil::M163);
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
  CHECK_GT(index_, 0u, base::NotFatalUntil::M163);
  index_--;
  return *this;
}

ChildIteratorBase ChildIteratorBase::operator--(int) {
  CHECK_GT(index_, 0u, base::NotFatalUntil::M163);
  ChildIteratorBase previous_state = *this;
  index_--;
  return previous_state;
}

gfx::NativeViewAccessible ChildIteratorBase::GetNativeViewAccessible() const {
  if (index_ < parent_->GetChildCount()) {
    return parent_->ChildAtIndex(index_);
  }

  return gfx::NativeViewAccessible();
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
  CHECK(platform_node, base::NotFatalUntil::M163);
  return *(platform_node->GetDelegate());
}

AXPlatformNodeDelegate* ChildIteratorBase::operator->() const {
  return get();
}

}  // namespace ui
